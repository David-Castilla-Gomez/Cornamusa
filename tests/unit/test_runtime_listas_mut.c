/*
 * Tests del runtime — Fase 5 Sesión 2: mutación, slicing, métodos.
 *
 * Cobertura:
 *   - `lista[i] = v` y `lista[i] += v`.
 *   - Slicing `lista[a:b]`, `lista[a:b:c]`, omisiones, paso negativo.
 *   - Built-ins de mutación: agregar, quitar, insertar, invertir, ordenar.
 *   - Semántica de referencia: mutar `b` cuando `b = a` afecta a `a`.
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
    static char buffer[4096];
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 4096);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (prog == NULL || p.tuvo_error) {
        if (error_out) *error_out = "<error de parseo>";
        arena_destruir(&a);
        return NULL;
    }
    Entorno globales; entorno_iniciar(&globales, NULL);
    nativos_registrar(&globales);
    Evaluador ev; evaluador_iniciar(&ev, &globales);
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

/* ───── Mutación ───── */

static void test_mutacion_simple(void) {
    verificar_var(
        "xs = [1, 2, 3]\n"
        "xs[0] = 99",
        "xs", "[99, 2, 3]");

    verificar_var(
        "xs = [1, 2, 3]\n"
        "xs[-1] = 99",
        "xs", "[1, 2, 99]");

    verificar_error(
        "xs = [1, 2]\n"
        "xs[5] = 0",
        "fuera de rango");

    /* Mutación cambia el tipo del elemento. */
    verificar_var(
        "xs = [1, 2, 3]\n"
        "xs[1] = \"hola\"",
        "xs", "[1, \"hola\", 3]");
}

static void test_mutacion_aug(void) {
    verificar_var(
        "xs = [10, 20, 30]\n"
        "xs[1] += 5",
        "xs", "[10, 25, 30]");

    verificar_var(
        "xs = [\"hola\", \"mundo\"]\n"
        "xs[0] += \" \"",
        "xs", "[\"hola \", \"mundo\"]");

    verificar_var(
        "xs = [1, 2, 3]\n"
        "xs[-1] *= 10",
        "xs", "[1, 2, 30]");
}

static void test_referencia_compartida_mutacion(void) {
    /* `b = a` comparte; mutar b afecta a a. */
    verificar_var(
        "a = [1, 2, 3]\n"
        "b = a\n"
        "b[0] = 99\n"
        "x = a[0]",
        "x", "99");

    verificar_var(
        "a = [1, 2, 3]\n"
        "b = a\n"
        "agregar(b, 4)\n"
        "x = longitud(a)",
        "x", "4");
}

/* ───── Slicing ───── */

static void test_slicing_basico(void) {
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[1:4]", "z", "[2, 3, 4]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[:3]", "z", "[1, 2, 3]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[2:]", "z", "[3, 4, 5]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[:]", "z", "[1, 2, 3, 4, 5]");
    /* Negativos. */
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[-2:]", "z", "[4, 5]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[:-2]", "z", "[1, 2, 3]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[-3:-1]", "z", "[3, 4]");
    /* Fuera de rango → clamp silencioso. */
    verificar_var("xs = [1, 2, 3]\nz = xs[0:99]", "z", "[1, 2, 3]");
    verificar_var("xs = [1, 2, 3]\nz = xs[5:10]", "z", "[]");
    /* Inicio > fin con paso positivo → vacío. */
    verificar_var("xs = [1, 2, 3, 4]\nz = xs[3:1]", "z", "[]");
}

static void test_slicing_paso(void) {
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[::2]", "z", "[1, 3, 5]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[1::2]", "z", "[2, 4]");
    /* Paso negativo: invierte. */
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[::-1]", "z", "[5, 4, 3, 2, 1]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[::-2]", "z", "[5, 3, 1]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[3:0:-1]", "z", "[4, 3, 2]");
    /* Paso 0 → error. */
    verificar_error(
        "xs = [1, 2, 3]\n"
        "x = xs[::0]",
        "no puede ser 0");
}

/* ───── Built-ins de mutación ───── */

static void test_agregar(void) {
    verificar_var(
        "xs = [1, 2, 3]\n"
        "agregar(xs, 4)",
        "xs", "[1, 2, 3, 4]");

    verificar_var(
        "xs = []\n"
        "agregar(xs, 1)\n"
        "agregar(xs, 2)\n"
        "agregar(xs, 3)",
        "xs", "[1, 2, 3]");
}

static void test_quitar(void) {
    /* Sin índice: quita el último. */
    verificar_var(
        "xs = [1, 2, 3]\n"
        "ultimo = quitar(xs)",
        "ultimo", "3");
    verificar_var(
        "xs = [1, 2, 3]\n"
        "quitar(xs)",
        "xs", "[1, 2]");

    /* Con índice. */
    verificar_var(
        "xs = [10, 20, 30, 40]\n"
        "extraido = quitar(xs, 1)",
        "extraido", "20");
    verificar_var(
        "xs = [10, 20, 30, 40]\n"
        "quitar(xs, 1)",
        "xs", "[10, 30, 40]");

    /* Negativo. */
    verificar_var(
        "xs = [10, 20, 30]\n"
        "x = quitar(xs, -1)",
        "x", "30");

    verificar_error(
        "xs = []\n"
        "x = quitar(xs)",
        "vacia");
}

static void test_insertar(void) {
    verificar_var(
        "xs = [1, 3]\n"
        "insertar(xs, 1, 2)",
        "xs", "[1, 2, 3]");

    verificar_var(
        "xs = [2, 3]\n"
        "insertar(xs, 0, 1)",
        "xs", "[1, 2, 3]");

    verificar_var(
        "xs = [1, 2]\n"
        "insertar(xs, 99, 3)",   /* clamp al final */
        "xs", "[1, 2, 3]");

    verificar_var(
        "xs = [1, 2]\n"
        "insertar(xs, -99, 0)",  /* clamp al inicio */
        "xs", "[0, 1, 2]");
}

static void test_invertir(void) {
    verificar_var(
        "xs = [1, 2, 3, 4, 5]\n"
        "invertir(xs)",
        "xs", "[5, 4, 3, 2, 1]");

    verificar_var(
        "xs = []\n"
        "invertir(xs)",
        "xs", "[]");

    verificar_var(
        "xs = [42]\n"
        "invertir(xs)",
        "xs", "[42]");
}

static void test_ordenar(void) {
    verificar_var(
        "xs = [3, 1, 4, 1, 5, 9, 2, 6]\n"
        "ordenar(xs)",
        "xs", "[1, 1, 2, 3, 4, 5, 6, 9]");

    /* Cadenas: lexicográfico. */
    verificar_var(
        "xs = [\"banana\", \"apple\", \"cherry\"]\n"
        "ordenar(xs)",
        "xs", "[\"apple\", \"banana\", \"cherry\"]");

    /* Mixto numérico (entero/decimal/booleano) → ok. */
    verificar_var(
        "xs = [3, 1.5, verdadero, 2]\n"
        "ordenar(xs)",
        "xs", "[verdadero, 1.5, 2, 3]");

    /* Mixto incomparable → error. */
    verificar_error(
        "xs = [1, \"hola\"]\n"
        "ordenar(xs)",
        "no puede comparar");
}

/* ───── Programa: quicksort manual ───── */

static void test_programa_quicksort(void) {
    /* Implementación in-place: solo asignación a índices y agregar. */
    verificar_var(
        "funcion ordenar_part(lista, lo, hi):\n"
        "    si lo >= hi:\n"
        "        retornar\n"
        "    fin si\n"
        "    pivote = lista[hi]\n"
        "    i = lo - 1\n"
        "    para j en rango(lo, hi):\n"
        "        si lista[j] <= pivote:\n"
        "            i += 1\n"
        "            t = lista[i]\n"
        "            lista[i] = lista[j]\n"
        "            lista[j] = t\n"
        "        fin si\n"
        "    fin para\n"
        "    t = lista[i + 1]\n"
        "    lista[i + 1] = lista[hi]\n"
        "    lista[hi] = t\n"
        "    p = i + 1\n"
        "    ordenar_part(lista, lo, p - 1)\n"
        "    ordenar_part(lista, p + 1, hi)\n"
        "fin funcion\n"
        "\n"
        "datos = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5]\n"
        "ordenar_part(datos, 0, longitud(datos) - 1)",
        "datos", "[1, 1, 2, 3, 3, 4, 5, 5, 5, 6, 9]");
}

int main(void) {
    test_mutacion_simple();
    test_mutacion_aug();
    test_referencia_compartida_mutacion();
    test_slicing_basico();
    test_slicing_paso();
    test_agregar();
    test_quitar();
    test_insertar();
    test_invertir();
    test_ordenar();
    test_programa_quicksort();

    if (fallos == 0) {
        printf("OK: todos los tests de mutacion/slicing/metodos pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
