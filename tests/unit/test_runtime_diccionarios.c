/*
 * Tests del runtime — Fase 5 Sesión 3: Diccionario.
 *
 * Cobertura:
 *   - Literal `{k: v, ...}`, vacío `{}`, mixto.
 *   - Acceso `dicc[clave]`, error `ErrorDeClave` cuando falta.
 *   - Asignación `dicc[k] = v` (insertar o actualizar).
 *   - `+=` aumentada sobre clave existente.
 *   - Operador `en` (membership de claves).
 *   - Iteración `para k en dicc` (yields claves).
 *   - Built-ins: longitud(dicc), tipo(dicc), claves(dicc), valores(dicc).
 *   - Igualdad estructural.
 *   - Hash: enteros y decimales numéricamente iguales acceden al mismo slot
 *     (`dicc[1]` y `dicc[1.0]` mismo slot).
 *   - Tipos no hashables como clave: error.
 *   - Semántica de referencia compartida.
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

/* ───── Literal y acceso ───── */

static void test_literal_y_acceso(void) {
    /* Vacío. */
    verificar_var("d = {}", "d", "{}");

    /* Una entrada. */
    verificar_var("d = {\"a\": 1}\nx = d[\"a\"]", "x", "1");

    /* Acceso a inexistente → error. */
    verificar_error(
        "d = {\"a\": 1}\n"
        "x = d[\"b\"]",
        "ErrorDeClave");

    /* Tipos mixtos. */
    verificar_var("d = {1: \"uno\", 2: \"dos\"}\nx = d[1]", "x", "uno");
}

/* ───── Asignación ───── */

static void test_asignar(void) {
    verificar_var(
        "d = {}\n"
        "d[\"clave\"] = 42\n"
        "x = d[\"clave\"]",
        "x", "42");

    /* Sobrescritura. */
    verificar_var(
        "d = {\"a\": 1}\n"
        "d[\"a\"] = 99\n"
        "x = d[\"a\"]",
        "x", "99");

    /* Aumentada. */
    verificar_var(
        "d = {\"x\": 10}\n"
        "d[\"x\"] += 5\n"
        "x = d[\"x\"]",
        "x", "15");

    /* Aumentada sobre clave inexistente → error. */
    verificar_error(
        "d = {}\n"
        "d[\"x\"] += 1",
        "ErrorDeClave");
}

/* ───── Operador en ───── */

static void test_membership(void) {
    verificar_var(
        "d = {\"a\": 1, \"b\": 2}\n"
        "x = \"a\" en d",
        "x", "verdadero");

    verificar_var(
        "d = {\"a\": 1}\n"
        "x = \"z\" en d",
        "x", "falso");

    verificar_var(
        "d = {\"a\": 1}\n"
        "x = \"a\" no en d",
        "x", "falso");
}

/* ───── Iteración ───── */

static void test_iteracion(void) {
    /* Sumar valores iterando claves. */
    verificar_var(
        "d = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
        "total = 0\n"
        "para k en d:\n"
        "    total += d[k]\n"
        "fin para",
        "total", "6");

    /* Romper. */
    verificar_var(
        "d = {\"a\": 1, \"b\": 2}\n"
        "n = 0\n"
        "para k en d:\n"
        "    n += 1\n"
        "    romper\n"
        "fin para",
        "n", "1");

    /* Cláusula sino. */
    verificar_var(
        "d = {\"a\": 1}\n"
        "ok = falso\n"
        "para k en d:\n"
        "    pasar\n"
        "sino:\n"
        "    ok = verdadero\n"
        "fin para",
        "ok", "verdadero");
}

/* ───── Built-ins ───── */

static void test_longitud_y_tipo(void) {
    verificar_var("x = longitud({\"a\": 1, \"b\": 2})", "x", "2");
    verificar_var("x = longitud({})", "x", "0");
    verificar_var("x = tipo({\"a\": 1})", "x", "diccionario");
}

static void test_claves_y_valores(void) {
    /* claves() devuelve una lista; el orden puede variar pero
       longitud y contenido son determinísticos. */
    verificar_var(
        "d = {\"a\": 1}\n"
        "ks = claves(d)\n"
        "x = longitud(ks)",
        "x", "1");

    /* Verificamos que claves() permite recorrer y reconstruir. */
    verificar_var(
        "d = {1: \"uno\", 2: \"dos\", 3: \"tres\"}\n"
        "total = 0\n"
        "para k en claves(d):\n"
        "    total += k\n"
        "fin para",
        "total", "6");

    /* valores() análogo. */
    verificar_var(
        "d = {\"a\": 10, \"b\": 20, \"c\": 30}\n"
        "total = 0\n"
        "para v en valores(d):\n"
        "    total += v\n"
        "fin para",
        "total", "60");
}

/* ───── Igualdad ───── */

static void test_igualdad(void) {
    verificar_var(
        "x = {\"a\": 1, \"b\": 2} == {\"b\": 2, \"a\": 1}",
        "x", "verdadero");

    verificar_var(
        "x = {\"a\": 1} == {\"a\": 1, \"b\": 2}",
        "x", "falso");

    verificar_var(
        "x = {\"a\": 1} == {\"a\": 2}",
        "x", "falso");

    verificar_var("x = {} == {}", "x", "verdadero");
}

/* ───── Hash unificado para entero/decimal/booleano ───── */

static void test_hash_numerico(void) {
    /* dicc[1] y dicc[1.0] deberían acceder al mismo slot. */
    verificar_var(
        "d = {1: \"primero\"}\n"
        "x = d[1.0]",
        "x", "primero");

    verificar_var(
        "d = {1.0: \"primero\"}\n"
        "x = d[1]",
        "x", "primero");

    /* Y verdadero/1 también. */
    verificar_var(
        "d = {verdadero: \"si\"}\n"
        "x = d[1]",
        "x", "si");

    /* Asignar luego con tipos diferentes sobrescribe. */
    verificar_var(
        "d = {}\n"
        "d[1] = \"a\"\n"
        "d[1.0] = \"b\"\n"
        "x = d[1]",
        "x", "b");
}

/* ───── Tipos no hashables ───── */

static void test_no_hashable(void) {
    verificar_error(
        "d = {[1, 2]: \"x\"}",
        "no se puede usar como clave");

    verificar_error(
        "d = {}\n"
        "d[[1, 2]] = \"x\"",
        "no se puede usar como clave");
}

/* ───── Referencia compartida ───── */

static void test_referencia_compartida(void) {
    verificar_var(
        "a = {\"x\": 1}\n"
        "b = a\n"
        "b[\"x\"] = 99\n"
        "x = a[\"x\"]",
        "x", "99");
}

/* ───── Programas realistas ───── */

static void test_programa_conteo(void) {
    /* Contar ocurrencias de cada caracter en una cadena. */
    verificar_var(
        "frase = \"abracadabra\"\n"
        "conteo = {}\n"
        "para c en frase:\n"
        "    si c en conteo:\n"
        "        conteo[c] += 1\n"
        "    sino:\n"
        "        conteo[c] = 1\n"
        "    fin si\n"
        "fin para\n"
        "x = conteo[\"a\"]",
        "x", "5");
}

static void test_programa_dicc_anidado(void) {
    verificar_var(
        "personas = {}\n"
        "personas[\"Ana\"] = {\"edad\": 30, \"ciudad\": \"Madrid\"}\n"
        "personas[\"Luis\"] = {\"edad\": 25, \"ciudad\": \"Sevilla\"}\n"
        "x = personas[\"Ana\"][\"ciudad\"]",
        "x", "Madrid");
}

int main(void) {
    test_literal_y_acceso();
    test_asignar();
    test_membership();
    test_iteracion();
    test_longitud_y_tipo();
    test_claves_y_valores();
    test_igualdad();
    test_hash_numerico();
    test_no_hashable();
    test_referencia_compartida();
    test_programa_conteo();
    test_programa_dicc_anidado();

    if (fallos == 0) {
        printf("OK: todos los tests de diccionarios pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
