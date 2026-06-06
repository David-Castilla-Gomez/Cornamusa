/*
 * Tests de `minimo` y `maximo` con parametro `clave` (v1.144).
 *
 * Pre v1.144 ambos solo aceptaban un iterable y comparaban con < / >
 * directamente sobre los elementos. v1.144 anade `clave=nulo` opcional
 * (paridad con Python `min(..., key=...)`): si se pasa, se compara
 * `clave(elemento)` pero se devuelve el ELEMENTO ORIGINAL.
 *
 * Sin cambios a VM, bytecode ni firma de llamada (las kwargs en metodos
 * ya iban por v1.143; aqui es kwarg sobre funcion libre, soportada
 * desde mucho antes).
 *
 * Cambio en stdlib/funcionales.cor — la maquinaria nueva de v1.142 y
 * v1.143 (kwargs en metodos, *args/**kw) NO se necesita aqui porque
 * `minimo`/`maximo` son funciones libres; solo se anade un parametro
 * con default.
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
        "test_min_max_out.txt";
#else
        "/tmp/test_min_max_out.txt";
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
    /* Regresion: sin clave, comparacion natural */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar minimo, maximo\n"
            "imprimir(minimo([3, 1, 4, 1, 5, 9, 2, 6]))\n"
            "imprimir(maximo([3, 1, 4, 1, 5, 9, 2, 6]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "minimo_sin_clave");
        AFIRMAR(strstr(out, "9") != NULL, "maximo_sin_clave");
    }

    /* Clave por segundo elemento de la tupla */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar minimo, maximo\n"
            "xs = [(1, \"z\"), (2, \"a\"), (3, \"m\")]\n"
            "imprimir(minimo(xs, clave=lambda p: p[1]))\n"
            "imprimir(maximo(xs, clave=lambda p: p[1]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(2, \"a\")") != NULL, "minimo_clave_tupla");
        AFIRMAR(strstr(out, "(1, \"z\")") != NULL, "maximo_clave_tupla");
    }

    /* Clave sobre atributo de instancia (caso real) */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar minimo, maximo\n"
            "clase Persona:\n"
            "    funcion __iniciar__(yo, nombre, edad):\n"
            "        yo.nombre = nombre\n"
            "        yo.edad = edad\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar yo.nombre\n"
            "    fin funcion\n"
            "fin clase\n"
            "gente = [Persona(\"Ana\", 30), Persona(\"Bea\", 25), Persona(\"Carlos\", 35)]\n"
            "imprimir(minimo(gente, clave=lambda p: p.edad))\n"
            "imprimir(maximo(gente, clave=lambda p: p.edad))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Bea") != NULL, "minimo_persona");
        AFIRMAR(strstr(out, "Carlos") != NULL, "maximo_persona");
    }

    /* Iterable vacio sigue lanzando ErrorDeValor */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar minimo, maximo\n"
            "intentar:\n"
            "    minimo([])\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"min vacio\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    maximo([], clave=lambda x: x)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"max vacio\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "min vacio") != NULL, "minimo_vacio");
        AFIRMAR(strstr(out, "max vacio") != NULL, "maximo_vacio");
    }

    /* Estabilidad: si dos elementos tienen la misma clave, se queda el
     * PRIMERO encontrado (porque usamos `<` y `>` estrictos). Esto
     * iguala el comportamiento de Python. */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar minimo, maximo\n"
            "xs = [(1, \"a\"), (2, \"a\"), (3, \"a\")]\n"
            "imprimir(minimo(xs, clave=lambda p: p[1]))\n"
            "imprimir(maximo(xs, clave=lambda p: p[1]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, \"a\")") != NULL, "estabilidad");
    }

    /* Iterable de un solo elemento */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar minimo, maximo\n"
            "imprimir(minimo([42]))\n"
            "imprimir(maximo([42], clave=lambda x: -x))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "un_elemento");
    }

    if (fallos == 0) {
        printf("min_max_clave: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "min_max_clave: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
