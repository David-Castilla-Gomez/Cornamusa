/*
 * Tests del runtime — Fase 5 Sesión 1: tipo Lista.
 *
 * Cobertura:
 *   - Literal `[a, b, c]`, lista vacía `[]`, anidamiento.
 *   - Indexación con índice positivo y negativo, fuera de rango.
 *   - Operadores `+` (concat), `*` (repetición), `en` (membership).
 *   - Igualdad estructural element-wise.
 *   - Iteración `para x en lista`.
 *   - Built-in `longitud()` y `tipo()` sobre listas.
 *   - Semántica de referencia compartida: `b = a` — la mutación de `b`
 *     se refleja en `a` (próxima sesión añadirá la mutación; aquí
 *     verificamos solo que la asignación comparte refcount sin
 *     romper).
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "entorno.h"
#include "evaluador.h"
#include "lexer.h"
#include "nativos.h"
#include "parser.h"
#include "valor.h"

static int fallos = 0;

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n",                            \
                    __FILE__, __LINE__, #cond);                                \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

static const char *ejecutar_y_leer(const char *fuente, const char *nombre_var,
                                    const char **error_out) {
    static char buffer[2048];

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");

    Arena a;
    arena_iniciar(&a, 4096);

    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");

    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (prog == NULL || p.tuvo_error) {
        if (error_out) *error_out = "<error de parseo>";
        arena_destruir(&a);
        return NULL;
    }

    Entorno globales;
    entorno_iniciar(&globales, NULL);
    nativos_registrar(&globales);

    Evaluador ev;
    evaluador_iniciar(&ev, &globales);

    evaluador_ejecutar_programa(&ev, prog, n);

    if (ev.error.tuvo_error) {
        if (error_out) {
            static char errbuf[1024];
            snprintf(errbuf, sizeof(errbuf), "%s", ev.error.mensaje);
            *error_out = errbuf;
        }
        valor_destruir(&ev.valor_retorno);
        entorno_destruir(&globales);
        arena_destruir(&a);
        return NULL;
    }

    Valor v;
    if (!entorno_obtener(&globales, nombre_var, (int)strlen(nombre_var), &v)) {
        if (error_out) *error_out = "<variable no encontrada>";
        valor_destruir(&ev.valor_retorno);
        entorno_destruir(&globales);
        arena_destruir(&a);
        return NULL;
    }

    valor_a_cadena(&v, buffer, sizeof(buffer));
    valor_destruir(&v);
    valor_destruir(&ev.valor_retorno);
    entorno_destruir(&globales);
    arena_destruir(&a);
    if (error_out) *error_out = NULL;
    return buffer;
}

static void verificar_var(const char *fuente, const char *var,
                          const char *esperado) {
    const char *err = NULL;
    const char *resultado = ejecutar_y_leer(fuente, var, &err);
    if (resultado == NULL) {
        fprintf(stderr, "FALLO en programa:\n%s\n  error: %s\n",
                fuente, err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(resultado, esperado) != 0) {
        fprintf(stderr, "FALLO en programa:\n%s\n  esperaba %s=%s\n  obtuvo: %s\n",
                fuente, var, esperado, resultado);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *resultado = ejecutar_y_leer(fuente, "x", &err);
    if (resultado != NULL) {
        fprintf(stderr, "FALLO: programa debería dar error pero ejecutó:\n%s\n  resultado: %s\n",
                fuente, resultado);
        fallos++;
        return;
    }
    if (err == NULL || strstr(err, substring) == NULL) {
        fprintf(stderr, "FALLO: programa\n%s\n  dio error '%s' pero se esperaba '%s'\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

/* ───── Literal ───── */

static void test_literal(void) {
    verificar_var("xs = []", "xs", "[]");
    verificar_var("xs = [1, 2, 3]", "xs", "[1, 2, 3]");
    /* Cadenas en lista usan repr (con comillas). */
    verificar_var("xs = [\"a\", \"b\"]", "xs", "[\"a\", \"b\"]");
    /* Mixta. */
    verificar_var("xs = [1, \"hola\", verdadero, nulo, 3.14]", "xs",
        "[1, \"hola\", verdadero, nulo, 3.14]");
    /* Anidada. */
    verificar_var("xs = [[1, 2], [3, 4]]", "xs", "[[1, 2], [3, 4]]");
    /* Trailing comma permitida por el parser. */
    verificar_var("xs = [1, 2, 3,]", "xs", "[1, 2, 3]");
}

/* ───── Indexación ───── */

static void test_indexacion(void) {
    verificar_var("xs = [10, 20, 30]\nx = xs[0]", "x", "10");
    verificar_var("xs = [10, 20, 30]\nx = xs[1]", "x", "20");
    verificar_var("xs = [10, 20, 30]\nx = xs[2]", "x", "30");
    /* Negativos: cuentan desde el final. */
    verificar_var("xs = [10, 20, 30]\nx = xs[-1]", "x", "30");
    verificar_var("xs = [10, 20, 30]\nx = xs[-3]", "x", "10");
    /* Fuera de rango. */
    verificar_error("xs = [1, 2]\nx = xs[5]", "fuera de rango");
    verificar_error("xs = [1, 2]\nx = xs[-3]", "fuera de rango");
    /* Indice no entero. */
    verificar_error("xs = [1, 2]\nx = xs[\"a\"]", "indice de lista debe ser entero");
    /* Anidada. */
    verificar_var("xs = [[1, 2], [3, 4]]\nx = xs[1][0]", "x", "3");
}

/* ───── Operadores ───── */

static void test_operadores(void) {
    verificar_var("xs = [1, 2] + [3, 4]", "xs", "[1, 2, 3, 4]");
    verificar_var("xs = [] + [1]", "xs", "[1]");
    verificar_var("xs = [1, 2] * 3", "xs", "[1, 2, 1, 2, 1, 2]");
    verificar_var("xs = 0 * [1, 2]", "xs", "[]");
    verificar_var("xs = [1, 2] * -1", "xs", "[]");

    /* Membership. */
    verificar_var("x = 2 en [1, 2, 3]", "x", "verdadero");
    verificar_var("x = 5 en [1, 2, 3]", "x", "falso");
    verificar_var("x = \"hola\" en [\"hola\", \"mundo\"]", "x", "verdadero");
    verificar_var("x = 2 no en [1, 2, 3]", "x", "falso");

    /* Igualdad estructural. */
    verificar_var("x = [1, 2, 3] == [1, 2, 3]", "x", "verdadero");
    verificar_var("x = [1, 2, 3] == [1, 2]", "x", "falso");
    verificar_var("x = [1, 2] == [1, 2.0]", "x", "verdadero");  /* cross-tipo */
    verificar_var("x = [] == []", "x", "verdadero");
}

/* ───── Iteración ───── */

static void test_iteracion(void) {
    verificar_var(
        "total = 0\n"
        "para x en [1, 2, 3, 4, 5]:\n"
        "    total += x\n"
        "fin para",
        "total", "15");

    /* Mezcla de tipos. */
    verificar_var(
        "concat = \"\"\n"
        "para x en [\"hola\", \" \", \"mundo\"]:\n"
        "    concat += x\n"
        "fin para",
        "concat", "hola mundo");

    /* Romper. */
    verificar_var(
        "encontrado = -1\n"
        "para x en [3, 1, 4, 1, 5, 9]:\n"
        "    si x == 4:\n"
        "        encontrado = x\n"
        "        romper\n"
        "    fin si\n"
        "fin para",
        "encontrado", "4");

    /* Sino: ejecutado si no hubo break. */
    verificar_var(
        "ok = falso\n"
        "para x en [1, 2, 3]:\n"
        "    pasar\n"
        "sino:\n"
        "    ok = verdadero\n"
        "fin para",
        "ok", "verdadero");
}

/* ───── Built-ins ───── */

static void test_builtins(void) {
    verificar_var("x = longitud([1, 2, 3])", "x", "3");
    verificar_var("x = longitud([])", "x", "0");
    verificar_var("x = tipo([1, 2])", "x", "lista");
}

/* ───── Referencia compartida ───── */

static void test_referencia_compartida(void) {
    /* `b = a` debe compartir referencia. La igualdad lo confirma — y
     * más importante: no produce double-free al destruir el entorno.
     * (Si fueran propiedad independiente, el segundo destruir sería
     * doble libre y crashearía.) */
    verificar_var(
        "a = [1, 2, 3]\n"
        "b = a\n"
        "x = b == a",
        "x", "verdadero");
}

/* ───── Programas realistas ───── */

static void test_programa_promedio(void) {
    verificar_var(
        "numeros = [10, 20, 30, 40, 50]\n"
        "total = 0\n"
        "para n en numeros:\n"
        "    total += n\n"
        "fin para\n"
        "promedio = total / longitud(numeros)",
        "promedio", "30.0");
}

static void test_programa_construir_con_rango(void) {
    /* Usar rango() con un acumulador para construir una lista de
     * cuadrados con `+ [n]`. */
    verificar_var(
        "cuadrados = []\n"
        "para n en rango(1, 6):\n"
        "    cuadrados = cuadrados + [n * n]\n"
        "fin para",
        "cuadrados", "[1, 4, 9, 16, 25]");
}

/* ───── Main ───── */

int main(void) {
    test_literal();
    test_indexacion();
    test_operadores();
    test_iteracion();
    test_builtins();
    test_referencia_compartida();
    test_programa_promedio();
    test_programa_construir_con_rango();

    if (fallos == 0) {
        printf("OK: todos los tests de listas pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
