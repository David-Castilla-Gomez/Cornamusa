/*
 * Tests de cadena.dividir_lineas() y alineacion (centrar /
 * alinear_izquierda / alinear_derecha) — v1.152.
 *
 * Cornamusa tenia separar(sep), recortar(), reemplazar(), unir(),
 * contiene(), empieza_con(), termina_con(), indice_de(),
 * mayusculas() y minusculas() como metodos de cadena. Faltaban:
 *
 *   - dividir_lineas(): paridad con Python str.splitlines(),
 *     respeta \n, \r\n (Windows), \r (legacy Mac) y descarta el
 *     terminador final si lo hay.
 *
 *   - centrar(ancho[, relleno=' ']): paridad con str.center().
 *   - alinear_izquierda(ancho[, relleno]): paridad con str.ljust().
 *   - alinear_derecha(ancho[, relleno]): paridad con str.rjust().
 *
 * Las tres de alineacion cuentan en code-points UTF-8 (no en
 * bytes), asi 'ñ'.centrar(5) deja la 'ñ' centrada en 5 columnas
 * visuales, no 4 (porque 'ñ' son 2 bytes UTF-8).
 *
 * Sin cambios a bytecode ni VM.
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
        "test_cad_la_out.txt";
#else
        "/tmp/test_cad_la_out.txt";
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
    /* dividir_lineas: descarta terminador final */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola\\nmundo\\nadios\\n\".dividir_lineas())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"hola\", \"mundo\", \"adios\"]") != NULL,
                "div_lin_terminador");
    }

    /* dividir_lineas: sin terminador final */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola\\nmundo\\nadios\".dividir_lineas())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"hola\", \"mundo\", \"adios\"]") != NULL,
                "div_lin_sin_term");
    }

    /* dividir_lineas: cadena vacia */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\".dividir_lineas())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "div_lin_vacia");
    }

    /* dividir_lineas: CRLF (Windows) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"uno\\r\\ndos\\r\\ntres\".dividir_lineas())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"uno\", \"dos\", \"tres\"]") != NULL,
                "div_lin_crlf");
    }

    /* dividir_lineas: solo \r (legacy Mac) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"a\\rb\\rc\".dividir_lineas())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"b\", \"c\"]") != NULL,
                "div_lin_cr");
    }

    /* dividir_lineas: mezcla de terminadores */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"a\\nb\\r\\nc\\rd\".dividir_lineas())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"b\", \"c\", \"d\"]") != NULL,
                "div_lin_mixto");
    }

    /* centrar con relleno por defecto (espacio) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"[\" + \"hola\".centrar(10) + \"]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[   hola   ]") != NULL, "centrar_default");
    }

    /* centrar con ancho impar — extra a la derecha */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"[\" + \"hola\".centrar(11) + \"]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[   hola    ]") != NULL, "centrar_impar");
    }

    /* centrar con relleno custom */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"[\" + \"hi\".centrar(8, \"-\") + \"]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[---hi---]") != NULL, "centrar_custom");
    }

    /* centrar con cadena ya mas larga que ancho — sin cambios */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"[\" + \"largisima\".centrar(3) + \"]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[largisima]") != NULL, "centrar_no_cabe");
    }

    /* alinear_izquierda */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"[\" + \"hi\".alinear_izquierda(8) + \"]\")\n"
            "imprimir(\"[\" + \"hi\".alinear_izquierda(8, \".\") + \"]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[hi      ]") != NULL, "ai_default");
        AFIRMAR(strstr(out, "[hi......]") != NULL, "ai_custom");
    }

    /* alinear_derecha */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"[\" + \"hi\".alinear_derecha(8) + \"]\")\n"
            "imprimir(\"[\" + \"hi\".alinear_derecha(8, \"*\") + \"]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[      hi]") != NULL, "ad_default");
        AFIRMAR(strstr(out, "[******hi]") != NULL, "ad_custom");
    }

    /* Unicode: alinear cuenta en code points, no bytes */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud(\"\xc3\xb1\".centrar(5)))\n",   /* ñ */
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "unicode_codepoints");
    }

    /* Error: relleno multi-caracter */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    \"hi\".centrar(10, \"ab\")\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "rel_multi_rechaza");
    }

    /* Error: ancho negativo gigante */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    \"hi\".centrar(-5)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "ancho_neg");
    }

    if (fallos == 0) {
        printf("cad_la: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "cad_la: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
