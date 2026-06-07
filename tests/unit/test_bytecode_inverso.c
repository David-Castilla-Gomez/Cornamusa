/*
 * Tests de `inverso(iterable)` (v1.160).
 *
 * Cornamusa tenia:
 *   lst.invertir()  - muta in-place. No sirve si quieres una copia.
 *   xs[::-1]        - slice inverso. Solo lista/cadena/tupla.
 *
 * Faltaba la version idiomatica de Python `list(reversed(it))`:
 *
 *   inverso(iterable)
 *
 * Acepta lista, tupla, cadena (code-points UTF-8) y conjunto.
 * Siempre devuelve una NUEVA LISTA, sin mutar el original.
 *
 * Rango se rechaza con error claro sugiriendo
 * `inverso(lista(rango(...)))` — su iteracion lazy haria mas
 * complejo el codigo de inverso() sin ganancia practica.
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
        "test_inverso_out.txt";
#else
        "/tmp/test_inverso_out.txt";
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
    /* lista: no muta el original */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3, 4, 5]\n"
            "imprimir(inverso(xs))\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[5, 4, 3, 2, 1]") != NULL, "inv_lista");
        AFIRMAR(strstr(out, "[1, 2, 3, 4, 5]") != NULL, "no_muta");
    }

    /* Lista vacia y singleton */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(inverso([]))\n"
            "imprimir(inverso([42]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "lista_vacia");
        AFIRMAR(strstr(out, "[42]") != NULL, "lista_singleton");
    }

    /* Tupla → lista */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(inverso((10, 20, 30)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[30, 20, 10]") != NULL, "inv_tupla");
    }

    /* Cadena ASCII → lista de cadenas de 1 cp */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(inverso(\"hola\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"l\", \"o\", \"h\"]") != NULL,
                "inv_ascii");
    }

    /* Cadena Unicode: 4 code-points, no 6 bytes */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(inverso(\"\xc3\xb1" "o\xc3\xb1" "o\"))\n"  /* ñoño */
            "imprimir(longitud(inverso(\"\xc3\xb1" "o\xc3\xb1" "o\")))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "inv_unicode_longitud");
        /* Cada ñ se preserva como cadena de 1 cp = 2 bytes UTF-8 */
        AFIRMAR(strstr(out, "\xc3\xb1") != NULL, "inv_unicode_contiene");
    }

    /* Cadena vacia */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(inverso(\"\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "inv_cadena_vacia");
    }

    /* Conjunto: orden no determinista, pero conserva longitud */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud(inverso({1, 2, 3, 4, 5})))\n"
            "imprimir(tipo(inverso({1, 2, 3})))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "inv_conjunto_longitud");
        AFIRMAR(strstr(out, "lista") != NULL, "inv_conjunto_tipo");
    }

    /* Uso idiomatico: iterar al reves sin copia visible */
    {
        char out[256];
        ejecutar_capturando(
            "salida = []\n"
            "para x en inverso([1, 2, 3]):\n"
            "    agregar(salida, x)\n"
            "fin para\n"
            "imprimir(salida)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 2, 1]") != NULL, "iter_inverso");
    }

    /* Doble inverso = lista original (como lista) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(inverso(inverso([1, 2, 3])))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "doble_inverso");
    }

    /* Error: no-iterable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    inverso(42)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "no_iter");
    }

    /* Error: rango (con sugerencia de workaround) */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    inverso(rango(0, 5))\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n"
            /* Workaround: convertir a lista primero */
            "imprimir(inverso(lista(rango(0, 5))))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "rango_rechaza");
        AFIRMAR(strstr(out, "[4, 3, 2, 1, 0]") != NULL, "rango_workaround");
    }

    if (fallos == 0) {
        printf("inverso: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "inverso: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
