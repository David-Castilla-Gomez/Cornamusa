/*
 * Tests del runtime — Fase 5 Sesión 4: Conjunto y Tupla.
 *
 * Cobertura:
 *   - Conjunto: literal, deduplicación, en, no en, iteración, longitud,
 *     igualdad estructural, tipos no hashables como elemento.
 *   - Tupla: literal vacío `()`, de un elemento `(x,)`, indexación,
 *     iteración, longitud, igualdad.
 *   - Tupla como clave de diccionario (hashable cuando elementos lo son).
 *   - Distinción `(x)` (grupo) vs `(x,)` (tupla 1).
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

/* ───── Conjunto ───── */

static void test_conjunto_literal(void) {
    /* Deduplicación. */
    verificar_var(
        "s = {1, 2, 3, 2, 1}\n"
        "x = longitud(s)",
        "x", "3");

    verificar_var("x = longitud({})", "x", "0");  /* {} es dicc, no conjunto */
    verificar_var("x = tipo({1, 2})", "x", "conjunto");
    verificar_var("x = tipo({})", "x", "diccionario");
}

static void test_conjunto_membership(void) {
    verificar_var(
        "s = {1, 2, 3}\n"
        "x = 2 en s",
        "x", "verdadero");

    verificar_var(
        "s = {1, 2, 3}\n"
        "x = 99 en s",
        "x", "falso");

    verificar_var(
        "s = {\"a\", \"b\"}\n"
        "x = \"c\" no en s",
        "x", "verdadero");
}

static void test_conjunto_iteracion(void) {
    /* Sumar todos los elementos: el orden interno no importa para suma. */
    verificar_var(
        "s = {1, 2, 3, 4, 5}\n"
        "total = 0\n"
        "para x en s:\n"
        "    total += x\n"
        "fin para",
        "total", "15");
}

static void test_conjunto_igualdad(void) {
    verificar_var(
        "x = {1, 2, 3} == {3, 2, 1}",
        "x", "verdadero");

    verificar_var(
        "x = {1, 2} == {1, 2, 3}",
        "x", "falso");

    /* Hash unificado: 1 y 1.0 dedupean. */
    verificar_var(
        "s = {1, 1.0, verdadero}\n"
        "x = longitud(s)",
        "x", "1");
}

static void test_conjunto_no_hashable(void) {
    verificar_error(
        "s = {[1, 2]}",
        "no se puede usar como elemento");
}

/* ───── Tupla ───── */

static void test_tupla_literal(void) {
    verificar_var("t = ()", "t", "()");
    verificar_var("t = (42,)", "t", "(42,)");
    verificar_var("t = (1, 2, 3)", "t", "(1, 2, 3)");
    /* Distinción tupla vs grupo: (x) es grupo, (x,) es tupla. */
    verificar_var("t = (5)", "t", "5");           /* grupo */
    verificar_var("t = (5,)", "t", "(5,)");       /* tupla */

    verificar_var("x = tipo((1, 2))", "x", "tupla");
    verificar_var("x = tipo(())", "x", "tupla");
    verificar_var("x = tipo((1,))", "x", "tupla");

    verificar_var("x = longitud((1, 2, 3))", "x", "3");
    verificar_var("x = longitud(())", "x", "0");
}

static void test_tupla_indexacion(void) {
    verificar_var("t = (10, 20, 30)\nx = t[0]", "x", "10");
    verificar_var("t = (10, 20, 30)\nx = t[-1]", "x", "30");
    verificar_error(
        "t = (1, 2)\n"
        "x = t[5]",
        "fuera de rango");
}

static void test_tupla_iteracion(void) {
    verificar_var(
        "total = 0\n"
        "para x en (1, 2, 3, 4):\n"
        "    total += x\n"
        "fin para",
        "total", "10");
}

static void test_tupla_igualdad(void) {
    verificar_var("x = (1, 2, 3) == (1, 2, 3)", "x", "verdadero");
    verificar_var("x = (1, 2, 3) == (1, 2)", "x", "falso");
    verificar_var("x = (1, 2) == [1, 2]", "x", "falso");  /* tupla != lista */
    verificar_var("x = () == ()", "x", "verdadero");
}

static void test_tupla_membership(void) {
    verificar_var("x = 2 en (1, 2, 3)", "x", "verdadero");
    verificar_var("x = 99 en (1, 2, 3)", "x", "falso");
}

static void test_tupla_como_clave(void) {
    /* Las tuplas son hashables si todos sus elementos lo son. */
    verificar_var(
        "mapa = {(1, 2): \"a\", (3, 4): \"b\"}\n"
        "x = mapa[(1, 2)]",
        "x", "a");

    verificar_var(
        "mapa = {(1, 2): \"a\"}\n"
        "x = (1, 2) en mapa",
        "x", "verdadero");

    /* Tupla con lista dentro NO es hashable. */
    verificar_error(
        "mapa = {(1, [2, 3]): \"x\"}",
        "no se puede usar como clave");
}

/* ───── Built-in conjunto() ───── */

static void test_conjunto_constructor(void) {
    /* Vacío. */
    verificar_var("c = conjunto()\nx = longitud(c)", "x", "0");
    /* Desde lista (con duplicados). */
    verificar_var("c = conjunto([1, 2, 3, 2, 1])\nx = longitud(c)", "x", "3");
    /* Desde tupla. */
    verificar_var("c = conjunto((\"a\", \"b\", \"a\"))\nx = longitud(c)", "x", "2");
}

/* ───── Programa: cuenta única de palabras ───── */

static void test_programa_palabras_unicas(void) {
    verificar_var(
        "palabras = conjunto()\n"
        "agregar(palabras, \"hola\")\n"
        "agregar(palabras, \"mundo\")\n"
        "agregar(palabras, \"hola\")\n"
        "agregar(palabras, \"adios\")\n"
        "x = longitud(palabras)",
        "x", "3");
}

int main(void) {
    test_conjunto_literal();
    test_conjunto_membership();
    test_conjunto_iteracion();
    test_conjunto_igualdad();
    test_conjunto_no_hashable();
    test_tupla_literal();
    test_tupla_indexacion();
    test_tupla_iteracion();
    test_tupla_igualdad();
    test_tupla_membership();
    test_tupla_como_clave();
    test_conjunto_constructor();
    test_programa_palabras_unicas();

    if (fallos == 0) {
        printf("OK: todos los tests de conjunto y tupla pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
