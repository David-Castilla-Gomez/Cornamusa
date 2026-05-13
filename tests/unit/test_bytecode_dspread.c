/*
 * Tests de spread `**dict` en llamadas (v1.25).
 *
 * Cubre:
 *   - `f(**dict)` expande dict como kwargs.
 *   - Mezcla con kwargs explícitos en cualquier orden.
 *   - Mezcla con posicionales (estos van antes).
 *   - `**dict` alimenta `**kwargs` receptor.
 *   - Errores: kwarg desconocido tras spread, clave duplicada,
 *     spread sobre no-dict, clave no-cadena.
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

/* ───── Spread básico ───── */

static void test_spread_solo(void) {
    verificar_var("f(**d)",
        "funcion f(a, b):\n"
        "  retornar [a, b]\n"
        "fin funcion\n"
        "d = {\"a\": 1, \"b\": 2}\n"
        "x = f(**d)",
        "x", "[1, 2]");
}

static void test_spread_y_explicito(void) {
    verificar_var("spread + kwarg explícito",
        "funcion f(a, b, c):\n"
        "  retornar [a, b, c]\n"
        "fin funcion\n"
        "d = {\"a\": 1, \"b\": 2}\n"
        "x = f(**d, c=3)",
        "x", "[1, 2, 3]");
}

static void test_explicito_y_spread(void) {
    verificar_var("kwarg explícito + spread",
        "funcion f(a, b, c):\n"
        "  retornar [a, b, c]\n"
        "fin funcion\n"
        "d = {\"b\": 2, \"c\": 3}\n"
        "x = f(a=1, **d)",
        "x", "[1, 2, 3]");
}

static void test_posicional_y_spread(void) {
    verificar_var("posicional + spread",
        "funcion f(a, b, c):\n"
        "  retornar [a, b, c]\n"
        "fin funcion\n"
        "d = {\"b\": 2, \"c\": 3}\n"
        "x = f(1, **d)",
        "x", "[1, 2, 3]");
}

/* ───── **dict alimenta receptor **kw ───── */

static void test_spread_a_kwkw(void) {
    verificar_var("spread alimenta **kw receptor",
        "funcion f(**kw):\n"
        "  retornar kw\n"
        "fin funcion\n"
        "d = {\"x\": 10, \"y\": 20}\n"
        "x = f(**d)",
        "x", "{\"x\": 10, \"y\": 20}");
}

static void test_spread_mas_extra(void) {
    verificar_var("spread + extra ambos a **kw",
        "funcion f(**kw):\n"
        "  retornar kw\n"
        "fin funcion\n"
        "d = {\"a\": 1}\n"
        "x = f(**d, b=2)",
        "x", "{\"a\": 1, \"b\": 2}");
}

/* ───── Errores ───── */

static void test_error_kwarg_desconocido_via_spread(void) {
    verificar_var("**dict con kw desconocido",
        "funcion f(a, b):\n"
        "  retornar [a, b]\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(**{\"a\": 1, \"zorro\": 99})\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_dup_posicional(void) {
    verificar_var("posicional + spread duplica",
        "funcion f(a, b):\n"
        "  retornar [a, b]\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(1, **{\"a\": 99})\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_no_dict(void) {
    verificar_var("**lista falla",
        "funcion f(**kw):\n"
        "  retornar kw\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(**[1, 2, 3])\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_clave_no_cadena(void) {
    verificar_var("clave entera en spread",
        "funcion f(a):\n"
        "  retornar a\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(**{1: \"uno\"})\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

int main(void) {
    test_spread_solo();
    test_spread_y_explicito();
    test_explicito_y_spread();
    test_posicional_y_spread();
    test_spread_a_kwkw();
    test_spread_mas_extra();
    test_error_kwarg_desconocido_via_spread();
    test_error_dup_posicional();
    test_error_no_dict();
    test_error_clave_no_cadena();

    if (fallos == 0) {
        printf("dspread: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "dspread: %d fallo(s)\n", fallos);
    return 1;
}
