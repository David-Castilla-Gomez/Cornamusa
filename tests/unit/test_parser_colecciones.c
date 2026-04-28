/*
 * Tests del parser — Fase 3 Sesión 5: literales de colección e
 * indexación.
 *
 * Cobertura:
 *   - Listas vacías, con uno o varios elementos.
 *   - Diccionarios vacíos, con uno o varios pares.
 *   - Conjuntos con uno o varios elementos.
 *   - Tuplas vacías, de un solo elemento, de varios.
 *   - Distinción tupla vs grupo.
 *   - Indexación obj[k].
 *   - Slicing obj[a:b], obj[a:b:c], con omisiones.
 *   - Anidamiento: lista de listas, dicc con valor lista, etc.
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"

static int fallos = 0;

static const char *parsear_y_imprimir(const char *fuente, bool *err_out) {
    static char buffer[4096];
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Arena a;
    arena_iniciar(&a, 2048);
    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");

    Expr *e = parser_parsear_expr(&p);
    if (err_out) *err_out = p.tuvo_error;

    if (e == NULL || p.tuvo_error) {
        arena_destruir(&a);
        return NULL;
    }

    expr_a_cadena(e, buffer, sizeof(buffer));
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
        fprintf(stderr, "FALLO: '%s'\n  esperaba: %s\n  obtuvo:   %s\n",
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

/* ───── Listas ───── */

static void test_lista_vacia(void)   { verificar("[]", "(lista)"); }
static void test_lista_un_elem(void) { verificar("[42]", "(lista (lit-int 42))"); }

static void test_lista_varios(void) {
    verificar("[1, 2, 3]",
        "(lista (lit-int 1) (lit-int 2) (lit-int 3))");
}

static void test_lista_con_expresiones(void) {
    verificar("[a + b, x * 2, f(z)]",
        "(lista (op \"+\" (ident a) (ident b)) "
        "(op \"*\" (ident x) (lit-int 2)) "
        "(llamada (ident f) (ident z)))");
}

static void test_lista_anidada(void) {
    verificar("[[1, 2], [3, 4]]",
        "(lista (lista (lit-int 1) (lit-int 2)) "
        "(lista (lit-int 3) (lit-int 4)))");
}

static void test_lista_trailing_coma(void) {
    /* Trailing coma es válida en listas y útil para diffs limpios. */
    verificar("[1, 2, 3,]",
        "(lista (lit-int 1) (lit-int 2) (lit-int 3))");
}

/* ───── Diccionarios ───── */

static void test_dicc_vacio(void)    { verificar("{}", "(dicc)"); }

static void test_dicc_un_par(void) {
    verificar("{\"clave\": \"valor\"}",
        "(dicc (par (lit-str \"clave\") (lit-str \"valor\")))");
}

static void test_dicc_varios_pares(void) {
    verificar("{\"a\": 1, \"b\": 2}",
        "(dicc (par (lit-str \"a\") (lit-int 1)) "
        "(par (lit-str \"b\") (lit-int 2)))");
}

static void test_dicc_clave_compleja(void) {
    verificar("{x + 1: \"valor\"}",
        "(dicc (par (op \"+\" (ident x) (lit-int 1)) (lit-str \"valor\")))");
}

/* ───── Conjuntos ───── */

static void test_conjunto_un_elem(void) {
    verificar("{42}", "(conjunto (lit-int 42))");
}

static void test_conjunto_varios(void) {
    verificar("{1, 2, 3}",
        "(conjunto (lit-int 1) (lit-int 2) (lit-int 3))");
}

/* ───── Tuplas vs grupo ───── */

static void test_tupla_vacia(void) { verificar("()", "(tupla)"); }

static void test_tupla_un_elem(void) {
    /* Tupla de 1 requiere coma final. */
    verificar("(42,)", "(tupla (lit-int 42))");
}

static void test_grupo_no_es_tupla(void) {
    /* (42) sin coma es grupo, NO tupla de 1. */
    verificar("(42)", "(grupo (lit-int 42))");
}

static void test_tupla_dos_elem(void) {
    verificar("(1, 2)", "(tupla (lit-int 1) (lit-int 2))");
}

static void test_tupla_varios(void) {
    verificar("(a, b, c, d)",
        "(tupla (ident a) (ident b) (ident c) (ident d))");
}

/* ───── Indexación ───── */

static void test_indice_simple(void) {
    verificar("lista[0]", "(indice (ident lista) (lit-int 0))");
}

static void test_indice_negativo(void) {
    verificar("lista[-1]",
        "(indice (ident lista) (uop \"-\" (lit-int 1)))");
}

static void test_indice_expresion(void) {
    verificar("dicc[k + 1]",
        "(indice (ident dicc) (op \"+\" (ident k) (lit-int 1)))");
}

static void test_indice_anidado(void) {
    verificar("matriz[i][j]",
        "(indice (indice (ident matriz) (ident i)) (ident j))");
}

static void test_atributo_e_indice(void) {
    /* obj.lista[0] */
    verificar("obj.lista[0]",
        "(indice (atr (ident obj) \"lista\") (lit-int 0))");
}

/* ───── Slicing ───── */

static void test_slice_basico(void) {
    verificar("lista[1:4]",
        "(rebanada (ident lista) (lit-int 1) (lit-int 4))");
}

static void test_slice_sin_inicio(void) {
    verificar("lista[:5]",
        "(rebanada (ident lista) nulo (lit-int 5))");
}

static void test_slice_sin_fin(void) {
    verificar("lista[2:]",
        "(rebanada (ident lista) (lit-int 2) nulo)");
}

static void test_slice_completo(void) {
    verificar("lista[:]",
        "(rebanada (ident lista) nulo nulo)");
}

static void test_slice_con_paso(void) {
    verificar("lista[0:10:2]",
        "(rebanada (ident lista) (lit-int 0) (lit-int 10) (lit-int 2))");
}

static void test_slice_solo_paso(void) {
    verificar("lista[::2]",
        "(rebanada (ident lista) nulo nulo (lit-int 2))");
}

/* ───── Anidamiento realista (de los ejemplos) ───── */

static void test_dicc_con_lista_valor(void) {
    verificar("{\"nombres\": [\"Ana\", \"Luis\"]}",
        "(dicc (par (lit-str \"nombres\") "
        "(lista (lit-str \"Ana\") (lit-str \"Luis\"))))");
}

static void test_acceso_a_atributo_de_indice(void) {
    /* lista[0].nombre */
    verificar("lista[0].nombre",
        "(atr (indice (ident lista) (lit-int 0)) \"nombre\")");
}

static void test_llamada_metodo_en_indice(void) {
    /* lista[0].metodo() */
    verificar("lista[0].metodo()",
        "(llamada (atr (indice (ident lista) (lit-int 0)) \"metodo\"))");
}

/* ───── Errores ───── */

static void test_lista_sin_cerrar(void)         { verificar_error("[1, 2"); }
static void test_dicc_clave_sin_valor(void)     { verificar_error("{x:}"); }
static void test_indice_sin_cerrar(void)        { verificar_error("lista[0"); }

int main(void) {
    /* Listas */
    test_lista_vacia();
    test_lista_un_elem();
    test_lista_varios();
    test_lista_con_expresiones();
    test_lista_anidada();
    test_lista_trailing_coma();

    /* Diccionarios */
    test_dicc_vacio();
    test_dicc_un_par();
    test_dicc_varios_pares();
    test_dicc_clave_compleja();

    /* Conjuntos */
    test_conjunto_un_elem();
    test_conjunto_varios();

    /* Tuplas */
    test_tupla_vacia();
    test_tupla_un_elem();
    test_grupo_no_es_tupla();
    test_tupla_dos_elem();
    test_tupla_varios();

    /* Indexación */
    test_indice_simple();
    test_indice_negativo();
    test_indice_expresion();
    test_indice_anidado();
    test_atributo_e_indice();

    /* Slicing */
    test_slice_basico();
    test_slice_sin_inicio();
    test_slice_sin_fin();
    test_slice_completo();
    test_slice_con_paso();
    test_slice_solo_paso();

    /* Anidamiento */
    test_dicc_con_lista_valor();
    test_acceso_a_atributo_de_indice();
    test_llamada_metodo_en_indice();

    /* Errores */
    fprintf(stdout, "\n--- Mensajes de error esperados ---\n");
    test_lista_sin_cerrar();
    test_dicc_clave_sin_valor();
    test_indice_sin_cerrar();
    fprintf(stdout, "--- Fin de mensajes de error ---\n\n");

    if (fallos == 0) {
        printf("test_parser_colecciones: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stdout, "test_parser_colecciones: %d fallo(s)\n", fallos);
    return 1;
}
