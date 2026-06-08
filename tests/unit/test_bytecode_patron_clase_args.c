/*
 * Tests de patron de clase con args en `coincidir/cuando` (v1.178).
 *
 * Cierra la limitacion documentada desde v1.16.3: antes solo
 * `Foo()` sin args. Ahora `Foo(a, b)` o `Foo(a=PAT, b=PAT)`.
 *
 * Sintaxis:
 *   - `Foo(a)` = `Foo(a=a)`: bindea sujeto.a a una variable local `a`.
 *   - `Foo(a=PAT, b=PAT)`: nombre antes del = es atributo; PAT es sub-patron.
 *   - Sub-patrones soportados: BIND, WILDCARD, LITERAL.
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
        "test_patron_clase_args_out.txt";
#else
        "/tmp/test_patron_clase_args_out.txt";
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

#define CLASE_PUNTO \
    "clase Punto:\n" \
    "    funcion __iniciar__(yo, ax, ay):\n" \
    "        yo.ax = ax\n" \
    "        yo.ay = ay\n" \
    "    fin funcion\n" \
    "fin clase\n"

int main(void) {
    /* Bind atributos al mismo nombre */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PUNTO
            "coincidir Punto(3, 4):\n"
            "    cuando Punto(ax, ay):\n"
            "        imprimir(ax, ay)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3 4") != NULL, "bind_basico");
    }

    /* Literal en atributo */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PUNTO
            "coincidir Punto(0, 5):\n"
            "    cuando Punto(ax=0, ay):\n"
            "        imprimir(\"origen_y\", ay)\n"
            "    cuando Punto(ax, ay):\n"
            "        imprimir(\"otro\", ax, ay)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "origen_y 5") != NULL, "literal_match");
    }

    /* Literal NO matchea -> cae a siguiente cuando */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PUNTO
            "coincidir Punto(3, 5):\n"
            "    cuando Punto(ax=0, ay):\n"
            "        imprimir(\"origen\")\n"
            "    cuando Punto(ax, ay):\n"
            "        imprimir(\"otro\", ax, ay)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "otro 3 5") != NULL, "literal_no_match");
    }

    /* Wildcard */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PUNTO
            "coincidir Punto(3, 4):\n"
            "    cuando Punto(ax=3, ay=_):\n"
            "        imprimir(\"ax_3\")\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ax_3") != NULL, "wildcard");
    }

    /* Bind con nombre custom */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PUNTO
            "coincidir Punto(7, 11):\n"
            "    cuando Punto(ax=primero, ay=segundo):\n"
            "        imprimir(primero, segundo)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "7 11") != NULL, "bind_custom");
    }

    /* Falla de tipo no entra al cuando */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PUNTO
            "coincidir [1, 2]:\n"
            "    cuando Punto(ax, ay):\n"
            "        imprimir(\"es punto\")\n"
            "    cuando [a, b]:\n"
            "        imprimir(\"lista\", a, b)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "lista 1 2") != NULL, "tipo_falla");
    }

    /* Punto sin args sigue funcionando (regresion) */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PUNTO
            "coincidir Punto(10, 20):\n"
            "    cuando Punto():\n"
            "        imprimir(\"es punto\")\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "es punto") != NULL, "sin_args_regresion");
    }

    /* Combinacion con `como` */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PUNTO
            "coincidir Punto(5, 6):\n"
            "    cuando Punto(ax) como pt:\n"
            "        imprimir(ax, pt.ay)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5 6") != NULL, "con_como");
    }

    /* Multiples atributos con literal */
    {
        char out[512];
        ejecutar_capturando(
            "clase Color:\n"
            "    funcion __iniciar__(yo, r, g, b):\n"
            "        yo.r = r\n"
            "        yo.g = g\n"
            "        yo.b = b\n"
            "    fin funcion\n"
            "fin clase\n"
            "coincidir Color(255, 0, 0):\n"
            "    cuando Color(r=255, g=0, b=0):\n"
            "        imprimir(\"rojo\")\n"
            "    cuando Color(r=0, g=255, b=0):\n"
            "        imprimir(\"verde\")\n"
            "    cuando Color(r, g, b):\n"
            "        imprimir(\"otro\", r, g, b)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "rojo") != NULL, "tres_literales");
    }

    /* En guarda */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PUNTO
            "coincidir Punto(7, 4):\n"
            "    cuando Punto(ax, ay) si ax > ay:\n"
            "        imprimir(\"x mayor\", ax)\n"
            "    cuando Punto(ax, ay):\n"
            "        imprimir(\"y mayor o igual\")\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "x mayor 7") != NULL, "con_guarda");
    }

    if (fallos == 0) {
        printf("patron_clase_args: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "patron_clase_args: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
