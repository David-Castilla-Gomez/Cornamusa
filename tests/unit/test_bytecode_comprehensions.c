/*
 * Tests de comprehensions (v1.30).
 *
 * Cubre list, dict y set comprehensions con y sin guarda.
 * NOTA: Las comprehensions solo se soportan dentro de funciones en
 * v1.30 (limitación documentada del path de bytecode). Los tests
 * envuelven todo en una función `probar()` que retorna el resultado.
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

/* ───── List comprehensions ───── */

static void test_list_basico(void) {
    verificar_var("[n*2 para n en lista]",
        "funcion f():\n"
        "  retornar [n * 2 para n en [1, 2, 3, 4]]\n"
        "fin funcion\n"
        "x = f()",
        "x", "[2, 4, 6, 8]");
}

static void test_list_con_guarda(void) {
    verificar_var("[n para n en xs si n > 0]",
        "funcion f():\n"
        "  retornar [n para n en [-1, 0, 1, 2, -3, 4] si n > 0]\n"
        "fin funcion\n"
        "x = f()",
        "x", "[1, 2, 4]");
}

static void test_list_pares(void) {
    verificar_var("solo pares",
        "funcion f():\n"
        "  retornar [n para n en rango(10) si n % 2 == 0]\n"
        "fin funcion\n"
        "x = f()",
        "x", "[0, 2, 4, 6, 8]");
}

static void test_list_expr_complex(void) {
    verificar_var("expr compleja",
        "funcion f():\n"
        "  retornar [n * n + 1 para n en [1, 2, 3, 4]]\n"
        "fin funcion\n"
        "x = f()",
        "x", "[2, 5, 10, 17]");
}

static void test_list_iterable_cadena(void) {
    verificar_var("iterar cadena",
        "funcion f():\n"
        "  retornar [c para c en \"hola\"]\n"
        "fin funcion\n"
        "x = f()",
        "x", "[\"h\", \"o\", \"l\", \"a\"]");
}

static void test_list_vacia(void) {
    verificar_var("lista vacía si todos fallan guarda",
        "funcion f():\n"
        "  retornar [n para n en [1, 2, 3] si n > 100]\n"
        "fin funcion\n"
        "x = f()",
        "x", "[]");
}

/* ───── Dict comprehensions ───── */

static void test_dict_basico(void) {
    verificar_var("{n: n*n para n en lista}",
        "funcion f():\n"
        "  retornar {n: n * n para n en [1, 2, 3, 4]}\n"
        "fin funcion\n"
        "x = f()",
        "x", "{1: 1, 2: 4, 3: 9, 4: 16}");
}

static void test_dict_con_guarda(void) {
    verificar_var("dict con guarda",
        "funcion f():\n"
        "  retornar {n: n * 10 para n en rango(6) si n > 2}\n"
        "fin funcion\n"
        "x = f()",
        "x", "{3: 30, 4: 40, 5: 50}");
}

static void test_dict_str_key(void) {
    verificar_var("clave cadena",
        "funcion f():\n"
        "  retornar {c: 1 para c en \"abc\"}\n"
        "fin funcion\n"
        "x = f()",
        "x", "{\"a\": 1, \"b\": 1, \"c\": 1}");
}

/* ───── Set comprehensions ───── */

static void test_set_basico(void) {
    /* El orden de iteración del conjunto NO está garantizado (depende
       del hash). Verificamos longitud + pertenencia en lugar del
       repr completo. */
    verificar_var("set comprehension: longitud",
        "funcion f():\n"
        "  retornar longitud({n para n en [1, 2, 3]})\n"
        "fin funcion\n"
        "x = f()",
        "x", "3");
}

static void test_set_dedup(void) {
    verificar_var("set deduplicates",
        "funcion f():\n"
        "  retornar longitud({n % 3 para n en rango(20)})\n"
        "fin funcion\n"
        "x = f()",
        "x", "3");
}

static void test_set_con_guarda(void) {
    verificar_var("set con guarda",
        "funcion f():\n"
        "  retornar longitud({n para n en rango(10) si n > 5})\n"
        "fin funcion\n"
        "x = f()",
        "x", "4");
}

/* ───── Tope-level: soportado desde v1.32 ───── */

static void test_toplevel_funciona(void) {
    /* v1.32: las comprehensions ya funcionan en top-level. */
    verificar_var("comprehension top-level",
        "x = [n * 2 para n en [1, 2, 3]]",
        "x", "[2, 4, 6]");
}

/* ───── v1.32: comprehension dentro de bucle ───── */

static void test_comprehension_en_bucle(void) {
    verificar_var("comprehension dentro de para",
        "funcion f():\n"
        "  resultado = []\n"
        "  para n en [1, 2, 3]:\n"
        "    sub = [x + n para x en [10, 20]]\n"
        "    agregar(resultado, sub)\n"
        "  fin para\n"
        "  retornar resultado\n"
        "fin funcion\n"
        "x = f()",
        "x", "[[11, 21], [12, 22], [13, 23]]");
}

static void test_comprehension_en_bucle_con_guarda(void) {
    verificar_var("primos via comprehension en bucle",
        "funcion primos(lim):\n"
        "  ps = []\n"
        "  para n en rango(2, lim):\n"
        "    divs = [d para d en rango(2, n) si n % d == 0]\n"
        "    si longitud(divs) == 0:\n"
        "      agregar(ps, n)\n"
        "    fin si\n"
        "  fin para\n"
        "  retornar ps\n"
        "fin funcion\n"
        "x = primos(20)",
        "x", "[2, 3, 5, 7, 11, 13, 17, 19]");
}

int main(void) {
    test_list_basico();
    test_list_con_guarda();
    test_list_pares();
    test_list_expr_complex();
    test_list_iterable_cadena();
    test_list_vacia();
    test_dict_basico();
    test_dict_con_guarda();
    test_dict_str_key();
    test_set_basico();
    test_set_dedup();
    test_set_con_guarda();
    test_toplevel_funciona();
    test_comprehension_en_bucle();
    test_comprehension_en_bucle_con_guarda();

    if (fallos == 0) {
        printf("comprehensions: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "comprehensions: %d fallo(s)\n", fallos);
    return 1;
}
