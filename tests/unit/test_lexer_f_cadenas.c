/*
 * Tests del lexer — Fase 2 Sesión 4: f-strings y triple-quoted.
 *
 * Cobertura:
 *   - f-strings simples sin interpolación.
 *   - f-strings con interpolación simple, múltiple y anidada.
 *   - f-strings con `{{` y `}}` literales.
 *   - Triple-quoted multilínea con conteo de líneas correcto.
 *   - Combinaciones f + triple.
 *   - Errores: f-string sin cerrar, interpolación sin cerrar, `}` desnudo.
 *   - El lexema reportado incluye prefijo `f` y comillas.
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

/* ───── f-strings simples ───── */

static void test_f_cadena_vacia(void) {
    Token t = primer_token("f\"\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
    AFIRMAR(t.longitud == 3); /* f"" */
}

static void test_f_cadena_sin_interpolacion(void) {
    Token t = primer_token("f\"hola\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
    AFIRMAR(t.longitud == 7); /* f"hola" */
}

static void test_f_cadena_F_mayuscula(void) {
    /* F mayúscula también disparga f-string. */
    Token t = primer_token("F\"hola\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

static void test_f_cadena_comilla_simple(void) {
    Token t = primer_token("f'hola'");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

/* ───── f-strings con interpolación ───── */

static void test_f_cadena_interp_simple(void) {
    Token t = primer_token("f\"hola {nombre}\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

static void test_f_cadena_interp_multiple(void) {
    Token t = primer_token("f\"{a}{b}{c}\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

static void test_f_cadena_interp_con_expresion(void) {
    /* La expresión dentro de {} puede ser cualquier cosa balanceada. */
    Token t = primer_token("f\"resultado: {x + y * 2}\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

static void test_f_cadena_interp_anidada(void) {
    /* {f(g(x))} — paréntesis anidados dentro de la interpolación. */
    Token t = primer_token("f\"{f(g(x))}\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

static void test_f_cadena_interp_con_diccionario_anidado(void) {
    /* {dict[k]} y similares — corchetes y llaves balanceadas. */
    Token t = primer_token("f\"{datos['nombre']}\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

/* ───── Llaves literales {{ y }} ───── */

static void test_f_cadena_llave_abierta_literal(void) {
    /* {{ es la llave literal '{'. */
    Token t = primer_token("f\"{{ no es interp }}\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

static void test_f_cadena_mezcla_literal_e_interp(void) {
    Token t = primer_token("f\"{{ + {x} + }}\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

/* ───── Triple-quoted simples (no f) ───── */

static void test_triple_doble_basica(void) {
    Token t = primer_token("\"\"\"hola\"\"\"");
    AFIRMAR_TIPO(t, TT_CADENA);
    AFIRMAR(t.longitud == 10); /* """hola""" */
}

static void test_triple_simple_basica(void) {
    Token t = primer_token("'''hola'''");
    AFIRMAR_TIPO(t, TT_CADENA);
    AFIRMAR(t.longitud == 10);
}

static void test_triple_vacia(void) {
    Token t = primer_token("\"\"\"\"\"\"");
    AFIRMAR_TIPO(t, TT_CADENA);
    AFIRMAR(t.longitud == 6);
}

static void test_triple_con_comilla_interna(void) {
    /* Una "comilla" sola dentro no rompe la triple. */
    Token t = primer_token("\"\"\"di \"hola\" tres\"\"\"");
    AFIRMAR_TIPO(t, TT_CADENA);
}

static void test_triple_con_dos_comillas_seguidas(void) {
    /* "" dentro está bien si no hay tercera. */
    Token t = primer_token("\"\"\"par \"\" no cierra\"\"\"");
    AFIRMAR_TIPO(t, TT_CADENA);
}

/* ───── Triple multilínea + tracking de línea ───── */

static void test_triple_multilinea_avanza_linea(void) {
    /* Tras """linea1\nlinea2\nlinea3""" el lexer debe estar en línea 3.
       Verificamos que el siguiente token está en la línea correcta. */
    const char *fuente = "\"\"\"linea1\nlinea2\nlinea3\"\"\"\nx";
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");

    Token cad = lexer_siguiente(&l);
    AFIRMAR_TIPO(cad, TT_CADENA);

    Token x = lexer_siguiente(&l);
    AFIRMAR_TIPO(x, TT_IDENT);
    AFIRMAR(x.linea == 4); /* tres '\n' avanzaron de 1 a 4 */
}

static void test_triple_con_lineas_intermedias(void) {
    /* Múltiples líneas vacías dentro de la triple. */
    Token t = primer_token("\"\"\"\n\n\nfinal\"\"\"");
    AFIRMAR_TIPO(t, TT_CADENA);
}

/* ───── f triple-quoted ───── */

static void test_f_triple_basica(void) {
    Token t = primer_token("f\"\"\"hola\"\"\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

static void test_f_triple_con_interp(void) {
    Token t = primer_token("f\"\"\"valor = {x}\"\"\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

static void test_f_triple_multilinea(void) {
    Token t = primer_token("f\"\"\"linea1\nlinea2 {x}\nlinea3\"\"\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

/* ───── Escapes en cadenas y f-cadenas ───── */

static void test_f_cadena_con_escape_n(void) {
    Token t = primer_token("f\"linea\\nfin\"");
    AFIRMAR_TIPO(t, TT_F_CADENA);
}

static void test_triple_con_escape(void) {
    Token t = primer_token("\"\"\"con \\\"escape\\\"\"\"\"");
    AFIRMAR_TIPO(t, TT_CADENA);
}

/* ───── Errores ───── */

static void test_f_cadena_sin_cerrar(void) {
    Token t = primer_token("f\"sin cerrar");
    AFIRMAR_TIPO(t, TT_ERROR);
}

static void test_f_cadena_interp_sin_cerrar_eof(void) {
    Token t = primer_token("f\"hola {sin cerrar");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.inicio, "interpolación") != NULL);
}

static void test_f_cadena_interp_sin_cerrar_linea(void) {
    /* Interpolación que cruza newline en f-string simple = error. */
    Token t = primer_token("f\"hola {x\ny}\"");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.inicio, "interpolación") != NULL);
}

static void test_f_cadena_llave_cierre_desnuda(void) {
    /* '}' fuera de interpolación y sin '}}' = error. */
    Token t = primer_token("f\"hola }\"");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.inicio, "}") != NULL);
}

static void test_triple_sin_cerrar(void) {
    Token t = primer_token("\"\"\"abierta sin cerrar");
    AFIRMAR_TIPO(t, TT_ERROR);
    AFIRMAR(strstr(t.inicio, "triple") != NULL);
}

static void test_triple_dos_comillas_finales_no_cierran(void) {
    /* "" al final NO es cierre de triple — necesita tres. */
    Token t = primer_token("\"\"\"texto\"\"");
    AFIRMAR_TIPO(t, TT_ERROR);
}

/* ───── La 'f' sin comilla es identificador, no f-string ───── */

static void test_f_aislada_es_ident(void) {
    /* `f x` → 'f' es identificador, no f-string. */
    Lexer l;
    lexer_iniciar(&l, "f x", "<test>");
    Token f = lexer_siguiente(&l);
    AFIRMAR_TIPO(f, TT_IDENT);
    AFIRMAR(f.longitud == 1);
    Token x = lexer_siguiente(&l);
    AFIRMAR_TIPO(x, TT_IDENT);
}

static void test_f_con_espacio_y_cadena_es_ident_y_cadena(void) {
    /* `f "hola"` (con espacio) → 'f' identificador + "hola" cadena. */
    Lexer l;
    lexer_iniciar(&l, "f \"hola\"", "<test>");
    Token f = lexer_siguiente(&l);
    AFIRMAR_TIPO(f, TT_IDENT);
    Token cad = lexer_siguiente(&l);
    AFIRMAR_TIPO(cad, TT_CADENA);
}

static void test_f_ident_largo_no_es_f_cadena(void) {
    /* `factor` es identificador completo, no `f` + `actor`. */
    Token t = primer_token("factor");
    AFIRMAR_TIPO(t, TT_IDENT);
    AFIRMAR(t.longitud == 6);
}

/* ───── Lexema reportado correctamente ───── */

static void test_lexema_f_cadena_incluye_prefijo(void) {
    const char *fuente = "  f\"hola\"  ";
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Token t = lexer_siguiente(&l);
    AFIRMAR_TIPO(t, TT_F_CADENA);
    AFIRMAR(t.inicio == fuente + 2);
    AFIRMAR(t.longitud == 7); /* f"hola" */
    AFIRMAR(t.inicio[0] == 'f');
    AFIRMAR(t.inicio[1] == '"');
    AFIRMAR(t.inicio[6] == '"');
}

/* ───── Secuencias realistas (basadas en examples/) ───── */

static void test_ejemplo_fibonacci(void) {
    /* Línea de ejemplo: imprimir(f"fib({i}) = {fib_iterativo(i)}") */
    TipoToken esperados[] = {
        TT_IDENT,        /* imprimir */
        TT_PARENT_IZQ,
        TT_F_CADENA,     /* f"fib({i}) = {fib_iterativo(i)}" */
        TT_PARENT_DER,
    };
    Lexer l;
    lexer_iniciar(&l,
        "imprimir(f\"fib({i}) = {fib_iterativo(i)}\")", "<test>");
    for (int i = 0; i < 4; i++) {
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

static void test_ejemplo_diccionarios(void) {
    /* Línea: imprimir(f"{palabra}: {conteo}") */
    Lexer l;
    lexer_iniciar(&l,
        "imprimir(f\"{palabra}: {conteo}\")", "<test>");
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_IDENT);
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_PARENT_IZQ);
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_F_CADENA);
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_PARENT_DER);
    AFIRMAR_TIPO(lexer_siguiente(&l), TT_FIN_ARCHIVO);
}

static void test_docstring_estilo(void) {
    /* Multilínea estilo docstring sigue siendo TT_CADENA. */
    const char *fuente =
        "\"\"\"\nDocumentación de una función.\n\n"
        "Hace cosas útiles.\n\"\"\"\nfuncion";
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");

    Token doc = lexer_siguiente(&l);
    AFIRMAR_TIPO(doc, TT_CADENA);

    Token sig = lexer_siguiente(&l);
    AFIRMAR_TIPO(sig, TT_FUNCION);
    AFIRMAR(sig.linea == 6); /* tras docstring de 5 líneas */
}

int main(void) {
    /* f-strings simples */
    test_f_cadena_vacia();
    test_f_cadena_sin_interpolacion();
    test_f_cadena_F_mayuscula();
    test_f_cadena_comilla_simple();

    /* f-strings con interpolación */
    test_f_cadena_interp_simple();
    test_f_cadena_interp_multiple();
    test_f_cadena_interp_con_expresion();
    test_f_cadena_interp_anidada();
    test_f_cadena_interp_con_diccionario_anidado();

    /* Llaves literales */
    test_f_cadena_llave_abierta_literal();
    test_f_cadena_mezcla_literal_e_interp();

    /* Triple-quoted (sin f) */
    test_triple_doble_basica();
    test_triple_simple_basica();
    test_triple_vacia();
    test_triple_con_comilla_interna();
    test_triple_con_dos_comillas_seguidas();
    test_triple_multilinea_avanza_linea();
    test_triple_con_lineas_intermedias();

    /* f triple */
    test_f_triple_basica();
    test_f_triple_con_interp();
    test_f_triple_multilinea();

    /* Escapes */
    test_f_cadena_con_escape_n();
    test_triple_con_escape();

    /* Errores */
    test_f_cadena_sin_cerrar();
    test_f_cadena_interp_sin_cerrar_eof();
    test_f_cadena_interp_sin_cerrar_linea();
    test_f_cadena_llave_cierre_desnuda();
    test_triple_sin_cerrar();
    test_triple_dos_comillas_finales_no_cierran();

    /* f sin comilla NO es f-string */
    test_f_aislada_es_ident();
    test_f_con_espacio_y_cadena_es_ident_y_cadena();
    test_f_ident_largo_no_es_f_cadena();

    /* Lexema y secuencias realistas */
    test_lexema_f_cadena_incluye_prefijo();
    test_ejemplo_fibonacci();
    test_ejemplo_diccionarios();
    test_docstring_estilo();

    if (fallos == 0) {
        printf("test_lexer_f_cadenas: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stderr, "test_lexer_f_cadenas: %d fallo(s)\n", fallos);
    return 1;
}
