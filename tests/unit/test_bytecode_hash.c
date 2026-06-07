/*
 * Tests de `hash(x)` (v1.163).
 *
 * Expone el hash interno que usan dicc y conjunto para indexar.
 * Mismo valor para claves iguales; rechaza no-hashables (lista,
 * dicc, conjunto) con ErrorDeTipo atrapable.
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
        "test_hash_out.txt";
#else
        "/tmp/test_hash_out.txt";
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
    /* Consistencia: misma clave -> mismo hash */
    {
        char out[512];
        ejecutar_capturando(
            "imprimir(hash(\"hola\") == hash(\"hola\"))\n"
            "imprimir(hash(42) == hash(42))\n"
            "imprimir(hash((1, 2, 3)) == hash((1, 2, 3)))\n"
            "imprimir(hash(nulo) == hash(nulo))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nverdadero\nverdadero\nverdadero") != NULL,
                "consistencia");
    }

    /* Distintas claves -> distinto hash (caso casi-seguro, no garantia) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(hash(\"hola\") != hash(\"adios\"))\n"
            "imprimir(hash(1) != hash(2))\n"
            "imprimir(hash((1, 2)) != hash((2, 1)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nverdadero\nverdadero") != NULL,
                "discriminacion");
    }

    /* Devuelve un entero */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(tipo(hash(\"x\")))\n"
            "imprimir(tipo(hash(42)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "entero\nentero") != NULL, "tipo_entero");
    }

    /* Booleanos, nulo y decimal son hashables */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(tipo(hash(verdadero)))\n"
            "imprimir(tipo(hash(falso)))\n"
            "imprimir(tipo(hash(3.14)))\n"
            "imprimir(tipo(hash(())))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "entero\nentero\nentero\nentero") != NULL,
                "primitivos");
    }

    /* Bignum tambien es hashable */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(tipo(hash(10**40)))\n"
            "imprimir(hash(10**40) == hash(10**40))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "entero\nverdadero") != NULL, "bignum");
    }

    /* Lista NO hashable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    hash([1, 2])\n"
            "    imprimir(\"no-error\")\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "lista_rechaza");
    }

    /* Dicc NO hashable */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    intentar:\n"
            "        hash({1: 2})\n"
            "        imprimir(\"no-error\")\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "dicc_rechaza");
    }

    /* Conjunto NO hashable (mutable) */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    intentar:\n"
            "        hash({1, 2})\n"
            "        imprimir(\"no-error\")\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "conjunto_rechaza");
    }

    /* Aridad: 0 args -> error */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    hash()\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "aridad_cero");
    }

    /* Aridad: 2 args -> error */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    hash(1, 2)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "aridad_dos");
    }

    /* Tupla anidada con valores hashables */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(hash(((1, 2), (3, 4))) == hash(((1, 2), (3, 4))))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "tupla_anidada");
    }

    /* Tupla con lista dentro -> no hashable (la lista la rompe) */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    hash((1, [2, 3]))\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "tupla_con_lista");
    }

    if (fallos == 0) {
        printf("hash: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "hash: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
