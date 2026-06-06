/*
 * Tests de `contar_si(p, xs)` y `unicos_por(xs, clave)` en stdlib
 * (v1.149).
 *
 * Cornamusa ya tenia:
 *   - `xs.contar(x)` como metodo de lista/tupla (contar igualdad).
 *   - `s.contar(sub)` como metodo de cadena (substring).
 *   - `unicos(xs)` funcional, deduplicar por igualdad directa.
 *
 * v1.149 anade las versiones FUNCIONALES por predicado/clave:
 *   - `contar_si(p, xs)`: cuenta elementos donde p(x) es verdadero.
 *     Sin materializar lista intermedia (mas eficiente que
 *     longitud(filtrar(p, xs))).
 *   - `unicos_por(xs, clave)`: deduplica usando clave(x) como
 *     identidad. El elemento devuelto es el ORIGINAL; en colision
 *     gana el primero. Util cuando los elementos no son hashables
 *     directamente o cuando "unico" significa "unico por algun
 *     aspecto".
 *
 * Sin cambios al nucleo. Solo stdlib usando features existentes.
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
        "test_contar_uniq_out.txt";
#else
        "/tmp/test_contar_uniq_out.txt";
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
    /* contar_si: caso basico */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar contar_si\n"
            "imprimir(contar_si(lambda x: x > 0, [-1, 2, -3, 4, 5]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "contar_positivos");
    }

    /* contar_si: igualdad sobre cadenas */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar contar_si\n"
            "imprimir(contar_si(lambda x: x == \"a\", [\"a\", \"b\", \"a\", \"c\", \"a\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "contar_letra");
    }

    /* contar_si: vacio */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar contar_si\n"
            "imprimir(contar_si(lambda x: verdadero, []))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "contar_vacio");
    }

    /* contar_si: ninguno cumple */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar contar_si\n"
            "imprimir(contar_si(lambda x: falso, [1, 2, 3]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "contar_ninguno");
    }

    /* contar_si: todos cumplen */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar contar_si\n"
            "imprimir(contar_si(lambda x: verdadero, [1, 2, 3, 4, 5]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "contar_todos");
    }

    /* unicos_por: dedup por atributo */
    {
        char out[512];
        ejecutar_capturando(
            "desde funcionales importar unicos_por\n"
            "users = [\n"
            "    {\"id\": 1, \"n\": \"Ana\"},\n"
            "    {\"id\": 2, \"n\": \"Bea\"},\n"
            "    {\"id\": 1, \"n\": \"AnaDup\"},\n"
            "    {\"id\": 3, \"n\": \"Carlos\"},\n"
            "]\n"
            "imprimir([u[\"n\"] para u en unicos_por(users, lambda d: d[\"id\"])])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"Ana\", \"Bea\", \"Carlos\"]") != NULL,
                "unicos_por_id");
    }

    /* unicos_por: primer encontrado gana en colision */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar unicos_por\n"
            "palabras = [\"arbol\", \"amigo\", \"barco\", \"cesta\", \"alma\", \"bici\"]\n"
            "imprimir(unicos_por(palabras, lambda s: s[0]))\n",
            out, sizeof(out));
        /* a -> arbol, b -> barco, c -> cesta. Los segundos (amigo,
         * alma, bici) se descartan. */
        AFIRMAR(strstr(out, "[\"arbol\", \"barco\", \"cesta\"]") != NULL,
                "unicos_por_primer_gana");
    }

    /* unicos_por: vacio */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar unicos_por\n"
            "imprimir(unicos_por([], lambda x: x))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "unicos_por_vacio");
    }

    /* unicos_por: todos distintos */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar unicos_por\n"
            "imprimir(unicos_por([1, 2, 3], lambda x: x))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "unicos_por_distintos");
    }

    /* Regresion: `unicos` clasico sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar unicos\n"
            "imprimir(unicos([1, 2, 1, 3, 2, 4]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4]") != NULL, "regr_unicos");
    }

    if (fallos == 0) {
        printf("contar_uniq: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "contar_uniq: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
