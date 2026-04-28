/*
 * Tests del lexer — Fase 2 Sesión 1: símbolos, operadores, comentarios.
 *
 * Cobertura:
 *   1. Fuente vacía y solo whitespace devuelven TT_FIN_ARCHIVO.
 *   2. Cada símbolo individual produce el TipoToken correcto.
 *   3. Operadores compuestos (==, <=, +=, **, //, etc.) tienen prioridad
 *      sobre sus prefijos.
 *   4. Comentarios `# ...` se ignoran.
 *   5. Saltos de línea avanzan el contador y reinician la columna.
 *   6. Caracteres no reconocidos producen TT_ERROR.
 *   7. Una llamada extra a lexer_siguiente() tras EOF sigue dando EOF.
 */

#include <stdio.h>
#include <string.h>

#include "lexer.h"

static int fallos = 0;

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n",                            \
                    __FILE__, __LINE__, #cond);                                \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

#define AFIRMAR_TIPO(token, tipo_esperado)                                     \
    do {                                                                       \
        if ((token).tipo != (tipo_esperado)) {                                 \
            fprintf(stderr,                                                    \
                "FALLO en %s:%d: esperaba %s, obtenido %s\n",                  \
                __FILE__, __LINE__,                                            \
                tipo_token_nombre(tipo_esperado),                              \
                tipo_token_nombre((token).tipo));                              \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

/*
 * Tokeniza `fuente` y verifica que la secuencia de tipos coincide con
 * `esperados` (terminada implícitamente por TT_FIN_ARCHIVO; no incluyas
 * EOF en el array, se verifica automáticamente).
 */
static void verificar_secuencia(const char *fuente,
                                const TipoToken *esperados,
                                int n) {
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    for (int i = 0; i < n; i++) {
        Token t = lexer_siguiente(&l);
        if (t.tipo != esperados[i]) {
            fprintf(stderr,
                "  fuente='%s' pos=%d esperaba=%s obtenido=%s\n",
                fuente, i,
                tipo_token_nombre(esperados[i]),
                tipo_token_nombre(t.tipo));
            fallos++;
            return;
        }
    }
    Token final = lexer_siguiente(&l);
    if (final.tipo != TT_FIN_ARCHIVO) {
        fprintf(stderr,
            "  fuente='%s' tras los %d tokens esperados, esperaba EOF, "
            "obtenido %s\n",
            fuente, n, tipo_token_nombre(final.tipo));
        fallos++;
    }
}

/* ───── 1. Fuente vacía y whitespace ───── */

static void test_fuente_vacia(void) {
    Lexer l;
    lexer_iniciar(&l, "", "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_FIN_ARCHIVO);
}

static void test_solo_whitespace(void) {
    Lexer l;
    lexer_iniciar(&l, "   \t\r  ", "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_FIN_ARCHIVO);
}

static void test_solo_saltos_de_linea(void) {
    Lexer l;
    lexer_iniciar(&l, "\n\n\n", "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_FIN_ARCHIVO);
    AFIRMAR(t.linea == 4); /* tres '\n' avanzan la línea de 1 a 4 */
}

/* ───── 2. Símbolos individuales ───── */

static void test_simbolos_simples(void) {
    TipoToken esperados[] = {
        TT_PARENT_IZQ, TT_PARENT_DER,
        TT_CORCH_IZQ, TT_CORCH_DER,
        TT_LLAVE_IZQ, TT_LLAVE_DER,
        TT_COMA, TT_PUNTO,
        TT_DOS_PUNTOS, TT_PUNTO_COMA,
        TT_AT, TT_TILDE_BIT,
    };
    verificar_secuencia("()[]{},.:;@~", esperados, 12);
}

/* ───── 3. Operadores con sus formas compuestas ───── */

static void test_aritmeticos_simples(void) {
    TipoToken esperados[] = {
        TT_MAS, TT_MENOS, TT_ASTERISCO, TT_BARRA, TT_PORCENTAJE,
    };
    verificar_secuencia("+ - * / %", esperados, 5);
}

static void test_aritmeticos_compuestos(void) {
    TipoToken esperados[] = {
        TT_DOBLE_BARRA, TT_DOBLE_ASTERISCO, TT_FLECHA,
    };
    verificar_secuencia("// ** ->", esperados, 3);
}

static void test_asignacion_y_compuestas(void) {
    TipoToken esperados[] = {
        TT_ASIGNAR,
        TT_ASIGNAR_MAS, TT_ASIGNAR_MENOS,
        TT_ASIGNAR_ASTERISCO, TT_ASIGNAR_BARRA,
        TT_ASIGNAR_DOBLE_BARRA, TT_ASIGNAR_PORCENTAJE,
        TT_ASIGNAR_DOBLE_ASTER,
    };
    verificar_secuencia("= += -= *= /= //= %= **=", esperados, 8);
}

static void test_comparaciones(void) {
    TipoToken esperados[] = {
        TT_IGUAL, TT_DISTINTO,
        TT_MENOR, TT_MENOR_IGUAL,
        TT_MAYOR, TT_MAYOR_IGUAL,
    };
    verificar_secuencia("== != < <= > >=", esperados, 6);
}

static void test_bitwise(void) {
    TipoToken esperados[] = {
        TT_AMPERSAND, TT_BARRA_VERT, TT_CIRCUNFLEJO,
        TT_DESPL_IZQ, TT_DESPL_DER,
    };
    verificar_secuencia("& | ^ << >>", esperados, 5);
}

/* ───── 4. Comentarios ───── */

static void test_comentario_solo(void) {
    Lexer l;
    lexer_iniciar(&l, "# esto es un comentario", "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_FIN_ARCHIVO);
}

static void test_comentario_entre_tokens(void) {
    TipoToken esperados[] = { TT_MAS, TT_MENOS };
    verificar_secuencia("+ # esto se ignora\n-", esperados, 2);
}

static void test_comentario_al_final(void) {
    TipoToken esperados[] = { TT_PARENT_IZQ, TT_PARENT_DER };
    verificar_secuencia("() # comentario sin salto", esperados, 2);
}

/* ───── 5. Tracking de línea y columna ───── */

static void test_linea_columna_basico(void) {
    Lexer l;
    lexer_iniciar(&l, "+\n  -", "<test>");

    Token mas = lexer_siguiente(&l);
    AFIRMAR_TIPO(mas, TT_MAS);
    AFIRMAR(mas.linea == 1);
    AFIRMAR(mas.columna == 1);

    Token menos = lexer_siguiente(&l);
    AFIRMAR_TIPO(menos, TT_MENOS);
    AFIRMAR(menos.linea == 2);
    AFIRMAR(menos.columna == 3); /* dos espacios delante */
}

static void test_columna_tras_comentario(void) {
    Lexer l;
    lexer_iniciar(&l, "# coment\n  *", "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_ASTERISCO);
    AFIRMAR(t.linea == 2);
    AFIRMAR(t.columna == 3);
}

/* ───── 6. Errores léxicos ───── */

static void test_caracter_no_reconocido(void) {
    Lexer l;
    lexer_iniciar(&l, "?", "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(t.linea == 1);
    AFIRMAR(t.columna == 1);
}

static void test_bang_solo_es_error(void) {
    /* '!' aislado no es operador en Cornamusa (solo '!=' lo es). */
    Lexer l;
    lexer_iniciar(&l, "!", "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_ERROR);
    /* El mensaje debería sugerir != */
    AFIRMAR(strstr(t.inicio, "!=") != NULL);
}

/* ───── 7. EOF idempotente ───── */

static void test_eof_repetido(void) {
    Lexer l;
    lexer_iniciar(&l, "+", "<test>");
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_MAS);
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_FIN_ARCHIVO);
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_FIN_ARCHIVO);
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_FIN_ARCHIVO);
}

/* ───── Lexema reportado correctamente ───── */

static void test_lexema_apunta_a_fuente(void) {
    /* `inicio` debe apuntar dentro del buffer original, no a una copia. */
    const char *fuente = "+ -";
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Token mas = lexer_siguiente(&l);
    AFIRMAR(mas.inicio == fuente);
    AFIRMAR(mas.longitud == 1);
    Token menos = lexer_siguiente(&l);
    AFIRMAR(menos.inicio == fuente + 2);
    AFIRMAR(menos.longitud == 1);
}

/* ───── Secuencia compleja realista ───── */

static void test_secuencia_realista(void) {
    /* `(a + b) <= 10` */
    TipoToken esperados[] = {
        TT_PARENT_IZQ,
        TT_ERROR,        /* 'a' es identificador, todavía no implementado */
        TT_MAS,
        TT_ERROR,        /* 'b' */
        TT_PARENT_DER,
        TT_MENOR_IGUAL,
        TT_ERROR,        /* '1' es número, todavía no implementado */
        TT_ERROR,        /* '0' */
    };
    verificar_secuencia("(a + b) <= 10", esperados, 8);
}

int main(void) {
    test_fuente_vacia();
    test_solo_whitespace();
    test_solo_saltos_de_linea();

    test_simbolos_simples();
    test_aritmeticos_simples();
    test_aritmeticos_compuestos();
    test_asignacion_y_compuestas();
    test_comparaciones();
    test_bitwise();

    test_comentario_solo();
    test_comentario_entre_tokens();
    test_comentario_al_final();

    test_linea_columna_basico();
    test_columna_tras_comentario();

    test_caracter_no_reconocido();
    test_bang_solo_es_error();

    test_eof_repetido();
    test_lexema_apunta_a_fuente();
    test_secuencia_realista();

    if (fallos == 0) {
        printf("test_lexer_simbolos: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stderr, "test_lexer_simbolos: %d fallo(s)\n", fallos);
    return 1;
}
