/*
 * Tests de `*args` en definiciones y llamadas (v1.22).
 *
 * Cubre:
 *   - Definición: `funcion f(*xs)` recoge args en tupla.
 *   - Fijos + estrella: `funcion f(a, *resto)`.
 *   - Spread en llamada: `f(*lista)`, `f(*tupla)`.
 *   - Mezcla posicionales + spread: `f(1, *xs, 99)`.
 *   - Forwarding genérico: `g(*args) → f(*args)`.
 *   - Lambda variádica.
 *   - Errores atrapables: aridad insuficiente, spread sobre no-iterable.
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

/* ───── Definición con *args ───── */

static void test_args_vacio(void) {
    verificar_var("f() con *args vacío",
        "funcion f(*xs):\n"
        "  retornar longitud(xs)\n"
        "fin funcion\n"
        "x = f()",
        "x", "0");
}

static void test_args_uno(void) {
    verificar_var("f(1) un solo arg",
        "funcion f(*xs):\n"
        "  retornar xs\n"
        "fin funcion\n"
        "x = f(7)",
        "x", "(7,)");
}

static void test_args_varios(void) {
    verificar_var("f(1,2,3) varios",
        "funcion f(*xs):\n"
        "  retornar xs\n"
        "fin funcion\n"
        "x = f(1, 2, 3)",
        "x", "(1, 2, 3)");
}

static void test_args_es_tupla(void) {
    verificar_var("xs es tupla",
        "funcion f(*xs):\n"
        "  retornar xs\n"
        "fin funcion\n"
        "x = f(1, 2)",
        "x", "(1, 2)");
}

/* ───── Fijos + estrella ───── */

static void test_fijo_mas_estrella(void) {
    verificar_var("fijo + estrella",
        "funcion f(a, *resto):\n"
        "  retornar [a, resto]\n"
        "fin funcion\n"
        "x = f(1, 2, 3, 4)",
        "x", "[1, (2, 3, 4)]");
}

static void test_fijo_mas_estrella_solo_fijo(void) {
    verificar_var("solo fijo, resto vacío",
        "funcion f(a, *resto):\n"
        "  retornar [a, resto]\n"
        "fin funcion\n"
        "x = f(42)",
        "x", "[42, ()]");
}

/* ───── Spread en llamadas ───── */

static void test_spread_lista(void) {
    verificar_var("spread de lista",
        "funcion suma(*xs):\n"
        "  total = 0\n"
        "  para n en xs:\n"
        "    total = total + n\n"
        "  fin para\n"
        "  retornar total\n"
        "fin funcion\n"
        "nums = [10, 20, 30]\n"
        "x = suma(*nums)",
        "x", "60");
}

static void test_spread_tupla(void) {
    verificar_var("spread de tupla",
        "funcion suma(*xs):\n"
        "  total = 0\n"
        "  para n en xs:\n"
        "    total = total + n\n"
        "  fin para\n"
        "  retornar total\n"
        "fin funcion\n"
        "t = (1, 2, 3)\n"
        "x = suma(*t)",
        "x", "6");
}

static void test_spread_mezclado(void) {
    verificar_var("posicional + spread + posicional",
        "funcion suma(*xs):\n"
        "  total = 0\n"
        "  para n en xs:\n"
        "    total = total + n\n"
        "  fin para\n"
        "  retornar total\n"
        "fin funcion\n"
        "medio = [10, 20]\n"
        "x = suma(1, *medio, 100)",
        "x", "131");
}

/* ───── Forwarding genérico ───── */

static void test_forwarding(void) {
    verificar_var("forwarding genérico",
        "funcion area(w, h):\n"
        "  retornar w * h\n"
        "fin funcion\n"
        "funcion forward(f, *args):\n"
        "  retornar f(*args)\n"
        "fin funcion\n"
        "x = forward(area, 5, 8)",
        "x", "40");
}

/* ───── Lambda con *args ───── */

static void test_lambda_estrella(void) {
    verificar_var("lambda *xs",
        "f = lambda *xs: longitud(xs)\n"
        "x = f(1, 2, 3, 4, 5)",
        "x", "5");
}

/* ───── Errores atrapables ───── */

static void test_error_pocos_args(void) {
    verificar_var("aridad insuficiente",
        "funcion f(a, b, *xs):\n"
        "  retornar a + b\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(1)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_spread_no_iterable(void) {
    verificar_var("spread de entero",
        "funcion f(*xs):\n"
        "  retornar longitud(xs)\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(*42)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

int main(void) {
    test_args_vacio();
    test_args_uno();
    test_args_varios();
    test_args_es_tupla();
    test_fijo_mas_estrella();
    test_fijo_mas_estrella_solo_fijo();
    test_spread_lista();
    test_spread_tupla();
    test_spread_mezclado();
    test_forwarding();
    test_lambda_estrella();
    test_error_pocos_args();
    test_error_spread_no_iterable();

    if (fallos == 0) {
        printf("varargs: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "varargs: %d fallo(s)\n", fallos);
    return 1;
}
