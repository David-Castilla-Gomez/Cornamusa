/*
 * Tests del orden de inserción en diccionarios (v1.20).
 *
 * Verifica que:
 *   - Iterar `para k en d` produce claves en orden de inserción.
 *   - `claves(d)` y `valores(d)` están en ese mismo orden.
 *   - Sobreescribir una clave NO cambia su posición.
 *   - Borrar y re-insertar mueve al final.
 *   - JSON output sigue el orden.
 *   - `cadena(d)` y `imprimir(d)` siguen el orden.
 *   - Literales `{...}` lo respetan también.
 *   - El orden sobrevive a un rehash (más de 8 inserciones).
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

/* ───── Inserción incremental ───── */

static void test_insercion_preserva_orden(void) {
    verificar_var(
        "d = {}\n"
        "d[\"a\"] = 1\n"
        "d[\"b\"] = 2\n"
        "d[\"c\"] = 3\n"
        "x = claves(d)",
        "x", "[\"a\", \"b\", \"c\"]");
}

static void test_insercion_inversa(void) {
    verificar_var(
        "d = {}\n"
        "d[\"z\"] = 1\n"
        "d[\"a\"] = 2\n"
        "d[\"m\"] = 3\n"
        "x = claves(d)",
        "x", "[\"z\", \"a\", \"m\"]");
}

static void test_valores_orden(void) {
    verificar_var(
        "d = {}\n"
        "d[\"a\"] = 100\n"
        "d[\"b\"] = 200\n"
        "d[\"c\"] = 300\n"
        "x = valores(d)",
        "x", "[100, 200, 300]");
}

/* ───── Sobreescribir no cambia orden ───── */

static void test_sobrescribir_mantiene_posicion(void) {
    verificar_var(
        "d = {}\n"
        "d[\"a\"] = 1\n"
        "d[\"b\"] = 2\n"
        "d[\"c\"] = 3\n"
        "d[\"a\"] = 999\n"
        "x = claves(d)",
        "x", "[\"a\", \"b\", \"c\"]");
}

static void test_sobrescribir_mantiene_valor(void) {
    verificar_var(
        "d = {\"a\": 1, \"b\": 2}\n"
        "d[\"a\"] = 999\n"
        "x = d[\"a\"]",
        "x", "999");
}

/* ───── Borrar y re-insertar va al final ───── */

static void test_quitar_y_reinsertar(void) {
    verificar_var(
        "d = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
        "quitar(d, \"a\")\n"
        "d[\"a\"] = 99\n"
        "x = claves(d)",
        "x", "[\"b\", \"c\", \"a\"]");
}

static void test_quitar_intermedio(void) {
    verificar_var(
        "d = {\"a\": 1, \"b\": 2, \"c\": 3, \"d\": 4}\n"
        "quitar(d, \"b\")\n"
        "x = claves(d)",
        "x", "[\"a\", \"c\", \"d\"]");
}

static void test_quitar_primero(void) {
    verificar_var(
        "d = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
        "quitar(d, \"a\")\n"
        "x = claves(d)",
        "x", "[\"b\", \"c\"]");
}

static void test_quitar_ultimo(void) {
    verificar_var(
        "d = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
        "quitar(d, \"c\")\n"
        "x = claves(d)",
        "x", "[\"a\", \"b\"]");
}

/* ───── Iteración para…en ───── */

static void test_para_en_dicc_orden(void) {
    verificar_var(
        "d = {}\n"
        "d[\"z\"] = 1\n"
        "d[\"y\"] = 2\n"
        "d[\"x\"] = 3\n"
        "resultado = []\n"
        "para k en d:\n"
        "  agregar(resultado, k)\n"
        "fin para\n"
        "x = resultado",
        "x", "[\"z\", \"y\", \"x\"]");
}

/* ───── JSON serializar respeta orden ───── */

static void test_json_orden(void) {
    /* Usa built-in directo (los tests unit no tienen acceso a stdlib). */
    verificar_var(
        "d = {}\n"
        "d[\"version\"] = 2\n"
        "d[\"nombre\"] = \"app\"\n"
        "d[\"activo\"] = verdadero\n"
        "x = json_serializar(d)",
        "x", "{\"version\":2,\"nombre\":\"app\",\"activo\":true}");
}

/* ───── cadena() / imprimir() ───── */

static void test_cadena_orden(void) {
    verificar_var(
        "d = {}\n"
        "d[\"a\"] = 1\n"
        "d[\"b\"] = 2\n"
        "x = cadena(d)",
        "x", "{\"a\": 1, \"b\": 2}");
}

/* ───── Literal mantiene orden ───── */

static void test_literal_orden(void) {
    verificar_var(
        "d = {\"x\": 10, \"y\": 20, \"z\": 30, \"w\": 40}\n"
        "x = claves(d)",
        "x", "[\"x\", \"y\", \"z\", \"w\"]");
}

/* ───── Sobrevive rehash ───── */

static void test_rehash_preserva_orden(void) {
    /* DICC_CAPACIDAD_INICIAL = 8. Insertamos 20 claves para forzar
       múltiples rehashes (8 → 16 → 32). El orden debe mantenerse. */
    verificar_var(
        "d = {}\n"
        "para i en rango(20):\n"
        "  d[cadena(i)] = i\n"
        "fin para\n"
        "x = claves(d)",
        "x",
        "[\"0\", \"1\", \"2\", \"3\", \"4\", \"5\", \"6\", \"7\", "
        "\"8\", \"9\", \"10\", \"11\", \"12\", \"13\", \"14\", \"15\", "
        "\"16\", \"17\", \"18\", \"19\"]");
}

/* ───── Copia con diccionario() preserva orden ───── */

static void test_copia_preserva_orden(void) {
    verificar_var(
        "d1 = {\"c\": 3, \"a\": 1, \"b\": 2}\n"
        "d2 = diccionario(d1)\n"
        "x = claves(d2)",
        "x", "[\"c\", \"a\", \"b\"]");
}

int main(void) {
    test_insercion_preserva_orden();
    test_insercion_inversa();
    test_valores_orden();
    test_sobrescribir_mantiene_posicion();
    test_sobrescribir_mantiene_valor();
    test_quitar_y_reinsertar();
    test_quitar_intermedio();
    test_quitar_primero();
    test_quitar_ultimo();
    test_para_en_dicc_orden();
    test_json_orden();
    test_cadena_orden();
    test_literal_orden();
    test_rehash_preserva_orden();
    test_copia_preserva_orden();

    if (fallos == 0) {
        printf("dict_orden: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "dict_orden: %d fallo(s)\n", fallos);
    return 1;
}
