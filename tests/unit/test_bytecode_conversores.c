/*
 * Tests de los built-ins de conversión explícita (v1.1).
 *
 * Cubre `cadena()`, `entero()`, `decimal()`, `booleano()` ejecutados
 * a traves del compilador bytecode + VM. Tres frentes:
 *
 *   1. Camino feliz para tipos compatibles (incl. bignum y decimales
 *      en la frontera de int64).
 *   2. Errores de tipo (`entero(lista)`) y de valor (`entero("abc")`).
 *   3. Idempotencia: `cadena(cadena(x)) == cadena(x)`, `entero(entero(x)) == entero(x)`.
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
        fprintf(stderr, "FALLO: %s -> error: %s\n", fuente,
                err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO: %s -> %s=%s (esperaba %s)\n",
                fuente, var, res, esperado);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, "_n", &err);
    if (res) {
        fprintf(stderr, "FALLO: '%s' debia dar error pero ejecuto\n", fuente);
        fallos++;
        return;
    }
    if (!err || !strstr(err, substring)) {
        fprintf(stderr, "FALLO: '%s' dio '%s' (esperaba '%s')\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

/* ───── cadena() ───── */

static void test_cadena_escalares(void) {
    verificar_var("x = cadena(42)",          "x", "42");
    verificar_var("x = cadena(-7)",          "x", "-7");
    verificar_var("x = cadena(3.14)",        "x", "3.14");
    verificar_var("x = cadena(verdadero)",   "x", "verdadero");
    verificar_var("x = cadena(falso)",       "x", "falso");
    verificar_var("x = cadena(nulo)",        "x", "nulo");
    verificar_var("x = cadena(\"hola\")",    "x", "hola");
    verificar_var("x = cadena(\"\")",        "x", "");
}

static void test_cadena_bignum(void) {
    /* 2^100 es un bignum (128 bits) — requiere alocación dinámica para
       el resultado, no cabe en stack buffers fijos. */
    verificar_var("x = cadena(2 ** 100)", "x",
                  "1267650600228229401496703205376");
    /* Negativo grande. */
    verificar_var("x = cadena(0 - (2 ** 100))", "x",
                  "-1267650600228229401496703205376");
}

static void test_cadena_idempotente(void) {
    verificar_var("x = cadena(cadena(42))", "x", "42");
    verificar_var("x = cadena(cadena(\"hola\"))", "x", "hola");
}

/* ───── entero() ───── */

static void test_entero_no_op(void) {
    verificar_var("x = entero(42)",        "x", "42");
    verificar_var("x = entero(-7)",        "x", "-7");
    verificar_var("x = entero(2 ** 100)",  "x", "1267650600228229401496703205376");
}

static void test_entero_desde_decimal(void) {
    verificar_var("x = entero(3.9)",   "x", "3");
    verificar_var("x = entero(-3.9)",  "x", "-3");
    verificar_var("x = entero(0.0)",   "x", "0");
    verificar_var("x = entero(3.14)",  "x", "3");
}

static void test_entero_desde_booleano(void) {
    verificar_var("x = entero(verdadero)", "x", "1");
    verificar_var("x = entero(falso)",     "x", "0");
}

static void test_entero_desde_cadena(void) {
    verificar_var("x = entero(\"42\")",      "x", "42");
    verificar_var("x = entero(\"-7\")",      "x", "-7");
    verificar_var("x = entero(\"+13\")",     "x", "13");
    verificar_var("x = entero(\"  100  \")", "x", "100");
    verificar_var("x = entero(\"1_000\")",   "x", "1000");
}

static void test_entero_errores(void) {
    verificar_error("x = entero(\"abc\")",  "no es entero valido");
    verificar_error("x = entero(\"\")",     "cadena vacia");
    verificar_error("x = entero(\"3.14\")", "no es entero valido");
    verificar_error("x = entero([1, 2])",   "entero() no acepta");
    verificar_error("x = entero(\"  \")",   "cadena vacia");
}

/* ───── decimal() ───── */

static void test_decimal_no_op(void) {
    verificar_var("x = decimal(3.14)", "x", "3.14");
    verificar_var("x = decimal(0.0)",  "x", "0.0");
}

static void test_decimal_desde_entero(void) {
    verificar_var("x = decimal(42)",   "x", "42.0");
    verificar_var("x = decimal(-7)",   "x", "-7.0");
    verificar_var("x = decimal(0)",    "x", "0.0");
}

static void test_decimal_desde_booleano(void) {
    verificar_var("x = decimal(verdadero)", "x", "1.0");
    verificar_var("x = decimal(falso)",     "x", "0.0");
}

static void test_decimal_desde_cadena(void) {
    verificar_var("x = decimal(\"3.14\")",   "x", "3.14");
    verificar_var("x = decimal(\"-2.5\")",   "x", "-2.5");
    verificar_var("x = decimal(\"1e3\")",    "x", "1000.0");
    verificar_var("x = decimal(\"  42  \")", "x", "42.0");
}

static void test_decimal_errores(void) {
    verificar_error("x = decimal(\"abc\")",  "no es decimal valido");
    verificar_error("x = decimal(\"\")",     "cadena vacia");
    verificar_error("x = decimal(\"3.14x\")","no es decimal valido");
    verificar_error("x = decimal([1, 2])",   "decimal() no acepta");
}

/* ───── booleano() ───── */

static void test_booleano_falsy(void) {
    verificar_var("x = booleano(0)",    "x", "falso");
    verificar_var("x = booleano(0.0)",  "x", "falso");
    verificar_var("x = booleano(\"\")", "x", "falso");
    verificar_var("x = booleano(nulo)", "x", "falso");
    verificar_var("x = booleano(falso)","x", "falso");
    verificar_var("x = booleano([])",   "x", "falso");
    verificar_var("x = booleano({})",   "x", "falso");
}

static void test_booleano_truthy(void) {
    verificar_var("x = booleano(1)",        "x", "verdadero");
    verificar_var("x = booleano(-1)",       "x", "verdadero");
    verificar_var("x = booleano(0.5)",      "x", "verdadero");
    verificar_var("x = booleano(\"hola\")", "x", "verdadero");
    verificar_var("x = booleano(verdadero)","x", "verdadero");
    verificar_var("x = booleano([1])",      "x", "verdadero");
    verificar_var("x = booleano({\"k\": 1})","x", "verdadero");
}

/* ───── lista() ───── */

static void test_lista_desde_colecciones(void) {
    verificar_var("x = lista([1, 2, 3])",   "x", "[1, 2, 3]");
    verificar_var("x = lista((9, 8))",      "x", "[9, 8]");
    verificar_var("x = lista(rango(4))",    "x", "[0, 1, 2, 3]");
    verificar_var("x = lista(\"abc\")",     "x", "[\"a\", \"b\", \"c\"]");
    verificar_var("x = lista()",            "x", "[]");
    /* Iterar diccionario produce claves. */
    verificar_var("x = lista({\"a\": 1})",  "x", "[\"a\"]");
    /* longitud de lista(conjunto) — comprobar materialización
       independientemente del orden interno. */
    verificar_var("x = longitud(lista({1, 2, 3}))", "x", "3");
}

static void test_lista_idempotente(void) {
    /* lista(lista(x)) crea NUEVA lista (no alias). */
    verificar_var("a = [1, 2]\nb = lista(a)\nb[0] = 99\nx = a[0]", "x", "1");
}

static void test_lista_errores(void) {
    verificar_error("x = lista(42)",       "lista() no acepta");
    verificar_error("x = lista(verdadero)","lista() no acepta");
    verificar_error("x = lista(1, 2)",     "0 o 1 argumento");
}

/* ───── tupla() ───── */

static void test_tupla_desde_colecciones(void) {
    verificar_var("x = tupla([1, 2, 3])", "x", "(1, 2, 3)");
    verificar_var("x = tupla(rango(3))",  "x", "(0, 1, 2)");
    verificar_var("x = tupla()",          "x", "()");
    verificar_var("x = tupla(\"ab\")",    "x", "(\"a\", \"b\")");
    verificar_var("x = longitud(tupla({1, 2, 3}))", "x", "3");
}

static void test_tupla_errores(void) {
    verificar_error("x = tupla(42)", "tupla() no acepta");
}

/* ───── diccionario() ───── */

static void test_diccionario_desde_pares(void) {
    /* Acepta lista de tuplas y lista de listas. */
    verificar_var("x = diccionario([(\"a\", 1)])", "x", "{\"a\": 1}");
    verificar_var("x = diccionario([[\"k\", 1]])", "x", "{\"k\": 1}");
    verificar_var("x = diccionario()",              "x", "{}");
    /* Verificamos longitud (orden de slots no determinado). */
    verificar_var("x = longitud(diccionario([(\"a\",1),(\"b\",2),(\"c\",3)]))",
                  "x", "3");
}

static void test_diccionario_desde_dicc(void) {
    /* dicc → dicc copia profundamente (no alias). */
    verificar_var("a = {\"k\": 1}\nb = diccionario(a)\nb[\"k\"] = 99\nx = a[\"k\"]",
                  "x", "1");
}

static void test_diccionario_errores(void) {
    verificar_error("x = diccionario(42)",
                    "diccionario() no acepta");
    verificar_error("x = diccionario([(\"a\", 1, 2)])",
                    "longitud 2");
    verificar_error("x = diccionario([42])",
                    "espera pares");
    verificar_error("x = diccionario([([1, 2], 99)])",
                    "no hashable");
}

/* ───── Arity ───── */

static void test_arity_errors(void) {
    verificar_error("x = cadena()",         "requiere 1 argumento");
    verificar_error("x = cadena(1, 2)",     "requiere 1 argumento");
    verificar_error("x = entero()",         "requiere 1 argumento");
    verificar_error("x = decimal(1, 2, 3)", "requiere 1 argumento");
    verificar_error("x = booleano()",       "requiere 1 argumento");
}

/* ───── leer() ───── */

static void test_leer_arity_y_tipo(void) {
    /* La lectura efectiva la cubre el example 26_leer_jugable.cor; aqui
       solo validamos rechazos correctos (que no requieren stdin). */
    verificar_error("x = leer(\"prompt\", \"otro\")", "0 o 1 argumento");
    verificar_error("x = leer(42)",   "cadena como prompt");
    verificar_error("x = leer([1])",  "cadena como prompt");
}

int main(void) {
    test_cadena_escalares();
    test_cadena_bignum();
    test_cadena_idempotente();
    test_entero_no_op();
    test_entero_desde_decimal();
    test_entero_desde_booleano();
    test_entero_desde_cadena();
    test_entero_errores();
    test_decimal_no_op();
    test_decimal_desde_entero();
    test_decimal_desde_booleano();
    test_decimal_desde_cadena();
    test_decimal_errores();
    test_booleano_falsy();
    test_booleano_truthy();
    test_lista_desde_colecciones();
    test_lista_idempotente();
    test_lista_errores();
    test_tupla_desde_colecciones();
    test_tupla_errores();
    test_diccionario_desde_pares();
    test_diccionario_desde_dicc();
    test_diccionario_errores();
    test_leer_arity_y_tipo();
    test_arity_errors();

    if (fallos == 0) {
        printf("conversores: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "conversores: %d fallo(s)\n", fallos);
    return 1;
}
