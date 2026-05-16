/*
 * Tests del generador de docs (v1.51 - Fase 5 tooling).
 *
 * Verifica:
 *   - H1 con nombre de modulo.
 *   - Doc del modulo: bloque inicial de comentarios.
 *   - H2 con firma de cada funcion top-level.
 *   - Comentario inmediatamente anterior se asocia al item.
 *   - Una linea en blanco corta la asociacion.
 *   - Clases generan H2 + metodos como H3.
 *   - Parametros con `*args`/`**kw` se muestran correctamente.
 *   - Defaults se muestran como `=...`.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "docs.h"
#include "lexer.h"
#include "parser.h"

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

static char *generar(const char *fuente, const char *nombre_modulo) {
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Arena a;
    arena_iniciar(&a, 4096);
    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    if (p.tuvo_error) {
        fprintf(stderr, "ERROR: el fuente de test no parsea:\n%s\n", fuente);
        fallos++;
        arena_destruir(&a);
        return strdup("");
    }
    DocsResultado r = docs_generar(fuente, nombre_modulo, sents, n);
    char *out = r.markdown ? strdup(r.markdown) : strdup("");
    docs_resultado_destruir(&r);
    arena_destruir(&a);
    return out;
}

static bool contiene(const char *h, const char *necesita) {
    return strstr(h, necesita) != NULL;
}

int main(void) {
    /* H1 con nombre de modulo. */
    {
        char *md = generar(
            "funcion f():\n"
            "    retornar 1\n"
            "fin funcion\n", "ejemplo");
        AFIRMAR(contiene(md, "# ejemplo\n"), "h1_modulo");
        free(md);
    }

    /* Doc del modulo: comentarios iniciales. */
    {
        char *md = generar(
            "# Modulo de prueba.\n"
            "# Segunda linea.\n"
            "funcion f():\n"
            "    retornar 1\n"
            "fin funcion\n",
            "ejemplo");
        AFIRMAR(contiene(md, "Modulo de prueba.\nSegunda linea."), "doc_modulo");
        free(md);
    }

    /* Funcion top-level genera H2 con su firma. */
    {
        char *md = generar(
            "funcion sumar(a, b):\n"
            "    retornar a + b\n"
            "fin funcion\n",
            "m");
        AFIRMAR(contiene(md, "## `sumar(a, b)`"), "firma_funcion_h2");
        free(md);
    }

    /* Comentario inmediatamente anterior se asocia a la funcion. */
    {
        char *md = generar(
            "# Multiplica dos numeros.\n"
            "funcion mul(a, b):\n"
            "    retornar a * b\n"
            "fin funcion\n",
            "m");
        AFIRMAR(contiene(md, "## `mul(a, b)`"), "firma_mul");
        AFIRMAR(contiene(md, "Multiplica dos numeros."), "doc_mul");
        free(md);
    }

    /* Linea en blanco entre comentario y declaracion CORTA la asociacion.
     * Ponemos el comentario aislado entre dos funciones para que no se lo
     * coma el doc del modulo. */
    {
        char *md = generar(
            "funcion primera():\n"
            "    retornar 1\n"
            "fin funcion\n"
            "\n"
            "# Comentario aislado.\n"
            "\n"
            "funcion g():\n"
            "    retornar 0\n"
            "fin funcion\n",
            "m");
        AFIRMAR(!contiene(md, "Comentario aislado."), "blank_corta_asociacion");
        free(md);
    }

    /* Defaults se muestran como `=...`. */
    {
        char *md = generar(
            "funcion f(a, b=10):\n"
            "    retornar a + b\n"
            "fin funcion\n",
            "m");
        AFIRMAR(contiene(md, "## `f(a, b=...)`"), "default_eq_puntos");
        free(md);
    }

    /* *args y **kw. */
    {
        char *md = generar(
            "funcion f(a, *rest, **kw):\n"
            "    retornar a\n"
            "fin funcion\n",
            "m");
        AFIRMAR(contiene(md, "## `f(a, *rest, **kw)`"), "varargs");
        free(md);
    }

    /* Clase con metodos -> H2 clase + H3 metodos. */
    {
        char *md = generar(
            "# Clase de prueba.\n"
            "clase Punto:\n"
            "    # Constructor.\n"
            "    funcion __iniciar__(yo, x):\n"
            "        yo.x = x\n"
            "    fin funcion\n"
            "    # Cuadrado de x.\n"
            "    funcion cuadrar(yo):\n"
            "        retornar yo.x * yo.x\n"
            "    fin funcion\n"
            "fin clase\n",
            "m");
        AFIRMAR(contiene(md, "## clase `Punto`"), "h2_clase");
        AFIRMAR(contiene(md, "Clase de prueba."), "doc_clase");
        AFIRMAR(contiene(md, "### `__iniciar__(yo, x)`"), "h3_init");
        AFIRMAR(contiene(md, "Constructor."), "doc_init");
        AFIRMAR(contiene(md, "### `cuadrar(yo)`"), "h3_cuadrar");
        AFIRMAR(contiene(md, "Cuadrado de x."), "doc_cuadrar");
        free(md);
    }

    /* Clase con extiende. */
    {
        char *md = generar(
            "clase Hijo extiende Padre:\n"
            "    pasar\n"
            "fin clase\n",
            "m");
        AFIRMAR(contiene(md, "extiende ..."), "extiende");
        free(md);
    }

    /* Funcion sin comentario no produce doc, solo firma. */
    {
        char *md = generar(
            "funcion h():\n"
            "    retornar 1\n"
            "fin funcion\n",
            "m");
        AFIRMAR(contiene(md, "## `h()`"), "sin_doc_solo_firma");
        free(md);
    }

    /* Multiples funciones, orden preservado. */
    {
        char *md = generar(
            "funcion primero():\n    retornar 1\nfin funcion\n"
            "funcion segundo():\n    retornar 2\nfin funcion\n"
            "funcion tercero():\n    retornar 3\nfin funcion\n",
            "m");
        const char *p1 = strstr(md, "primero");
        const char *p2 = strstr(md, "segundo");
        const char *p3 = strstr(md, "tercero");
        AFIRMAR(p1 && p2 && p3 && p1 < p2 && p2 < p3, "orden_preservado");
        free(md);
    }

    /* Modulo vacio: solo H1 (y \n\n). */
    {
        char *md = generar("", "vacio");
        AFIRMAR(contiene(md, "# vacio"), "h1_vacio");
        free(md);
    }

    if (fallos == 0) {
        printf("docs: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "docs: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
