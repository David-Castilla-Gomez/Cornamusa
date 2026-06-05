/*
 * Tests de generator expressions con multiples `para` y destructuring
 * (v1.136).
 *
 * Antes:
 *   (x + j para x en xs para j en ys)
 *      -> ErrorDeSintaxis "se esperaba ')' al final de la generator"
 *   (a + b para a, b en pares)
 *      -> ErrorDeSintaxis "se esperaba 'en' tras la variable"
 *   o, tras v1.135, rechazo claro con
 *      "destructuring en cabecera de generator expression no soportado"
 *
 * v1.136: paridad de features con list/dict/set comprehensions.
 *   Parser: el path de genex ya recolecta extras + patron via
 *   parsear_comprehension_cola (no pasa NULL).
 *   Compilador: la rama tipo_destino == 3 se expande para soportar
 *   N clausulas con destructuring opcional. El cuerpo emite
 *   OP_PRODUCIR en lugar de OP_LISTA_AGREGAR. El primer iterable
 *   sigue pasandose como parametro $gx_param; los demas se evaluan
 *   dentro del scope del generador y pueden referenciar vars de
 *   clausulas anteriores como locales/upvalues.
 *
 * Limitacion: max 16 clausulas anidadas (limite practico del
 * bytecode v0.6, mismo que list comprehensions).
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
        "test_genex_multi_out.txt";
#else
        "/tmp/test_genex_multi_out.txt";
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
    /* Multi-para basico en genex */
    {
        char out[256];
        ejecutar_capturando(
            "g = (x + j para x en [1, 2] para j en [10, 20])\n"
            "para v en g: imprimir(v)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "11") != NULL, "multi_11");
        AFIRMAR(strstr(out, "21") != NULL, "multi_21");
        AFIRMAR(strstr(out, "12") != NULL, "multi_12");
        AFIRMAR(strstr(out, "22") != NULL, "multi_22");
    }

    /* Destructuring en genex */
    {
        char out[256];
        ejecutar_capturando(
            "g = (a + b para a, b en [(1, 10), (2, 20), (3, 30)])\n"
            "para v en g: imprimir(v)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "11") != NULL, "destr_11");
        AFIRMAR(strstr(out, "22") != NULL, "destr_22");
        AFIRMAR(strstr(out, "33") != NULL, "destr_33");
    }

    /* Star inicial en genex */
    {
        char out[256];
        ejecutar_capturando(
            "g = (ult para *_, ult en [[1, 2, 3], [10, 20]])\n"
            "para v en g: imprimir(v)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "star_3");
        AFIRMAR(strstr(out, "20") != NULL, "star_20");
    }

    /* Star final en genex */
    {
        char out[256];
        ejecutar_capturando(
            "g = (resto para primero, *resto en [[1, 2, 3, 4], [10, 20]])\n"
            "para v en g: imprimir(v)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 3, 4]") != NULL, "star_final_1");
        AFIRMAR(strstr(out, "[20]") != NULL, "star_final_2");
    }

    /* Guarda + destructuring */
    {
        char out[256];
        ejecutar_capturando(
            "g = (a para a, b en [(1, 10), (2, 5), (3, 30)] si b > 9)\n"
            "para v en g: imprimir(v)\n",
            out, sizeof(out));
        /* a=1 con b=10>9 ok; a=2 con b=5 no; a=3 con b=30 ok */
        AFIRMAR(strstr(out, "1") != NULL, "guarda_1");
        AFIRMAR(strstr(out, "3") != NULL, "guarda_3");
        AFIRMAR(strstr(out, "2") == NULL || strstr(out, "2\n") == NULL,
                "guarda_excluye_2");
    }

    /* Multi-para con destructuring + guarda */
    {
        char out[512];
        ejecutar_capturando(
            "g = ((a, c) para a, _ en [(1, 10), (2, 20)] "
            "para c, _ en [(100, \"x\"), (200, \"x\")] si a < c)\n"
            "para par en g: imprimir(par)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 100)") != NULL, "mm_1_100");
        AFIRMAR(strstr(out, "(2, 200)") != NULL, "mm_2_200");
    }

    /* Genex dentro de funcion */
    {
        char out[256];
        ejecutar_capturando(
            "funcion sumas(pares):\n"
            "    retornar (a + b para a, b en pares)\n"
            "fin funcion\n"
            "para v en sumas([(1, 1), (2, 2), (3, 3)]):\n"
            "    imprimir(v)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL && strstr(out, "4") != NULL
                && strstr(out, "6") != NULL, "func_genex");
    }

    /* Genex con romper desde el bucle exterior — interrumpe el
     * generador (lazy: no debe computar elementos no demandados). */
    {
        char out[256];
        ejecutar_capturando(
            "g = (mm * 10 para mm en [1, 2, 3, 4, 5])\n"
            "vistos = []\n"
            "para v en g:\n"
            "    agregar(vistos, v)\n"
            "    si v >= 20:\n"
            "        romper\n"
            "    fin si\n"
            "fin para\n"
            "imprimir(vistos)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[10, 20]") != NULL, "lazy_romper");
    }

    /* Regresion: genex simple sin extras ni destructuring */
    {
        char out[256];
        ejecutar_capturando(
            "g = (mm * 2 para mm en [1, 2, 3])\n"
            "para v en g: imprimir(v)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL && strstr(out, "4") != NULL
                && strstr(out, "6") != NULL, "regresion");
    }

    /* Aridad incorrecta en genex con destructuring lanza ErrorDeValor */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    g = (a + b para a, b en [(1, 2), (3,)])\n"
            "    para v en g:\n"
            "        imprimir(v)\n"
            "    fin para\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        /* La primera tupla (1,2) -> 3 emitido; la segunda (3,) -> error */
        AFIRMAR(strstr(out, "3") != NULL, "aridad_primera");
        AFIRMAR(strstr(out, "err") != NULL, "aridad_atrap");
    }

    if (fallos == 0) {
        printf("genex_multi: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "genex_multi: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
