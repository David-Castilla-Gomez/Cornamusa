/*
 * Tests del lexer — Fase 2 Sesión 2: literales numéricos y cadenas.
 *
 * Cobertura:
 *   - Enteros decimales (con y sin '_').
 *   - Hexadecimal, octal, binario.
 *   - Decimales con punto y notación científica.
 *   - Cadenas con comillas dobles y simples.
 *   - Escape sequences básicos.
 *   - Errores: literal mal formado, cadena sin cerrar, escape inválido.
 *   - Lexema reportado correctamente (incluye comillas, etc.).
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

/* Tokeniza `fuente` y devuelve el primer token. Útil para verificar
   que un literal aislado se reconoce con el tipo correcto. */
static Token primer_token(const char *fuente) {
    static Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    return lexer_siguiente(&l);
}

/* ───── Enteros decimales ───── */

static void test_entero_cero(void) {
    Token t = primer_token("0");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 1);
}

static void test_entero_simple(void) {
    Token t = primer_token("42");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 2);
}

static void test_entero_largo(void) {
    Token t = primer_token("123456789");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 9);
}

static void test_entero_con_underscore(void) {
    Token t = primer_token("1_000_000");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 9);
}

static void test_entero_con_underscore_irregular(void) {
    /* Posiciones libres: 1_00_00 también válido (estilo indio, etc.) */
    Token t = primer_token("1_00_00");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 7);
}

/* ───── Errores en enteros decimales ───── */

static void test_entero_underscore_consecutivo(void) {
    Token t = primer_token("1__2");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.mensaje, "consecutiv") != NULL);
}

static void test_entero_underscore_final(void) {
    Token t = primer_token("12_");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.mensaje, "terminar") != NULL);
}

/* ───── Bases especiales: hexadecimal ───── */

static void test_hex_minusculas(void) {
    Token t = primer_token("0xff");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 4);
}

static void test_hex_mayusculas(void) {
    Token t = primer_token("0XCAFE");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 6);
}

static void test_hex_mixto_con_underscore(void) {
    Token t = primer_token("0xCa_fE");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 7);
}

static void test_hex_underscore_inicial_permitido(void) {
    /* 0x_ff es válido: el _ inmediatamente tras prefijo de base se
       acepta para permitir agrupación visual. */
    Token t = primer_token("0x_ff");
    AFIRMAR_TIPO(t, TT_ENTERO);
}

static void test_hex_vacio_es_error(void) {
    Token t = primer_token("0x");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.mensaje, "hexadecimal") != NULL);
}

/* ───── Octal ───── */

static void test_octal(void) {
    Token t = primer_token("0o755");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 5);
}

static void test_octal_vacio_es_error(void) {
    Token t = primer_token("0o");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.mensaje, "octal") != NULL);
}

/* ───── Binario ───── */

static void test_binario(void) {
    Token t = primer_token("0b1010");
    AFIRMAR_TIPO(t, TT_ENTERO);
    AFIRMAR(t.longitud == 6);
}

static void test_binario_con_underscore(void) {
    Token t = primer_token("0b1010_1010");
    AFIRMAR_TIPO(t, TT_ENTERO);
}

static void test_binario_vacio_es_error(void) {
    Token t = primer_token("0b");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.mensaje, "binario") != NULL);
}

/* ───── Decimales ───── */

static void test_decimal_simple(void) {
    Token t = primer_token("3.14");
    AFIRMAR_TIPO(t, TT_DECIMAL);
    AFIRMAR(t.longitud == 4);
}

static void test_decimal_con_cero_inicial(void) {
    Token t = primer_token("0.5");
    AFIRMAR_TIPO(t, TT_DECIMAL);
    AFIRMAR(t.longitud == 3);
}

static void test_decimal_largo(void) {
    Token t = primer_token("3.141592653589793");
    AFIRMAR_TIPO(t, TT_DECIMAL);
}

static void test_decimal_con_underscore_en_fraccionaria(void) {
    Token t = primer_token("0.000_001");
    AFIRMAR_TIPO(t, TT_DECIMAL);
}

static void test_punto_sin_digito_no_es_decimal(void) {
    /* '1.' no es decimal: el '.' queda como TT_PUNTO. Así obj.metodo
       y enteros siguen siendo unambiguous. */
    Lexer l;
    lexer_iniciar(&l, "1.x", "<test>");
    Token a = lexer_siguiente(&l);
    AFIRMAR_TIPO(a, TT_ENTERO);
    AFIRMAR(a.longitud == 1);
    Token b = lexer_siguiente(&l);
    AFIRMAR_TIPO(b, TT_PUNTO);
}

/* ───── Notación científica ───── */

static void test_cientifica_basica(void) {
    Token t = primer_token("1e10");
    AFIRMAR_TIPO(t, TT_DECIMAL);
}

static void test_cientifica_con_signo_positivo(void) {
    Token t = primer_token("1.5e+3");
    AFIRMAR_TIPO(t, TT_DECIMAL);
}

static void test_cientifica_con_signo_negativo(void) {
    Token t = primer_token("2.5E-10");
    AFIRMAR_TIPO(t, TT_DECIMAL);
}

static void test_cientifica_mayuscula(void) {
    Token t = primer_token("3E5");
    AFIRMAR_TIPO(t, TT_DECIMAL);
}

static void test_cientifica_exponente_vacio_es_error(void) {
    Token t = primer_token("1e");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.mensaje, "exponente") != NULL);
}

static void test_cientifica_exponente_solo_signo_es_error(void) {
    Token t = primer_token("1e+");
    AFIRMAR_TIPO(t, TT_ERROR);
}

/* ───── Cadenas: comillas dobles ───── */

static void test_cadena_doble_vacia(void) {
    Token t = primer_token("\"\"");
    AFIRMAR_TIPO(t, TT_CADENA);
    AFIRMAR(t.longitud == 2); /* incluye comillas */
}

static void test_cadena_doble_simple(void) {
    Token t = primer_token("\"hola\"");
    AFIRMAR_TIPO(t, TT_CADENA);
    AFIRMAR(t.longitud == 6); /* "hola" incluyendo comillas */
}

static void test_cadena_doble_con_espacios(void) {
    Token t = primer_token("\"hola mundo\"");
    AFIRMAR_TIPO(t, TT_CADENA);
}

/* ───── Cadenas: comillas simples ───── */

static void test_cadena_simple_vacia(void) {
    Token t = primer_token("''");
    AFIRMAR_TIPO(t, TT_CADENA);
    AFIRMAR(t.longitud == 2);
}

static void test_cadena_simple_con_texto(void) {
    Token t = primer_token("'hola'");
    AFIRMAR_TIPO(t, TT_CADENA);
}

/* ───── Escapes ───── */

static void test_cadena_con_escape_n(void) {
    Token t = primer_token("\"linea1\\nlinea2\"");
    AFIRMAR_TIPO(t, TT_CADENA);
}

static void test_cadena_con_comilla_escapada(void) {
    Token t = primer_token("\"di \\\"hola\\\"\"");
    AFIRMAR_TIPO(t, TT_CADENA);
}

static void test_cadena_con_backslash_escapado(void) {
    Token t = primer_token("\"C:\\\\ruta\"");
    AFIRMAR_TIPO(t, TT_CADENA);
}

static void test_escape_invalido_es_error(void) {
    Token t = primer_token("\"\\z\"");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.mensaje, "escape") != NULL);
}

/* ───── Cadenas con UTF-8 (bytes opacos) ───── */

static void test_cadena_con_utf8(void) {
    /* "niño" tiene un byte de continuación UTF-8 en 'ñ'. El lexer
       no lo decodifica todavía pero tampoco se debe romper. */
    Token t = primer_token("\"niño\"");
    AFIRMAR_TIPO(t, TT_CADENA);
}

/* ───── Errores en cadenas ───── */

static void test_cadena_sin_cerrar_eof(void) {
    Token t = primer_token("\"sin cerrar");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.mensaje, "fin de archivo") != NULL);
}

static void test_cadena_con_salto_de_linea_es_error(void) {
    Token t = primer_token("\"sin\ncerrar\"");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.mensaje, "fin de línea") != NULL);
}

/* ───── Secuencias mixtas realistas ───── */

static void test_asignacion_decimal(void) {
    /* `pi = 3.14` (sin TT_IDENT todavía, pero verifico la parte numérica) */
    Lexer l;
    lexer_iniciar(&l, "3.14 + 42", "<test>");

    Token a = lexer_siguiente(&l);
    AFIRMAR_TIPO(a, TT_DECIMAL);
    AFIRMAR(a.longitud == 4);

    Token b = lexer_siguiente(&l);
    AFIRMAR_TIPO(b, TT_MAS);

    Token c = lexer_siguiente(&l);
    AFIRMAR_TIPO(c, TT_ENTERO);
    AFIRMAR(c.longitud == 2);

    AFIRMAR_TIPO(lexer_siguiente(&l), TT_FIN_ARCHIVO);
}

static void test_lista_de_numeros(void) {
    /* `[1, 2, 3.14, 0xff]` */
    TipoToken esperados[] = {
        TT_CORCH_IZQ,
        TT_ENTERO,
        TT_COMA,
        TT_ENTERO,
        TT_COMA,
        TT_DECIMAL,
        TT_COMA,
        TT_ENTERO,
        TT_CORCH_DER,
    };
    Lexer l;
    lexer_iniciar(&l, "[1, 2, 3.14, 0xff]", "<test>");
    for (int i = 0; i < 9; i++) {
        Token t = lexer_siguiente(&l);
        AFIRMAR(t.tipo == esperados[i]);
    }
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_FIN_ARCHIVO);
}

static void test_lexema_de_cadena_apunta_a_fuente(void) {
    /* El lexema de una cadena debe incluir las comillas y apuntar al
       inicio del literal en el buffer original. */
    const char *fuente = "  \"hola\"  ";
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_CADENA);
    AFIRMAR(t.inicio == fuente + 2);
    AFIRMAR(t.longitud == 6);
    /* Verifica que las comillas están incluidas. */
    AFIRMAR(t.inicio[0] == '"');
    AFIRMAR(t.inicio[5] == '"');
}

int main(void) {
    /* Enteros */
    test_entero_cero();
    test_entero_simple();
    test_entero_largo();
    test_entero_con_underscore();
    test_entero_con_underscore_irregular();
    test_entero_underscore_consecutivo();
    test_entero_underscore_final();

    /* Bases */
    test_hex_minusculas();
    test_hex_mayusculas();
    test_hex_mixto_con_underscore();
    test_hex_underscore_inicial_permitido();
    test_hex_vacio_es_error();
    test_octal();
    test_octal_vacio_es_error();
    test_binario();
    test_binario_con_underscore();
    test_binario_vacio_es_error();

    /* Decimales */
    test_decimal_simple();
    test_decimal_con_cero_inicial();
    test_decimal_largo();
    test_decimal_con_underscore_en_fraccionaria();
    test_punto_sin_digito_no_es_decimal();

    /* Científica */
    test_cientifica_basica();
    test_cientifica_con_signo_positivo();
    test_cientifica_con_signo_negativo();
    test_cientifica_mayuscula();
    test_cientifica_exponente_vacio_es_error();
    test_cientifica_exponente_solo_signo_es_error();

    /* Cadenas */
    test_cadena_doble_vacia();
    test_cadena_doble_simple();
    test_cadena_doble_con_espacios();
    test_cadena_simple_vacia();
    test_cadena_simple_con_texto();

    /* Escapes */
    test_cadena_con_escape_n();
    test_cadena_con_comilla_escapada();
    test_cadena_con_backslash_escapado();
    test_escape_invalido_es_error();
    test_cadena_con_utf8();

    /* Errores */
    test_cadena_sin_cerrar_eof();
    test_cadena_con_salto_de_linea_es_error();

    /* Mixtas */
    test_asignacion_decimal();
    test_lista_de_numeros();
    test_lexema_de_cadena_apunta_a_fuente();

    if (fallos == 0) {
        printf("test_lexer_numeros_cadenas: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stderr, "test_lexer_numeros_cadenas: %d fallo(s)\n", fallos);
    return 1;
}
