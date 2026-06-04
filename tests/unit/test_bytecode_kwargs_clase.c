/*
 * Tests de keyword arguments en constructores de clase (v1.121).
 *
 * Antes de v1.121, llamar a `Clase(x=1)` lanzaba
 * "ErrorDeTipo: keyword args solo soportados para funciones bytecode
 *  (no 'clase')".
 * v1.121 transforma VAL_CLASE -> closure de __iniciar__ + instancia
 * como primer posicional dentro del helper kw, y marca el frame como
 * constructor.
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
        "test_kwcls_out.txt";
#else
        "/tmp/test_kwcls_out.txt";
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
    /* Caso clasico: kwargs construyen instancia */
    {
        char out[256];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo, x, b):\n"
            "        yo.x = x\n"
            "        yo.b = b\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = P(x=10, b=20)\n"
            "imprimir(p.x)\n"
            "imprimir(p.b)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10") != NULL, "kwarg_x");
        AFIRMAR(strstr(out, "20") != NULL, "kwarg_b");
    }

    /* Mezcla posicional + kwargs */
    {
        char out[256];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo, a, b, c):\n"
            "        yo.t = a + b + c\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(P(1, c=3, b=2).t)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "6") != NULL, "mezcla_pos_kw");
    }

    /* Defaults se rellenan cuando no hay kwarg */
    {
        char out[256];
        ejecutar_capturando(
            "clase Q:\n"
            "    funcion __iniciar__(yo, n, ciudad=\"Madrid\"):\n"
            "        yo.n = n\n"
            "        yo.c = ciudad\n"
            "    fin funcion\n"
            "fin clase\n"
            "q = Q(n=\"Ana\")\n"
            "imprimir(q.n)\n"
            "imprimir(q.c)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Ana") != NULL, "default_n");
        AFIRMAR(strstr(out, "Madrid") != NULL, "default_ciudad");
    }

    /* Override default por kwarg */
    {
        char out[256];
        ejecutar_capturando(
            "clase Q:\n"
            "    funcion __iniciar__(yo, n, ciudad=\"Madrid\"):\n"
            "        yo.c = ciudad\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(Q(n=\"x\", ciudad=\"Sevilla\").c)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Sevilla") != NULL, "override_default");
    }

    /* Clase sin __iniciar__ rechaza cualquier kwarg */
    {
        char out[256];
        ejecutar_capturando(
            "clase V:\n"
            "fin clase\n"
            "intentar:\n"
            "    V(x=1)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "vacia_con_kwarg_lanza");
    }

    /* Clase sin __iniciar__ sin kwargs OK */
    {
        char out[256];
        ejecutar_capturando(
            "clase V:\n"
            "fin clase\n"
            "v = V()\n"
            "imprimir(\"ok\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "vacia_sin_args_ok");
    }

    /* Kwarg duplicado: posicional + kw con mismo nombre */
    {
        char out[256];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo, x):\n"
            "        yo.x = x\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    P(1, x=2)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "kw_duplicado_lanza");
    }

    /* Kwarg desconocido lanza */
    {
        char out[256];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo, x):\n"
            "        yo.x = x\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    P(x=1, z=99)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "kw_desconocido_lanza");
    }

    /* Heap con clave=lambda funciona (motivacion original) */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap(clave=lambda p: p[0])\n"
            "h.poner([3, \"tres\"])\n"
            "h.poner([1, \"uno\"])\n"
            "h.poner([2, \"dos\"])\n"
            "imprimir(h.sacar())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, \"uno\"]") != NULL, "heap_kwarg_clave");
    }

    /* La instancia se devuelve aunque __iniciar__ tenga retornar */
    {
        char out[256];
        ejecutar_capturando(
            "clase R:\n"
            "    funcion __iniciar__(yo, x):\n"
            "        yo.x = x\n"
            "        retornar 999\n"
            "    fin funcion\n"
            "fin clase\n"
            "r = R(x=42)\n"
            "imprimir(r.x)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "constructor_descarta_retorno");
    }

    if (fallos == 0) {
        printf("kwargs_clase: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "kwargs_clase: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
