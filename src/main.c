/*
 * Cornamusa — punto de entrada.
 *
 * En esta versión (v0.2.0) el binario:
 *   - Lee y normaliza un archivo `.cor` a UTF-8 NFC (decisión B4).
 *   - Lo tokeniza con el lexer.
 *   - Reporta errores léxicos siguiendo MENSAJES.md (con caret indicators).
 *
 * Modos:
 *   - Sin argumentos: REPL (eco trivial; el intérprete llega en v0.4).
 *   - <archivo>:     tokeniza el archivo y reporta errores. Ejecutar el
 *                    programa requiere v0.4+.
 *   - --tokens <a>:  vuelca todos los tokens al stdout (debug del lexer).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "common.h"
#include "errores.h"
#include "fuente.h"
#include "lexer.h"

#define LINEA_MAX 1024

/*
 * Configura la consola para emitir y aceptar UTF-8 (decisión I5).
 * En Linux/macOS no hace nada — la consola ya es UTF-8 por defecto.
 */
static void configurar_consola_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static void imprimir_uso(const char *programa) {
    fprintf(stderr,
        "Uso: %s [opciones] [archivo.cor]\n"
        "\n"
        "Opciones:\n"
        "  -h, --ayuda      Muestra esta ayuda\n"
        "  -v, --version    Muestra la versión\n"
        "      --tokens     Tokeniza el archivo y vuelca los tokens\n"
        "\n"
        "Sin argumentos abre el REPL interactivo (eco hasta v0.4).\n",
        programa);
}

static void imprimir_version(void) {
    printf("Cornamusa %s\n", CORNAMUSA_VERSION);
}

static void imprimir_banner(void) {
    printf("Cornamusa %s — REPL (eco)\n", CORNAMUSA_VERSION);
    printf("Escribe 'salir' o pulsa Ctrl-D para terminar.\n");
}

static int correr_repl(void) {
    imprimir_banner();

    char linea[LINEA_MAX];
    for (;;) {
        fputs(">>> ", stdout);
        fflush(stdout);

        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            putchar('\n');
            return 0;
        }

        size_t len = strlen(linea);
        while (len > 0 && (linea[len - 1] == '\n' || linea[len - 1] == '\r')) {
            linea[--len] = '\0';
        }

        if (strcmp(linea, "salir") == 0) {
            return 0;
        }

        /* v0.2.0: eco. El intérprete llega en v0.4. */
        printf("%s\n", linea);
    }
}

/*
 * Tokeniza el archivo. Si encuentra errores los reporta en stderr y
 * devuelve 65 (EX_DATAERR). Si todo va bien y `volcar_tokens` es true,
 * imprime cada token en stdout (formato debug).
 */
static int tokenizar_archivo(const char *ruta, bool volcar_tokens) {
    FuenteCargada fc = fuente_cargar_archivo(ruta);
    if (fc.codigo != FUENTE_OK) {
        fprintf(stderr, "Error al cargar '%s': %s\n",
            ruta, fc.mensaje_error);
        return 74; /* EX_IOERR */
    }

    Lexer l;
    lexer_iniciar(&l, fc.fuente, ruta);

    int errores = 0;
    int tokens_emitidos = 0;
    Token t;
    do {
        t = lexer_siguiente(&l);
        tokens_emitidos++;

        if (t.tipo == TT_ERROR) {
            error_imprimir_token(&t, fc.fuente, ruta, stderr);
            errores++;
            /* Continuamos tokenizando para reportar todos los errores. */
            continue;
        }

        if (volcar_tokens) {
            /* Formato compacto: línea:col TIPO "lexema" */
            printf("%4d:%-3d  %-25s  ", t.linea, t.columna,
                tipo_token_nombre(t.tipo));
            if (t.tipo == TT_FIN_ARCHIVO) {
                printf("(fin)\n");
            } else {
                /* Lexema entre comillas, escapando \n y \" para legibilidad. */
                fputc('"', stdout);
                for (int i = 0; i < t.longitud; i++) {
                    char c = t.inicio[i];
                    if (c == '\n')      fputs("\\n", stdout);
                    else if (c == '\t') fputs("\\t", stdout);
                    else if (c == '"')  fputs("\\\"", stdout);
                    else                fputc(c, stdout);
                }
                fputs("\"\n", stdout);
            }
        }
    } while (t.tipo != TT_FIN_ARCHIVO);

    fuente_destruir(&fc);

    if (errores > 0) {
        fprintf(stderr,
            "\n%d error(es) léxicos. La tokenización se completó pero el "
            "programa no es válido.\n", errores);
        return 65; /* EX_DATAERR */
    }

    if (volcar_tokens) {
        printf("\n%d tokens emitidos (incluyendo TT_FIN_ARCHIVO).\n",
            tokens_emitidos);
    } else {
        /* Modo "ejecutar archivo": v0.2.0 solo lexa.
           El parser/intérprete llegan en v0.3+. */
        fprintf(stderr,
            "Cornamusa %s solo tokeniza por ahora; el intérprete llega "
            "en v0.4.\nUsa --tokens para volcar los tokens del archivo.\n",
            CORNAMUSA_VERSION);
    }
    return 0;
}

int main(int argc, char **argv) {
    configurar_consola_utf8();

    /* Parseo simple de argumentos. */
    const char *archivo = NULL;
    bool volcar_tokens = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--ayuda") == 0 ||
            strcmp(arg, "--help") == 0) {
            imprimir_uso(argv[0]);
            return 0;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0 ||
            strcmp(arg, "--versión") == 0) {
            imprimir_version();
            return 0;
        }
        if (strcmp(arg, "--tokens") == 0) {
            volcar_tokens = true;
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "Opción no reconocida: %s\n", arg);
            imprimir_uso(argv[0]);
            return 64; /* EX_USAGE */
        }
        archivo = arg;
    }

    if (archivo != NULL) {
        return tokenizar_archivo(archivo, volcar_tokens);
    }

    if (volcar_tokens) {
        fprintf(stderr, "--tokens requiere un archivo .cor\n");
        return 64;
    }

    return correr_repl();
}
