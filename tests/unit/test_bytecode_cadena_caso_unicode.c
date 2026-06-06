/*
 * Tests de `mayusculas()` y `minusculas()` Unicode-aware (v1.153).
 *
 * Antes v1.153 ambos metodos usaban una conversion ASCII-only que
 * dejaba intactos cualquier code-point >= 0x80. Resultado embarazoso
 * para hablantes de castellano: `'ñoño'.mayusculas()` daba `'ñOñO'`
 * (solo el `o` cambia, la `ñ` no), `'CAFÉ'.minusculas()` daba
 * `'cafÉ'`.
 *
 * v1.153 usa utf8proc_toupper/tolower por code-point. Recorre la
 * cadena con utf8proc_iterate y re-encode el resultado con
 * utf8proc_encode_char en un buffer dinamico (algunos mappings
 * cambian el numero de bytes UTF-8 de un code-point a otro).
 *
 * Sin cambios a bytecode ni VM. Las nativas
 * `cadena_minusculas_ascii` y `cadena_mayusculas_ascii` quedan
 * disponibles bajo sus nombres explicitos por compatibilidad —
 * solo los METODOS `s.minusculas()` / `s.mayusculas()` se
 * redirigen a Unicode.
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
        "test_cad_caso_out.txt";
#else
        "/tmp/test_cad_caso_out.txt";
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
    /* Castellano: ñ y vocales acentuadas */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\xc3\xb1o\xc3\xb1o\".mayusculas())\n"   /* ñoño */
            "imprimir(\"CAF\xc3\x89\".minusculas())\n"            /* CAFÉ */
            "imprimir(\"Espa\xc3\xb1" "a\".mayusculas())\n"       /* España */
            "imprimir(\"A\xc3\xb1o\".mayusculas())\n",            /* Año */
            out, sizeof(out));
        AFIRMAR(strstr(out, "\xc3\x91O\xc3\x91O") != NULL, "ñoño_upper");
        AFIRMAR(strstr(out, "caf\xc3\xa9") != NULL, "café_lower");
        AFIRMAR(strstr(out, "ESPA\xc3\x91" "A") != NULL, "españa_upper");
        AFIRMAR(strstr(out, "A\xc3\x91" "O") != NULL, "año_upper");
    }

    /* Vocales acentuadas a minusculas */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\xc3\x81\xc3\x89\xc3\x8d\xc3\x93\xc3\x9a\xc3\x91\xc3\x9c\""
            ".minusculas())\n",   /* ÁÉÍÓÚÑÜ */
            out, sizeof(out));
        AFIRMAR(strstr(out, "\xc3\xa1\xc3\xa9\xc3\xad\xc3\xb3\xc3\xba\xc3\xb1\xc3\xbc") != NULL,
                "vocales_acent_lower");
    }

    /* ASCII regresion */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola\".mayusculas())\n"
            "imprimir(\"HOLA\".minusculas())\n"
            "imprimir(\"abc123XYZ\".mayusculas())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "HOLA") != NULL, "ascii_upper");
        AFIRMAR(strstr(out, "hola") != NULL, "ascii_lower");
        AFIRMAR(strstr(out, "ABC123XYZ") != NULL, "ascii_alfanum");
    }

    /* Mezcla ASCII + Unicode con signos */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"Hola, \xc2\xbfc\xc3\xb3mo est\xc3\xa1s?\".mayusculas())\n",
            /* "Hola, ¿cómo estás?" */
            out, sizeof(out));
        AFIRMAR(strstr(out, "HOLA, \xc2\xbfC\xc3\x93MO EST\xc3\x81S?") != NULL,
                "mezcla_upper");
    }

    /* Cadena vacia */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"[\" + \"\".mayusculas() + \"]\")\n"
            "imprimir(\"[\" + \"\".minusculas() + \"]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "vacia");
    }

    /* Otros idiomas: griego y aleman */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\xce\xb1\xce\xb2\xce\xb3\".mayusculas())\n"   /* αβγ */
            "imprimir(\"M\xc3\x9cNCHEN\".minusculas())\n",             /* MÜNCHEN */
            out, sizeof(out));
        AFIRMAR(strstr(out, "\xce\x91\xce\x92\xce\x93") != NULL, "griego_upper");
        AFIRMAR(strstr(out, "m\xc3\xbcnchen") != NULL, "aleman_lower");
    }

    /* Longitud en code-points despues de la conversion. */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud(\"\xc3\xb1o\xc3\xb1o\".mayusculas()))\n",  /* ñoño */
            out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "longitud_preservada");
    }

    /* Idempotente para letras ya en el caso correcto */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"HOLA\".mayusculas() == \"HOLA\")\n"
            "imprimir(\"hola\".minusculas() == \"hola\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "idempotente");
    }

    if (fallos == 0) {
        printf("cad_caso: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "cad_caso: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
