/*
 * Tests del contexto de línea-fuente en error_imprimir (v1.37).
 *
 * Verifica que `error_imprimir` muestra la línea de código fuente con
 * un caret `^`, incluyendo el caso `columna_inicio == 0` (errores de
 * runtime de la VM, que no rastrean columnas precisas) donde el caret
 * apunta al primer carácter no-blanco.
 */

#include <stdio.h>
#include <string.h>

#include "errores.h"

static int fallos = 0;

/* Captura la salida de error_imprimir a un buffer usando tmpfile. */
static void capturar_error(const Error *e, const char *fuente,
                             int longitud_span, char *out, size_t out_cap) {
    FILE *tmp = tmpfile();
    if (!tmp) {
        snprintf(out, out_cap, "<tmpfile fallo>");
        return;
    }
    error_imprimir(e, fuente, longitud_span, tmp);
    fflush(tmp);
    rewind(tmp);
    size_t n = fread(out, 1, out_cap - 1, tmp);
    out[n] = '\0';
    fclose(tmp);
}

static void verificar_contiene(const char *desc, const char *texto,
                                 const char *esperado) {
    if (strstr(texto, esperado) == NULL) {
        fprintf(stderr, "FALLO [%s]: salida no contiene '%s'\n  salida: %s\n",
                desc, esperado, texto);
        fallos++;
    }
}

/* ───── columna_inicio > 0: error de parser ───── */

static void test_caret_con_columna(void) {
    const char *fuente = "primera\nsegunda linea\ntercera\n";
    Error e;
    error_iniciar(&e, "ErrorDeSintaxis");
    e.archivo = "<test>";
    e.linea = 2;
    e.columna_inicio = 9;  /* apunta a 'linea' en "segunda linea" */
    e.columna_fin = 14;
    error_set_mensaje(&e, "token inesperado");

    char buf[1024];
    capturar_error(&e, fuente, 5, buf, sizeof(buf));
    error_destruir(&e);

    verificar_contiene("muestra cabecera", buf, "ErrorDeSintaxis en <test>:2:9");
    verificar_contiene("muestra la línea fuente", buf, "segunda linea");
    verificar_contiene("muestra caret", buf, "^");
    verificar_contiene("muestra el mensaje", buf, "token inesperado");
}

/* ───── columna_inicio == 0: error de runtime ───── */

static void test_caret_sin_columna(void) {
    /* v1.37: aunque columna sea 0, debe mostrar la línea fuente. */
    const char *fuente = "uno\n    dos = c + 1\ntres\n";
    Error e;
    error_iniciar(&e, "ErrorDeNombre");
    e.archivo = "<test>";
    e.linea = 2;
    e.columna_inicio = 0;  /* runtime: sin columna precisa */
    e.columna_fin = 0;
    error_set_mensaje(&e, "nombre 'c' no esta definido");

    char buf[1024];
    capturar_error(&e, fuente, 1, buf, sizeof(buf));
    error_destruir(&e);

    verificar_contiene("muestra cabecera", buf, "ErrorDeNombre en <test>:2:0");
    /* Lo clave de v1.37: la línea fuente APARECE aunque columna==0. */
    verificar_contiene("muestra la línea fuente (col 0)", buf, "dos = c + 1");
    verificar_contiene("muestra caret", buf, "^");
    verificar_contiene("muestra el mensaje", buf, "nombre 'c' no esta definido");
}

/* ───── caret apunta al primer no-blanco con columna 0 ───── */

static void test_caret_apunta_contenido(void) {
    /* Línea con indentación: el caret debe ir bajo el primer
       carácter no-blanco, no en la columna 0. */
    const char *fuente = "        codigo_indentado\n";
    Error e;
    error_iniciar(&e, "ErrorDeTipo");
    e.archivo = "<test>";
    e.linea = 1;
    e.columna_inicio = 0;
    e.columna_fin = 0;
    error_set_mensaje(&e, "tipos incompatibles");

    char buf[1024];
    capturar_error(&e, fuente, 1, buf, sizeof(buf));
    error_destruir(&e);

    /* El caret debe estar precedido de "    " (margen) + 8 espacios
       (indentación) → "            ^". */
    verificar_contiene("caret tras indentación", buf, "            ^");
}

/* ───── sin fuente: solo cabecera + mensaje ───── */

static void test_sin_fuente(void) {
    Error e;
    error_iniciar(&e, "ErrorDeValor");
    e.archivo = "<test>";
    e.linea = 5;
    e.columna_inicio = 0;
    error_set_mensaje(&e, "valor invalido");

    char buf[1024];
    capturar_error(&e, NULL, 1, buf, sizeof(buf));
    error_destruir(&e);

    verificar_contiene("cabecera sin fuente", buf, "ErrorDeValor en <test>:5:0");
    verificar_contiene("mensaje sin fuente", buf, "valor invalido");
    /* No debe haber caret (no hay fuente). */
    if (strchr(buf, '^') != NULL) {
        fprintf(stderr, "FALLO [sin fuente]: no debería haber caret\n");
        fallos++;
    }
}

int main(void) {
    test_caret_con_columna();
    test_caret_sin_columna();
    test_caret_apunta_contenido();
    test_sin_fuente();

    if (fallos == 0) {
        printf("errores_contexto: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "errores_contexto: %d fallo(s)\n", fallos);
    return 1;
}
