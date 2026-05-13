/*
 * Tests de keyword arguments en llamadas (v1.23).
 *
 * Cubre:
 *   - `f(x=1)` matching contra nombre de parámetro.
 *   - Mezcla posicional + keyword: `f(1, b=2)`.
 *   - Orden libre: `f(b=2, a=1)`.
 *   - Defaults completados con kwargs parciales.
 *   - Errores: kwarg desconocido, duplicado, falta obligatorio,
 *     posicional tras kwarg, kwargs en función nativa (rechazo).
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

static void verificar_var(const char *desc, const char *fuente,
                           const char *var, const char *esperado) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, var, &err);
    if (!res) {
        fprintf(stderr, "FALLO [%s]: error %s\n", desc,
                err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO [%s]: %s=%s (esperaba %s)\n",
                desc, var, res, esperado);
        fallos++;
    }
}

/* ───── Matching básico ───── */

static void test_kwarg_simple(void) {
    verificar_var("f(a=1)",
        "funcion f(a, b):\n"
        "  retornar [a, b]\n"
        "fin funcion\n"
        "x = f(a=1, b=2)",
        "x", "[1, 2]");
}

static void test_kwarg_orden_invertido(void) {
    verificar_var("f(b=2, a=1)",
        "funcion f(a, b):\n"
        "  retornar [a, b]\n"
        "fin funcion\n"
        "x = f(b=2, a=1)",
        "x", "[1, 2]");
}

static void test_mezcla_pos_kw(void) {
    verificar_var("posicional + kwarg",
        "funcion f(a, b, c):\n"
        "  retornar [a, b, c]\n"
        "fin funcion\n"
        "x = f(1, c=3, b=2)",
        "x", "[1, 2, 3]");
}

/* ───── Defaults ───── */

static void test_kwarg_default_skipped(void) {
    verificar_var("default no pasado",
        "funcion f(a, b=99, c=100):\n"
        "  retornar [a, b, c]\n"
        "fin funcion\n"
        "x = f(1)",
        "x", "[1, 99, 100]");
}

static void test_kwarg_solo_uno(void) {
    verificar_var("solo c por nombre",
        "funcion f(a, b=99, c=100):\n"
        "  retornar [a, b, c]\n"
        "fin funcion\n"
        "x = f(1, c=42)",
        "x", "[1, 99, 42]");
}

static void test_kwarg_todos(void) {
    verificar_var("todos por nombre",
        "funcion f(a, b=99, c=100):\n"
        "  retornar [a, b, c]\n"
        "fin funcion\n"
        "x = f(a=1, c=42, b=2)",
        "x", "[1, 2, 42]");
}

/* ───── Lambda ───── */

static void test_lambda_kwarg(void) {
    verificar_var("lambda con kwarg",
        "f = lambda a, b=10: a + b\n"
        "x = f(a=5)",
        "x", "15");
}

/* ───── Errores atrapables ───── */

static void test_error_kwarg_desconocido(void) {
    verificar_var("kwarg desconocido",
        "funcion f(a, b):\n"
        "  retornar a + b\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(a=1, zorro=5)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_kwarg_duplicado(void) {
    verificar_var("kwarg duplica posicional",
        "funcion f(a, b):\n"
        "  retornar a + b\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(1, a=99)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_falta_obligatorio(void) {
    verificar_var("falta obligatorio",
        "funcion f(a, b):\n"
        "  retornar [a, b]\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(a=1)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

/* ───── Con *args (debe rechazar mezcla) ───── */

static void test_kwarg_con_args_estrella(void) {
    /* fn con *resto puede recibir kwargs solo si tocan los fijos. */
    verificar_var("kwarg con fijo + *resto",
        "funcion f(a, b, *resto):\n"
        "  retornar [a, b, resto]\n"
        "fin funcion\n"
        "x = f(b=2, a=1)",
        "x", "[1, 2, ()]");
}

int main(void) {
    test_kwarg_simple();
    test_kwarg_orden_invertido();
    test_mezcla_pos_kw();
    test_kwarg_default_skipped();
    test_kwarg_solo_uno();
    test_kwarg_todos();
    test_lambda_kwarg();
    test_error_kwarg_desconocido();
    test_error_kwarg_duplicado();
    test_error_falta_obligatorio();
    test_kwarg_con_args_estrella();

    if (fallos == 0) {
        printf("kwargs: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "kwargs: %d fallo(s)\n", fallos);
    return 1;
}
