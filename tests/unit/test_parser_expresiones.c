/*
 * Tests del parser — Fase 3 Sesión 1: expresiones.
 *
 * Cobertura:
 *   - Literales (entero, decimal, cadena, f-cadena, booleano, nulo).
 *   - Identificadores.
 *   - Operadores binarios con precedencia y asociatividad correctas.
 *   - Operadores unarios (-, +, no, ~).
 *   - Operadores lógicos (y, o) con su precedencia.
 *   - Llamadas con 0, 1, varios argumentos.
 *   - Acceso a atributo (`obj.attr`).
 *   - Agrupación con paréntesis.
 *   - Errores: paréntesis sin cerrar, falta de expresión, atributo sin nombre.
 *
 * Forma de testear: pretty-printer en S-expression. Los tests
 * comparan el string producido por `expr_a_cadena` con un literal
 * esperado, lo que verifica simultáneamente el AST y el printer.
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"

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
 * Parsea `fuente` como una sola expresión y devuelve el resultado en
 * un buffer estático. Retorna NULL si hubo error de parseo.
 *
 * El segundo parámetro recibe la longitud de la cadena producida.
 * El tercer parámetro `tuvo_error_out` se llena con el flag de error.
 */
static const char *parsear_y_imprimir(const char *fuente, bool *tuvo_error_out) {
    static char buffer[1024];

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");

    Arena a;
    arena_iniciar(&a, 1024);

    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");

    Expr *e = parser_parsear_expr(&p);

    if (tuvo_error_out) *tuvo_error_out = p.tuvo_error;

    if (e == NULL || p.tuvo_error) {
        arena_destruir(&a);
        return NULL;
    }

    expr_a_cadena(e, buffer, sizeof(buffer));
    arena_destruir(&a);
    return buffer;
}

/* Atajo: parsea y verifica que el AST imprime exactamente `esperado`. */
static void verificar(const char *fuente, const char *esperado) {
    bool err = false;
    const char *resultado = parsear_y_imprimir(fuente, &err);
    if (err || resultado == NULL) {
        fprintf(stderr, "FALLO: '%s' produjo error de parseo\n", fuente);
        fallos++;
        return;
    }
    if (strcmp(resultado, esperado) != 0) {
        fprintf(stderr,
            "FALLO: '%s'\n  esperaba: %s\n  obtuvo:   %s\n",
            fuente, esperado, resultado);
        fallos++;
    }
}

/* Atajo: parsea y verifica que SE produjo un error. */
static void verificar_error(const char *fuente) {
    /* Suprimimos stderr durante el parseo para no inundar de
       mensajes durante los tests. Usamos un truco simple:
       redirigimos stderr a /dev/null (o NUL en Windows). */
    bool err = false;
    fflush(stderr);
    FILE *real = stderr;
    (void)real;
    /* Para portabilidad y simplicidad, dejamos que los mensajes salgan;
       el test sigue siendo correcto si comparamos solo el flag. */
    parsear_y_imprimir(fuente, &err);
    if (!err) {
        fprintf(stderr, "FALLO: '%s' debería haber dado error pero no lo hizo\n", fuente);
        fallos++;
    }
}

/* ───── Literales ───── */

static void test_literal_entero(void) {
    verificar("42", "(lit-int 42)");
    verificar("0xff", "(lit-int 0xff)");
    verificar("1_000_000", "(lit-int 1_000_000)");
}

static void test_literal_decimal(void) {
    verificar("3.14", "(lit-dec 3.14)");
    verificar("1.5e-3", "(lit-dec 1.5e-3)");
}

static void test_literal_cadena(void) {
    verificar("\"hola\"", "(lit-str \"hola\")");
    verificar("'mundo'", "(lit-str 'mundo')");
}

static void test_literal_f_cadena(void) {
    verificar("f\"hola {x}\"", "(lit-fstr f\"hola {x}\")");
}

static void test_literal_booleano(void) {
    verificar("verdadero", "(lit-bool verdadero)");
    verificar("falso", "(lit-bool falso)");
}

static void test_literal_nulo(void) {
    verificar("nulo", "(lit-nulo)");
}

/* ───── Identificador ───── */

static void test_ident(void) {
    verificar("x", "(ident x)");
    verificar("calcular_total", "(ident calcular_total)");
    verificar("niño", "(ident niño)");
}

/* ───── Operadores binarios — precedencia ───── */

static void test_suma_simple(void) {
    verificar("1 + 2", "(op \"+\" (lit-int 1) (lit-int 2))");
}

static void test_suma_mas_multiplicacion(void) {
    /* * tiene precedencia mayor que +, así que: 1 + (2 * 3) */
    verificar("1 + 2 * 3",
        "(op \"+\" (lit-int 1) (op \"*\" (lit-int 2) (lit-int 3)))");
}

static void test_resta_asociativa_izq(void) {
    /* - es asociativa por la izquierda: (1 - 2) - 3 */
    verificar("1 - 2 - 3",
        "(op \"-\" (op \"-\" (lit-int 1) (lit-int 2)) (lit-int 3))");
}

static void test_potencia_asociativa_der(void) {
    /* ** es asociativa por la derecha: 2 ** (3 ** 4) */
    verificar("2 ** 3 ** 4",
        "(op \"**\" (lit-int 2) (op \"**\" (lit-int 3) (lit-int 4)))");
}

static void test_division_entera_y_modulo(void) {
    verificar("a // b", "(op \"//\" (ident a) (ident b))");
    verificar("a % b", "(op \"%\" (ident a) (ident b))");
}

static void test_comparacion(void) {
    verificar("a == b", "(op \"==\" (ident a) (ident b))");
    verificar("a != b", "(op \"!=\" (ident a) (ident b))");
    verificar("a <= b", "(op \"<=\" (ident a) (ident b))");
}

static void test_comparacion_combinada_con_aritmetica(void) {
    /* Aritmética antes de comparación: (a + b) == c */
    verificar("a + b == c",
        "(op \"==\" (op \"+\" (ident a) (ident b)) (ident c))");
}

static void test_bitwise(void) {
    verificar("a & b", "(op \"&\" (ident a) (ident b))");
    verificar("a | b", "(op \"|\" (ident a) (ident b))");
    verificar("a << 2", "(op \"<<\" (ident a) (lit-int 2))");
}

/* ───── Unarios ───── */

static void test_negacion_aritmetica(void) {
    verificar("-x", "(uop \"-\" (ident x))");
    verificar("-3.14", "(uop \"-\" (lit-dec 3.14))");
}

static void test_no_logico(void) {
    verificar("no x", "(uop \"no\" (ident x))");
}

static void test_no_y_comparacion(void) {
    /* `no` debe envolver toda la comparación, no solo `a`. */
    verificar("no a == b",
        "(uop \"no\" (op \"==\" (ident a) (ident b)))");
}

static void test_negacion_bit(void) {
    verificar("~mask", "(uop \"~\" (ident mask))");
}

static void test_unario_anidado(void) {
    /* --x = -(-x) */
    verificar("--x", "(uop \"-\" (uop \"-\" (ident x)))");
}

/* ───── Lógicas y / o ───── */

static void test_logica_y(void) {
    verificar("a y b", "(y (ident a) (ident b))");
}

static void test_logica_o(void) {
    verificar("a o b", "(o (ident a) (ident b))");
}

static void test_logica_y_tiene_precedencia_sobre_o(void) {
    /* a o (b y c) */
    verificar("a o b y c",
        "(o (ident a) (y (ident b) (ident c)))");
}

static void test_no_tiene_precedencia_sobre_y(void) {
    /* (no a) y b */
    verificar("no a y b",
        "(y (uop \"no\" (ident a)) (ident b))");
}

/* ───── Agrupación ───── */

static void test_grupo_cambia_precedencia(void) {
    verificar("(1 + 2) * 3",
        "(op \"*\" (grupo (op \"+\" (lit-int 1) (lit-int 2))) (lit-int 3))");
}

static void test_grupo_redundante(void) {
    /* Aunque sea innecesario, el grupo se preserva en el AST. */
    verificar("(42)", "(grupo (lit-int 42))");
}

/* ───── Llamadas ───── */

static void test_llamada_sin_args(void) {
    verificar("f()", "(llamada (ident f))");
}

static void test_llamada_un_arg(void) {
    verificar("f(x)", "(llamada (ident f) (ident x))");
}

static void test_llamada_varios_args(void) {
    verificar("f(a, b, c)",
        "(llamada (ident f) (ident a) (ident b) (ident c))");
}

static void test_llamada_con_expresion(void) {
    verificar("imprimir(a + b)",
        "(llamada (ident imprimir) (op \"+\" (ident a) (ident b)))");
}

static void test_llamada_anidada(void) {
    verificar("f(g(x))",
        "(llamada (ident f) (llamada (ident g) (ident x)))");
}

/* ───── Acceso a atributo ───── */

static void test_atributo_simple(void) {
    verificar("obj.atr",
        "(atr (ident obj) \"atr\")");
}

static void test_atributo_encadenado(void) {
    verificar("a.b.c",
        "(atr (atr (ident a) \"b\") \"c\")");
}

static void test_metodo_llamada(void) {
    /* obj.metodo(arg) */
    verificar("obj.metodo(arg)",
        "(llamada (atr (ident obj) \"metodo\") (ident arg))");
}

/* ───── Combinaciones realistas ───── */

static void test_tipo_yo_nombre(void) {
    /* tipo(yo).__nombre__ — del ejemplo 07_clases_herencia.cor */
    verificar("tipo(yo).__nombre__",
        "(atr (llamada (ident tipo) (ident yo)) \"__nombre__\")");
}

static void test_factorial_recursivo(void) {
    /* n * factorial(n - 1) */
    verificar("n * factorial(n - 1)",
        "(op \"*\" (ident n) (llamada (ident factorial) "
        "(op \"-\" (ident n) (lit-int 1))))");
}

static void test_condicion_compleja(void) {
    /* x > 0 y x < 100 */
    verificar("x > 0 y x < 100",
        "(y (op \">\" (ident x) (lit-int 0)) "
        "(op \"<\" (ident x) (lit-int 100)))");
}

/* ───── Errores ───── */

static void test_parentesis_sin_cerrar(void) {
    verificar_error("(1 + 2");
}

static void test_atributo_sin_nombre(void) {
    verificar_error("obj.");
}

static void test_operador_sin_operando(void) {
    verificar_error("1 +");
}

static void test_expresion_vacia(void) {
    verificar_error("");
}

int main(void) {
    /* Literales */
    test_literal_entero();
    test_literal_decimal();
    test_literal_cadena();
    test_literal_f_cadena();
    test_literal_booleano();
    test_literal_nulo();

    /* Identificador */
    test_ident();

    /* Binarios + precedencia */
    test_suma_simple();
    test_suma_mas_multiplicacion();
    test_resta_asociativa_izq();
    test_potencia_asociativa_der();
    test_division_entera_y_modulo();
    test_comparacion();
    test_comparacion_combinada_con_aritmetica();
    test_bitwise();

    /* Unarios */
    test_negacion_aritmetica();
    test_no_logico();
    test_no_y_comparacion();
    test_negacion_bit();
    test_unario_anidado();

    /* Lógicas */
    test_logica_y();
    test_logica_o();
    test_logica_y_tiene_precedencia_sobre_o();
    test_no_tiene_precedencia_sobre_y();

    /* Agrupación */
    test_grupo_cambia_precedencia();
    test_grupo_redundante();

    /* Llamadas */
    test_llamada_sin_args();
    test_llamada_un_arg();
    test_llamada_varios_args();
    test_llamada_con_expresion();
    test_llamada_anidada();

    /* Atributo */
    test_atributo_simple();
    test_atributo_encadenado();
    test_metodo_llamada();

    /* Combinaciones realistas */
    test_tipo_yo_nombre();
    test_factorial_recursivo();
    test_condicion_compleja();

    /* Errores (los mensajes irán a stderr pero los tests verifican
       solo el flag; ignorar el ruido visual durante el test) */
    fprintf(stdout,
        "\n--- Mensajes de error esperados a continuación (ruido normal) ---\n");
    test_parentesis_sin_cerrar();
    test_atributo_sin_nombre();
    test_operador_sin_operando();
    test_expresion_vacia();
    fprintf(stdout,
        "--- Fin de mensajes de error esperados ---\n\n");

    if (fallos == 0) {
        printf("test_parser_expresiones: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stdout, "test_parser_expresiones: %d fallo(s)\n", fallos);
    return 1;
}
