/*
 * Tests del parser — Fase 3 Sesión 4: excepciones y módulos.
 *
 * Cobertura:
 *   - intentar/atrapar/finalmente con todas las combinaciones.
 *   - lanzar con expresión y bare (re-raise).
 *   - importar simple, dotted, con alias.
 *   - desde X importar Y, Z; con alias; *.
 *   - global y nolocal con uno o varios nombres.
 *   - Errores: intentar sin atrapar/finalmente, importar sin nombre, etc.
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"

static int fallos = 0;

static const char *parsear_y_imprimir(const char *fuente, bool *err_out) {
    static char buffer[8192];
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Arena a;
    arena_iniciar(&a, 4096);
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
    const char *r = parsear_y_imprimir(fuente, &err);
    if (err || r == NULL) {
        fprintf(stderr, "FALLO: '%s' produjo error\n", fuente);
        fallos++;
        return;
    }
    if (strcmp(r, esperado) != 0) {
        fprintf(stderr,
            "FALLO: '%s'\n  esperaba: %s\n  obtuvo:   %s\n",
            fuente, esperado, r);
        fallos++;
    }
}

static void verificar_error(const char *fuente) {
    bool err = false;
    parsear_y_imprimir(fuente, &err);
    if (!err) {
        fprintf(stderr, "FALLO: '%s' debería dar error\n", fuente);
        fallos++;
    }
}

/* ───── intentar/atrapar/finalmente ───── */

static void test_intentar_atrapar_simple(void) {
    verificar(
        "intentar:\n"
        "    pasar\n"
        "atrapar:\n"
        "    pasar\n"
        "fin intentar",
        "(intentar (bloque (pasar)) "
        "(atrapar nulo (bloque (pasar))))");
}

static void test_intentar_atrapar_con_tipo(void) {
    verificar(
        "intentar:\n"
        "    pasar\n"
        "atrapar ErrorDeIO:\n"
        "    pasar\n"
        "fin intentar",
        "(intentar (bloque (pasar)) "
        "(atrapar (ident ErrorDeIO) (bloque (pasar))))");
}

static void test_intentar_atrapar_con_alias(void) {
    verificar(
        "intentar:\n"
        "    pasar\n"
        "atrapar ErrorDeIO como e:\n"
        "    pasar\n"
        "fin intentar",
        "(intentar (bloque (pasar)) "
        "(atrapar (ident ErrorDeIO) (alias e) (bloque (pasar))))");
}

static void test_intentar_varios_atrapar(void) {
    verificar(
        "intentar:\n"
        "    pasar\n"
        "atrapar ErrorDeIO:\n"
        "    pasar\n"
        "atrapar ErrorDeValor como v:\n"
        "    pasar\n"
        "fin intentar",
        "(intentar (bloque (pasar)) "
        "(atrapar (ident ErrorDeIO) (bloque (pasar))) "
        "(atrapar (ident ErrorDeValor) (alias v) (bloque (pasar))))");
}

static void test_intentar_con_finalmente(void) {
    verificar(
        "intentar:\n"
        "    pasar\n"
        "finalmente:\n"
        "    pasar\n"
        "fin intentar",
        "(intentar (bloque (pasar)) "
        "(finalmente (bloque (pasar))))");
}

static void test_intentar_completo(void) {
    /* try/except/else/finally */
    verificar(
        "intentar:\n"
        "    pasar\n"
        "atrapar Error como e:\n"
        "    pasar\n"
        "sino:\n"
        "    pasar\n"
        "finalmente:\n"
        "    pasar\n"
        "fin intentar",
        "(intentar (bloque (pasar)) "
        "(atrapar (ident Error) (alias e) (bloque (pasar))) "
        "(sino (bloque (pasar))) "
        "(finalmente (bloque (pasar))))");
}

/* ───── lanzar ───── */

static void test_lanzar_con_valor(void) {
    verificar("lanzar ErrorDeValor(\"mal\")",
        "(lanzar (llamada (ident ErrorDeValor) (lit-str \"mal\")))");
}

static void test_lanzar_bare(void) {
    /* Re-raise sin valor (válido dentro de atrapar). */
    verificar("lanzar", "(lanzar)");
}

/* ───── importar ───── */

static void test_importar_simple(void) {
    verificar("importar matematicas",
        "(importar matematicas)");
}

static void test_importar_dotted(void) {
    verificar("importar matematicas.geometria",
        "(importar matematicas.geometria)");
}

static void test_importar_con_alias(void) {
    verificar("importar matematicas como mat",
        "(importar matematicas (alias mat))");
}

/* ───── desde X importar ───── */

static void test_desde_importar_uno(void) {
    verificar("desde cadenas importar mayusculas",
        "(desde cadenas importar (item mayusculas))");
}

static void test_desde_importar_varios(void) {
    verificar("desde cadenas importar mayusculas, dividir, unir",
        "(desde cadenas importar (item mayusculas) (item dividir) (item unir))");
}

static void test_desde_importar_con_alias(void) {
    verificar("desde cadenas importar mayusculas como mayus",
        "(desde cadenas importar (item mayusculas (alias mayus)))");
}

static void test_desde_importar_estrella(void) {
    verificar("desde matematicas importar *",
        "(desde matematicas importar *)");
}

static void test_desde_importar_dotted(void) {
    verificar("desde matematicas.geometria importar circulo",
        "(desde matematicas.geometria importar (item circulo))");
}

/* ───── global / nolocal ───── */

static void test_global_uno(void) {
    verificar("global x", "(global x)");
}

static void test_global_varios(void) {
    verificar("global a, b, c", "(global a b c)");
}

static void test_nolocal_uno(void) {
    verificar("nolocal valor", "(nolocal valor)");
}

static void test_nolocal_varios(void) {
    /* `y` es keyword (operador AND); usamos `n` y `z`. */
    verificar("nolocal x, n, z", "(nolocal x n z)");
}

/* ───── Errores ───── */

static void test_intentar_sin_atrapar_ni_finalmente(void) {
    verificar_error(
        "intentar:\n"
        "    pasar\n"
        "fin intentar");
}

static void test_intentar_alias_sin_nombre(void) {
    verificar_error(
        "intentar:\n"
        "    pasar\n"
        "atrapar Error como :\n"
        "    pasar\n"
        "fin intentar");
}

static void test_importar_sin_nombre(void) {
    verificar_error("importar");
}

static void test_desde_sin_importar(void) {
    verificar_error("desde modulo");
}

static void test_desde_sin_items(void) {
    verificar_error("desde modulo importar");
}

static void test_global_sin_nombre(void) {
    verificar_error("global");
}

/* ───── Anidamiento realista (del ejemplo 08_excepciones.cor) ───── */

static void test_funcion_con_intentar(void) {
    verificar(
        "funcion dividir_seguro(a, b):\n"
        "    intentar:\n"
        "        retornar a / b\n"
        "    atrapar ErrorDivisionPorCero:\n"
        "        retornar nulo\n"
        "    fin intentar\n"
        "fin funcion",
        "(funcion dividir_seguro (param a) (param b) "
        "(bloque "
        "(intentar (bloque (retornar (op \"/\" (ident a) (ident b)))) "
        "(atrapar (ident ErrorDivisionPorCero) "
        "(bloque (retornar (lit-nulo)))))))");
}

static void test_closure_con_nolocal(void) {
    /* Patrón del ejemplo 09_closures.cor */
    verificar(
        "funcion incrementar(paso=1):\n"
        "    nolocal valor\n"
        "    valor += paso\n"
        "    retornar valor\n"
        "fin funcion",
        "(funcion incrementar (param paso (defecto (lit-int 1))) "
        "(bloque "
        "(nolocal valor) "
        "(asignar-aug \"+=\" (ident valor) (ident paso)) "
        "(retornar (ident valor))))");
}

int main(void) {
    /* intentar */
    test_intentar_atrapar_simple();
    test_intentar_atrapar_con_tipo();
    test_intentar_atrapar_con_alias();
    test_intentar_varios_atrapar();
    test_intentar_con_finalmente();
    test_intentar_completo();

    /* lanzar */
    test_lanzar_con_valor();
    test_lanzar_bare();

    /* importar */
    test_importar_simple();
    test_importar_dotted();
    test_importar_con_alias();

    /* desde importar */
    test_desde_importar_uno();
    test_desde_importar_varios();
    test_desde_importar_con_alias();
    test_desde_importar_estrella();
    test_desde_importar_dotted();

    /* global / nolocal */
    test_global_uno();
    test_global_varios();
    test_nolocal_uno();
    test_nolocal_varios();

    /* Anidamiento realista */
    test_funcion_con_intentar();
    test_closure_con_nolocal();

    /* Errores (cabecera para distinguir) */
    fprintf(stdout, "\n--- Mensajes de error esperados ---\n");
    test_intentar_sin_atrapar_ni_finalmente();
    test_intentar_alias_sin_nombre();
    test_importar_sin_nombre();
    test_desde_sin_importar();
    test_desde_sin_items();
    test_global_sin_nombre();
    fprintf(stdout, "--- Fin de mensajes de error ---\n\n");

    if (fallos == 0) {
        printf("test_parser_excepciones_modulos: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stdout, "test_parser_excepciones_modulos: %d fallo(s)\n", fallos);
    return 1;
}
