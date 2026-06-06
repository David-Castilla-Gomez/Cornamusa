/*
 * Tests de `lst.vaciar()` y `lst.extender(iterable)` (v1.155).
 *
 * Paralelos a `d.vaciar()` y `d.actualizar()` de v1.150/v1.151
 * pero para listas. Python tiene list.clear() y list.extend() como
 * basicos.
 *
 * Cornamusa ya tenia agregar/quitar/insertar/contar/contiene/
 * copiar/indice_de/ordenar/invertir. Faltaba vaciar (limpiar
 * todo in-place) y extender (anadir todos los elementos de un
 * iterable).
 *
 * `extender` acepta lista, tupla, cadena (cada code-point), y
 * conjunto. Mismo iterable que en bucles `para X en it`.
 * Detalle: si se pasa la propia lista (auto-extender), se toma
 * snapshot de la cuenta para evitar bucle infinito (paridad con
 * Python).
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
        "test_lst_vacext_out.txt";
#else
        "/tmp/test_lst_vacext_out.txt";
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
    /* vaciar lista no vacia */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3, 4, 5]\n"
            "xs.vaciar()\n"
            "imprimir(longitud(xs))\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "vaciar_longitud");
        AFIRMAR(strstr(out, "[]") != NULL, "vaciar_repr");
    }

    /* vaciar ya vacia */
    {
        char out[256];
        ejecutar_capturando(
            "xs = []\n"
            "xs.vaciar()\n"
            "imprimir(longitud(xs))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "vaciar_ya_vacia");
    }

    /* Lista reutilizable tras vaciar */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "xs.vaciar()\n"
            "xs.agregar(99)\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[99]") != NULL, "vaciar_y_reusar");
    }

    /* extender con lista */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "xs.extender([10, 20])\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 10, 20]") != NULL, "ext_lista");
    }

    /* extender con tupla */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1]\n"
            "xs.extender((10, 20, 30))\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 10, 20, 30]") != NULL, "ext_tupla");
    }

    /* extender con cadena: cada code-point como cadena */
    {
        char out[256];
        ejecutar_capturando(
            "xs = []\n"
            "xs.extender(\"abc\")\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"b\", \"c\"]") != NULL, "ext_cadena");
    }

    /* extender con conjunto: longitud aumenta por # de elementos */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2]\n"
            "xs.extender({10, 20, 30})\n"
            "imprimir(longitud(xs))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "ext_conjunto");
    }

    /* extender con vacio no cambia */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "xs.extender([])\n"
            "imprimir(xs)\n"
            "imprimir(longitud(xs))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "ext_vacio_no_cambia");
        AFIRMAR(strstr(out, "3") != NULL, "ext_vacio_long");
    }

    /* Auto-extender: snapshot de cuenta evita bucle infinito */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "xs.extender(xs)\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 1, 2, 3]") != NULL, "ext_self");
    }

    /* Error: extender con no-iterable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    [1, 2].extender(42)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "ext_no_iter");
    }

    /* Error: extender sobre tupla (no muta) */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    (1, 2).extender([3])\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "extender_no_tupla");
    }

    if (fallos == 0) {
        printf("lst_vacext: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "lst_vacext: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
