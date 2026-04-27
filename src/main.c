/*
 * Cornamusa — punto de entrada.
 *
 * En esta versión (v0.1.0) el binario solo provee un REPL trivial que hace
 * eco de la entrada y un modo archivo que la imprime. El lexer, parser e
 * intérprete se incorporan en versiones posteriores.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define LINEA_MAX 1024

static void imprimir_uso(const char *programa) {
    fprintf(stderr,
        "Uso: %s [opciones] [archivo.cor]\n"
        "\n"
        "Opciones:\n"
        "  -h, --ayuda      Muestra esta ayuda\n"
        "  -v, --version    Muestra la versión\n"
        "\n"
        "Sin argumentos abre el REPL interactivo.\n",
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

        /* v0.1.0: solo eco. El lexer/parser/VM llegan en versiones futuras. */
        printf("%s\n", linea);
    }
}

static int correr_archivo(const char *ruta) {
    FILE *f = fopen(ruta, "rb");
    if (f == NULL) {
        fprintf(stderr, "Error: no se pudo abrir '%s'\n", ruta);
        return 74; /* EX_IOERR */
    }

    /* v0.1.0: solo lee y vuelca el contenido. */
    char buffer[4096];
    size_t leidos;
    while ((leidos = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        fwrite(buffer, 1, leidos, stdout);
    }

    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    /* Parseo simple de argumentos (sin getopt para evitar dependencia POSIX). */
    const char *archivo = NULL;
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
        if (arg[0] == '-') {
            fprintf(stderr, "Opción no reconocida: %s\n", arg);
            imprimir_uso(argv[0]);
            return 64; /* EX_USAGE */
        }
        archivo = arg;
    }

    if (archivo != NULL) {
        return correr_archivo(archivo);
    }
    return correr_repl();
}
