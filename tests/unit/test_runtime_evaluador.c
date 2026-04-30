/*
 * Tests del runtime — Fase 4 Sesión 2: evaluador de expresiones.
 *
 * Cobertura:
 *   - Literales: entero (decimal/hex/oct/bin/grandes), decimal, cadena
 *     con escapes, booleano, nulo.
 *   - Identificadores: lookup en entorno, scope chain, error si no existe.
 *   - Aritmética entero⊕entero: + - * / // % ** con bignum (factorial,
 *     potencias grandes), incluyendo división por cero.
 *   - Aritmética con decimales y promociones mixtas (entero+decimal).
 *   - True division `/` siempre produce decimal.
 *   - Floor division `//` con semántica Python (negativos redondean a -∞).
 *   - Módulo con semántica Python (-7 % 3 == 2).
 *   - Comparaciones: ==, !=, <, <=, >, >=, incluyendo cross-tipo.
 *   - Bitwise: & | ^ << >> ~.
 *   - Lógicos: y, o con cortocircuito (short-circuit verifiable porque
 *     un sub-expresión "ruidosa" no se evalúa si el primer operando ya
 *     decide).
 *   - Unarios: -x, +x, no x, ~x.
 *   - Cadenas: + (concatenación), * (repetición), comparaciones.
 *   - `es` (identidad estructural en este nivel) y `en` (substring).
 *   - Errores: tipo, división por cero, nombre no definido.
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "entorno.h"
#include "evaluador.h"
#include "lexer.h"
#include "parser.h"
#include "valor.h"

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
 * Evalúa `fuente` como una sola expresión en un entorno fresco, copia
 * la representación textual del resultado a un buffer estático y
 * devuelve un puntero a éste. Devuelve NULL si hubo error léxico,
 * sintáctico o de runtime.
 */
static const char *evaluar(const char *fuente, const char **error_out) {
    static char buffer[1024];

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");

    Arena a;
    arena_iniciar(&a, 1024);

    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");

    Expr *e = parser_parsear_expr(&p);
    if (e == NULL || p.tuvo_error) {
        if (error_out) *error_out = "<error de parseo>";
        arena_destruir(&a);
        return NULL;
    }

    Entorno globales;
    entorno_iniciar(&globales, NULL);

    Evaluador ev;
    evaluador_iniciar(&ev, &globales);

    Valor v = evaluador_evaluar_expr(&ev, e);

    if (ev.error.tuvo_error) {
        if (error_out) {
            static char errbuf[1024];
            snprintf(errbuf, sizeof(errbuf), "%s", ev.error.mensaje);
            *error_out = errbuf;
        }
        valor_destruir(&v);
        entorno_destruir(&globales);
        arena_destruir(&a);
        return NULL;
    }

    valor_a_cadena(&v, buffer, sizeof(buffer));
    valor_destruir(&v);
    entorno_destruir(&globales);
    arena_destruir(&a);
    if (error_out) *error_out = NULL;
    return buffer;
}

static void verificar(const char *fuente, const char *esperado) {
    const char *err = NULL;
    const char *resultado = evaluar(fuente, &err);
    if (resultado == NULL) {
        fprintf(stderr, "FALLO: '%s' produjo error: %s\n",
                fuente, err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(resultado, esperado) != 0) {
        fprintf(stderr, "FALLO: '%s'\n  esperaba: %s\n  obtuvo:   %s\n",
                fuente, esperado, resultado);
        fallos++;
    }
}

/* Atajo para tests que esperan un error en runtime (no en parseo). */
static void verificar_error(const char *fuente, const char *substring_esperado) {
    const char *err = NULL;
    const char *resultado = evaluar(fuente, &err);
    if (resultado != NULL) {
        fprintf(stderr, "FALLO: '%s' debería dar error pero dio '%s'\n",
                fuente, resultado);
        fallos++;
        return;
    }
    if (err == NULL || strstr(err, substring_esperado) == NULL) {
        fprintf(stderr, "FALLO: '%s' dio error '%s' pero se esperaba contener '%s'\n",
                fuente, err ? err : "<null>", substring_esperado);
        fallos++;
    }
}

/* ───── Literales ───── */

static void test_literales(void) {
    verificar("42", "42");
    verificar("0", "0");
    verificar("0xff", "255");
    verificar("0b1010", "10");
    verificar("0o755", "493");
    verificar("1_000_000", "1000000");

    verificar("3.14", "3.14");
    verificar("0.5", "0.5");
    verificar("1.5e2", "150.0");
    verificar("2.5e-1", "0.25");

    verificar("verdadero", "verdadero");
    verificar("falso", "falso");
    verificar("nulo", "nulo");

    verificar("\"hola\"", "hola");
    verificar("'mundo'", "mundo");
    verificar("\"con\\nsalto\"", "con\nsalto");
    verificar("\"comilla \\\" interna\"", "comilla \" interna");
}

/* ───── Aritmética entero⊕entero ───── */

static void test_aritmetica_entera(void) {
    verificar("1 + 2", "3");
    verificar("10 - 3", "7");
    verificar("4 * 5", "20");
    verificar("7 // 2", "3");
    verificar("7 % 3", "1");
    verificar("2 ** 10", "1024");

    /* Precedencia. */
    verificar("1 + 2 * 3", "7");
    verificar("(1 + 2) * 3", "9");
    verificar("2 ** 3 ** 2", "512");  /* derecha asociativa */

    /* Negativos: floor division y módulo Python style. */
    verificar("-7 // 2", "-4");        /* Python: -4 (floor) no -3 (trunc) */
    verificar("-7 % 2", "1");          /* Python: 1 (siempre positivo si b>0) */
    verificar("7 // -2", "-4");
    verificar("7 % -2", "-1");

    /* Bignum: factorial chico para no traer un loop, pero suficientemente
     * grande para exceder int64. */
    verificar("100 ** 5", "10000000000");          /* 1e10 */
    verificar("2 ** 100", "1267650600228229401496703205376");
}

/* ───── True division y mixed ───── */

static void test_division_y_mixto(void) {
    /* `/` siempre da decimal. */
    verificar("7 / 2", "3.5");
    verificar("6 / 2", "3.0");      /* incluso si exacto */
    verificar("1 / 4", "0.25");

    /* Promoción entero+decimal. */
    verificar("1 + 2.5", "3.5");
    verificar("3 - 0.5", "2.5");
    verificar("2 * 1.5", "3.0");
    verificar("0.5 ** 2", "0.25");
}

/* ───── Decimales puros ───── */

static void test_aritmetica_decimal(void) {
    verificar("1.5 + 2.5", "4.0");
    verificar("3.0 - 1.5", "1.5");
    verificar("2.0 * 2.5", "5.0");
    verificar("5.0 / 2.0", "2.5");
    verificar("7.0 // 2.0", "3.0");
    verificar("7.0 % 3.0", "1.0");
    verificar("-7.5 % 3.0", "1.5");  /* Python: a - floor(a/b)*b */
}

/* ───── Comparaciones ───── */

static void test_comparaciones(void) {
    verificar("1 == 1", "verdadero");
    verificar("1 == 2", "falso");
    verificar("1 != 2", "verdadero");
    verificar("3 < 5", "verdadero");
    verificar("5 < 3", "falso");
    verificar("3 <= 3", "verdadero");
    verificar("3 >= 4", "falso");

    /* Cross-tipo numérico. */
    verificar("1 == 1.0", "verdadero");
    verificar("1.0 < 2", "verdadero");
    verificar("verdadero == 1", "verdadero");
    verificar("falso < 1", "verdadero");

    /* Cadenas. */
    verificar("\"hola\" == \"hola\"", "verdadero");
    verificar("\"a\" < \"b\"", "verdadero");
    verificar("\"abc\" < \"abd\"", "verdadero");

    /* Tipos incomparables NO dan false silencioso para <, dan error. */
    verificar_error("1 < \"hola\"", "no se puede comparar");
    verificar("1 == \"hola\"", "falso");  /* == sí permite tipos distintos */
}

/* ───── Bitwise ───── */

static void test_bitwise(void) {
    verificar("0b1100 & 0b1010", "8");
    verificar("0b1100 | 0b1010", "14");
    verificar("0b1100 ^ 0b1010", "6");
    verificar("1 << 8", "256");
    verificar("256 >> 4", "16");
    verificar("~0", "-1");
    verificar("~5", "-6");
}

/* ───── Unarios ───── */

static void test_unarios(void) {
    verificar("-5", "-5");
    verificar("- -5", "5");
    verificar("+5", "5");
    verificar("-3.14", "-3.14");
    verificar("no verdadero", "falso");
    verificar("no falso", "verdadero");
    verificar("no nulo", "verdadero");
    verificar("no 0", "verdadero");
    verificar("no \"\"", "verdadero");
    verificar("no \"x\"", "falso");
    verificar("no 42", "falso");
    /* Doble negación. */
    verificar("no no verdadero", "verdadero");
}

/* ───── Lógica con cortocircuito ───── */

static void test_logica(void) {
    /* Resultados básicos. */
    verificar("verdadero y verdadero", "verdadero");
    verificar("verdadero y falso", "falso");
    verificar("falso y verdadero", "falso");
    verificar("verdadero o falso", "verdadero");
    verificar("falso o falso", "falso");

    /* Devuelve el valor decisor (no booleano), estilo Python. */
    verificar("0 o 42", "42");
    verificar("\"\" o \"x\"", "x");
    verificar("nulo o 7", "7");
    verificar("1 y 2", "2");
    verificar("\"a\" y \"b\"", "b");

    /* Cortocircuito: si el primer operando decide, el segundo no se
     * evalúa. Verificamos esto haciendo que el segundo sea una
     * expresión que daría error si se evaluara (división por cero). */
    verificar("verdadero o (1 // 0)", "verdadero");
    verificar("falso y (1 // 0)", "falso");
}

/* ───── Cadenas: concatenación, repetición, en ───── */

static void test_cadenas(void) {
    verificar("\"hola \" + \"mundo\"", "hola mundo");
    verificar("\"ab\" * 3", "ababab");
    verificar("3 * \"ab\"", "ababab");
    verificar("\"x\" * 0", "");
    verificar("\"x\" * -1", "");

    /* `en` para substring. */
    verificar("\"ola\" en \"hola\"", "verdadero");
    verificar("\"\" en \"hola\"", "verdadero");
    verificar("\"z\" en \"hola\"", "falso");
}

/* ───── Identidad ───── */

static void test_identidad(void) {
    verificar("nulo es nulo", "verdadero");
    verificar("verdadero es verdadero", "verdadero");
    verificar("verdadero es no falso", "verdadero");
    verificar("1 es 1", "verdadero");
    verificar("1 no es 2", "verdadero");
}

/* ───── Identificadores y scope ───── */

static void test_identificadores(void) {
    /* Para esto necesitamos meter variables a mano en el entorno.
     * Nota: 'y' es keyword (operador lógico) — usamos 'a' y 'b'. */
    Entorno globales;
    entorno_iniciar(&globales, NULL);

    static const char clave_a[] = "a";
    static const char clave_b[] = "b";
    entorno_definir(&globales, clave_a, 1, valor_entero_de_long(10));
    entorno_definir(&globales, clave_b, 1, valor_entero_de_long(3));

    Arena ar;
    arena_iniciar(&ar, 1024);

    const char *fuente = "a + b * 2";
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Parser p;
    parser_iniciar(&p, &l, &ar, fuente, "<test>");
    Expr *e = parser_parsear_expr(&p);
    AFIRMAR(e != NULL && !p.tuvo_error);

    Evaluador ev;
    evaluador_iniciar(&ev, &globales);
    Valor v = evaluador_evaluar_expr(&ev, e);
    AFIRMAR(!ev.error.tuvo_error);
    AFIRMAR(valor_es_entero(&v));

    char buf[64];
    valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "16") == 0);

    valor_destruir(&v);
    arena_destruir(&ar);
    entorno_destruir(&globales);
}

static void test_identificador_no_definido(void) {
    verificar_error("x + 1", "no esta definido");
}

/* ───── Errores varios ───── */

static void test_errores_runtime(void) {
    verificar_error("1 // 0", "division por cero");
    verificar_error("1 % 0", "modulo por cero");
    verificar_error("1 / 0", "division por cero");
    verificar_error("\"a\" + 1", "no aplica");
    verificar_error("1 & \"a\"", "bitwise");
}

/* ───── Combinaciones realistas ───── */

static void test_realistas(void) {
    /* Pitagoras: c^2 = a^2 + b^2 con 3-4-5. */
    verificar("3 ** 2 + 4 ** 2", "25");

    /* Gugol — enteros enormes sin overflow. */
    verificar("10 ** 100",
        "10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");

    /* Promediar enteros via /. */
    verificar("(10 + 20 + 30) / 3", "20.0");

    /* Encadenar relaciones via 'y' (Cornamusa NO tiene chained comparisons
     * tipo Python `0 < x < 10`, así que se escribe explícitamente). */
    verificar("0 < 5 y 5 < 10", "verdadero");
    verificar("0 < 5 y 5 < 3", "falso");

    /* Concat con condición. */
    verificar("verdadero y \"si\" o \"no\"", "si");
    verificar("falso y \"si\" o \"no\"", "no");
}

/* ───── Main ───── */

int main(void) {
    test_literales();
    test_aritmetica_entera();
    test_division_y_mixto();
    test_aritmetica_decimal();
    test_comparaciones();
    test_bitwise();
    test_unarios();
    test_logica();
    test_cadenas();
    test_identidad();
    test_identificadores();
    test_identificador_no_definido();
    test_errores_runtime();
    test_realistas();

    if (fallos == 0) {
        printf("OK: todos los tests del evaluador pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
