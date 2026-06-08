/*
 * Tests de `**resto` en dict patterns (v1.181). Continua v1.179/v1.180.
 *
 * Sintaxis: `{k1: PAT, k2: PAT, ..., **NOMBRE}`. El bind NOMBRE recibe
 * un dict nuevo con las (k, v) del sujeto cuyas claves NO estan
 * mencionadas en el patron. `**_` descarta.
 *
 * Implementacion: nuevo OP_DICC_RESTO con argumento u8 n_claves.
 * Stack: [..., dict, k1, ..., kN] -> [..., dict_resto].
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
        "test_patron_dict_resto_out.txt";
#else
        "/tmp/test_patron_dict_resto_out.txt";
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
    /* Basico */
    {
        char out[512];
        ejecutar_capturando(
            "coincidir {\"a\": 1, \"b\": 2, \"c\": 3, \"d\": 4}:\n"
            "    cuando {\"a\": ax, **resto}:\n"
            "        imprimir(ax)\n"
            "        imprimir(longitud(resto))\n"
            "        imprimir(resto[\"b\"])\n"
            "        imprimir(resto[\"c\"])\n"
            "        imprimir(resto[\"d\"])\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1\n3\n2\n3\n4") != NULL, "basico");
    }

    /* Solo **resto matchea cualquier dict */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"x\": 1, \"z\": 2}:\n"
            "    cuando {**todo}:\n"
            "        imprimir(longitud(todo))\n"
            "        imprimir(todo[\"x\"])\n"
            "        imprimir(todo[\"z\"])\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2\n1\n2") != NULL, "solo_resto");
    }

    /* **_ descarta el resto */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"a\": 1, \"b\": 2}:\n"
            "    cuando {\"a\": ax, **_}:\n"
            "        imprimir(ax)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "wildcard_resto");
    }

    /* resto vacio cuando todas las claves estan nombradas */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"a\": 1}:\n"
            "    cuando {\"a\": ax, **resto}:\n"
            "        imprimir(longitud(resto))\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "resto_vacio");
    }

    /* Sin clave en sujeto -> no match */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"x\": 1}:\n"
            "    cuando {\"a\": ax, **resto}:\n"
            "        imprimir(\"si match\")\n"
            "    cuando {**resto}:\n"
            "        imprimir(\"solo resto\", longitud(resto))\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "solo resto 1") != NULL, "falta_clave");
    }

    /* Multiples bind + resto */
    {
        char out[512];
        ejecutar_capturando(
            "coincidir {\"a\": 1, \"b\": 2, \"c\": 3, \"d\": 4, \"e\": 5}:\n"
            "    cuando {\"a\": ax, \"b\": bx, **resto}:\n"
            "        imprimir(ax, bx)\n"
            "        imprimir(longitud(resto))\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2\n3") != NULL, "multi_bind_resto");
    }

    /* Sub-patrones + resto */
    {
        char out[512];
        ejecutar_capturando(
            "coincidir {\"par\": (1, 2), \"x\": 10, \"z\": 20}:\n"
            "    cuando {\"par\": (ax, bx), **r}:\n"
            "        imprimir(ax, bx)\n"
            "        imprimir(longitud(r))\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2\n2") != NULL, "subpatron_y_resto");
    }

    /* Literal + resto */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"tipo\": \"punto\", \"x\": 10, \"z\": 20}:\n"
            "    cuando {\"tipo\": \"circulo\", **r}:\n"
            "        imprimir(\"circulo\")\n"
            "    cuando {\"tipo\": \"punto\", **r}:\n"
            "        imprimir(\"punto\", longitud(r))\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "punto 2") != NULL, "literal_y_resto");
    }

    /* Claves enteras + resto */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {1: \"uno\", 2: \"dos\", 3: \"tres\"}:\n"
            "    cuando {1: u, **r}:\n"
            "        imprimir(u)\n"
            "        imprimir(longitud(r))\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "uno\n2") != NULL, "claves_enteras");
    }

    /* Combinacion con `como` */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"k\": 1, \"v\": 99, \"extra\": \"x\"}:\n"
            "    cuando {\"k\": k, **r} como d:\n"
            "        imprimir(k, longitud(d), longitud(r))\n"
            "fin coincidir\n",
            out, sizeof(out));
        /* d tiene 3 claves, r tiene 2 (v y extra) */
        AFIRMAR(strstr(out, "1 3 2") != NULL, "con_como");
    }

    if (fallos == 0) {
        printf("patron_dict_resto: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "patron_dict_resto: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
