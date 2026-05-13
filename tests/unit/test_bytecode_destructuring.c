/*
 * Tests de destructuring assignment (v1.21).
 *
 * Cubre:
 *   - Tupla básica: `a, b = par`.
 *   - Lista LHS: `[a, b, c] = lista`.
 *   - Swap: `a, b = b, a`.
 *   - Anidado: `(a, (b, c)) = (1, (2, 3))`.
 *   - Tres elementos de lista.
 *   - Sobre cadena (iterable indexable por code point).
 *   - Errores atrapables: aridad incorrecta → ErrorDeValor.
 *   - Errores atrapables: tipo no iterable → ErrorDeTipo.
 *   - Mezcla con tipos heterogéneos.
 *   - Destructuring sobre RHS computado.
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

static void test_tupla_par(void) {
    verificar_var("tupla par",
        "par = (1, 2)\n"
        "a, b = par\n"
        "x = a + b * 10",
        "x", "21");
}

static void test_tupla_a_local(void) {
    verificar_var("tupla guardada en globales",
        "p = (10, 20)\n"
        "a, b = p\n"
        "x = a",
        "x", "10");
}

static void test_lista_lhs(void) {
    verificar_var("lista LHS",
        "lista = [100, 200, 300]\n"
        "[x_a, x_b, x_c] = lista\n"
        "x = x_a * 1000 + x_b * 10 + x_c",
        "x", "102300");
}

static void test_tres_elementos(void) {
    verificar_var("tres elementos tupla",
        "t = (1, 2, 3)\n"
        "a, b, c = t\n"
        "x = [a, b, c]",
        "x", "[1, 2, 3]");
}

/* ───── Swap ───── */

static void test_swap(void) {
    verificar_var("swap a,b = b,a",
        "a = 1\n"
        "b = 2\n"
        "a, b = b, a\n"
        "x = [a, b]",
        "x", "[2, 1]");
}

static void test_swap_triple(void) {
    verificar_var("rotar a,b,c = c,a,b",
        "a = 1\n"
        "b = 2\n"
        "c = 3\n"
        "a, b, c = c, a, b\n"
        "x = [a, b, c]",
        "x", "[3, 1, 2]");
}

/* ───── Anidado ───── */

static void test_anidado_basico(void) {
    verificar_var("anidado (a,(b,c))",
        "(a, (b, c)) = (1, (2, 3))\n"
        "x = [a, b, c]",
        "x", "[1, 2, 3]");
}

static void test_anidado_lista_en_tupla(void) {
    verificar_var("anidado tupla+lista",
        "(a, [b, c]) = (1, [2, 3])\n"
        "x = [a, b, c]",
        "x", "[1, 2, 3]");
}

/* ───── Cadenas ───── */

static void test_cadena_ascii(void) {
    verificar_var("cadena ASCII destruct",
        "a, b, c = \"abc\"\n"
        "x = b",
        "x", "b");
}

/* ───── Tipos mezclados ───── */

static void test_heterogeneo(void) {
    verificar_var("tipos heterogéneos",
        "a, b, c = (\"hola\", 42, verdadero)\n"
        "x = b",
        "x", "42");
}

/* ───── Errores atrapables ───── */

static void test_error_aridad_pocos(void) {
    verificar_var("aridad: pocos elementos",
        "intentar:\n"
        "  a, b, c = (1, 2)\n"
        "atrapar ErrorDeValor como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_aridad_muchos(void) {
    verificar_var("aridad: demasiados",
        "intentar:\n"
        "  a, b = (1, 2, 3)\n"
        "atrapar ErrorDeValor como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_tipo_entero(void) {
    verificar_var("tipo entero no destruct",
        "intentar:\n"
        "  a, b = 42\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

/* ───── RHS computado ───── */

static void test_rhs_funcion(void) {
    verificar_var("RHS desde función",
        "funcion par():\n"
        "  retornar (1, 2)\n"
        "fin funcion\n"
        "a, b = par()\n"
        "x = a + b",
        "x", "3");
}

static void test_rhs_indice(void) {
    verificar_var("RHS desde indexación",
        "datos = [(1, 2), (3, 4), (5, 6)]\n"
        "a, b = datos[1]\n"
        "x = a * 10 + b",
        "x", "34");
}

int main(void) {
    test_tupla_par();
    test_tupla_a_local();
    test_lista_lhs();
    test_tres_elementos();
    test_swap();
    test_swap_triple();
    test_anidado_basico();
    test_anidado_lista_en_tupla();
    test_cadena_ascii();
    test_heterogeneo();
    test_error_aridad_pocos();
    test_error_aridad_muchos();
    test_error_tipo_entero();
    test_rhs_funcion();
    test_rhs_indice();

    if (fallos == 0) {
        printf("destructuring: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "destructuring: %d fallo(s)\n", fallos);
    return 1;
}
