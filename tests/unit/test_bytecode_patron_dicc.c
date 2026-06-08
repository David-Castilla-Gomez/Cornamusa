/*
 * Tests de patron de diccionario en `coincidir/cuando` (v1.179).
 *
 * Sintaxis: `{k1: PAT, k2: PAT, ...}`. Las claves son literales
 * (entero, decimal, cadena, booleano, nulo). Los sub-patrones
 * soportados en v1.179 son BIND, WILDCARD y LITERAL.
 *
 * Semantica super-set: el dict del sujeto puede tener mas claves
 * que el patron. Solo se exige que las claves del patron existan.
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
        "test_patron_dicc_out.txt";
#else
        "/tmp/test_patron_dicc_out.txt";
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
    /* dict pattern basico */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"nombre\": \"Ana\", \"edad\": 30}:\n"
            "    cuando {\"nombre\": n, \"edad\": e}:\n"
            "        imprimir(n, e)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Ana 30") != NULL, "basico");
    }

    /* Super-set: dict con mas claves */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"a\": 1, \"b\": 2, \"c\": 3}:\n"
            "    cuando {\"a\": ax}:\n"
            "        imprimir(ax)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "super_set");
    }

    /* Literal en valor para discriminar */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"tipo\": \"punto\", \"x\": 10}:\n"
            "    cuando {\"tipo\": \"circulo\"}:\n"
            "        imprimir(\"circulo\")\n"
            "    cuando {\"tipo\": \"punto\", \"x\": x}:\n"
            "        imprimir(\"punto\", x)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "punto 10") != NULL, "literal_discrimina");
    }

    /* Falta clave -> no match */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"a\": 1}:\n"
            "    cuando {\"a\": ax, \"b\": bx}:\n"
            "        imprimir(\"ambas\")\n"
            "    cuando {\"a\": ax}:\n"
            "        imprimir(\"solo a\", ax)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "solo a 1") != NULL, "falta_clave");
    }

    /* Wildcard */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"x\": 5, \"z\": 10}:\n"
            "    cuando {\"x\": _}:\n"
            "        imprimir(\"tiene x\")\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "tiene x") != NULL, "wildcard");
    }

    /* Tipo erroneo (no dict) no matchea */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir [1, 2]:\n"
            "    cuando {\"a\": ax}:\n"
            "        imprimir(\"es dict\")\n"
            "    cuando [a, b]:\n"
            "        imprimir(\"es lista\", a, b)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "es lista 1 2") != NULL, "tipo_falla");
    }

    /* Claves enteras */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {1: \"uno\", 2: \"dos\"}:\n"
            "    cuando {1: u, 2: d}:\n"
            "        imprimir(u, d)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "uno dos") != NULL, "claves_enteras");
    }

    /* Mezcla con `como` */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"k\": 1, \"v\": 99}:\n"
            "    cuando {\"k\": k} como d:\n"
            "        imprimir(k, d[\"v\"])\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 99") != NULL, "con_como");
    }

    /* Vacio: matchea cualquier dict */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"a\": 1}:\n"
            "    cuando {}:\n"
            "        imprimir(\"ok\")\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "vacio_matchea_todos");
    }

    /* Con guarda */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"edad\": 25}:\n"
            "    cuando {\"edad\": e} si e >= 18:\n"
            "        imprimir(\"adulto\", e)\n"
            "    cuando {\"edad\": e}:\n"
            "        imprimir(\"menor\", e)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "adulto 25") != NULL, "con_guarda");
    }

    /* Literal nulo como valor */
    {
        char out[256];
        ejecutar_capturando(
            "coincidir {\"k\": nulo}:\n"
            "    cuando {\"k\": nulo}:\n"
            "        imprimir(\"nulo_match\")\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "nulo_match") != NULL, "valor_nulo");
    }

    if (fallos == 0) {
        printf("patron_dicc: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "patron_dicc: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
