/*
 * Tests de los built-ins JSON (v1.9): json_parsear, json_serializar.
 *
 * Cubre:
 *   - Round-trip de tipos primitivos.
 *   - Auto-traducción true/false/null ↔ verdadero/falso/nulo.
 *   - Estructuras anidadas.
 *   - Escapes en cadenas (\n, \t, \", \\, \uXXXX).
 *   - Errores de sintaxis JSON.
 *   - Errores de tipo al serializar (instancia, función).
 */

#include <stdio.h>
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

static const char *ejecutar(const char *fuente, const char *nombre_var,
                              const char **error_out) {
    static char buffer[8192];
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (!prog || p.tuvo_error) {
        if (error_out) *error_out = "<error de parseo>";
        arena_destruir(&a);
        return NULL;
    }
    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, prog, n)) {
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", c.error.mensaje);
            *error_out = errbuf;
        }
        chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    VM vm; vm_iniciar(&vm);
    Valor resultado = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &resultado);
    if (rc != VM_OK) {
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", vm.error.mensaje);
            *error_out = errbuf;
        }
        valor_destruir(&resultado);
        vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    Valor nombre = valor_cadena_referencia(nombre_var, (int)strlen(nombre_var));
    Valor v;
    if (!dicc_obtener(vm.globales, &nombre, &v)) {
        if (error_out) *error_out = "<variable no encontrada>";
        valor_destruir(&resultado);
        vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    valor_a_cadena(&v, buffer, sizeof(buffer));
    valor_destruir(&v);
    valor_destruir(&resultado);
    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
    if (error_out) *error_out = NULL;
    return buffer;
}

static void verificar_var(const char *fuente, const char *var,
                           const char *esperado) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, var, &err);
    if (!res) {
        fprintf(stderr, "FALLO: %s\n  -> error: %s\n", fuente,
                err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO: %s\n  -> %s=%s (esperaba %s)\n",
                fuente, var, res, esperado);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, "_x", &err);
    if (res) {
        fprintf(stderr, "FALLO: '%s' debia dar error pero ejecuto\n", fuente);
        fallos++;
        return;
    }
    if (!err || !strstr(err, substring)) {
        fprintf(stderr, "FALLO: '%s' dio '%s' (esperaba '%s')\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

/* ───── Parse de primitivos ───── */

static void test_parse_primitivos(void) {
    verificar_var("x = json_parsear(\"42\")",        "x", "42");
    verificar_var("x = json_parsear(\"-7\")",        "x", "-7");
    verificar_var("x = json_parsear(\"3.14\")",      "x", "3.14");
    verificar_var("x = json_parsear(\"1.5e2\")",     "x", "150.0");
    verificar_var("x = json_parsear(\"\\\"hola\\\"\")", "x", "hola");
    verificar_var("x = json_parsear(\"true\")",      "x", "verdadero");
    verificar_var("x = json_parsear(\"false\")",     "x", "falso");
    verificar_var("x = json_parsear(\"null\")",      "x", "nulo");
}

static void test_parse_arrays(void) {
    verificar_var("x = json_parsear(\"[1,2,3]\")",   "x", "[1, 2, 3]");
    verificar_var("x = json_parsear(\"[]\")",        "x", "[]");
    verificar_var("x = json_parsear(\"[ 1 , 2 ]\")", "x", "[1, 2]");
    verificar_var("_a = json_parsear(\"[true, false, null]\")\n"
                  "x = _a[0]", "x", "verdadero");
}

static void test_parse_objects(void) {
    verificar_var("_o = json_parsear(\"{\\\"a\\\": 1}\")\n"
                  "x = _o[\"a\"]", "x", "1");
    verificar_var("_o = json_parsear(\"{}\")\n"
                  "x = longitud(_o)", "x", "0");
}

static void test_parse_anidado(void) {
    verificar_var(
        "_o = json_parsear(\"{\\\"capas\\\": {\\\"a\\\": [1, [2, [3]]]}}\")\n"
        "x = _o[\"capas\"][\"a\"][1][1][0]",
        "x", "3");
}

static void test_parse_escapes(void) {
    verificar_var("x = json_parsear(\"\\\"a\\\\nb\\\"\")", "x", "a\nb");
    verificar_var("x = json_parsear(\"\\\"\\\\\\\"\\\"\")", "x", "\"");
    /* \uHHHH a UTF-8 */
    verificar_var("x = json_parsear(\"\\\"\\\\u00e1\\\"\")", "x", "á");
    /* CJK punto medio */
    verificar_var("x = json_parsear(\"\\\"\\\\u4e2d\\\"\")", "x", "中");
}

/* ───── Round trip ───── */

static void test_round_trip(void) {
    /* Verifica que parse(serialize(x)) == x para tipos básicos. */
    verificar_var(
        "_d = {\"a\": 1, \"b\": verdadero, \"c\": [1, 2]}\n"
        "_s = json_serializar(_d)\n"
        "_p = json_parsear(_s)\n"
        "x = _p[\"a\"] * 100 + longitud(_p[\"c\"])",
        "x", "102");
}

static void test_round_trip_nulos(void) {
    /* nulo/verdadero/falso sobreviven el round trip vía JSON. */
    verificar_var(
        "_d = [verdadero, falso, nulo]\n"
        "_p = json_parsear(json_serializar(_d))\n"
        "x = _p[0]", "x", "verdadero");
    verificar_var(
        "_d = [verdadero, falso, nulo]\n"
        "_p = json_parsear(json_serializar(_d))\n"
        "x = _p[1]", "x", "falso");
    verificar_var(
        "_d = [verdadero, falso, nulo]\n"
        "_p = json_parsear(json_serializar(_d))\n"
        "x = _p[2]", "x", "nulo");
}

/* ───── Serializer ───── */

static void test_serializar_basicos(void) {
    verificar_var("x = json_serializar(42)",         "x", "42");
    verificar_var("x = json_serializar(-7)",         "x", "-7");
    verificar_var("x = json_serializar(verdadero)",  "x", "true");
    verificar_var("x = json_serializar(falso)",      "x", "false");
    verificar_var("x = json_serializar(nulo)",       "x", "null");
    verificar_var("x = json_serializar(\"hola\")",   "x", "\"hola\"");
    verificar_var("x = json_serializar([1, 2])",     "x", "[1,2]");
}

static void test_serializar_escapes(void) {
    verificar_var("x = json_serializar(\"a\\nb\")", "x", "\"a\\nb\"");
    verificar_var("x = json_serializar(\"\\\"\")",  "x", "\"\\\"\"");
}

/* ───── Errores ───── */

static void test_parse_errores(void) {
    verificar_error("x = json_parsear(\"{\")",       "JSON invalido");
    verificar_error("x = json_parsear(\"[1, 2,\")",  "JSON invalido");
    verificar_error("x = json_parsear(\"\")",        "JSON invalido");
    verificar_error("x = json_parsear(\"42 garbage\")", "sobrantes");
    verificar_error("x = json_parsear(\"undefined\")", "JSON invalido");
}

static void test_serializar_errores(void) {
    /* Función no es serializable. */
    verificar_error(
        "funcion f():\n"
        "  retornar 1\n"
        "fin funcion\n"
        "x = json_serializar(f)",
        "no es serializable");
    /* Diccionario con clave no-cadena. */
    verificar_error(
        "x = json_serializar({1: 2})",
        "claves cadena");
    /* Rango no es serializable. */
    verificar_error(
        "x = json_serializar(rango(5))",
        "no es serializable");
}

int main(void) {
    test_parse_primitivos();
    test_parse_arrays();
    test_parse_objects();
    test_parse_anidado();
    test_parse_escapes();
    test_round_trip();
    test_round_trip_nulos();
    test_serializar_basicos();
    test_serializar_escapes();
    test_parse_errores();
    test_serializar_errores();

    if (fallos == 0) {
        printf("json: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "json: %d fallo(s)\n", fallos);
    return 1;
}
