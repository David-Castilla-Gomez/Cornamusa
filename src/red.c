/*
 * Cornamusa v1.29 — Cliente HTTP/1.1 plano (implementación).
 *
 * Flujo:
 *   1. Parsear URL: scheme, host, puerto, path.
 *   2. getaddrinfo() para resolver host.
 *   3. socket() + connect() con timeout.
 *   4. send() del request HTTP.
 *   5. recv() en bucle hasta EOF o tamaño completo.
 *   6. Parsear status line, headers, body.
 *
 * No soporta:
 *   - HTTPS (TLS).
 *   - Redirecciones automáticas (el caller decide qué hacer con 3xx).
 *   - Keep-alive (cada request nueva conexión).
 *   - Transfer-Encoding: chunked (asume Content-Length o EOF).
 *   - Compresión (gzip).
 */

#include "red.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_MAX_RESPUESTA (16 * 1024 * 1024)  /* 16 MB */

#if defined(_WIN32) || defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

typedef int socklen_t;
typedef SOCKET socket_t;
#define INVALID_SOCK INVALID_SOCKET
#define SOCK_CLOSE(s) closesocket(s)
#define SOCK_LAST_ERR() WSAGetLastError()

static int winsock_init(void) {
    static int hecho = 0;
    if (hecho) return 0;
    WSADATA wsa;
    int r = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (r != 0) return -1;
    hecho = 1;
    return 0;
}

#else

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

typedef int socket_t;
#define INVALID_SOCK (-1)
#define SOCK_CLOSE(s) close(s)
#define SOCK_LAST_ERR() errno
static int winsock_init(void) { return 0; }

#endif

/* ────────────────────────────────────────────────────────────────
 * URL parsing
 * ──────────────────────────────────────────────────────────────── */

typedef struct {
    char host[256];
    int  puerto;
    char path[2048];
} UrlParsed;

static bool parsear_url(const char *url, UrlParsed *out, char *err, size_t err_cap) {
    if (strncmp(url, "http://", 7) != 0) {
        snprintf(err, err_cap, "URL debe empezar con 'http://' (v1.29 sin TLS)");
        return false;
    }
    const char *p = url + 7;
    /* Host hasta `:` `/` o final. */
    const char *q = p;
    while (*q && *q != ':' && *q != '/') q++;
    int host_len = (int)(q - p);
    if (host_len == 0 || host_len >= (int)sizeof(out->host)) {
        snprintf(err, err_cap, "host en URL vacío o demasiado largo");
        return false;
    }
    memcpy(out->host, p, (size_t)host_len);
    out->host[host_len] = '\0';
    /* Puerto opcional. */
    out->puerto = 80;
    p = q;
    if (*p == ':') {
        p++;
        char puerto_buf[8] = {0};
        int i = 0;
        while (*p && *p != '/' && i < 7) {
            if (*p < '0' || *p > '9') {
                snprintf(err, err_cap, "puerto invalido en URL");
                return false;
            }
            puerto_buf[i++] = *p++;
        }
        out->puerto = atoi(puerto_buf);
        if (out->puerto <= 0 || out->puerto > 65535) {
            snprintf(err, err_cap, "puerto fuera de rango");
            return false;
        }
    }
    /* Path: lo restante (incluye `/` si está). Si no hay nada, `/`. */
    if (*p == '\0') {
        strcpy(out->path, "/");
    } else {
        int path_len = (int)strlen(p);
        if (path_len >= (int)sizeof(out->path)) {
            snprintf(err, err_cap, "path en URL demasiado largo");
            return false;
        }
        memcpy(out->path, p, (size_t)path_len + 1);
    }
    return true;
}

/* ────────────────────────────────────────────────────────────────
 * Conexión TCP
 * ──────────────────────────────────────────────────────────────── */

static socket_t conectar_tcp(const char *host, int puerto, int timeout_seg,
                              char *err, size_t err_cap) {
    if (winsock_init() != 0) {
        snprintf(err, err_cap, "WSAStartup fallo");
        return INVALID_SOCK;
    }
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo *res = NULL;
    char puerto_str[16];
    snprintf(puerto_str, sizeof(puerto_str), "%d", puerto);
    int r = getaddrinfo(host, puerto_str, &hints, &res);
    if (r != 0 || !res) {
        snprintf(err, err_cap, "getaddrinfo('%s') fallo (err=%d)", host, r);
        if (res) freeaddrinfo(res);
        return INVALID_SOCK;
    }
    socket_t s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCK) {
        snprintf(err, err_cap, "socket() fallo");
        freeaddrinfo(res);
        return INVALID_SOCK;
    }
    /* Timeout de recv/send. */
#if defined(_WIN32) || defined(_MSC_VER)
    DWORD to = (DWORD)(timeout_seg * 1000);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&to, sizeof(to));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&to, sizeof(to));
#else
    struct timeval tv = { timeout_seg, 0 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
#endif
    if (connect(s, res->ai_addr, (socklen_t)res->ai_addrlen) != 0) {
        snprintf(err, err_cap, "connect('%s:%d') fallo (err=%d)",
            host, puerto, SOCK_LAST_ERR());
        SOCK_CLOSE(s);
        freeaddrinfo(res);
        return INVALID_SOCK;
    }
    freeaddrinfo(res);
    return s;
}

/* ────────────────────────────────────────────────────────────────
 * Send / Recv buffers
 * ──────────────────────────────────────────────────────────────── */

static bool enviar_todo(socket_t s, const char *buf, int len,
                          char *err, size_t err_cap) {
    int total = 0;
    while (total < len) {
        int n = send(s, buf + total, len - total, 0);
        if (n <= 0) {
            snprintf(err, err_cap, "send() fallo (err=%d)", SOCK_LAST_ERR());
            return false;
        }
        total += n;
    }
    return true;
}

/* Lee hasta EOF o hasta el límite. Retorna un buffer heap con todo el
   contenido y len_out con su tamaño. */
static char *recv_todo(socket_t s, int *len_out, char *err, size_t err_cap) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        snprintf(err, err_cap, "memoria insuficiente");
        return NULL;
    }
    for (;;) {
        if (len + 4096 > cap) {
            if (cap >= HTTP_MAX_RESPUESTA) {
                snprintf(err, err_cap, "respuesta excede %d bytes", HTTP_MAX_RESPUESTA);
                free(buf);
                return NULL;
            }
            cap *= 2;
            char *nuevo = (char *)realloc(buf, cap);
            if (!nuevo) {
                snprintf(err, err_cap, "memoria insuficiente");
                free(buf);
                return NULL;
            }
            buf = nuevo;
        }
        int n = recv(s, buf + len, (int)(cap - len), 0);
        if (n < 0) {
            snprintf(err, err_cap, "recv() fallo (err=%d)", SOCK_LAST_ERR());
            free(buf);
            return NULL;
        }
        if (n == 0) break;  /* EOF */
        len += (size_t)n;
    }
    *len_out = (int)len;
    return buf;
}

/* ────────────────────────────────────────────────────────────────
 * Parseo de respuesta HTTP
 * ──────────────────────────────────────────────────────────────── */

static bool parsear_respuesta(const char *raw, int raw_len,
                                RedHttpResultado *out,
                                char *err, size_t err_cap) {
    /* Línea de status: "HTTP/1.1 200 OK\r\n" */
    if (raw_len < 12 || strncmp(raw, "HTTP/", 5) != 0) {
        snprintf(err, err_cap, "respuesta no es HTTP");
        return false;
    }
    /* Buscar fin de status line. */
    const char *fin_status = (const char *)memchr(raw, '\n', (size_t)raw_len);
    if (!fin_status) {
        snprintf(err, err_cap, "status line sin \\n");
        return false;
    }
    /* Status code es el segundo token. */
    const char *sp = (const char *)memchr(raw, ' ', (size_t)(fin_status - raw));
    if (!sp) {
        snprintf(err, err_cap, "status line malformada");
        return false;
    }
    out->codigo = atoi(sp + 1);
    /* Cabeceras: desde fin_status+1 hasta "\r\n\r\n". */
    const char *inicio_headers = fin_status + 1;
    const char *fin_headers = NULL;
    for (const char *p = inicio_headers; p + 3 <= raw + raw_len; p++) {
        if (p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n') {
            fin_headers = p;
            break;
        }
        if (p[0] == '\n' && p[1] == '\n') {  /* LF only */
            fin_headers = p;
            break;
        }
    }
    if (!fin_headers) {
        snprintf(err, err_cap, "cabeceras sin separador en blanco");
        return false;
    }
    int cab_len = (int)(fin_headers - inicio_headers);
    out->cabeceras_raw = (char *)malloc((size_t)cab_len + 1);
    if (!out->cabeceras_raw) {
        snprintf(err, err_cap, "memoria insuficiente");
        return false;
    }
    memcpy(out->cabeceras_raw, inicio_headers, (size_t)cab_len);
    out->cabeceras_raw[cab_len] = '\0';
    out->cabeceras_len = cab_len;
    /* Cuerpo: desde fin_headers+4 (o +2 si LFLF). */
    int salto = (fin_headers[0] == '\r') ? 4 : 2;
    int cuerpo_offset = (int)(fin_headers - raw) + salto;
    int cuerpo_len = raw_len - cuerpo_offset;
    if (cuerpo_len < 0) cuerpo_len = 0;
    out->cuerpo = (char *)malloc((size_t)cuerpo_len + 1);
    if (!out->cuerpo) {
        free(out->cabeceras_raw);
        out->cabeceras_raw = NULL;
        snprintf(err, err_cap, "memoria insuficiente");
        return false;
    }
    if (cuerpo_len > 0) {
        memcpy(out->cuerpo, raw + cuerpo_offset, (size_t)cuerpo_len);
    }
    out->cuerpo[cuerpo_len] = '\0';
    out->cuerpo_len = cuerpo_len;
    return true;
}

/* ────────────────────────────────────────────────────────────────
 * API pública
 * ──────────────────────────────────────────────────────────────── */

int red_http_obtener_c(const char *url,
                         const char *cabeceras_extra,
                         int timeout_seg,
                         RedHttpResultado *out) {
    memset(out, 0, sizeof(*out));
    UrlParsed u = {0};
    if (!parsear_url(url, &u, out->mensaje_error, sizeof(out->mensaje_error))) {
        return -1;
    }
    socket_t s = conectar_tcp(u.host, u.puerto, timeout_seg,
                                out->mensaje_error, sizeof(out->mensaje_error));
    if (s == INVALID_SOCK) return -1;

    /* Construir request GET. */
    char req[8192];
    int n = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Cornamusa/1.29\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        u.path, u.host,
        cabeceras_extra ? cabeceras_extra : "");
    if (n < 0 || n >= (int)sizeof(req)) {
        SOCK_CLOSE(s);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "request HTTP demasiado grande (>8KB)");
        return -1;
    }
    if (!enviar_todo(s, req, n,
                       out->mensaje_error, sizeof(out->mensaje_error))) {
        SOCK_CLOSE(s);
        return -1;
    }
    int raw_len = 0;
    char *raw = recv_todo(s, &raw_len,
                            out->mensaje_error, sizeof(out->mensaje_error));
    SOCK_CLOSE(s);
    if (!raw) return -1;
    bool ok = parsear_respuesta(raw, raw_len, out,
                                  out->mensaje_error, sizeof(out->mensaje_error));
    free(raw);
    if (!ok) return -1;
    return 0;
}
