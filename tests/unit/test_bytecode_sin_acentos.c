/*
 * Tests de `cadena.sin_acentos()` (v1.162).
 *
 * Elimina acentos y marcas combinantes Unicode. Usa utf8proc_map
 * con DECOMPOSE | STRIPMARK | STABLE — descompone los precomposed
 * (é -> e + acento combinante) y luego elimina las marcas.
 *
 * Casos de uso tipicos:
 *   - Slugs de URL (sin_acentos + minusculas + reemplazar).
 *   - Busqueda tolerante a acentos.
 *   - Comparacion fuzzy de nombres.
 *
 * Nota documentada: la 'ñ' tambien pierde su tilde (es marca
 * combinante). 'ñoño' -> 'nono'. Para preservarla habria que
 * hacer un postproceso a medida.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "compilador.h"
#include "lexer.h"
#include "parser.h"
#include "valor.h"
#include "vm.h"

static int fallos = 0;
static int casos = 0;

#define AFIRMAR(cond, etiqueta)                                                \
    do {                                                                        \
        casos++;                                                                \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FALLO %s:%d (%s)\n", __FILE__, __LINE__, etiqueta);\
            fallos++;                                                           \
        }                                                                       \
    } while (0)

static int ejecutar_capturando(const char *fuente, char *out_buf, int out_cap) {
    const char *tmpfile =
#ifdef _WIN32
        "test_sin_acentos_out.txt";
#else
        "/tmp/test_sin_acentos_out.txt";
#endif
    if (!freopen(tmpfile, "w+", stdout)) return -1;

    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    int rc = -1;
    if (!p.tuvo_error) {
        Chunk chunk; chunk_iniciar(&chunk);
        Compilador c; compilador_iniciar(&c, &chunk);
        if (compilador_compilar_programa(&c, sents, n)) {
            VM vm; vm_iniciar(&vm);
            Valor r = valor_nulo();
            ResultadoVM rcvm = vm_ejecutar(&vm, &chunk, &r);
            valor_destruir(&r);
            vm_destruir(&vm);
            if (rcvm == VM_OK) rc = 0;
        }
        chunk_destruir(&chunk);
    }
    arena_destruir(&a);

    fflush(stdout);
#ifdef _WIN32
    freopen("CON", "w", stdout);
#else
    freopen("/dev/tty", "w", stdout);
#endif

    FILE *f = fopen(tmpfile, "r");
    if (f) {
        int leido = (int)fread(out_buf, 1, (size_t)(out_cap - 1), f);
        out_buf[leido] = '\0';
        fclose(f);
        remove(tmpfile);
    } else {
        out_buf[0] = '\0';
    }
    return rc;
}

int main(void) {
    /* Vocales acentuadas castellanas */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"CAF\xc3\x89\".sin_acentos())\n"   /* CAFÉ */
            "imprimir(\"\xc3\x81\xc3\x89\xc3\x8d\xc3\x93\xc3\x9a\".sin_acentos())\n",  /* ÁÉÍÓÚ */
            out, sizeof(out));
        AFIRMAR(strstr(out, "CAFE") != NULL, "cafe");
        AFIRMAR(strstr(out, "AEIOU") != NULL, "vocales_mayus");
    }

    /* ñ → n (tilde es marca combinante, también desaparece) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\xc3\xb1" "o\xc3\xb1" "o\".sin_acentos())\n"   /* ñoño */
            "imprimir(\"Espa\xc3\xb1" "a\".sin_acentos())\n",          /* España */
            out, sizeof(out));
        AFIRMAR(strstr(out, "nono") != NULL, "n_tilde_pierde");
        AFIRMAR(strstr(out, "Espana") != NULL, "espana");
    }

    /* ASCII no cambia */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola mundo\".sin_acentos())\n"
            "imprimir(\"abc123XYZ!?\".sin_acentos())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "hola mundo") != NULL, "ascii_intacto");
        AFIRMAR(strstr(out, "abc123XYZ!?") != NULL, "ascii_simbolos");
    }

    /* Cadena vacía */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"[\" + \"\".sin_acentos() + \"]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "vacia");
    }

    /* Otros idiomas: ü, ï, ô, etc. */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"na\xc3\xaf" "ve r\xc3\xa9sum\xc3\xa9 \xc3\xbc" "ber\".sin_acentos())\n",
            /* naïve résumé über */
            out, sizeof(out));
        AFIRMAR(strstr(out, "naive resume uber") != NULL, "frances_aleman");
    }

    /* Idiomático: slugificar */
    {
        char out[256];
        ejecutar_capturando(
            "s = \"El a\xc3\xb1" "o 2026 - \xc2\xa1" "Pr\xc3\xb3spero!\"\n"
            "imprimir(s.sin_acentos().minusculas())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "el ano 2026 - \xc2\xa1" "prospero!") != NULL,
                "slugificar");
    }

    /* Mensaje con interrogacion/exclamacion */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"Hola, \xc2\xbf" "c\xc3\xb3mo est\xc3\xa1" "s?\".sin_acentos())\n",
            /* Hola, ¿cómo estás? */
            out, sizeof(out));
        AFIRMAR(strstr(out, "Hola, \xc2\xbf" "como estas?") != NULL,
                "pregunta_completa");
    }

    /* Longitud cambia (los caracteres acentuados son 2 bytes, los
     * resultantes son 1 byte cada uno) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud(\"CAF\xc3\x89\".sin_acentos()))\n",  /* CAFÉ -> CAFE */
            out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "longitud");
    }

    /* Error: tipo no-cadena */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    [1, 2].sin_acentos()\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "tipo_invalido");
    }

    if (fallos == 0) {
        printf("sin_acentos: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "sin_acentos: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
