/*
 * Tests de `**kwargs` en definiciones (v1.24).
 *
 * Cubre:
 *   - `funcion f(**kw)` recoge keywords sobrantes en dict.
 *   - Combinable con fijos + posicionales.
 *   - Combinable con `*args` para parámetros mixed.
 *   - Sin keywords pasados → dict vacío.
 *   - Funcion sin **kw rechaza keyword desconocido (regresión).
 *   - Ordering: validar slot final correcto.
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

/* ───── Casos básicos ───── */

static void test_kwargs_solo(void) {
    verificar_var("f(**kw) sin args",
        "funcion f(**kw):\n"
        "  retornar kw\n"
        "fin funcion\n"
        "x = f()",
        "x", "{}");
}

static void test_kwargs_recoger(void) {
    verificar_var("f(**kw) con keywords",
        "funcion f(**kw):\n"
        "  retornar kw\n"
        "fin funcion\n"
        "x = f(a=1, b=2)",
        "x", "{\"a\": 1, \"b\": 2}");
}

static void test_fijos_mas_kwargs(void) {
    verificar_var("f(host, **opts)",
        "funcion f(host, **opts):\n"
        "  retornar [host, opts]\n"
        "fin funcion\n"
        "x = f(\"api.dev\", puerto=443)",
        "x", "[\"api.dev\", {\"puerto\": 443}]");
}

static void test_fijo_consume_kw(void) {
    verificar_var("fijo pasado como kw, resto al dict",
        "funcion f(host, **opts):\n"
        "  retornar [host, opts]\n"
        "fin funcion\n"
        "x = f(host=\"api\", puerto=443, tls=verdadero)",
        "x", "[\"api\", {\"puerto\": 443, \"tls\": verdadero}]");
}

/* ───── Combinaciones con *args ───── */

static void test_args_y_kwargs(void) {
    verificar_var("f(a, b, *args, **kw)",
        "funcion f(a, b, *args, **kw):\n"
        "  retornar [a, b, args, kw]\n"
        "fin funcion\n"
        "x = f(1, 2, 3, 4, key=\"v\")",
        "x", "[1, 2, (3, 4), {\"key\": \"v\"}]");
}

static void test_solo_args_no_kwargs(void) {
    verificar_var("sin kwargs pero **kw acepta",
        "funcion f(a, *args, **kw):\n"
        "  retornar [a, args, kw]\n"
        "fin funcion\n"
        "x = f(1, 2, 3)",
        "x", "[1, (2, 3), {}]");
}

static void test_solo_kwargs_no_args(void) {
    verificar_var("solo kwargs, args vacía",
        "funcion f(a, *args, **kw):\n"
        "  retornar [a, args, kw]\n"
        "fin funcion\n"
        "x = f(1, k=10)",
        "x", "[1, (), {\"k\": 10}]");
}

/* ───── Regresión: sin **kw, keyword desconocido = error ───── */

static void test_sin_kwkw_rechaza(void) {
    verificar_var("sin **kw rechaza kwarg desconocido",
        "funcion f(a):\n"
        "  retornar a\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(a=1, zorro=5)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

/* ───── Lambda con **kw ───── */

static void test_lambda_kwkw(void) {
    verificar_var("lambda **kw",
        "f = lambda **kw: longitud(kw)\n"
        "x = f(a=1, b=2, c=3)",
        "x", "3");
}

int main(void) {
    test_kwargs_solo();
    test_kwargs_recoger();
    test_fijos_mas_kwargs();
    test_fijo_consume_kw();
    test_args_y_kwargs();
    test_solo_args_no_kwargs();
    test_solo_kwargs_no_args();
    test_sin_kwkw_rechaza();
    test_lambda_kwkw();

    if (fallos == 0) {
        printf("kwkw: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "kwkw: %d fallo(s)\n", fallos);
    return 1;
}
