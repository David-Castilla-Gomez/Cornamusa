/*
 * Tests del bytecode con colecciones — Fase 6 sesión 6.
 *
 * Cubre:
 *   - Literales: lista [a,b,c], tupla (a,b), diccionario {k:v}, conjunto.
 *   - Indexación: obj[key] (lista, tupla, diccionario).
 *   - Asignación a índice: lista[i] = v, dicc[k] = v.
 *   - Built-ins nativas vía OP_LLAMAR: longitud, tipo, rango, agregar.
 *   - Programas mixtos.
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
    static char buffer[4096];

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
        fprintf(stderr, "FALLO en programa:\n%s\n  error: %s\n",
                fuente, err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO en programa:\n%s\n  esperaba %s=%s\n  obtuvo: %s\n",
                fuente, var, esperado, res);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, "_n", &err);
    if (res) {
        fprintf(stderr, "FALLO: programa debería dar error pero ejecutó:\n%s\n",
                fuente);
        fallos++;
        return;
    }
    if (!err || !strstr(err, substring)) {
        fprintf(stderr, "FALLO: '%s' dio '%s' pero se esperaba '%s'\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

/* ───── Literales ───── */

static void test_lista_literal(void) {
    verificar_var("xs = [1, 2, 3]", "xs", "[1, 2, 3]");
    verificar_var("xs = []", "xs", "[]");
    verificar_var("xs = [\"a\", \"b\"]", "xs", "[\"a\", \"b\"]");
    verificar_var("xs = [1, \"hola\", verdadero, nulo]", "xs",
        "[1, \"hola\", verdadero, nulo]");
    /* Anidada. */
    verificar_var("xs = [[1, 2], [3, 4]]", "xs", "[[1, 2], [3, 4]]");
}

static void test_tupla_literal(void) {
    verificar_var("xs = (1, 2, 3)", "xs", "(1, 2, 3)");
    verificar_var("xs = ()", "xs", "()");
    verificar_var("xs = (42,)", "xs", "(42,)");
    verificar_var("xs = (5)", "xs", "5");   /* (x) es grupo */
}

static void test_dicc_literal(void) {
    verificar_var("d = {}", "d", "{}");
    verificar_var("d = {\"a\": 1}", "d", "{\"a\": 1}");
    verificar_var("d = {1: \"uno\"}\nx = d[1]", "x", "uno");
}

static void test_conjunto_literal(void) {
    /* Deduplicación. */
    verificar_var(
        "s = {1, 2, 3, 2, 1}\n"
        "x = longitud(s)",
        "x", "3");
}

/* ───── Indexación ───── */

static void test_indexacion(void) {
    verificar_var("xs = [10, 20, 30]\nx = xs[0]", "x", "10");
    verificar_var("xs = [10, 20, 30]\nx = xs[-1]", "x", "30");
    verificar_var("t = (1, 2, 3)\nx = t[1]", "x", "2");
    verificar_var("d = {\"a\": 1, \"b\": 2}\nx = d[\"a\"]", "x", "1");

    verificar_error("xs = [1, 2]\nx = xs[5]", "fuera de rango");
    verificar_error("d = {}\nx = d[\"k\"]", "ErrorDeClave");
}

/* ───── Asignación a índice ───── */

static void test_asignacion_indice(void) {
    verificar_var(
        "xs = [1, 2, 3]\n"
        "xs[0] = 99",
        "xs", "[99, 2, 3]");

    verificar_var(
        "xs = [1, 2, 3]\n"
        "xs[-1] = 99",
        "xs", "[1, 2, 99]");

    verificar_var(
        "d = {}\n"
        "d[\"x\"] = 42",
        "d", "{\"x\": 42}");

    verificar_var(
        "d = {\"a\": 1}\n"
        "d[\"a\"] = 99\n"
        "x = d[\"a\"]",
        "x", "99");
}

/* ───── Built-ins via OP_LLAMAR ───── */

static void test_nativas_sobre_colecciones(void) {
    verificar_var("x = longitud([1, 2, 3])", "x", "3");
    verificar_var("x = longitud((\"a\", \"b\"))", "x", "2");
    verificar_var("x = longitud({\"a\": 1, \"b\": 2})", "x", "2");
    verificar_var("x = tipo([1, 2])", "x", "lista");
    verificar_var("x = tipo((1, 2))", "x", "tupla");
    verificar_var("x = tipo({1: 2})", "x", "diccionario");
    verificar_var("x = tipo({1, 2})", "x", "conjunto");

    /* agregar() sobre lista. */
    verificar_var(
        "xs = [1, 2, 3]\n"
        "agregar(xs, 4)",
        "xs", "[1, 2, 3, 4]");
}

/* ───── Programas combinados ───── */

static void test_programa_indice_con_funcion(void) {
    /* Función que devuelve elemento de lista por índice. */
    verificar_var(
        "funcion enesimo(lista, n):\n"
        "    retornar lista[n]\n"
        "fin funcion\n"
        "x = enesimo([10, 20, 30, 40], 2)",
        "x", "30");
}

static void test_programa_dicc_acumulador(void) {
    /* Acumular usando dicc[k] += 1 emulado con dicc[k] = (dicc[k] o 0) + 1.
     * El cortocircuito y la indexación deben coordinarse. */
    verificar_var(
        "d = {}\n"
        "d[\"a\"] = 1\n"
        "d[\"a\"] = d[\"a\"] + 10\n"
        "x = d[\"a\"]",
        "x", "11");
}

int main(void) {
    test_lista_literal();
    test_tupla_literal();
    test_dicc_literal();
    test_conjunto_literal();
    test_indexacion();
    test_asignacion_indice();
    test_nativas_sobre_colecciones();
    test_programa_indice_con_funcion();
    test_programa_dicc_acumulador();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode con colecciones pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
