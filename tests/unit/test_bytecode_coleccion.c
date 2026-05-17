/*
 * Tests de stdlib/coleccion.cor (v1.88): Pila, Cola, ColaDoble.
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
        "test_colec_out.txt";
#else
        "/tmp/test_colec_out.txt";
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
    /* Pila: LIFO basico. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "p = coleccion.Pila()\n"
            "p.poner(1)\n"
            "p.poner(2)\n"
            "p.poner(3)\n"
            "imprimir(p.sacar())\n"
            "imprimir(p.sacar())\n"
            "imprimir(p.sacar())\n", out, sizeof(out));
        /* LIFO: 3, 2, 1 */
        AFIRMAR(strstr(out, "3\n2\n1") != NULL ||
                strstr(out, "3\r\n2\r\n1") != NULL, "pila_lifo");
    }

    /* Pila: vista no remueve. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "p = coleccion.Pila()\n"
            "p.poner(42)\n"
            "imprimir(p.vista())\n"
            "imprimir(longitud(p))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "pila_vista_valor");
        AFIRMAR(strstr(out, "1") != NULL, "pila_vista_no_remueve");
    }

    /* Pila: sacar vacia lanza. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "intentar:\n"
            "    coleccion.Pila().sacar()\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"rechazado\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "rechazado") != NULL, "pila_sacar_vacia");
    }

    /* Pila: vacia() */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "p = coleccion.Pila()\n"
            "imprimir(p.vacia())\n"
            "p.poner(1)\n"
            "imprimir(p.vacia())\n"
            "p.sacar()\n"
            "imprimir(p.vacia())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso\nverdadero") != NULL ||
                strstr(out, "verdadero\r\nfalso\r\nverdadero") != NULL,
                "pila_vacia_estado");
    }

    /* Cola: FIFO basico. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "c = coleccion.Cola()\n"
            "c.poner(\"a\")\n"
            "c.poner(\"b\")\n"
            "c.poner(\"c\")\n"
            "imprimir(c.sacar())\n"
            "imprimir(c.sacar())\n"
            "imprimir(c.sacar())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "a\nb\nc") != NULL ||
                strstr(out, "a\r\nb\r\nc") != NULL, "cola_fifo");
    }

    /* Cola: vista. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "c = coleccion.Cola()\n"
            "c.poner(99)\n"
            "imprimir(c.vista())\n"
            "imprimir(longitud(c))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "99") != NULL, "cola_vista_valor");
    }

    /* ColaDoble: insertar y sacar por ambos extremos. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "d = coleccion.ColaDoble()\n"
            "d.poner_final(\"medio\")\n"
            "d.poner_frente(\"inicio\")\n"
            "d.poner_final(\"fin\")\n"
            "imprimir(d.sacar_frente())\n"
            "imprimir(d.sacar_final())\n"
            "imprimir(d.vista_frente())\n", out, sizeof(out));
        /* Tras poner_final(medio), poner_frente(inicio), poner_final(fin):
         * cola es [inicio, medio, fin].
         * sacar_frente → inicio; sacar_final → fin; queda [medio]. */
        AFIRMAR(strstr(out, "inicio") != NULL, "deque_sacar_frente");
        AFIRMAR(strstr(out, "fin") != NULL, "deque_sacar_final");
        AFIRMAR(strstr(out, "medio") != NULL, "deque_queda");
    }

    /* ColaDoble: errores en vacia. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "d = coleccion.ColaDoble()\n"
            "intentar:\n"
            "    d.sacar_frente()\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"vacia\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "vacia") != NULL, "deque_sacar_vacio");
    }

    /* Uso real: balanceo de parentesis con Pila. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "funcion balanceado(s):\n"
            "    p = coleccion.Pila()\n"
            "    para c en s:\n"
            "        si c == \"(\":\n"
            "            p.poner(c)\n"
            "        sino si c == \")\":\n"
            "            si p.vacia():\n"
            "                retornar falso\n"
            "            fin si\n"
            "            p.sacar()\n"
            "        fin si\n"
            "    fin para\n"
            "    retornar p.vacia()\n"
            "fin funcion\n"
            "imprimir(balanceado(\"((1+2))\"))\n"
            "imprimir(balanceado(\"((1+2)\"))\n"
            "imprimir(balanceado(\"x*(y+z)\"))\n", out, sizeof(out));
        int n_verdaderos = 0, n_falsos = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_verdaderos++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_falsos++; p++; }
        AFIRMAR(n_verdaderos == 2 && n_falsos == 1, "uso_balanceo");
    }

    /* Uso real: BFS sobre grafo simple con Cola. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "grafo = {\"a\": [\"b\", \"c\"], \"b\": [\"d\"], \"c\": [\"d\"], \"d\": []}\n"
            "c = coleccion.Cola()\n"
            "c.poner(\"a\")\n"
            "vistos = conjunto()\n"
            "agregar(vistos, \"a\")\n"
            "orden = []\n"
            "mientras no c.vacia():\n"
            "    nodo = c.sacar()\n"
            "    agregar(orden, nodo)\n"
            "    para vec en grafo[nodo]:\n"
            "        si vec en vistos:\n"
            "            continuar\n"
            "        fin si\n"
            "        agregar(vistos, vec)\n"
            "        c.poner(vec)\n"
            "    fin para\n"
            "fin mientras\n"
            "imprimir(orden)\n", out, sizeof(out));
        /* BFS desde "a": orden esperado a, b, c, d. */
        AFIRMAR(strstr(out, "[\"a\", \"b\", \"c\", \"d\"]") != NULL,
                "uso_bfs");
    }

    if (fallos == 0) {
        printf("coleccion: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "coleccion: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
