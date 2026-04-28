/*
 * Tests del pipeline bytecode end-to-end — Fase 6 sesión 2.
 *
 * Verifica que `lex → parse → compilar → vm_ejecutar` produce el
 * mismo resultado que el evaluador tree-walking para expresiones
 * básicas (literales, aritmética, comparaciones, unarios). Es la
 * primera comprobación funcional del nuevo motor.
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

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n",                            \
                    __FILE__, __LINE__, #cond);                                \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

/*
 * Compila y ejecuta `fuente` como una sola expresión, devuelve la
 * representación textual del resultado en un buffer estático.
 * Devuelve NULL si hubo error en cualquier fase.
 */
static const char *ejecutar_expr(const char *fuente, const char **error_out) {
    static char buffer[1024];

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");

    Arena a;
    arena_iniciar(&a, 1024);

    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");

    Expr *e = parser_parsear_expr(&p);
    if (!e || p.tuvo_error) {
        if (error_out) *error_out = "<error de parseo>";
        arena_destruir(&a);
        return NULL;
    }

    Chunk chunk;
    chunk_iniciar(&chunk);

    Compilador c;
    compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_expr_top(&c, e)) {
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", c.error.mensaje);
            *error_out = errbuf;
        }
        chunk_destruir(&chunk);
        arena_destruir(&a);
        return NULL;
    }

    VM vm;
    vm_iniciar(&vm);
    Valor resultado = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &resultado);
    if (rc != VM_OK) {
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", vm.error.mensaje);
            *error_out = errbuf;
        }
        valor_destruir(&resultado);
        vm_destruir(&vm);
        chunk_destruir(&chunk);
        arena_destruir(&a);
        return NULL;
    }

    valor_a_cadena(&resultado, buffer, sizeof(buffer));
    valor_destruir(&resultado);
    vm_destruir(&vm);
    chunk_destruir(&chunk);
    arena_destruir(&a);
    if (error_out) *error_out = NULL;
    return buffer;
}

static void verificar(const char *fuente, const char *esperado) {
    const char *err = NULL;
    const char *res = ejecutar_expr(fuente, &err);
    if (!res) {
        fprintf(stderr, "FALLO: '%s' produjo error: %s\n",
                fuente, err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO: '%s'\n  esperaba: %s\n  obtuvo:   %s\n",
                fuente, esperado, res);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar_expr(fuente, &err);
    if (res) {
        fprintf(stderr, "FALLO: '%s' debería dar error pero dio '%s'\n",
                fuente, res);
        fallos++;
        return;
    }
    if (!err || !strstr(err, substring)) {
        fprintf(stderr, "FALLO: '%s' dio '%s' pero se esperaba contener '%s'\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

/* ───── Literales ───── */

static void test_literales(void) {
    verificar("42", "42");
    verificar("0", "0");
    verificar("0xff", "255");
    verificar("3.14", "3.14");
    verificar("verdadero", "verdadero");
    verificar("falso", "falso");
    verificar("nulo", "nulo");
    verificar("\"hola\"", "hola");
    verificar("\"con\\nsalto\"", "con\nsalto");
}

/* ───── Aritmética entera (bignum) ───── */

static void test_aritmetica(void) {
    verificar("1 + 2", "3");
    verificar("10 - 3", "7");
    verificar("4 * 5", "20");
    verificar("7 // 2", "3");
    verificar("7 % 3", "1");
    verificar("2 ** 10", "1024");

    /* Precedencia. */
    verificar("1 + 2 * 3", "7");
    verificar("(1 + 2) * 3", "9");
    verificar("2 ** 3 ** 2", "512");

    /* Negativos: floor division Python. */
    verificar("-7 // 2", "-4");
    verificar("-7 % 2", "1");

    /* Bignum: 2^100 da 31 dígitos. */
    verificar("2 ** 100", "1267650600228229401496703205376");
}

/* ───── Aritmética decimal y mixta ───── */

static void test_decimal_y_mixto(void) {
    verificar("7 / 2", "3.5");
    verificar("6 / 2", "3.0");
    verificar("1 + 2.5", "3.5");
    verificar("0.5 ** 2", "0.25");
    verificar("1.5 + 2.5", "4.0");
}

/* ───── Comparaciones ───── */

static void test_comparaciones(void) {
    verificar("1 == 1", "verdadero");
    verificar("1 == 2", "falso");
    verificar("1 != 2", "verdadero");
    verificar("3 < 5", "verdadero");
    verificar("3 <= 3", "verdadero");
    verificar("5 > 3", "verdadero");
    verificar("3 >= 4", "falso");
    verificar("1 == 1.0", "verdadero");
    verificar("\"abc\" < \"abd\"", "verdadero");
    verificar_error("1 < \"hola\"", "no se puede comparar");
}

/* ───── Unarios ───── */

static void test_unarios(void) {
    verificar("-5", "-5");
    verificar("- -5", "5");
    verificar("+5", "5");
    verificar("-3.14", "-3.14");
    verificar("no verdadero", "falso");
    verificar("no falso", "verdadero");
    verificar("no 0", "verdadero");
    verificar("no \"\"", "verdadero");
    verificar("no \"x\"", "falso");
    verificar("no no verdadero", "verdadero");
}

/* ───── Combinaciones realistas ───── */

static void test_realistas(void) {
    verificar("3 ** 2 + 4 ** 2", "25");
    verificar("(10 + 20 + 30) / 3", "20.0");
    /* Cornamusa NO tiene chained comparisons (Python sí). Aquí se
       evalúa izquierda a derecha: (0 < 5) == verdadero → verdadero. */
    verificar("0 < 5 == verdadero", "verdadero");
}

/* ───── Errores en runtime ───── */

static void test_errores_runtime(void) {
    verificar_error("1 // 0", "division por cero");
    verificar_error("1 % 0", "modulo por cero");
    verificar_error("1 / 0", "division por cero");
    verificar_error("\"a\" + 1", "no aplica");
}

/* ───── Errores de compilación / runtime ───── */

static void test_errores_compilacion(void) {
    /* `x` ya se compila a OP_OBTENER_GLOBAL en S3; el error es ahora
       de runtime cuando la variable no está definida. */
    verificar_error("x", "no esta definido");
    verificar_error("x + 1", "no esta definido");
    /* Lambdas y llamadas a funciones de usuario siguen sin compilar. */
    verificar_error("lambda x: x", "no esta implementada");
    verificar_error("[1, 2]", "no esta implementada");
}

int main(void) {
    test_literales();
    test_aritmetica();
    test_decimal_y_mixto();
    test_comparaciones();
    test_unarios();
    test_realistas();
    test_errores_runtime();
    test_errores_compilacion();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode (compilador + VM) pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
