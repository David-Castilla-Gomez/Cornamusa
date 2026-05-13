/*
 * Cornamusa v1.27 — Implementación cross-platform de `proceso_ejecutar_c`.
 *
 * Estrategia:
 *   - Windows: CreateProcess + pipes anónimos creados con
 *     CreatePipe() y SetHandleInformation para hacer heredables el
 *     extremo escritura del child.
 *   - POSIX: fork() en el padre; en el child execvp() tras dup2 de
 *     pipes a stdout/stderr.
 *
 * Lectura de pipes: en Windows usamos ReadFile en bucle; en POSIX
 * leemos con `read(2)` hasta EOF. La lectura es secuencial — primero
 * vacía stdout, luego stderr. Esto puede causar bloqueo si el child
 * escribe muchísimo a stderr mientras stdout está vacío y luego
 * intenta más stdout — pero para v1.27 (sin streaming) es aceptable
 * porque limitamos a 10 MB y la mayoría de procesos cooperativos
 * no tienen este patrón patológico.
 *
 * En POSIX usamos un loop con select() para evitar el deadlock arriba,
 * leyendo del pipe que tiene datos. Windows usa el ordenamiento
 * stdout-luego-stderr más simple por ahora.
 */

#include "proceso.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROC_MAX_OUTPUT (10 * 1024 * 1024)  /* 10 MB de tope por stream */

#if defined(_WIN32) || defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Leer todo lo disponible de un handle hasta EOF. Buffer crece dinámicamente. */
static int leer_handle_hasta_eof(HANDLE h, char **out_buf, int *out_len,
                                   char *err, size_t err_cap) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        snprintf(err, err_cap, "memoria insuficiente al leer pipe");
        return -1;
    }
    for (;;) {
        if (len + 4096 > cap) {
            if (cap >= PROC_MAX_OUTPUT) {
                snprintf(err, err_cap,
                    "salida excede %d bytes (limite v1.27)", PROC_MAX_OUTPUT);
                free(buf);
                return -1;
            }
            cap *= 2;
            char *nuevo = (char *)realloc(buf, cap);
            if (!nuevo) {
                snprintf(err, err_cap, "memoria insuficiente al leer pipe");
                free(buf);
                return -1;
            }
            buf = nuevo;
        }
        DWORD leidos = 0;
        BOOL ok = ReadFile(h, buf + len, (DWORD)(cap - len), &leidos, NULL);
        if (!ok || leidos == 0) break;  /* EOF o broken pipe */
        len += leidos;
    }
    *out_buf = buf;
    *out_len = (int)len;
    return 0;
}

/* Devuelve 1 si la cadena necesita entrecomillado para Win32:
   contiene espacio/tab/comilla, o está vacía. */
static int arg_necesita_comillas(const char *a) {
    if (*a == '\0') return 1;
    for (const char *p = a; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == '"') return 1;
    }
    return 0;
}

/* Construye la línea de comando con escapado mínimo Win32. Solo
   entrecomilla los args con espacios/comillas — necesario porque
   cmd.exe es sensible al entrecomillado redundante (cmd /c "echo"
   "hola" no funciona como "cmd /c echo hola"). Para comillas internas
   las escapamos con backslash, regla MS_CRT estándar. */
static char *construir_comando_win(const char *programa,
                                     const char *const *argv) {
    size_t cap = 1024;
    char *cmd = (char *)malloc(cap);
    if (!cmd) return NULL;
    size_t len = 0;
    #define APPEND_STR(s, n) do { \
        if (len + (n) + 4 >= cap) { \
            while (cap < len + (n) + 4) cap *= 2; \
            char *nuevo = (char *)realloc(cmd, cap); \
            if (!nuevo) { free(cmd); return NULL; } \
            cmd = nuevo; \
        } \
        memcpy(cmd + len, (s), (n)); \
        len += (n); \
    } while (0)

    /* Programa: siempre entrecomillado (puede tener espacios en path). */
    APPEND_STR("\"", 1);
    APPEND_STR(programa, strlen(programa));
    APPEND_STR("\"", 1);

    /* Args: argv[0] es convención (nombre del programa); lo saltamos.
       Para cada arg subsiguiente, entrecomillar solo si necesario. */
    for (int i = 0; argv[i] != NULL; i++) {
        if (i == 0) continue;
        const char *a = argv[i];
        int comillas = arg_necesita_comillas(a);
        APPEND_STR(" ", 1);
        if (comillas) APPEND_STR("\"", 1);
        for (const char *p = a; *p; p++) {
            if (*p == '"') {
                APPEND_STR("\\\"", 2);
            } else {
                APPEND_STR(p, 1);
            }
        }
        if (comillas) APPEND_STR("\"", 1);
    }
    APPEND_STR("\0", 1);
    return cmd;
    #undef APPEND_STR
}

int proceso_ejecutar_c(const char *programa,
                        const char *const *argv,
                        ProcesoResultado *out) {
    memset(out, 0, sizeof(*out));
    out->exit_codigo = -1;

    /* Pipes para stdout y stderr. inheritFlag en el extremo escritura. */
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdout_r = NULL, stdout_w = NULL;
    HANDLE stderr_r = NULL, stderr_w = NULL;
    if (!CreatePipe(&stdout_r, &stdout_w, &sa, 0)) {
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "CreatePipe(stdout) fallo (err=%lu)", GetLastError());
        return -1;
    }
    if (!SetHandleInformation(stdout_r, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdout_r); CloseHandle(stdout_w);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "SetHandleInformation(stdout_r) fallo");
        return -1;
    }
    if (!CreatePipe(&stderr_r, &stderr_w, &sa, 0)) {
        CloseHandle(stdout_r); CloseHandle(stdout_w);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "CreatePipe(stderr) fallo");
        return -1;
    }
    if (!SetHandleInformation(stderr_r, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdout_r); CloseHandle(stdout_w);
        CloseHandle(stderr_r); CloseHandle(stderr_w);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "SetHandleInformation(stderr_r) fallo");
        return -1;
    }

    char *cmdline = construir_comando_win(programa, argv);
    if (!cmdline) {
        CloseHandle(stdout_r); CloseHandle(stdout_w);
        CloseHandle(stderr_r); CloseHandle(stderr_w);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "memoria insuficiente al construir comando");
        return -1;
    }

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = stdout_w;
    si.hStdError = stderr_w;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);  /* hereda stdin del padre */
    PROCESS_INFORMATION pi = {0};
    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL,
                              TRUE,  /* inherit handles */
                              0, NULL, NULL, &si, &pi);
    free(cmdline);

    /* Cerrar los extremos escritura del padre — el child los hereda. */
    CloseHandle(stdout_w);
    CloseHandle(stderr_w);

    if (!ok) {
        CloseHandle(stdout_r); CloseHandle(stderr_r);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "CreateProcess fallo: %s (err=%lu)", programa, GetLastError());
        return -1;
    }

    /* Leer stdout y stderr. Orden: primero stdout, luego stderr. Para
       buffers pequeños (<64 KB) el OS bufferea ambos sin bloquear. */
    int rc = leer_handle_hasta_eof(stdout_r, &out->stdout_buf, &out->stdout_len,
                                     out->mensaje_error, sizeof(out->mensaje_error));
    if (rc != 0) {
        CloseHandle(stdout_r); CloseHandle(stderr_r);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return -1;
    }
    rc = leer_handle_hasta_eof(stderr_r, &out->stderr_buf, &out->stderr_len,
                                 out->mensaje_error, sizeof(out->mensaje_error));
    CloseHandle(stdout_r);
    CloseHandle(stderr_r);
    if (rc != 0) {
        free(out->stdout_buf); out->stdout_buf = NULL;
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return -1;
    }

    /* Esperar al child y leer exit code. */
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out->exit_codigo = (int)exit_code;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

#else  /* POSIX */

#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

static int leer_fd_hasta_eof(int fd, char **out_buf, int *out_len,
                              char *err, size_t err_cap) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        snprintf(err, err_cap, "memoria insuficiente al leer pipe");
        return -1;
    }
    for (;;) {
        if (len + 4096 > cap) {
            if (cap >= PROC_MAX_OUTPUT) {
                snprintf(err, err_cap,
                    "salida excede %d bytes", PROC_MAX_OUTPUT);
                free(buf);
                return -1;
            }
            cap *= 2;
            char *nuevo = (char *)realloc(buf, cap);
            if (!nuevo) {
                snprintf(err, err_cap, "memoria insuficiente");
                free(buf);
                return -1;
            }
            buf = nuevo;
        }
        ssize_t n = read(fd, buf + len, cap - len);
        if (n < 0) {
            if (errno == EINTR) continue;
            snprintf(err, err_cap, "read fallo: %s", strerror(errno));
            free(buf);
            return -1;
        }
        if (n == 0) break;
        len += (size_t)n;
    }
    *out_buf = buf;
    *out_len = (int)len;
    return 0;
}

int proceso_ejecutar_c(const char *programa,
                        const char *const *argv,
                        ProcesoResultado *out) {
    memset(out, 0, sizeof(*out));
    out->exit_codigo = -1;

    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    if (pipe(stdout_pipe) != 0) {
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "pipe(stdout) fallo: %s", strerror(errno));
        return -1;
    }
    if (pipe(stderr_pipe) != 0) {
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "pipe(stderr) fallo: %s", strerror(errno));
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "fork fallo: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        /* Child: dup pipes a stdout/stderr y exec. */
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        /* execvp acepta `char *const argv[]`; el caller ya hizo el cast. */
        execvp(programa, (char *const *)argv);
        /* Si llegamos aquí, exec falló. */
        fprintf(stderr, "execvp fallo: %s\n", strerror(errno));
        _exit(127);
    }

    /* Padre: cerrar extremos escritura. */
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    /* Leer ambos pipes alternando con select() para evitar deadlock. */
    int out_fd = stdout_pipe[0];
    int err_fd = stderr_pipe[0];
    size_t cap_o = 4096, cap_e = 4096;
    size_t len_o = 0, len_e = 0;
    char *buf_o = (char *)malloc(cap_o);
    char *buf_e = (char *)malloc(cap_e);
    if (!buf_o || !buf_e) {
        free(buf_o); free(buf_e);
        close(out_fd); close(err_fd);
        waitpid(pid, NULL, 0);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "memoria insuficiente al asignar buffers");
        return -1;
    }
    int o_abierto = 1, e_abierto = 1;
    while (o_abierto || e_abierto) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        if (o_abierto) { FD_SET(out_fd, &rfds); if (out_fd > maxfd) maxfd = out_fd; }
        if (e_abierto) { FD_SET(err_fd, &rfds); if (err_fd > maxfd) maxfd = err_fd; }
        int s = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (s < 0) {
            if (errno == EINTR) continue;
            free(buf_o); free(buf_e);
            close(out_fd); close(err_fd);
            waitpid(pid, NULL, 0);
            snprintf(out->mensaje_error, sizeof(out->mensaje_error),
                "select fallo: %s", strerror(errno));
            return -1;
        }
        if (o_abierto && FD_ISSET(out_fd, &rfds)) {
            if (len_o + 4096 > cap_o) {
                if (cap_o >= PROC_MAX_OUTPUT) goto overflow;
                cap_o *= 2;
                char *nuevo = (char *)realloc(buf_o, cap_o);
                if (!nuevo) goto oom;
                buf_o = nuevo;
            }
            ssize_t n = read(out_fd, buf_o + len_o, cap_o - len_o);
            if (n <= 0) { o_abierto = 0; }
            else len_o += (size_t)n;
        }
        if (e_abierto && FD_ISSET(err_fd, &rfds)) {
            if (len_e + 4096 > cap_e) {
                if (cap_e >= PROC_MAX_OUTPUT) goto overflow;
                cap_e *= 2;
                char *nuevo = (char *)realloc(buf_e, cap_e);
                if (!nuevo) goto oom;
                buf_e = nuevo;
            }
            ssize_t n = read(err_fd, buf_e + len_e, cap_e - len_e);
            if (n <= 0) { e_abierto = 0; }
            else len_e += (size_t)n;
        }
        continue;
    overflow:
        free(buf_o); free(buf_e);
        close(out_fd); close(err_fd);
        waitpid(pid, NULL, 0);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "salida excede %d bytes", PROC_MAX_OUTPUT);
        return -1;
    oom:
        free(buf_o); free(buf_e);
        close(out_fd); close(err_fd);
        waitpid(pid, NULL, 0);
        snprintf(out->mensaje_error, sizeof(out->mensaje_error),
            "memoria insuficiente");
        return -1;
    }
    close(out_fd);
    close(err_fd);

    int status = 0;
    waitpid(pid, &status, 0);
    out->stdout_buf = buf_o;
    out->stdout_len = (int)len_o;
    out->stderr_buf = buf_e;
    out->stderr_len = (int)len_e;
    if (WIFEXITED(status)) {
        out->exit_codigo = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        out->exit_codigo = 128 + WTERMSIG(status);
    } else {
        out->exit_codigo = -1;
    }
    return 0;
}

#endif
