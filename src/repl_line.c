#include "repl_line.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────
 * Detección de plataforma y headers
 * ────────────────────────────────────────────────────────────────── */

#ifdef _WIN32
#  include <windows.h>
#  include <conio.h>
#  include <io.h>
#  define ESTOY_EN_TTY()  _isatty(_fileno(stdin))
#else
#  include <unistd.h>
#  include <termios.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  define ESTOY_EN_TTY()  isatty(STDIN_FILENO)
#endif

/* ──────────────────────────────────────────────────────────────────
 * Historial
 * ────────────────────────────────────────────────────────────────── */

struct ReplHistorial {
    char **lineas;
    int cuenta;
    int capacidad;
};

#define HISTORIAL_MAX 1000  /* recorta entradas más viejas */

ReplHistorial *repl_historial_nuevo(void) {
    ReplHistorial *h = (ReplHistorial *)calloc(1, sizeof(ReplHistorial));
    return h;
}

void repl_historial_liberar(ReplHistorial *h) {
    if (!h) return;
    for (int i = 0; i < h->cuenta; i++) free(h->lineas[i]);
    free(h->lineas);
    free(h);
}

void repl_historial_agregar(ReplHistorial *h, const char *linea) {
    if (!h || !linea || linea[0] == '\0') return;
    /* No duplicar la última entrada. */
    if (h->cuenta > 0 && strcmp(h->lineas[h->cuenta - 1], linea) == 0) return;
    if (h->cuenta == h->capacidad) {
        int nueva_cap = h->capacidad ? h->capacidad * 2 : 16;
        if (nueva_cap > HISTORIAL_MAX) nueva_cap = HISTORIAL_MAX;
        char **nuevo = (char **)realloc(h->lineas, sizeof(char *) * (size_t)nueva_cap);
        if (!nuevo) return;
        h->lineas = nuevo;
        h->capacidad = nueva_cap;
    }
    /* Si llegamos al tope, descartar la más vieja. */
    if (h->cuenta == HISTORIAL_MAX) {
        free(h->lineas[0]);
        memmove(h->lineas, h->lineas + 1, sizeof(char *) * (size_t)(HISTORIAL_MAX - 1));
        h->cuenta--;
    }
    h->lineas[h->cuenta++] = strdup(linea);
}

static void ruta_historial(char *out, size_t cap) {
#ifdef _WIN32
    const char *base = getenv("USERPROFILE");
    if (!base) base = ".";
    snprintf(out, cap, "%s\\.cornamusa_historial", base);
#else
    const char *base = getenv("HOME");
    if (!base) base = ".";
    snprintf(out, cap, "%s/.cornamusa_historial", base);
#endif
}

void repl_historial_cargar(ReplHistorial *h) {
    if (!h) return;
    char ruta[1024];
    ruta_historial(ruta, sizeof(ruta));
    FILE *f = fopen(ruta, "r");
    if (!f) return;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
        if (n > 0) repl_historial_agregar(h, buf);
    }
    fclose(f);
}

void repl_historial_guardar(const ReplHistorial *h) {
    if (!h) return;
    char ruta[1024];
    ruta_historial(ruta, sizeof(ruta));
    FILE *f = fopen(ruta, "w");
    if (!f) return;
    for (int i = 0; i < h->cuenta; i++) {
        fputs(h->lineas[i], f);
        fputc('\n', f);
    }
    fclose(f);
}

/* ──────────────────────────────────────────────────────────────────
 * Modo raw del terminal (POSIX y Windows)
 * ────────────────────────────────────────────────────────────────── */

#ifdef _WIN32
static DWORD g_modo_consola_in_prev  = 0;
static DWORD g_modo_consola_out_prev = 0;
static bool  g_consola_modificada    = false;
static UINT  g_codepage_prev         = 0;

static bool entrar_modo_raw(void) {
    HANDLE h_in  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_in == INVALID_HANDLE_VALUE || h_out == INVALID_HANDLE_VALUE) return false;
    if (!GetConsoleMode(h_in, &g_modo_consola_in_prev)) return false;
    if (!GetConsoleMode(h_out, &g_modo_consola_out_prev)) return false;
    /* Salida: habilitar virtual terminal processing para ANSI. */
    DWORD nuevo_out = g_modo_consola_out_prev | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(h_out, nuevo_out)) return false;
    /* Entrada: no necesitamos cambiar el modo — _getch() lee char-por-
       char en modo raw aunque la consola esté en ENABLE_LINE_INPUT. */
    /* UTF-8 en la consola para que los caracteres se muestren bien. */
    g_codepage_prev = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    g_consola_modificada = true;
    return true;
}

static void salir_modo_raw(void) {
    if (!g_consola_modificada) return;
    HANDLE h_in  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_in != INVALID_HANDLE_VALUE) SetConsoleMode(h_in, g_modo_consola_in_prev);
    if (h_out != INVALID_HANDLE_VALUE) SetConsoleMode(h_out, g_modo_consola_out_prev);
    if (g_codepage_prev) SetConsoleOutputCP(g_codepage_prev);
    g_consola_modificada = false;
}

#else  /* POSIX */

static struct termios g_termios_prev;
static bool g_termios_modificado = false;

static bool entrar_modo_raw(void) {
    if (tcgetattr(STDIN_FILENO, &g_termios_prev) != 0) return false;
    struct termios raw = g_termios_prev;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return false;
    g_termios_modificado = true;
    return true;
}

static void salir_modo_raw(void) {
    if (!g_termios_modificado) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &g_termios_prev);
    g_termios_modificado = false;
}

#endif

/* ──────────────────────────────────────────────────────────────────
 * Lectura de tecla — abstracción cross-platform
 * ────────────────────────────────────────────────────────────────── */

typedef enum {
    TECLA_CHAR,
    TECLA_ENTER,
    TECLA_BACKSPACE,
    TECLA_DELETE,
    TECLA_IZQUIERDA,
    TECLA_DERECHA,
    TECLA_ARRIBA,
    TECLA_ABAJO,
    TECLA_INICIO,
    TECLA_FIN,
    TECLA_CTRL_C,
    TECLA_CTRL_D,    /* EOF en POSIX */
    TECLA_CTRL_Z,    /* EOF en Windows */
    TECLA_OTRO,
} TipoTecla;

typedef struct {
    TipoTecla tipo;
    int       ch;    /* valor del char para TECLA_CHAR */
} Tecla;

#ifdef _WIN32
static Tecla leer_tecla(void) {
    int c = _getch();
    if (c == 0 || c == 0xE0) {
        /* Tecla extendida — el segundo byte indica cuál. */
        int e = _getch();
        switch (e) {
            case 0x48: return (Tecla){TECLA_ARRIBA, 0};
            case 0x50: return (Tecla){TECLA_ABAJO, 0};
            case 0x4B: return (Tecla){TECLA_IZQUIERDA, 0};
            case 0x4D: return (Tecla){TECLA_DERECHA, 0};
            case 0x47: return (Tecla){TECLA_INICIO, 0};
            case 0x4F: return (Tecla){TECLA_FIN, 0};
            case 0x53: return (Tecla){TECLA_DELETE, 0};
            default:   return (Tecla){TECLA_OTRO, 0};
        }
    }
    if (c == '\r' || c == '\n') return (Tecla){TECLA_ENTER, 0};
    if (c == 8 || c == 127)     return (Tecla){TECLA_BACKSPACE, 0};
    if (c == 3)                  return (Tecla){TECLA_CTRL_C, 0};
    if (c == 26)                 return (Tecla){TECLA_CTRL_Z, 0};
    if (c == 1)                  return (Tecla){TECLA_INICIO, 0};   /* Ctrl-A */
    if (c == 5)                  return (Tecla){TECLA_FIN, 0};      /* Ctrl-E */
    return (Tecla){TECLA_CHAR, c};
}
#else
static int leer_byte(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n != 1) return -1;
    return (int)c;
}
static Tecla leer_tecla(void) {
    int c = leer_byte();
    if (c < 0) return (Tecla){TECLA_CTRL_D, 0};
    if (c == '\r' || c == '\n') return (Tecla){TECLA_ENTER, 0};
    if (c == 8 || c == 127)     return (Tecla){TECLA_BACKSPACE, 0};
    if (c == 3)                  return (Tecla){TECLA_CTRL_C, 0};
    if (c == 4)                  return (Tecla){TECLA_CTRL_D, 0};
    if (c == 1)                  return (Tecla){TECLA_INICIO, 0};   /* Ctrl-A */
    if (c == 5)                  return (Tecla){TECLA_FIN, 0};      /* Ctrl-E */
    if (c == 0x1B) {
        /* Secuencia escape: lee siguiente. */
        int b1 = leer_byte();
        if (b1 == '[') {
            int b2 = leer_byte();
            switch (b2) {
                case 'A': return (Tecla){TECLA_ARRIBA, 0};
                case 'B': return (Tecla){TECLA_ABAJO, 0};
                case 'C': return (Tecla){TECLA_DERECHA, 0};
                case 'D': return (Tecla){TECLA_IZQUIERDA, 0};
                case 'H': return (Tecla){TECLA_INICIO, 0};
                case 'F': return (Tecla){TECLA_FIN, 0};
                case '3': {
                    /* `\x1b[3~` = Delete; consumir el ~ */
                    int b3 = leer_byte();
                    if (b3 == '~') return (Tecla){TECLA_DELETE, 0};
                    return (Tecla){TECLA_OTRO, 0};
                }
                default: return (Tecla){TECLA_OTRO, 0};
            }
        }
        return (Tecla){TECLA_OTRO, 0};
    }
    return (Tecla){TECLA_CHAR, c};
}
#endif

/* ──────────────────────────────────────────────────────────────────
 * Editor de línea
 * ────────────────────────────────────────────────────────────────── */

typedef struct {
    char  *buf;
    int    cuenta;
    int    capacidad;
    int    cursor;     /* posición del cursor (0..cuenta) */
} Linea;

static void linea_iniciar(Linea *l) {
    l->capacidad = 128;
    l->buf = (char *)malloc((size_t)l->capacidad);
    if (l->buf) l->buf[0] = '\0';
    l->cuenta = 0;
    l->cursor = 0;
}
static void linea_liberar(Linea *l) { free(l->buf); }

static bool linea_asegurar_cap(Linea *l, int necesario) {
    if (necesario <= l->capacidad) return true;
    int nueva = l->capacidad;
    while (nueva < necesario) nueva *= 2;
    char *nuevo = (char *)realloc(l->buf, (size_t)nueva);
    if (!nuevo) return false;
    l->buf = nuevo;
    l->capacidad = nueva;
    return true;
}

static void linea_insertar_char(Linea *l, int c) {
    if (!linea_asegurar_cap(l, l->cuenta + 2)) return;
    memmove(l->buf + l->cursor + 1, l->buf + l->cursor,
        (size_t)(l->cuenta - l->cursor + 1));   /* incluye \0 */
    l->buf[l->cursor] = (char)c;
    l->cuenta++;
    l->cursor++;
}

static void linea_borrar_atras(Linea *l) {
    if (l->cursor == 0) return;
    memmove(l->buf + l->cursor - 1, l->buf + l->cursor,
        (size_t)(l->cuenta - l->cursor + 1));
    l->cuenta--;
    l->cursor--;
}

static void linea_borrar_adelante(Linea *l) {
    if (l->cursor == l->cuenta) return;
    memmove(l->buf + l->cursor, l->buf + l->cursor + 1,
        (size_t)(l->cuenta - l->cursor));
    l->cuenta--;
}

static void linea_set(Linea *l, const char *texto) {
    size_t n = strlen(texto);
    if (!linea_asegurar_cap(l, (int)n + 1)) return;
    memcpy(l->buf, texto, n + 1);
    l->cuenta = (int)n;
    l->cursor = l->cuenta;
}

/* Repinta la línea: \r, prompt, buffer, limpiar resto, posicionar
   cursor al lugar correcto. Asume terminal ANSI. */
static void repintar(const char *prompt, const Linea *l) {
    /* \r vuelve a col 0; \x1b[K limpia hasta fin de línea. */
    fputs("\r", stdout);
    fputs(prompt, stdout);
    fwrite(l->buf, 1, (size_t)l->cuenta, stdout);
    fputs("\x1b[K", stdout);
    /* Si el cursor lógico no está al final, moverlo a su sitio. */
    int despues_de_cursor = l->cuenta - l->cursor;
    if (despues_de_cursor > 0) {
        fprintf(stdout, "\x1b[%dD", despues_de_cursor);
    }
    fflush(stdout);
}

/* ──────────────────────────────────────────────────────────────────
 * Fallback no-TTY: fgets, sin edición ni historial.
 * ────────────────────────────────────────────────────────────────── */

static char *leer_linea_fallback(const char *prompt) {
    fputs(prompt, stdout);
    fflush(stdout);
    char buf[16384];
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, buf, n + 1);
    return r;
}

/* ──────────────────────────────────────────────────────────────────
 * Entrada principal
 * ────────────────────────────────────────────────────────────────── */

char *repl_leer_linea(const char *prompt, ReplHistorial *historial) {
    if (!ESTOY_EN_TTY()) {
        return leer_linea_fallback(prompt);
    }
    if (!entrar_modo_raw()) {
        return leer_linea_fallback(prompt);
    }

    Linea l;
    linea_iniciar(&l);
    if (!l.buf) { salir_modo_raw(); return NULL; }

    /* hist_idx == historial->cuenta significa "editando línea nueva" */
    int hist_idx = historial ? historial->cuenta : 0;
    /* Buffer escondido cuando navegamos historial — para poder volver
       a la línea-en-progreso si llegamos al final con Down. */
    char *buf_pendiente = NULL;

    repintar(prompt, &l);

    char *resultado = NULL;
    bool salir = false;
    bool eof = false;

    while (!salir) {
        Tecla t = leer_tecla();
        switch (t.tipo) {
            case TECLA_ENTER:
                fputs("\r\n", stdout);
                fflush(stdout);
                resultado = strdup(l.buf);
                salir = true;
                break;

            case TECLA_CTRL_C:
                fputs("^C\r\n", stdout);
                fflush(stdout);
                /* Devuelve cadena vacía — el caller decide qué hacer. */
                resultado = strdup("");
                salir = true;
                break;

            case TECLA_CTRL_D:
            case TECLA_CTRL_Z:
                /* EOF con buffer vacío → null. Con buffer no vacío,
                   borra char a la derecha (igual que readline). */
                if (l.cuenta == 0) {
                    fputs("\r\n", stdout);
                    fflush(stdout);
                    eof = true;
                    salir = true;
                } else if (l.cursor < l.cuenta) {
                    linea_borrar_adelante(&l);
                    repintar(prompt, &l);
                }
                break;

            case TECLA_BACKSPACE:
                linea_borrar_atras(&l);
                repintar(prompt, &l);
                break;

            case TECLA_DELETE:
                linea_borrar_adelante(&l);
                repintar(prompt, &l);
                break;

            case TECLA_IZQUIERDA:
                if (l.cursor > 0) { l.cursor--; repintar(prompt, &l); }
                break;

            case TECLA_DERECHA:
                if (l.cursor < l.cuenta) { l.cursor++; repintar(prompt, &l); }
                break;

            case TECLA_INICIO:
                if (l.cursor != 0) { l.cursor = 0; repintar(prompt, &l); }
                break;

            case TECLA_FIN:
                if (l.cursor != l.cuenta) { l.cursor = l.cuenta; repintar(prompt, &l); }
                break;

            case TECLA_ARRIBA:
                if (historial && hist_idx > 0) {
                    /* Al salir de "editando línea nueva", guardarla. */
                    if (hist_idx == historial->cuenta) {
                        free(buf_pendiente);
                        buf_pendiente = strdup(l.buf);
                    }
                    hist_idx--;
                    linea_set(&l, historial->lineas[hist_idx]);
                    repintar(prompt, &l);
                }
                break;

            case TECLA_ABAJO:
                if (historial && hist_idx < historial->cuenta) {
                    hist_idx++;
                    if (hist_idx == historial->cuenta) {
                        linea_set(&l, buf_pendiente ? buf_pendiente : "");
                    } else {
                        linea_set(&l, historial->lineas[hist_idx]);
                    }
                    repintar(prompt, &l);
                }
                break;

            case TECLA_CHAR:
                if (t.ch >= 32 && t.ch < 127) {
                    /* ASCII imprimible — inserta directamente. */
                    linea_insertar_char(&l, t.ch);
                    repintar(prompt, &l);
                } else if ((unsigned char)t.ch >= 0x80) {
                    /* Byte alto de UTF-8 — insertamos tal cual. La
                       consola se encarga de continuar con los bytes
                       siguientes. Para repintado correcto necesitamos
                       todos los bytes del codepoint antes de continuar;
                       como leemos byte a byte y todos son >=0x80,
                       cada uno se inserta y la repintura recalcula. */
                    linea_insertar_char(&l, t.ch);
                    /* Solo repintamos cuando creamos haber completado
                       el codepoint (heurística: byte que no es
                       continuación 10xxxxxx). Conservador: repintamos
                       siempre — coste mínimo. */
                    repintar(prompt, &l);
                }
                /* Otros chars de control no manejados se ignoran. */
                break;

            case TECLA_OTRO:
                break;
        }
    }

    salir_modo_raw();
    free(buf_pendiente);
    linea_liberar(&l);
    if (eof) return NULL;
    return resultado;
}
