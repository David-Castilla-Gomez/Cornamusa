/*
 * Tests de los metodos nativos anadidos en v1.123:
 *   cadena: separar, reemplazar, recortar, contiene, unir
 *   lista:  contar, contiene, copiar
 *   dict:   items, obtener
 *
 * Complementa test_bytecode_metodos_nativos.c (los 13 metodos
 * iniciales de v1.122). El nombre lleva el sufijo _v123 porque
 * cada test del bytecode se compila en su propio .exe y necesita
 * un identificador unico.
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
        "test_met_v123_out.txt";
#else
        "/tmp/test_met_v123_out.txt";
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
    /* cadena.separar */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"a,b,c\".separar(\",\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"b\", \"c\"]") != NULL, "separar_simple");
    }
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"abc\".separar(\"\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"b\", \"c\"]") != NULL, "separar_vacio");
    }
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola\".separar(\",\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[\"hola\"]") != NULL, "separar_sin_match");
    }
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"a,b,\".separar(\",\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"b\", \"\"]") != NULL, "separar_trailing");
    }

    /* cadena.reemplazar */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola mundo\".reemplazar(\"mundo\", \"tierra\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "hola tierra") != NULL, "reemplazar_simple");
    }
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"aaa\".reemplazar(\"a\", \"bb\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "bbbbbb") != NULL, "reemplazar_expansion");
    }
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"abc\".reemplazar(\"xyz\", \"_\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "abc") != NULL, "reemplazar_sin_match");
    }

    /* cadena.recortar */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"|\" + \"  hola \\n\".recortar() + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|hola|") != NULL, "recortar_extremos");
    }
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"|\" + \"hola\".recortar() + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|hola|") != NULL, "recortar_idempotente");
    }

    /* cadena.contiene */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola mundo\".contiene(\"mu\"))\n"
            "imprimir(\"hola\".contiene(\"xyz\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "contiene_si");
        AFIRMAR(strstr(out, "falso") != NULL, "contiene_no");
    }

    /* cadena.unir */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"-\".unir([\"a\", \"b\", \"c\"]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "a-b-c") != NULL, "unir_sep_normal");
    }
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\".unir([\"a\", \"b\"]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "ab") != NULL, "unir_sep_vacio");
    }

    /* lista.contar */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([1, 2, 3, 2, 1, 2].contar(2))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "contar");
    }
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([1, 2, 3].contar(99))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "contar_cero");
    }

    /* lista.contiene */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "imprimir(xs.contiene(2))\n"
            "imprimir(xs.contiene(9))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "lista_contiene_si");
        AFIRMAR(strstr(out, "falso") != NULL, "lista_contiene_no");
    }

    /* lista.copiar */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "c = xs.copiar()\n"
            "xs.agregar(4)\n"
            "imprimir(c)\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]\n[1, 2, 3, 4]") != NULL
                || strstr(out, "[1, 2, 3]\r\n[1, 2, 3, 4]") != NULL,
                "copiar_independiente");
    }

    /* dict.items */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1, \"b\": 2}\n"
            "imprimir(d.items())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[\"a\", 1], [\"b\", 2]]") != NULL, "items_pares");
    }

    /* dict.obtener */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1}\n"
            "imprimir(d.obtener(\"a\", -1))\n"
            "imprimir(d.obtener(\"xyz\", -1))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "obtener_presente");
        AFIRMAR(strstr(out, "-1") != NULL, "obtener_default");
    }

    /* Iteracion natural sobre dict.items() */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"x\": 10, \"y\": 20}\n"
            "total = 0\n"
            "para par en d.items():\n"
            "    total = total + par[1]\n"
            "fin para\n"
            "imprimir(total)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "30") != NULL, "items_iterable");
    }

    if (fallos == 0) {
        printf("met_nativos_v123: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "met_nativos_v123: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
