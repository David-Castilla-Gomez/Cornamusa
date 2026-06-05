/*
 * Tests de destructuring anidado (tuplas dentro de tuplas) en el
 * bucle `para` (v1.137).
 *
 * Antes:
 *   para (a, (b, c)) en triples:  -> "se esperaba un nombre de variable"
 *
 * v1.137: parsear_destino_para acepta `(...)` como sub-patron.
 * El nodo devuelto puede ser EXPR_IDENT, EXPR_STAR_BIND o
 * EXPR_TUPLA (recursivo).
 *
 * El compilador NO necesito cambios: la reescritura del AST a
 *   para $item en iterable:
 *       <patron> = $item
 *       <cuerpo original>
 * y la maquinaria de SENT_ASIGNAR + emitir_destructuring (que
 * desde v1.123 soporta destinos anidados) + pre_reservar_locales
 * (que recurre sobre EXPR_TUPLA) hacen todo el trabajo.
 *
 * Esto cubre `para` SOLAMENTE. Comprehensions y generator
 * expressions con destinos anidados siguen rechazandose con error
 * claro — parsear_destino_compr no acepta `(...)`. Cerrarlas
 * tambien requiere refactor del destructuring inline del
 * compilador de comprehensions a recursivo y queda para una
 * release futura.
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
        "test_para_anid_out.txt";
#else
        "/tmp/test_para_anid_out.txt";
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
    /* Patron anidado simple */
    {
        char out[512];
        ejecutar_capturando(
            "para (a, (b, c)) en [(1, (10, 100)), (2, (20, 200))]:\n"
            "    imprimir(a, b, c)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 10 100") != NULL, "anid_1");
        AFIRMAR(strstr(out, "2 20 200") != NULL, "anid_2");
    }

    /* Mezcla plana + anidado (sin envolver el primero) */
    {
        char out[512];
        ejecutar_capturando(
            "para a, (b, c) en [(1, (10, 100)), (2, (20, 200))]:\n"
            "    imprimir(a, b, c)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 10 100") != NULL, "mezcla_1");
        AFIRMAR(strstr(out, "2 20 200") != NULL, "mezcla_2");
    }

    /* Triple anidado profundo */
    {
        char out[512];
        ejecutar_capturando(
            "para (a, (b, (c, d))) en [(1, (2, (3, 4))), (10, (20, (30, 40)))]:\n"
            "    imprimir(a, b, c, d)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2 3 4") != NULL, "profundo_1");
        AFIRMAR(strstr(out, "10 20 30 40") != NULL, "profundo_2");
    }

    /* Star binding dentro de patron anidado */
    {
        char out[512];
        ejecutar_capturando(
            "para (a, (*r, ult)) en [(1, (10, 20, 30)), (2, (100, 200))]:\n"
            "    imprimir(a, r, ult)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 [10, 20] 30") != NULL, "star_anid_1");
        AFIRMAR(strstr(out, "2 [100] 200") != NULL, "star_anid_2");
    }

    /* Dentro de funcion (slots locales) */
    {
        char out[512];
        ejecutar_capturando(
            "funcion procesar(triples):\n"
            "    res = []\n"
            "    para (a, (b, c)) en triples:\n"
            "        agregar(res, a + b + c)\n"
            "    fin para\n"
            "    retornar res\n"
            "fin funcion\n"
            "imprimir(procesar([(1, (10, 100)), (2, (20, 200))]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[111, 222]") != NULL, "en_funcion");
    }

    /* Aridad incorrecta del sub-patron lanza ErrorDeValor */
    {
        char out[512];
        ejecutar_capturando(
            "intentar:\n"
            "    para (a, (b, c)) en [(1, (10, 100, 999))]:\n"
            "        imprimir(a, b, c)\n"
            "    fin para\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "aridad_subpatron");
    }

    /* Regresion: para clasico */
    {
        char out[256];
        ejecutar_capturando(
            "para mm en [10, 20, 30]:\n"
            "    imprimir(mm)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10") != NULL && strstr(out, "30") != NULL,
                "regresion_clasico");
    }

    /* Regresion: para con destructuring plano */
    {
        char out[256];
        ejecutar_capturando(
            "para mm, n en [(1, 2), (10, 20)]:\n"
            "    imprimir(mm, n)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2") != NULL && strstr(out, "10 20") != NULL,
                "regresion_plano");
    }

    /* Regresion: para con star */
    {
        char out[256];
        ejecutar_capturando(
            "para mm, *resto en [[1, 2, 3], [10, 20]]:\n"
            "    imprimir(mm, resto)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 [2, 3]") != NULL, "regresion_star");
    }

    if (fallos == 0) {
        printf("para_anidado: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "para_anidado: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
