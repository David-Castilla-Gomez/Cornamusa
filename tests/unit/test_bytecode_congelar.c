/*
 * Tests de `congelar(s)` (v1.164).
 *
 * Devuelve un conjunto inmutable y hashable (Python frozenset).
 * El original no se modifica. Acepta iterables y devuelve siempre
 * un nuevo conjunto frozen.
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
        "test_congelar_out.txt";
#else
        "/tmp/test_congelar_out.txt";
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
    /* congelar(set) -> hashable, mismo contenido */
    {
        char out[512];
        ejecutar_capturando(
            "s = {1, 2, 3}\n"
            "f = congelar(s)\n"
            "imprimir(longitud(f))\n"
            "imprimir(1 en f)\n"
            "imprimir(2 en f)\n"
            "imprimir(99 en f)\n"
            "imprimir(tipo(hash(f)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\nverdadero\nverdadero\nfalso\nentero") != NULL,
                "contenido_y_hash");
    }

    /* repr distinto: conjunto_fijo({...}) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(congelar({1, 2}))\n"
            "imprimir(congelar([]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "conjunto_fijo({") != NULL, "repr_no_vacio");
        AFIRMAR(strstr(out, "conjunto_fijo()") != NULL, "repr_vacio");
    }

    /* Hash es orden-independiente (XOR-based) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(hash(congelar([3, 1, 2])) == hash(congelar([1, 2, 3])))\n"
            "imprimir(hash(congelar([1, 2])) == hash(congelar([2, 1])))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nverdadero") != NULL, "orden_independiente");
    }

    /* El original no se modifica */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2}\n"
            "f = congelar(s)\n"
            "s.agregar(3)\n"
            "imprimir(longitud(s))\n"
            "imprimir(longitud(f))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\n2") != NULL, "no_aliasing");
    }

    /* Mutar frozen lanza ErrorDeTipo (cada metodo) */
    {
        char out[512];
        ejecutar_capturando(
            "f = congelar({1, 2})\n"
            "funcion p():\n"
            "    intentar:\n"
            "        f.agregar(3)\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok-ag\")\n"
            "    fin intentar\n"
            "    intentar:\n"
            "        f.quitar(1)\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok-qu\")\n"
            "    fin intentar\n"
            "    intentar:\n"
            "        f.descartar(1)\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok-de\")\n"
            "    fin intentar\n"
            "    intentar:\n"
            "        f.vaciar()\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok-va\")\n"
            "    fin intentar\n"
            "    intentar:\n"
            "        f.actualizar({99})\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok-ac\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok-ag") != NULL, "agregar_bloqueado");
        AFIRMAR(strstr(out, "ok-qu") != NULL, "quitar_bloqueado");
        AFIRMAR(strstr(out, "ok-de") != NULL, "descartar_bloqueado");
        AFIRMAR(strstr(out, "ok-va") != NULL, "vaciar_bloqueado");
        AFIRMAR(strstr(out, "ok-ac") != NULL, "actualizar_bloqueado");
    }

    /* Frozen como clave de dicc */
    {
        char out[256];
        ejecutar_capturando(
            "d = {}\n"
            "d[congelar({1, 2})] = \"a\"\n"
            "d[congelar({3, 4})] = \"b\"\n"
            "imprimir(d[congelar({1, 2})])\n"
            "imprimir(d[congelar({3, 4})])\n"
            "imprimir(longitud(d))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "a\nb\n2") != NULL, "frozen_como_clave");
    }

    /* Frozen como elemento de conjunto (dedup) */
    {
        char out[256];
        ejecutar_capturando(
            "cc = {congelar({1, 2}), congelar({3, 4}), congelar({1, 2})}\n"
            "imprimir(longitud(cc))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "frozen_dedup_en_set");
    }

    /* Iterar un frozen */
    {
        char out[256];
        ejecutar_capturando(
            "f = congelar({10, 20, 30})\n"
            "total = 0\n"
            "para x en f:\n"
            "    total = total + x\n"
            "fin para\n"
            "imprimir(total)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "60") != NULL, "iterar_frozen");
    }

    /* congelar acepta iterables */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud(congelar([1, 2, 2, 3])))\n"
            "imprimir(longitud(congelar((1, 2, 3, 1))))\n"
            "imprimir(longitud(congelar(\"hola\")))\n",
            /* "hola" -> {h,o,l,a} = 4 */
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\n3\n4") != NULL, "iterables");
    }

    /* congelar de frozen devuelve nuevo frozen (con copia) */
    {
        char out[256];
        ejecutar_capturando(
            "f1 = congelar({1, 2})\n"
            "f2 = congelar(f1)\n"
            "imprimir(hash(f1) == hash(f2))\n"
            "imprimir(longitud(f2))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\n2") != NULL, "frozen_de_frozen");
    }

    /* Aridad */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    congelar()\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "aridad");
    }

    /* Tipo invalido */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    congelar(42)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "tipo_invalido");
    }

    if (fallos == 0) {
        printf("congelar: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "congelar: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
