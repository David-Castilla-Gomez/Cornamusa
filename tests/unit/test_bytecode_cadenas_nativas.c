/*
 * Tests directos de las 6 nativas de cadenas perf-optimizadas
 * (v1.61-v1.62). Hasta v1.74 solo tenian cobertura indirecta via
 * stdlib/cadenas.cor; aqui ejercitamos cada una con edge cases.
 *
 *   - cadena_unir (v1.61): O(n) replacement del antiguo loop O(n²).
 *   - cadena_indice_de, cadena_empieza_con, cadena_termina_con,
 *     cadena_minusculas_ascii, cadena_mayusculas_ascii (v1.62).
 *
 * Edge cases cubiertos: vacios, UTF-8 multibyte, prefijo mas largo
 * que cadena, separador vacio, mayusculas non-ASCII (que no deben
 * tocarse).
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
        "test_cadnat_out.txt";
#else
        "/tmp/test_cadnat_out.txt";
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
    /* ─── cadena_unir (v1.61) ─── */

    /* unir lista vacia -> "". */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(\"<\" + cadenas.unir([], \",\") + \">\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "<>") != NULL, "unir_vacia");
    }

    /* unir un solo elemento -> ese elemento, sin separador. */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(\"<\" + cadenas.unir([\"solo\"], \", \") + \">\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "<solo>") != NULL, "unir_un_elemento");
    }

    /* unir con separador vacio. */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.unir([\"a\", \"b\", \"c\"], \"\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "abc") != NULL, "unir_sep_vacio");
    }

    /* unir UTF-8 multibyte (verifica que no rompe encoding). */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.unir([\"café\", \"niño\"], \" — \"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "café — niño") != NULL, "unir_utf8");
    }

    /* ─── cadena_indice_de (v1.62) ─── */

    /* sub presente al inicio. */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.indice_de(\"hola mundo\", \"hola\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "indice_inicio");
    }

    /* sub presente al final. */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.indice_de(\"hola mundo\", \"mundo\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "indice_final");
    }

    /* sub no presente -> -1 (convencion). */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.indice_de(\"hola mundo\", \"xyz\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-1") != NULL, "indice_no_presente");
    }

    /* sub vacia -> 0 (o convencion definida). */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.indice_de(\"hola\", \"\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "indice_sub_vacia");
    }

    /* ─── cadena_empieza_con (v1.62) ─── */

    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.empieza_con(\"hola mundo\", \"hola\"))\n"
            "imprimir(cadenas.empieza_con(\"hola mundo\", \"mundo\"))\n"
            "imprimir(cadenas.empieza_con(\"hi\", \"\"))\n"
            "imprimir(cadenas.empieza_con(\"hi\", \"hola\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso") != NULL ||
                strstr(out, "verdadero\r\nfalso") != NULL,
                "empieza_con_basico");
        /* Prefijo vacio -> true. */
        /* Prefijo mas largo que cadena -> false. */
        int n_verdaderos = 0, n_falsos = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_verdaderos++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_falsos++; p++; }
        AFIRMAR(n_verdaderos == 2 && n_falsos == 2, "empieza_con_conteo");
    }

    /* ─── cadena_termina_con (v1.62) ─── */

    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.termina_con(\"hola.cor\", \".cor\"))\n"
            "imprimir(cadenas.termina_con(\"hola.cor\", \".py\"))\n"
            "imprimir(cadenas.termina_con(\"hi\", \"\"))\n"
            "imprimir(cadenas.termina_con(\"hi\", \"hello\"))\n",
            out, sizeof(out));
        int n_verdaderos = 0, n_falsos = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_verdaderos++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_falsos++; p++; }
        AFIRMAR(n_verdaderos == 2 && n_falsos == 2, "termina_con_conteo");
    }

    /* ─── cadena_minusculas_ascii (v1.62) ─── */

    /* ASCII se convierte. */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.minusculas_ascii(\"HOLA MUNDO\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "hola mundo") != NULL, "minusculas_ascii");
    }

    /* Non-ASCII (acentos, ñ) NO se tocan — sigue siendo ASCII-only. */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.minusculas_ascii(\"CAFÉ NIÑO\"))\n",
            out, sizeof(out));
        /* Esperamos: "cafÉ niÑo" — ASCII bajado, acentos intactos. */
        AFIRMAR(strstr(out, "cafÉ niÑo") != NULL, "minusculas_acentos_intactos");
    }

    /* Cadena vacia. */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(\"<\" + cadenas.minusculas_ascii(\"\") + \">\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "<>") != NULL, "minusculas_vacia");
    }

    /* ─── cadena_mayusculas_ascii (v1.62) ─── */

    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.mayusculas_ascii(\"hola mundo\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "HOLA MUNDO") != NULL, "mayusculas_ascii");
    }

    /* Non-ASCII intacto: solo letras ASCII a-z → A-Z. */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "imprimir(cadenas.mayusculas_ascii(\"café niño\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "CAFé NIñO") != NULL, "mayusculas_acentos_intactos");
    }

    /* Round-trip: ascii -> mayusculas -> minusculas == ascii original
     * (solo ASCII letras). */
    {
        char out[256];
        ejecutar_capturando(
            "importar cadenas\n"
            "s = \"hola\"\n"
            "imprimir(cadenas.minusculas_ascii(cadenas.mayusculas_ascii(s)) == s)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "roundtrip_caso");
    }

    if (fallos == 0) {
        printf("cadenas_nativas: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "cadenas_nativas: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
