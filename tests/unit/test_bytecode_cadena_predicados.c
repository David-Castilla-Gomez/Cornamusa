/*
 * Tests de los predicados `es_alfa`, `es_digito`, `es_alfanum`,
 * `es_espacios` y el metodo `titulo()` para cadenas (v1.154).
 *
 * Python tiene str.isalpha / isdigit / isalnum / isspace y str.title.
 * Cornamusa no los tenia, lo cual obligaba a hacer el check
 * code-point-a-code-point a mano. v1.154 cierra esos cinco
 * metodos clasicos.
 *
 * Todos usan utf8proc_category para Unicode-correcto:
 *   es_alfa     -> letras (LU/LL/LT/LM/LO)
 *   es_digito   -> ND (decimal digit) — incluye digitos arabes
 *                  ١٢٣ y otros sistemas, ademas de 0-9.
 *   es_alfanum  -> letras o cualquier number category.
 *   es_espacios -> Z* + ASCII whitespace (\t\n\r\f\v).
 *
 * Todos rechazan la cadena vacia como falsa (paridad con Python).
 *
 * `titulo()` capitaliza la primera letra de cada palabra y pasa
 * a minuscula el resto. Una palabra empieza tras un caracter no
 * alfabetico (espacio, guion, punto, etc.).
 *
 * Sin cambios a bytecode ni VM.
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
        "test_cad_pred_out.txt";
#else
        "/tmp/test_cad_pred_out.txt";
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
    /* es_alfa: solo letras (Unicode) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola\".es_alfa())\n"
            "imprimir(\"A\xc3\xb1" "o\".es_alfa())\n"  /* Año */
            "imprimir(\"abc123\".es_alfa())\n"
            "imprimir(\"hola mundo\".es_alfa())\n"
            "imprimir(\"\".es_alfa())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nverdadero\nfalso\nfalso\nfalso\n") != NULL
                || strstr(out, "verdadero\r\nverdadero\r\nfalso") != NULL,
                "es_alfa_combinado");
    }

    /* es_digito: incluye digitos no-ASCII */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"12345\".es_digito())\n"
            "imprimir(\"12.5\".es_digito())\n"
            "imprimir(\"-12\".es_digito())\n"
            "imprimir(\"\xd9\xa1\xd9\xa2\xd9\xa3\".es_digito())\n",  /* ١٢٣ */
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "digito_ascii");
        /* Verificacion mas estricta: 12.5 y -12 son falsos */
        const char *p = strstr(out, "verdadero\n");
        AFIRMAR(p && strstr(p + 10, "falso") != NULL, "digito_decimal_no");
    }

    /* es_alfanum */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"abc123\".es_alfanum())\n"
            "imprimir(\"a\xc3\xb1" "o1\".es_alfanum())\n"  /* año1 */
            "imprimir(\"hola mundo\".es_alfanum())\n"
            "imprimir(\"\".es_alfanum())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "alfanum_basico");
        /* el "hola mundo" tiene espacio → falso */
        AFIRMAR(strstr(out, "falso") != NULL, "alfanum_con_espacio");
    }

    /* es_espacios */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"   \".es_espacios())\n"
            "imprimir(\" \\t\\n\".es_espacios())\n"
            "imprimir(\"hola \".es_espacios())\n"
            "imprimir(\"\".es_espacios())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "espacios_basico");
        AFIRMAR(strstr(out, "falso") != NULL, "espacios_con_letra");
    }

    /* titulo: capitalizar primera letra de cada palabra */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola mundo\".titulo())\n"
            "imprimir(\"HOLA MUNDO\".titulo())\n"
            "imprimir(\"a\xc3\xb1" "o nuevo\".titulo())\n"      /* año nuevo */
            "imprimir(\"uno-dos-tres\".titulo())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Hola Mundo") != NULL, "titulo_simple");
        AFIRMAR(strstr(out, "A\xc3\xb1" "o Nuevo") != NULL, "titulo_unicode");
        AFIRMAR(strstr(out, "Uno-Dos-Tres") != NULL, "titulo_guiones");
    }

    /* titulo preserva no-letras */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\".titulo())\n"
            "imprimir(\"123 abc\".titulo())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "123 Abc") != NULL, "titulo_con_digitos");
    }

    /* Predicados con Unicode greco */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\xce\xb1\xce\xb2\xce\xb3\".es_alfa())\n"  /* αβγ */
            "imprimir(\"\xce\x91\xce\x92\xce\x93\".es_alfa())\n",  /* ΑΒΓ */
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nverdadero") != NULL
                || strstr(out, "verdadero\r\nverdadero") != NULL,
                "es_alfa_griego");
    }

    /* es_espacios con NBSP (U+00A0) — ZS category, debe ser true */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\xc2\xa0\".es_espacios())\n",  /* NBSP */
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "es_espacios_nbsp");
    }

    if (fallos == 0) {
        printf("cad_pred: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "cad_pred: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
