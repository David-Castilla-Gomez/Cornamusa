/*
 * Tests del lexer — Fase 2 Sesión 3: identificadores y keywords.
 *
 * Cobertura:
 *   - Identificadores ASCII básicos.
 *   - Identificadores con dígitos al final / en medio.
 *   - Identificadores con `_` y `$`.
 *   - Cada keyword castellana se reconoce y devuelve el TipoToken correcto.
 *   - Identificadores con tildes (`niño`, `año`, `función_principal`).
 *   - Identificadores que CONTIENEN una keyword pero no son la keyword
 *     (`silencio` no es `si`, `funcionario` no es `funcion`).
 *   - Las keywords son case-sensitive (`Si`, `FUNCION` son identificadores).
 *   - Bytes UTF-8 inválidos producen error.
 *   - El lexema reportado es correcto en bytes.
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

static Token primer_token(const char *fuente) {
    static Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    return lexer_siguiente(&l);
}

/* ───── Identificadores ASCII básicos ───── */

static void test_ident_letra_simple(void) {
    Token t = primer_token("x");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 1);
}

static void test_ident_palabra(void) {
    Token t = primer_token("calcular");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 8);
}

static void test_ident_serpiente_minuscula(void) {
    Token t = primer_token("calcular_total_anual");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 20);
}

static void test_ident_camel_case(void) {
    Token t = primer_token("ListaEnlazada");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 13);
}

static void test_ident_con_digitos(void) {
    Token t = primer_token("variable42");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 10);
}

static void test_ident_inicio_subrayado(void) {
    Token t = primer_token("_privado");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 8);
}

static void test_ident_solo_subrayado(void) {
    Token t = primer_token("_");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 1);
}

static void test_ident_con_dolar(void) {
    Token t = primer_token("$variable");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 9);
}

static void test_ident_dunder(void) {
    Token t = primer_token("__iniciar__");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 11);
}

/* ───── Identificadores con Unicode ───── */

static void test_ident_con_enie(void) {
    /* "niño" en UTF-8: 4 letras pero 5 bytes (ñ = 2 bytes). */
    const char *fuente = "niño";
    Token t = primer_token(fuente);
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 5);
}

static void test_ident_con_tilde(void) {
    /* "función" — 7 letras, 8 bytes (ó = 2 bytes). */
    const char *fuente = "función";
    Token t = primer_token(fuente);
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 8);
}

static void test_ident_mixto_ascii_y_unicode(void) {
    /* "contar_niños" — combina ASCII y Unicode dentro del ident. */
    const char *fuente = "contar_niños";
    Token t = primer_token(fuente);
    AFIRMAR_TIPO(t, TT_IDENT);
    /* contar_ni = 9 bytes ASCII; ñ = 2 bytes; os = 2 bytes ASCII = 13 bytes. */
    AFIRMAR(t.longitud == 13);
}

static void test_ident_solo_unicode(void) {
    Token t = primer_token("año");
    AFIRMAR_TIPO(t, TT_IDENT);
    /* a = 1 byte; ñ = 2; o = 1 → 4 bytes. */
    AFIRMAR(t.longitud == 4);
}

/* ───── Cada keyword se reconoce ───── */

static void verificar_keyword(const char *texto, TipoToken esperado) {
    Token t = primer_token(texto);
    if (t.tipo != esperado) {
        fprintf(stderr,
            "FALLO: '%s' esperaba %s, obtenido %s\n",
            texto, tipo_token_nombre(esperado), tipo_token_nombre(t.tipo));
        fallos++;
    } else if ((int)strlen(texto) != t.longitud) {
        fprintf(stderr,
            "FALLO: '%s' longitud esperada %d, obtenida %d\n",
            texto, (int)strlen(texto), t.longitud);
        fallos++;
    }
}

static void test_keywords_control_flujo(void) {
    verificar_keyword("si",        TT_SI);
    verificar_keyword("sino",      TT_SINO);
    verificar_keyword("mientras",  TT_MIENTRAS);
    verificar_keyword("para",      TT_PARA);
    verificar_keyword("en",        TT_EN);
    verificar_keyword("romper",    TT_ROMPER);
    verificar_keyword("continuar", TT_CONTINUAR);
    verificar_keyword("retornar",  TT_RETORNAR);
    verificar_keyword("pasar",     TT_PASAR);
    verificar_keyword("fin",       TT_FIN);
}

static void test_keywords_funciones_clases(void) {
    verificar_keyword("funcion",   TT_FUNCION);
    verificar_keyword("lambda",    TT_LAMBDA);
    verificar_keyword("clase",     TT_CLASE);
    verificar_keyword("extiende",  TT_EXTIENDE);
    verificar_keyword("super",     TT_SUPER);
    verificar_keyword("importar",  TT_IMPORTAR);
    verificar_keyword("desde",     TT_DESDE);
    verificar_keyword("como",      TT_COMO);
    verificar_keyword("global",    TT_GLOBAL);
    verificar_keyword("nolocal",   TT_NOLOCAL);
}

static void test_keywords_excepciones(void) {
    verificar_keyword("intentar",   TT_INTENTAR);
    verificar_keyword("atrapar",    TT_ATRAPAR);
    verificar_keyword("finalmente", TT_FINALMENTE);
    verificar_keyword("lanzar",     TT_LANZAR);
}

static void test_keywords_logicas(void) {
    verificar_keyword("y",  TT_Y);
    verificar_keyword("o",  TT_O);
    verificar_keyword("no", TT_NO);
    verificar_keyword("es", TT_ES);
}

static void test_keywords_literales(void) {
    verificar_keyword("verdadero", TT_VERDADERO);
    verificar_keyword("falso",     TT_FALSO);
    verificar_keyword("nulo",      TT_NULO);
}

static void test_keywords_reservadas(void) {
    verificar_keyword("producir",  TT_PRODUCIR);
    verificar_keyword("asincrono", TT_ASINCRONO);
    verificar_keyword("esperar",   TT_ESPERAR);
    verificar_keyword("con",       TT_CON);
    verificar_keyword("borrar",    TT_BORRAR);
    verificar_keyword("coincidir", TT_COINCIDIR);
}

/* ───── Casos delicados de keyword vs identificador ───── */

static void test_palabra_que_empieza_con_keyword(void) {
    /* 'silencio' no es 'si' aunque empieza igual. */
    Token t = primer_token("silencio");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 8);
}

static void test_palabra_que_contiene_keyword(void) {
    /* 'funcionario' no es 'funcion'. */
    Token t = primer_token("funcionario");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 11);
}

static void test_keyword_con_subrayado_es_ident(void) {
    /* 'si_' es identificador, no keyword 'si'. */
    Token t = primer_token("si_");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 3);
}

static void test_keyword_case_sensitive(void) {
    /* 'Si' y 'FUNCION' son identificadores; las keywords son minúscula
       estricta. */
    AFIRMAR_TIPO(primer_token("Si"),      TT_IDENT);
    AFIRMAR_TIPO(primer_token("FUNCION"), TT_IDENT);
    AFIRMAR_TIPO(primer_token("CLASE"),   TT_IDENT);
}

static void test_keyword_con_tilde_es_ident(void) {
    /* 'función' (con tilde) es IDENT, no keyword (decisión B4). */
    Token t = primer_token("función");
    AFIRMAR_TIPO(t, TT_IDENT);
}

/* ───── Errores ───── */

static void test_byte_utf8_invalido(void) {
    /* 0xFF aislado no es comienzo válido de UTF-8. */
    char fuente[] = { (char)0xFF, '\0' };
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.inicio, "UTF-8") != NULL);
}

/* ───── Secuencias realistas ───── */

static void test_secuencia_funcion_completa(void) {
    /* `funcion saludar(nombre):` */
    TipoToken esperados[] = {
        TT_FUNCION,
        TT_IDENT,        /* saludar */
        TT_PARENT_IZQ,
        TT_IDENT,        /* nombre */
        TT_PARENT_DER,
        TT_DOS_PUNTOS,
    };
    Lexer l;
    lexer_iniciar(&l, "funcion saludar(nombre):", "<test>");
    for (int i = 0; i < 6; i++) {
        Token t = lexer_siguiente(&l);
        if (t.tipo != esperados[i]) {
            fprintf(stderr, "  pos %d: esperaba %s, obtenido %s\n",
                i, tipo_token_nombre(esperados[i]),
                tipo_token_nombre(t.tipo));
            fallos++;
        }
    }
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_FIN_ARCHIVO);
}

static void test_secuencia_si_sino(void) {
    /* `si x > 0: imprimir(x) sino: pasar fin si` */
    TipoToken esperados[] = {
        TT_SI, TT_IDENT, TT_MAYOR, TT_ENTERO, TT_DOS_PUNTOS,
        TT_IDENT,  /* imprimir */
        TT_PARENT_IZQ, TT_IDENT, TT_PARENT_DER,
        TT_SINO, TT_DOS_PUNTOS,
        TT_PASAR,
        TT_FIN, TT_SI,
    };
    Lexer l;
    lexer_iniciar(&l,
        "si x > 0: imprimir(x) sino: pasar fin si", "<test>");
    for (int i = 0; i < 14; i++) {
        Token t = lexer_siguiente(&l);
        if (t.tipo != esperados[i]) {
            fprintf(stderr, "  pos %d: esperaba %s, obtenido %s\n",
                i, tipo_token_nombre(esperados[i]),
                tipo_token_nombre(t.tipo));
            fallos++;
        }
    }
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_FIN_ARCHIVO);
}

static void test_secuencia_clase_con_metodo(void) {
    /* `clase Persona: funcion __iniciar__(yo, nombre): yo.nombre = nombre fin funcion fin clase` */
    TipoToken esperados[] = {
        TT_CLASE, TT_IDENT,            /* clase Persona */
        TT_DOS_PUNTOS,
        TT_FUNCION, TT_IDENT,           /* funcion __iniciar__ */
        TT_PARENT_IZQ,
        TT_IDENT, TT_COMA, TT_IDENT,    /* yo, nombre */
        TT_PARENT_DER,
        TT_DOS_PUNTOS,
        TT_IDENT, TT_PUNTO, TT_IDENT,   /* yo.nombre */
        TT_ASIGNAR,
        TT_IDENT,                        /* nombre */
        TT_FIN, TT_FUNCION,
        TT_FIN, TT_CLASE,
    };
    Lexer l;
    lexer_iniciar(&l,
        "clase Persona: funcion __iniciar__(yo, nombre): yo.nombre = nombre fin funcion fin clase",
        "<test>");
    int n = (int)(sizeof(esperados) / sizeof(esperados[0]));
    for (int i = 0; i < n; i++) {
        Token t = lexer_siguiente(&l);
        if (t.tipo != esperados[i]) {
            fprintf(stderr, "  pos %d: esperaba %s, obtenido %s\n",
                i, tipo_token_nombre(esperados[i]),
                tipo_token_nombre(t.tipo));
            fallos++;
        }
    }
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_FIN_ARCHIVO);
}

static void test_lexema_apunta_a_fuente_ident(void) {
    const char *fuente = "  contar_niños  ";
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.inicio == fuente + 2);
    AFIRMAR(t.longitud == 13);
    /* Los bytes de ñ deberían estar dentro del lexema. */
    AFIRMAR((unsigned char)t.inicio[9] == 0xC3); /* primer byte de ñ */
    AFIRMAR((unsigned char)t.inicio[10] == 0xB1); /* segundo byte */
}

int main(void) {
    /* ASCII básicos */
    test_ident_letra_simple();
    test_ident_palabra();
    test_ident_serpiente_minuscula();
    test_ident_camel_case();
    test_ident_con_digitos();
    test_ident_inicio_subrayado();
    test_ident_solo_subrayado();
    test_ident_con_dolar();
    test_ident_dunder();

    /* Unicode */
    test_ident_con_enie();
    test_ident_con_tilde();
    test_ident_mixto_ascii_y_unicode();
    test_ident_solo_unicode();

    /* Keywords */
    test_keywords_control_flujo();
    test_keywords_funciones_clases();
    test_keywords_excepciones();
    test_keywords_logicas();
    test_keywords_literales();
    test_keywords_reservadas();

    /* Casos delicados */
    test_palabra_que_empieza_con_keyword();
    test_palabra_que_contiene_keyword();
    test_keyword_con_subrayado_es_ident();
    test_keyword_case_sensitive();
    test_keyword_con_tilde_es_ident();

    /* Errores */
    test_byte_utf8_invalido();

    /* Secuencias realistas */
    test_secuencia_funcion_completa();
    test_secuencia_si_sino();
    test_secuencia_clase_con_metodo();
    test_lexema_apunta_a_fuente_ident();

    if (fallos == 0) {
        printf("test_lexer_identificadores: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stderr, "test_lexer_identificadores: %d fallo(s)\n", fallos);
    return 1;
}
