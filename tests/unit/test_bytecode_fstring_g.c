/*
 * Tests del tipo de formato `g`/`G` en f-strings (v1.187).
 *
 * Paridad printf %g: notacion general que selecciona entre decimal y
 * cientifica segun la magnitud. Precision por defecto 6 (dígitos
 * significativos, no decimales).
 *
 * `G` es la variante con exponente en mayúsculas (`1E+10`).
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
        "test_fstring_g_out.txt";
#else
        "/tmp/test_fstring_g_out.txt";
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
    /* Precision por defecto (6 dígitos sig) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{3.14159:g}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3.14159") != NULL, "default_prec");
    }

    /* Precision custom */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{3.14159:.4g}\")\n"
            "imprimir(f\"{3.14159:.2g}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3.142") != NULL, "prec_4");
        AFIRMAR(strstr(out, "3.1") != NULL, "prec_2");
    }

    /* Numeros grandes → notacion cientifica */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{1000000.0:g}\")\n"
            "imprimir(f\"{1e10:g}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1e+06") != NULL, "gran_1m");
        AFIRMAR(strstr(out, "1e+10") != NULL, "gran_1e10");
    }

    /* Numeros pequenos → decimal o cientifica */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{0.0001:g}\")\n"
            "imprimir(f\"{0.00001:g}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0.0001") != NULL, "pequeno_decimal");
        AFIRMAR(strstr(out, "1e-05") != NULL, "pequeno_cientifico");
    }

    /* Variante G mayusculas */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{1e10:G}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1E+10") != NULL, "G_mayus");
    }

    /* Con entero */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{42:g}\")\n"
            "imprimir(f\"{0:g}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "g_entero");
        AFIRMAR(strstr(out, "0") != NULL, "g_cero");
    }

    /* Combinado con ancho y alineacion */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"|{3.14:>10g}|\")\n"
            "imprimir(f\"|{3.14:<10g}|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|      3.14|") != NULL, "g_alineado_der");
        AFIRMAR(strstr(out, "|3.14      |") != NULL, "g_alineado_izq");
    }

    /* Negativo */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{-3.14:g}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-3.14") != NULL, "negativo");
    }

    if (fallos == 0) {
        printf("fstring_g: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "fstring_g: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
