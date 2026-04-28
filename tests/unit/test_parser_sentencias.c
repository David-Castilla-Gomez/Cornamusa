/*
 * Tests del parser — Fase 3 Sesión 2: sentencias.
 *
 * Cobertura:
 *   - Sentencias simples: pasar, romper, continuar, retornar, expr.
 *   - Asignación simple y aumentada.
 *   - `si` / `sino si` / `sino` con `fin si` y one-liner.
 *   - `mientras` con `sino` opcional y one-liner.
 *   - `para X en Y:` con `sino` opcional y one-liner.
 *   - Validación de etiquetas: `fin si` solo cierra `si`, etc.
 *   - Errores: `fin` desnudo, etiqueta de `fin` mal, falta `fin`,
 *     falta `:`.
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
 * Parsea `fuente` como una sola sentencia y devuelve su pretty-print.
 * Si hay error de parseo, devuelve NULL.
 */
static const char *parsear_y_imprimir(const char *fuente, bool *err_out) {
    static char buffer[4096];

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");

    Arena a;
    arena_iniciar(&a, 1024);

    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");

    Sent *s = parser_parsear_sentencia(&p);
    if (err_out) *err_out = p.tuvo_error;

    if (s == NULL || p.tuvo_error) {
        arena_destruir(&a);
        return NULL;
    }

    sent_a_cadena(s, buffer, sizeof(buffer));
    arena_destruir(&a);
    return buffer;
}

static void verificar(const char *fuente, const char *esperado) {
    bool err = false;
    const char *resultado = parsear_y_imprimir(fuente, &err);
    if (err || resultado == NULL) {
        fprintf(stderr, "FALLO: '%s' produjo error\n", fuente);
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

static void verificar_error(const char *fuente) {
    bool err = false;
    parsear_y_imprimir(fuente, &err);
    if (!err) {
        fprintf(stderr, "FALLO: '%s' debería haber dado error\n", fuente);
        fallos++;
    }
}

/* ───── Sentencias simples ───── */

static void test_pasar(void)     { verificar("pasar", "(pasar)"); }
static void test_romper(void)    { verificar("romper", "(romper)"); }
static void test_continuar(void) { verificar("continuar", "(continuar)"); }

static void test_retornar_sin_valor(void) {
    verificar("retornar", "(retornar)");
}

static void test_retornar_con_valor(void) {
    verificar("retornar 42", "(retornar (lit-int 42))");
}

static void test_retornar_expresion_compleja(void) {
    verificar("retornar n * factorial(n - 1)",
        "(retornar (op \"*\" (ident n) "
        "(llamada (ident factorial) (op \"-\" (ident n) (lit-int 1)))))");
}

/* ───── Sentencia-expresión ───── */

static void test_sent_expr_llamada(void) {
    verificar("imprimir(x)",
        "(sent-expr (llamada (ident imprimir) (ident x)))");
}

/* ───── Asignación ───── */

static void test_asignacion_simple(void) {
    verificar("x = 42",
        "(asignar (ident x) (lit-int 42))");
}

static void test_asignacion_con_expresion(void) {
    verificar("total = a + b * c",
        "(asignar (ident total) "
        "(op \"+\" (ident a) (op \"*\" (ident b) (ident c))))");
}

static void test_asignacion_a_atributo(void) {
    verificar("yo.nombre = nombre",
        "(asignar (atr (ident yo) \"nombre\") (ident nombre))");
}

static void test_asignacion_aumentada_mas(void) {
    verificar("x += 1",
        "(asignar-aug \"+=\" (ident x) (lit-int 1))");
}

static void test_asignacion_aumentada_estrella(void) {
    verificar("contador *= 2",
        "(asignar-aug \"*=\" (ident contador) (lit-int 2))");
}

/* ───── si / sino / fin si ───── */

static void test_si_sin_sino(void) {
    verificar(
        "si x > 0:\n"
        "    imprimir(x)\n"
        "fin si",
        "(si (rama (op \">\" (ident x) (lit-int 0)) "
        "(bloque (sent-expr (llamada (ident imprimir) (ident x))))))");
}

static void test_si_con_sino(void) {
    verificar(
        "si x > 0:\n"
        "    a = 1\n"
        "sino:\n"
        "    a = 2\n"
        "fin si",
        "(si "
        "(rama (op \">\" (ident x) (lit-int 0)) (bloque (asignar (ident a) (lit-int 1)))) "
        "(rama nulo (bloque (asignar (ident a) (lit-int 2)))))");
}

static void test_si_con_sino_si_y_sino(void) {
    verificar(
        "si x > 0:\n"
        "    pasar\n"
        "sino si x < 0:\n"
        "    romper\n"
        "sino:\n"
        "    continuar\n"
        "fin si",
        "(si "
        "(rama (op \">\" (ident x) (lit-int 0)) (bloque (pasar))) "
        "(rama (op \"<\" (ident x) (lit-int 0)) (bloque (romper))) "
        "(rama nulo (bloque (continuar))))");
}

static void test_si_one_liner(void) {
    verificar("si x > 0: pasar",
        "(si (rama (op \">\" (ident x) (lit-int 0)) (bloque (pasar))))");
}

/* ───── mientras ───── */

static void test_mientras_basico(void) {
    verificar(
        "mientras x < 10:\n"
        "    x += 1\n"
        "fin mientras",
        "(mientras (op \"<\" (ident x) (lit-int 10)) "
        "(bloque (asignar-aug \"+=\" (ident x) (lit-int 1))))");
}

static void test_mientras_con_sino(void) {
    verificar(
        "mientras x < 10:\n"
        "    x += 1\n"
        "sino:\n"
        "    pasar\n"
        "fin mientras",
        "(mientras (op \"<\" (ident x) (lit-int 10)) "
        "(bloque (asignar-aug \"+=\" (ident x) (lit-int 1))) "
        "(bloque (pasar)))");
}

static void test_mientras_one_liner(void) {
    verificar("mientras verdadero: pasar",
        "(mientras (lit-bool verdadero) (bloque (pasar)))");
}

/* ───── para ───── */

static void test_para_basico(void) {
    verificar(
        "para i en rango(10):\n"
        "    imprimir(i)\n"
        "fin para",
        "(para (ident i) (llamada (ident rango) (lit-int 10)) "
        "(bloque (sent-expr (llamada (ident imprimir) (ident i)))))");
}

static void test_para_one_liner(void) {
    verificar("para x en lista: imprimir(x)",
        "(para (ident x) (ident lista) "
        "(bloque (sent-expr (llamada (ident imprimir) (ident x)))))");
}

static void test_para_con_sino(void) {
    verificar(
        "para i en rango(3):\n"
        "    pasar\n"
        "sino:\n"
        "    romper\n"
        "fin para",
        "(para (ident i) (llamada (ident rango) (lit-int 3)) "
        "(bloque (pasar)) "
        "(bloque (romper)))");
}

/* ───── Anidamiento ───── */

static void test_si_anidado_en_para(void) {
    verificar(
        "para i en rango(10):\n"
        "    si i % 2 == 0:\n"
        "        imprimir(i)\n"
        "    fin si\n"
        "fin para",
        "(para (ident i) (llamada (ident rango) (lit-int 10)) "
        "(bloque "
        "(si (rama (op \"==\" (op \"%\" (ident i) (lit-int 2)) (lit-int 0)) "
        "(bloque (sent-expr (llamada (ident imprimir) (ident i))))))))");
}

static void test_mientras_dentro_de_si(void) {
    verificar(
        "si x > 0:\n"
        "    mientras x > 0:\n"
        "        x -= 1\n"
        "    fin mientras\n"
        "fin si",
        "(si (rama (op \">\" (ident x) (lit-int 0)) "
        "(bloque (mientras (op \">\" (ident x) (lit-int 0)) "
        "(bloque (asignar-aug \"-=\" (ident x) (lit-int 1)))))))");
}

/* ───── Validación de etiquetas `fin <X>` ───── */

static void test_fin_si_cierra_si(void) {
    /* Ya verificado en tests anteriores; este test es explícito por claridad. */
    verificar(
        "si x:\n"
        "    pasar\n"
        "fin si",
        "(si (rama (ident x) (bloque (pasar))))");
}

static void test_fin_etiqueta_no_coincide_es_error(void) {
    verificar_error(
        "si x:\n"
        "    pasar\n"
        "fin para");
}

static void test_fin_si_para_mientras_es_error(void) {
    verificar_error(
        "mientras x:\n"
        "    pasar\n"
        "fin si");
}

static void test_fin_sin_etiqueta_es_error(void) {
    /* `fin` desnudo nunca es válido (decisión B1). El parser debería
       quejarse cuando consumir_fin ve `fin` sin la palabra esperada. */
    verificar_error(
        "si x:\n"
        "    pasar\n"
        "fin");
}

static void test_falta_fin_es_error(void) {
    /* Llegamos a EOF sin haber visto `fin si`. */
    verificar_error(
        "si x:\n"
        "    pasar");
}

static void test_falta_dos_puntos_si(void) {
    verificar_error("si x pasar");
}

static void test_falta_dos_puntos_mientras(void) {
    verificar_error("mientras x pasar");
}

/* ───── Programa completo (varias sentencias) ───── */

static void test_programa_completo(void) {
    const char *fuente =
        "x = 0\n"
        "mientras x < 5:\n"
        "    imprimir(x)\n"
        "    x += 1\n"
        "fin mientras";

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Arena a;
    arena_iniciar(&a, 1024);
    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");

    int n;
    Sent **sents = parser_parsear_programa(&p, &n);

    AFIRMAR(!p.tuvo_error);
    AFIRMAR(n == 2);
    AFIRMAR(sents != NULL);
    AFIRMAR(sents[0]->tipo == SENT_ASIGNAR);
    AFIRMAR(sents[1]->tipo == SENT_MIENTRAS);

    arena_destruir(&a);
}

int main(void) {
    /* Sentencias simples */
    test_pasar();
    test_romper();
    test_continuar();
    test_retornar_sin_valor();
    test_retornar_con_valor();
    test_retornar_expresion_compleja();
    test_sent_expr_llamada();

    /* Asignación */
    test_asignacion_simple();
    test_asignacion_con_expresion();
    test_asignacion_a_atributo();
    test_asignacion_aumentada_mas();
    test_asignacion_aumentada_estrella();

    /* si */
    test_si_sin_sino();
    test_si_con_sino();
    test_si_con_sino_si_y_sino();
    test_si_one_liner();

    /* mientras */
    test_mientras_basico();
    test_mientras_con_sino();
    test_mientras_one_liner();

    /* para */
    test_para_basico();
    test_para_one_liner();
    test_para_con_sino();

    /* Anidamiento */
    test_si_anidado_en_para();
    test_mientras_dentro_de_si();

    /* Validación fin */
    test_fin_si_cierra_si();

    /* Programa completo */
    test_programa_completo();

    /* Errores (suprimidos visualmente con cabecera) */
    fprintf(stdout, "\n--- Mensajes de error esperados a continuación ---\n");
    test_fin_etiqueta_no_coincide_es_error();
    test_fin_si_para_mientras_es_error();
    test_fin_sin_etiqueta_es_error();
    test_falta_fin_es_error();
    test_falta_dos_puntos_si();
    test_falta_dos_puntos_mientras();
    fprintf(stdout, "--- Fin de mensajes de error esperados ---\n\n");

    if (fallos == 0) {
        printf("test_parser_sentencias: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stdout, "test_parser_sentencias: %d fallo(s)\n", fallos);
    return 1;
}
