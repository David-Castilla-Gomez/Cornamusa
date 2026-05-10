/*
 * Tests de built-ins de v1.11: numéricos (absoluto, redondear) y
 * reflexión (instancia_de, subclase_de, id, repr).
 *
 * Las funciones de orden superior (mapear, filtrar, reducir, enumerar,
 * suma, minimo, maximo) viven en `stdlib/funcionales.cor` y se cubren
 * con el test diferencial sobre `examples/34_funcionales.cor`. Aquí
 * sólo verificamos los built-ins C — los tests unit corren sin acceso
 * a stdlib (cwd = build/tests).
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

static void verificar_error(const char *fuente, const char *fragmento_esperado) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, "x", &err);
    if (res) {
        fprintf(stderr, "FALLO (esperaba error): %s\n  -> exito\n", fuente);
        fallos++;
        return;
    }
    if (!err || !strstr(err, fragmento_esperado)) {
        fprintf(stderr, "FALLO: %s\n  -> error '%s' no contiene '%s'\n",
                fuente, err ? err : "<null>", fragmento_esperado);
        fallos++;
    }
}

/* ───── absoluto ───── */

static void test_absoluto_entero_positivo(void) {
    verificar_var("x = absoluto(5)", "x", "5");
}

static void test_absoluto_entero_negativo(void) {
    verificar_var("x = absoluto(-5)", "x", "5");
}

static void test_absoluto_cero(void) {
    verificar_var("x = absoluto(0)", "x", "0");
}

static void test_absoluto_decimal(void) {
    verificar_var("x = absoluto(-3.14)", "x", "3.14");
}

static void test_absoluto_booleano(void) {
    /* `verdadero` se trata como 1, `falso` como 0. Lo mismo que entero(). */
    verificar_var("x = absoluto(verdadero)", "x", "1");
    verificar_var("x = absoluto(falso)", "x", "0");
}

static void test_absoluto_bignum(void) {
    /* 2^100 = 1267650600228229401496703205376. Negar y absoluto. */
    verificar_var(
        "n = -(2 ** 100)\n"
        "x = absoluto(n)",
        "x", "1267650600228229401496703205376");
}

static void test_absoluto_tipo_invalido(void) {
    verificar_error("x = absoluto(\"hola\")",
                    "absoluto() no acepta 'cadena'");
}

static void test_absoluto_aridad(void) {
    verificar_error("x = absoluto()",
                    "absoluto() requiere 1 argumento");
    verificar_error("x = absoluto(1, 2)",
                    "absoluto() requiere 1 argumento");
}

/* ───── redondear ───── */

static void test_redondear_decimal_arriba(void) {
    verificar_var("x = redondear(3.7)", "x", "4");
}

static void test_redondear_decimal_abajo(void) {
    verificar_var("x = redondear(3.2)", "x", "3");
}

static void test_redondear_half_away_zero(void) {
    /* 2.5 → 3 (no 2, como bankers'). -2.5 → -3. */
    verificar_var("x = redondear(2.5)", "x", "3");
    verificar_var("x = redondear(-2.5)", "x", "-3");
}

static void test_redondear_negativo(void) {
    verificar_var("x = redondear(-3.7)", "x", "-4");
    verificar_var("x = redondear(-3.2)", "x", "-3");
}

static void test_redondear_entero(void) {
    /* redondear(entero) sin decimales = no-op. */
    verificar_var("x = redondear(5)", "x", "5");
    verificar_var("x = redondear(-7)", "x", "-7");
}

static void test_redondear_con_decimales(void) {
    verificar_var("x = redondear(3.14159, 2)", "x", "3.14");
    verificar_var("x = redondear(3.14159, 4)", "x", "3.1416");
    /* k=0 con decimal → decimal con valor entero. */
    verificar_var("x = redondear(3.7, 0)", "x", "4.0");
}

static void test_redondear_aridad(void) {
    verificar_error("x = redondear()",
                    "redondear() acepta 1 o 2 argumentos");
    verificar_error("x = redondear(1, 2, 3)",
                    "redondear() acepta 1 o 2 argumentos");
}

static void test_redondear_decimales_invalido(void) {
    verificar_error("x = redondear(3.14, -1)",
                    "redondear() requiere entero >= 0");
    verificar_error("x = redondear(3.14, 1.5)",
                    "redondear() requiere entero >= 0");
}

/* ───── instancia_de ───── */

static void test_instancia_de_basico(void) {
    verificar_var(
        "clase A:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "a = A()\n"
        "x = instancia_de(a, A)",
        "x", "verdadero");
}

static void test_instancia_de_herencia(void) {
    verificar_var(
        "clase A:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "clase B extiende A:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 2\n"
        "  fin funcion\n"
        "fin clase\n"
        "b = B()\n"
        "x = instancia_de(b, A)",
        "x", "verdadero");
}

static void test_instancia_de_clase_distinta(void) {
    verificar_var(
        "clase A:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "clase B:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 2\n"
        "  fin funcion\n"
        "fin clase\n"
        "a = A()\n"
        "x = instancia_de(a, B)",
        "x", "falso");
}

static void test_instancia_de_primitivo(void) {
    /* Tipos primitivos no son VAL_INSTANCIA → siempre falso. */
    verificar_var(
        "clase A:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "x = instancia_de(5, A)",
        "x", "falso");
}

static void test_instancia_de_segundo_no_clase(void) {
    verificar_error(
        "x = instancia_de(5, 10)",
        "instancia_de() requiere una clase");
}

/* ───── subclase_de ───── */

static void test_subclase_de_directa(void) {
    verificar_var(
        "clase A:\n"
        "  funcion m(yo):\n"
        "    retornar 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "clase B extiende A:\n"
        "  funcion n(yo):\n"
        "    retornar 2\n"
        "  fin funcion\n"
        "fin clase\n"
        "x = subclase_de(B, A)",
        "x", "verdadero");
}

static void test_subclase_de_reflexivo(void) {
    /* A es subclase de A (igual que Python). */
    verificar_var(
        "clase A:\n"
        "  funcion m(yo):\n"
        "    retornar 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "x = subclase_de(A, A)",
        "x", "verdadero");
}

static void test_subclase_de_inversa(void) {
    verificar_var(
        "clase A:\n"
        "  funcion m(yo):\n"
        "    retornar 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "clase B extiende A:\n"
        "  funcion n(yo):\n"
        "    retornar 2\n"
        "  fin funcion\n"
        "fin clase\n"
        "x = subclase_de(A, B)",
        "x", "falso");
}

static void test_subclase_de_no_clase(void) {
    verificar_error(
        "x = subclase_de(5, 10)",
        "subclase_de() requiere una clase como primer argumento");
}

/* ───── id ───── */

static void test_id_misma_referencia(void) {
    /* Misma instancia → mismo id. */
    verificar_var(
        "clase A:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "a = A()\n"
        "b = a\n"
        "x = id(a) == id(b)",
        "x", "verdadero");
}

static void test_id_distintas_instancias(void) {
    /* Dos `A()` distintas → ids distintos. */
    verificar_var(
        "clase A:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "a = A()\n"
        "b = A()\n"
        "x = id(a) != id(b)",
        "x", "verdadero");
}

static void test_id_es_entero(void) {
    /* tipo(id(...)) debe ser entero. */
    verificar_var(
        "x = tipo(id(\"hola\"))",
        "x", "entero");
}

static void test_id_aridad(void) {
    verificar_error("x = id()", "id() requiere 1 argumento");
    verificar_error("x = id(1, 2)", "id() requiere 1 argumento");
}

/* ───── repr ───── */

static void test_repr_cadena(void) {
    /* repr() añade comillas a las cadenas — eso la distingue de cadena(). */
    verificar_var("x = repr(\"hola\")", "x", "\"hola\"");
}

static void test_repr_entero(void) {
    verificar_var("x = repr(42)", "x", "42");
}

static void test_repr_decimal(void) {
    verificar_var("x = repr(3.14)", "x", "3.14");
}

static void test_repr_lista(void) {
    /* Listas con cadenas: repr usa comillas en los elementos cadena. */
    verificar_var("x = repr([\"a\", \"b\"])", "x", "[\"a\", \"b\"]");
}

static void test_repr_aridad(void) {
    verificar_error("x = repr()", "repr() requiere 1 argumento");
}

int main(void) {
    test_absoluto_entero_positivo();
    test_absoluto_entero_negativo();
    test_absoluto_cero();
    test_absoluto_decimal();
    test_absoluto_booleano();
    test_absoluto_bignum();
    test_absoluto_tipo_invalido();
    test_absoluto_aridad();

    test_redondear_decimal_arriba();
    test_redondear_decimal_abajo();
    test_redondear_half_away_zero();
    test_redondear_negativo();
    test_redondear_entero();
    test_redondear_con_decimales();
    test_redondear_aridad();
    test_redondear_decimales_invalido();

    test_instancia_de_basico();
    test_instancia_de_herencia();
    test_instancia_de_clase_distinta();
    test_instancia_de_primitivo();
    test_instancia_de_segundo_no_clase();

    test_subclase_de_directa();
    test_subclase_de_reflexivo();
    test_subclase_de_inversa();
    test_subclase_de_no_clase();

    test_id_misma_referencia();
    test_id_distintas_instancias();
    test_id_es_entero();
    test_id_aridad();

    test_repr_cadena();
    test_repr_entero();
    test_repr_decimal();
    test_repr_lista();
    test_repr_aridad();

    if (fallos == 0) {
        printf("funcionales: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "funcionales: %d fallo(s)\n", fallos);
    return 1;
}
