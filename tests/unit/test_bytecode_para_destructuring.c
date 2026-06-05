/*
 * Tests de destructuring en el bucle `para` (v1.134).
 *
 * Antes:
 *   para a, b en pares:                 -> ErrorDeSintaxis ("se esperaba 'en'")
 *   para *previos, ultimo en xs:        -> ErrorDeSintaxis ("nombre tras 'para'")
 *
 * v1.134:
 *   Parser: parsear_para acepta lista coma-separada de destinos
 *   (IDENT o *IDENT). Si hay mas de un destino, se reescribe a:
 *     para $item_L_C en iterable:
 *         (a, b) = $item_L_C
 *         <cuerpo original>
 *     fin para
 *   donde $item_L_C es un nombre temporal unico por posicion del
 *   `para`. Esto reusa toda la maquinaria de destructuring de
 *   SENT_ASIGNAR (emitir_destructuring + verificacion de aridad
 *   + EXPR_STAR_BIND).
 *
 *   pre_reservar_locales: extendido para reconocer EXPR_STAR_BIND
 *   en patrones tupla. Sin esto, el slot del star se creaba dentro
 *   del loop (OP_NULO reejecutado cada iter, mismo bug que la
 *   v1.130 arreglo para compilar_mientras).
 *
 * El compilador NO necesito cambios — sigue trabajando sobre
 * EXPR_IDENT como objetivo del SENT_PARA; el destructuring vive
 * en el cuerpo y se compila como cualquier SENT_ASIGNAR.
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
        "test_para_destr_out.txt";
#else
        "/tmp/test_para_destr_out.txt";
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
    /* Multi-destino clasico (tupla de pares) */
    {
        char out[512];
        ejecutar_capturando(
            "para a, b en [(1, 10), (2, 20), (3, 30)]:\n"
            "    imprimir(a, b)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 10") != NULL, "multi_a");
        AFIRMAR(strstr(out, "3 30") != NULL, "multi_c");
    }

    /* Star al final */
    {
        char out[512];
        ejecutar_capturando(
            "para primero, *resto en [[1, 2, 3], [10, 20, 30, 40], [99]]:\n"
            "    imprimir(primero, resto)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 [2, 3]") != NULL, "star_final_1");
        AFIRMAR(strstr(out, "10 [20, 30, 40]") != NULL, "star_final_2");
        AFIRMAR(strstr(out, "99 []") != NULL, "star_final_vacio");
    }

    /* Star al inicio */
    {
        char out[512];
        ejecutar_capturando(
            "para *previos, ultimo en [[1, 2, 3, 4], [10, 20, 30], [100, 200]]:\n"
            "    imprimir(ultimo, previos)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4 [1, 2, 3]") != NULL, "star_inicial_1");
        AFIRMAR(strstr(out, "30 [10, 20]") != NULL, "star_inicial_2");
        AFIRMAR(strstr(out, "200 [100]") != NULL, "star_inicial_3");
    }

    /* Star en medio */
    {
        char out[512];
        ejecutar_capturando(
            "para p, *m, u en [[1, 2, 3, 4, 5], [10, 20]]:\n"
            "    imprimir(p, m, u)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 [2, 3, 4] 5") != NULL, "star_medio_1");
        AFIRMAR(strstr(out, "10 [] 20") != NULL, "star_medio_vacio");
    }

    /* Dentro de funcion (slots locales) */
    {
        char out[512];
        ejecutar_capturando(
            "funcion sumar_pares(lst):\n"
            "    total = 0\n"
            "    para mm, n en lst:\n"
            "        total = total + mm + n\n"
            "    fin para\n"
            "    retornar total\n"
            "fin funcion\n"
            "imprimir(sumar_pares([(1, 2), (10, 20), (100, 200)]))\n",
            out, sizeof(out));
        /* 1+2 + 10+20 + 100+200 = 333 */
        AFIRMAR(strstr(out, "333") != NULL, "en_funcion");
    }

    /* Star dentro de funcion */
    {
        char out[512];
        ejecutar_capturando(
            "funcion segundos(filas):\n"
            "    res = []\n"
            "    para _, segundo, *_2 en filas:\n"
            "        agregar(res, segundo)\n"
            "    fin para\n"
            "    retornar res\n"
            "fin funcion\n"
            "imprimir(segundos([[1, 2, 3], [10, 20, 30, 40], [100, 200]]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 20, 200]") != NULL, "star_en_funcion");
    }

    /* Iteracion repetida no crece el stack (sanity: que las 100 iters
     * no rebosen ni dejen rastro entre vueltas) */
    {
        char out[512];
        ejecutar_capturando(
            "pares = []\n"
            "para mm en rango(0, 100):\n"
            "    agregar(pares, (mm, mm * 2))\n"
            "fin para\n"
            "total = 0\n"
            "para mm, n en pares:\n"
            "    total = total + n - mm\n"
            "fin para\n"
            "imprimir(total)\n",
            out, sizeof(out));
        /* sum(i for i in range(100)) = 4950 */
        AFIRMAR(strstr(out, "4950") != NULL, "loop_largo_sin_crecer");
    }

    /* Aridad incorrecta: lanza ErrorDeValor */
    {
        char out[512];
        ejecutar_capturando(
            "intentar:\n"
            "    para mm, n en [(1, 2), (3,)]:\n"
            "        imprimir(mm, n)\n"
            "    fin para\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2") != NULL, "aridad_imp_ok");
        AFIRMAR(strstr(out, "err") != NULL, "aridad_atrap");
    }

    /* Regresion: single ident sigue funcionando (clasico) */
    {
        char out[512];
        ejecutar_capturando(
            "para mm en [10, 20, 30]:\n"
            "    imprimir(mm)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10") != NULL && strstr(out, "30") != NULL,
                "regresion_clasico");
    }

    /* Regresion: one-liner clasico */
    {
        char out[512];
        ejecutar_capturando(
            "para mm en [1, 2, 3]: imprimir(mm)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL && strstr(out, "3") != NULL,
                "regresion_oneliner");
    }

    /* Anidados: para externo con destructuring + para interno
     * con ident clasico. Sanity de scopes. */
    {
        char out[512];
        ejecutar_capturando(
            "para mm, n en [(1, 2), (10, 20)]:\n"
            "    para x en [mm, n]:\n"
            "        imprimir(x)\n"
            "    fin para\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL && strstr(out, "2") != NULL
                && strstr(out, "10") != NULL && strstr(out, "20") != NULL,
                "anidados");
    }

    if (fallos == 0) {
        printf("para_destr: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "para_destr: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
