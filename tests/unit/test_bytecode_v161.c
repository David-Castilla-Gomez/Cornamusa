/*
 * Tests de patches menores v1.16.1:
 *   - JSON pretty-print con `json_serializar(obj, indent)`.
 *   - `quitar` extendido a dict y conjunto.
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

/* ───── JSON pretty-print ───── */

static void test_json_compacto_sin_cambio(void) {
    /* Sin segundo arg, el comportamiento debe ser idéntico a v1.9. */
    verificar_var(
        "x = json_serializar([1, 2, 3])",
        "x", "[1,2,3]");
}

static void test_json_indent_lista(void) {
    verificar_var(
        "x = json_serializar([1, 2, 3], 2)",
        "x", "[\n  1,\n  2,\n  3\n]");
}

static void test_json_indent_lista_vacia(void) {
    /* Lista vacía: sin newlines, mantiene `[]`. */
    verificar_var(
        "x = json_serializar([], 2)",
        "x", "[]");
}

static void test_json_indent_indent4(void) {
    verificar_var(
        "x = json_serializar([1], 4)",
        "x", "[\n    1\n]");
}

static void test_json_indent_negativo_error(void) {
    const char *err = NULL;
    const char *res = ejecutar(
        "intentar:\n"
        "  x = json_serializar([], -1)\n"
        "atrapar Excepcion como e:\n"
        "  x = \"capturado\"\n"
        "fin intentar",
        "x", &err);
    if (!res || strcmp(res, "capturado") != 0) {
        fprintf(stderr, "FALLO: indent negativo no atrapado: res=%s\n",
                res ? res : "<null>");
        fallos++;
    }
}

/* ───── quitar(dicc, k) ───── */

static void test_quitar_dicc(void) {
    verificar_var(
        "d = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
        "v = quitar(d, \"b\")\n"
        "x = [v, longitud(d)]",
        "x", "[2, 2]");
}

static void test_quitar_dicc_clave_ausente(void) {
    verificar_var(
        "d = {\"a\": 1}\n"
        "_msg = \"\"\n"
        "intentar:\n"
        "  quitar(d, \"z\")\n"
        "atrapar ErrorDeClave:\n"
        "  _msg = \"capturado\"\n"
        "fin intentar\n"
        "x = _msg",
        "x", "capturado");
}

/* ───── quitar(conjunto, x) ───── */

static void test_quitar_conjunto(void) {
    verificar_var(
        "s = conjunto([1, 2, 3])\n"
        "quitar(s, 2)\n"
        "x = longitud(s)",
        "x", "2");
}

static void test_quitar_conjunto_ausente(void) {
    verificar_var(
        "s = conjunto([1, 2])\n"
        "_msg = \"\"\n"
        "intentar:\n"
        "  quitar(s, 99)\n"
        "atrapar ErrorDeClave:\n"
        "  _msg = \"capturado\"\n"
        "fin intentar\n"
        "x = _msg",
        "x", "capturado");
}

/* ───── Sin regresión: quitar(lista, idx) ───── */

static void test_quitar_lista_sigue_funcionando(void) {
    verificar_var(
        "l = [10, 20, 30]\n"
        "v = quitar(l, 1)\n"
        "x = [v, l]",
        "x", "[20, [10, 30]]");
}

int main(void) {
    test_json_compacto_sin_cambio();
    test_json_indent_lista();
    test_json_indent_lista_vacia();
    test_json_indent_indent4();
    test_json_indent_negativo_error();

    test_quitar_dicc();
    test_quitar_dicc_clave_ausente();
    test_quitar_conjunto();
    test_quitar_conjunto_ausente();
    test_quitar_lista_sigue_funcionando();

    if (fallos == 0) {
        printf("v161: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "v161: %d fallo(s)\n", fallos);
    return 1;
}
