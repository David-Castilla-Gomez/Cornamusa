/*
 * Tests de `conj.vaciar()`, `conj.actualizar(it)` y `conj.descartar(e)`
 * (v1.156).
 *
 * Cierra la simetria de operaciones bulk entre los 3 contenedores
 * principales:
 *
 *                   clear      pop / remove silent     extend / union
 *   list (v0.6/55)  vaciar     quitar / -              extender
 *   dict (v1.150/51) vaciar    sacar / -               actualizar
 *   set (v1.156)    vaciar     quitar / descartar      actualizar
 *
 * `descartar` es la version "silenciosa" de `quitar`: si el
 * elemento no esta presente, no lanza (paridad con
 * Python set.discard, vs set.remove que lanza KeyError).
 *
 * `actualizar` acepta cualquier iterable (conjunto, lista, tupla,
 * cadena code-points). Idempotente sobre duplicados — la semantica
 * de set.
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
        "test_conj_vad_out.txt";
#else
        "/tmp/test_conj_vad_out.txt";
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
    /* vaciar: conjunto no-vacio */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2, 3}\n"
            "s.vaciar()\n"
            "imprimir(longitud(s))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "vaciar_no_vacio");
    }

    /* vaciar: ya vacio (no crashea) */
    {
        char out[256];
        ejecutar_capturando(
            "s = conjunto()\n"
            "s.vaciar()\n"
            "imprimir(longitud(s))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "vaciar_ya_vacio");
    }

    /* Reutilizable tras vaciar */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2, 3}\n"
            "s.vaciar()\n"
            "s.agregar(99)\n"
            "imprimir(longitud(s))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "vaciar_reutilizable");
    }

    /* actualizar con conjunto: duplicados se ignoran */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2, 3}\n"
            "s.actualizar({3, 4, 5})\n"
            "imprimir(longitud(s))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "act_conj");
    }

    /* actualizar con lista */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2}\n"
            "s.actualizar([10, 20, 30])\n"
            "imprimir(longitud(s))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "act_lista");
    }

    /* actualizar con tupla */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1}\n"
            "s.actualizar((10, 20))\n"
            "imprimir(longitud(s))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "act_tupla");
    }

    /* actualizar con cadena */
    {
        char out[256];
        ejecutar_capturando(
            "s = {\"x\"}\n"
            "s.actualizar(\"abc\")\n"
            "imprimir(longitud(s))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "act_cadena");
    }

    /* descartar elemento presente */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2, 3}\n"
            "s.descartar(2)\n"
            "imprimir(longitud(s))\n"
            "imprimir(2 en s)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL && strstr(out, "falso") != NULL,
                "descartar_quita");
    }

    /* descartar elemento ausente NO lanza */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2}\n"
            "s.descartar(99)\n"
            "imprimir(longitud(s))\n"
            "imprimir(\"sin_excepcion\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "sin_excepcion") != NULL, "descartar_silencioso");
    }

    /* Comparativa: quitar sí lanza si no existe */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    {1, 2}.quitar(99)\n"
            "atrapar ErrorDeClave:\n"
            "    imprimir(\"ok lanza\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok lanza") != NULL, "quitar_lanza");
    }

    /* Error: actualizar con no-iterable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    {1, 2}.actualizar(42)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "act_no_iter");
    }

    /* vaciar rechaza no-conjunto */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    (1, 2).vaciar()\n"   /* tupla no tiene vaciar */
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "vaciar_no_conj");
    }

    if (fallos == 0) {
        printf("conj_vad: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "conj_vad: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
