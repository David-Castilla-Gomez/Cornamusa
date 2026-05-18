/*
 * Tests de stdlib/validacion.cor (v1.92).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "compilador.h"
#include "lexer.h"
#include "parser.h"
#include "valor.h"
#include "vm.h"

static int fallos = 0;
static int casos = 0;

#define AFIRMAR(cond, etiqueta)                                                \
    do {                                                                        \
        casos++;                                                                \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FALLO %s:%d (%s)\n", __FILE__, __LINE__, etiqueta);\
            fallos++;                                                           \
        }                                                                       \
    } while (0)

static int ejecutar_capturando(const char *fuente, char *out_buf, int out_cap) {
    const char *tmpfile =
#ifdef _WIN32
        "test_val_out.txt";
#else
        "/tmp/test_val_out.txt";
#endif
    if (!freopen(tmpfile, "w+", stdout)) return -1;

    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    int rc = -1;
    if (!p.tuvo_error) {
        Chunk chunk; chunk_iniciar(&chunk);
        Compilador c; compilador_iniciar(&c, &chunk);
        if (compilador_compilar_programa(&c, sents, n)) {
            VM vm; vm_iniciar(&vm);
            Valor r = valor_nulo();
            ResultadoVM rcvm = vm_ejecutar(&vm, &chunk, &r);
            valor_destruir(&r);
            vm_destruir(&vm);
            if (rcvm == VM_OK) rc = 0;
        }
        chunk_destruir(&chunk);
    }
    arena_destruir(&a);

    fflush(stdout);
#ifdef _WIN32
    freopen("CON", "w", stdout);
#else
    freopen("/dev/tty", "w", stdout);
#endif

    FILE *f = fopen(tmpfile, "r");
    if (f) {
        int leido = (int)fread(out_buf, 1, (size_t)(out_cap - 1), f);
        out_buf[leido] = '\0';
        fclose(f);
        remove(tmpfile);
    } else {
        out_buf[0] = '\0';
    }
    return rc;
}

int main(void) {
    /* es_email */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "imprimir(validacion.es_email(\"ana@ejemplo.com\"))\n"
            "imprimir(validacion.es_email(\"no-email\"))\n"
            "imprimir(validacion.es_email(\"x@y.io\"))\n"
            "imprimir(validacion.es_email(42))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "email_valido");
        int n_falsos = 0;
        const char *p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_falsos++; p++; }
        AFIRMAR(n_falsos == 2, "email_dos_falsos");
    }

    /* es_url */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "imprimir(validacion.es_url(\"http://x.com\"))\n"
            "imprimir(validacion.es_url(\"https://x.io\"))\n"
            "imprimir(validacion.es_url(\"ftp://x\"))\n"
            "imprimir(validacion.es_url(\"sin-protocolo\"))\n", out, sizeof(out));
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 2 && n_f == 2, "url_contadores");
    }

    /* es_fecha_iso */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "imprimir(validacion.es_fecha_iso(\"2026-05-18\"))\n"
            "imprimir(validacion.es_fecha_iso(\"2026-13-01\"))\n"
            "imprimir(validacion.es_fecha_iso(\"2026-05-32\"))\n"
            "imprimir(validacion.es_fecha_iso(\"20260518\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "fecha_iso_valida");
        int n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_f == 3, "fecha_iso_3_invalidas");
    }

    /* en_rango */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "imprimir(validacion.en_rango(50, 0, 100))\n"
            "imprimir(validacion.en_rango(0, 0, 100))\n"
            "imprimir(validacion.en_rango(100, 0, 100))\n"
            "imprimir(validacion.en_rango(-1, 0, 100))\n"
            "imprimir(validacion.en_rango(101, 0, 100))\n", out, sizeof(out));
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 3 && n_f == 2, "rango_inclusivo");
    }

    /* longitud_en_rango */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "imprimir(validacion.longitud_en_rango(\"hola\", 1, 10))\n"
            "imprimir(validacion.longitud_en_rango(\"\", 1, 10))\n"
            "imprimir(validacion.longitud_en_rango(\"muy muy larga\", 1, 5))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso\nfalso") != NULL ||
                strstr(out, "verdadero\r\nfalso\r\nfalso") != NULL,
                "longitud_rangos");
    }

    /* no_vacia */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "imprimir(validacion.no_vacia(\"hi\"))\n"
            "imprimir(validacion.no_vacia(\"\"))\n"
            "imprimir(validacion.no_vacia(\"   \"))\n"
            "imprimir(validacion.no_vacia(\"  hi  \"))\n", out, sizeof(out));
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 2 && n_f == 2, "no_vacia_trim");
    }

    /* en_conjunto */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "imprimir(validacion.en_conjunto(\"a\", [\"a\", \"b\", \"c\"]))\n"
            "imprimir(validacion.en_conjunto(\"x\", [\"a\", \"b\", \"c\"]))\n"
            "imprimir(validacion.en_conjunto(2, [1, 2, 3]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso\nverdadero") != NULL ||
                strstr(out, "verdadero\r\nfalso\r\nverdadero") != NULL,
                "en_conjunto");
    }

    /* Validador acumula errores */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "v = validacion.Validador()\n"
            "v.verificar(\"x\", falso, \"error de x\")\n"
            "v.verificar(\"z\", verdadero, \"no deberia entrar\")\n"
            "v.verificar(\"w\", falso, \"error de w\")\n"
            "imprimir(v.tiene_errores())\n"
            "imprimir(v.valido())\n"
            "imprimir(longitud(v.errores))\n"
            "imprimir(v.errores[\"x\"])\n"
            "imprimir(v.errores[\"w\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "error de x") != NULL, "validador_x");
        AFIRMAR(strstr(out, "error de w") != NULL, "validador_w");
        AFIRMAR(strstr(out, "2") != NULL, "validador_2_errores");
    }

    /* Validador sin errores */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "v = validacion.Validador()\n"
            "v.verificar(\"x\", verdadero, \"todo OK\")\n"
            "imprimir(v.valido())\n"
            "imprimir(v.tiene_errores())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso") != NULL ||
                strstr(out, "verdadero\r\nfalso") != NULL,
                "validador_sin_errores");
    }

    /* Patron generico via coincide */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "imprimir(validacion.coincide(\"abc123\", \"^[a-z]+[0-9]+$\"))\n"
            "imprimir(validacion.coincide(\"123abc\", \"^[a-z]+[0-9]+$\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso") != NULL ||
                strstr(out, "verdadero\r\nfalso") != NULL,
                "coincide_regex");
    }

    /* tipos no-cadena devuelven falso silenciosamente */
    {
        char out[1024];
        ejecutar_capturando(
            "importar validacion\n"
            "imprimir(validacion.es_email(42))\n"
            "imprimir(validacion.es_url(nulo))\n"
            "imprimir(validacion.no_vacia([1,2]))\n", out, sizeof(out));
        int n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_f == 3, "no_cadena_falso");
    }

    if (fallos == 0) {
        printf("validacion: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "validacion: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
