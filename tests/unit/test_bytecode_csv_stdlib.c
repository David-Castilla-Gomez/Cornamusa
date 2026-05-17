/*
 * Tests del modulo stdlib/csv.cor (v1.58).
 *
 * Cubre edge cases declarados como soportados:
 *   - Parseo basico de filas separadas por coma.
 *   - Separadores alternativos (`;`, `\t`).
 *   - Campos entre comillas con `,` y `\n` internos.
 *   - Escape `""` para comilla literal dentro de quoted.
 *   - Round-trip: parsear -> serializar -> parsear igual.
 *   - Cadena vacia devuelve lista vacia.
 *   - Linea con un solo campo.
 *   - Salto de linea final no produce fila vacia espuria.
 *   - Distincion `\n` vs `\r\n` al parsear.
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
        "test_csv_out.txt";
#else
        "/tmp/test_csv_out.txt";
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
    /* Test 1: parseo basico. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "f = csv.parsear(\"a,b,c\\n1,2,3\")\n"
            "imprimir(longitud(f))\n"
            "imprimir(f[0][0], f[0][1], f[0][2])\n"
            "imprimir(f[1][0], f[1][1], f[1][2])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "2\n") != NULL || strstr(out, "2\r\n") != NULL, "basico_2_filas");
        AFIRMAR(strstr(out, "a b c") != NULL, "basico_header");
        AFIRMAR(strstr(out, "1 2 3") != NULL, "basico_datos");
    }

    /* Test 2: separador alternativo `;`. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "f = csv.parsear(\"a;b\\n1;2\", \";\")\n"
            "imprimir(f[1][1])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "sep_punto_coma");
    }

    /* Test 3: campo entre comillas con coma interna. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "f = csv.parsear(\"\\\"hola, mundo\\\",fin\")\n"
            "imprimir(f[0][0])\n"
            "imprimir(f[0][1])\n"
            "imprimir(longitud(f[0]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "hola, mundo") != NULL, "quoted_coma_interna");
        AFIRMAR(strstr(out, "fin") != NULL, "quoted_fin");
        AFIRMAR(strstr(out, "2") != NULL, "quoted_2_campos");
    }

    /* Test 4: escape `""` para comilla literal. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "f = csv.parsear(\"\\\"di \\\"\\\"hola\\\"\\\"\\\"\")\n"
            "imprimir(f[0][0])\n", out, sizeof(out));
        /* La cadena fuente representa: parsear("\"di \"\"hola\"\"\"")
         * que es un campo quoted con `di ""hola""` -> di "hola" */
        AFIRMAR(strstr(out, "di \"hola\"") != NULL, "escape_comilla_doble");
    }

    /* Test 5: salto de linea dentro de campo quoted. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "f = csv.parsear(\"\\\"linea1\\nlinea2\\\",b\")\n"
            "imprimir(longitud(f))\n"
            "imprimir(longitud(f[0]))\n", out, sizeof(out));
        /* Solo UNA fila aunque haya \n interno. */
        AFIRMAR(strstr(out, "1\n") != NULL || strstr(out, "1\r\n") != NULL,
                "quoted_nl_una_fila");
        AFIRMAR(strstr(out, "2") != NULL, "quoted_nl_dos_campos");
    }

    /* Test 6: cadena vacia -> lista vacia. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "f = csv.parsear(\"\")\n"
            "imprimir(longitud(f))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "vacia_lista_vacia");
    }

    /* Test 7: salto de linea final no produce fila vacia espuria. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "f = csv.parsear(\"a,b\\n1,2\\n\")\n"
            "imprimir(longitud(f))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "2\n") != NULL || strstr(out, "2\r\n") != NULL,
                "trailing_nl_2_filas");
    }

    /* Test 8: `\r\n` como separador de linea. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "f = csv.parsear(\"a,b\\r\\n1,2\")\n"
            "imprimir(longitud(f))\n"
            "imprimir(f[1][1])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "crlf_2_filas");
    }

    /* Test 9: round-trip parsear -> serializar -> parsear. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "original = [[\"nombre\", \"edad\"], [\"Ana\", \"30\"], [\"Luis\", \"25\"]]\n"
            "texto = csv.serializar(original)\n"
            "parseado = csv.parsear(texto)\n"
            "imprimir(longitud(parseado))\n"
            "imprimir(parseado[1][0], parseado[1][1])\n"
            "imprimir(parseado[2][0], parseado[2][1])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "roundtrip_3_filas");
        AFIRMAR(strstr(out, "Ana 30") != NULL, "roundtrip_ana");
        AFIRMAR(strstr(out, "Luis 25") != NULL, "roundtrip_luis");
    }

    /* Test 10: serializar campo con coma se quotea automaticamente. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "t = csv.serializar([[\"a,b\", \"x\"]])\n"
            "imprimir(t)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "\"a,b\"") != NULL, "serializar_quotea_coma");
    }

    /* Test 11: serializar campo con `"` interno escapa con `""`. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar csv\n"
            "t = csv.serializar([[\"di \\\"hola\\\"\"]])\n"
            "imprimir(t)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "\"\"") != NULL, "serializar_escapa_comilla");
    }

    if (fallos == 0) {
        printf("csv_stdlib: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "csv_stdlib: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
