/*
 * Tests: los builtins nativos consumen generadores (v1.200).
 *
 * Bug de correccion SILENCIOSA arreglado: el Iterador generico
 * (iter_nuevo/iter_siguiente en valor.c) no tenia case VAL_GENERADOR
 * y caia al `default`, devolviendo false al primer paso. Como
 * `valor_es_iterable` SI aceptaba VAL_GENERADOR, el generador pasaba
 * el guard pero se iteraba como VACIO sin error:
 *     lista(gen())  -> []   (deberia [3, 1, 2])
 *     suma(gen())   -> 0    (deberia 6)
 *     maximo(gen()) -> ErrorDeValor "iterable vacio"
 * Todo con exit 0, violando la doc ("cualquier iterable").
 *
 * Fix: hook valor_set_gen_paso_hook registrado por la VM; iter_siguiente
 * delega VAL_GENERADOR en vm_generador_paso. Todos los builtins que
 * usan el iterador generico (lista, tupla, diccionario, suma, minimo,
 * maximo, ordenado, cualquiera, todos, juntar, mapear, filtrar,
 * reducir) + enumerar heredan el soporte.
 *
 * CLAVE de correccion: si el generador LANZA a mitad, el error se
 * propaga; no se traga como fin de iteracion ni se enmascara con
 * "iterable vacio" (caso maximo/reducir).
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
        "test_builtins_gen_out.txt";
#else
        "/tmp/test_builtins_gen_out.txt";
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

/* Prefijo de generador reutilizado: produce 3, 1, 2. */
#define GEN3 \
    "funcion g3():\n" \
    "    producir 3\n" \
    "    producir 1\n" \
    "    producir 2\n" \
    "fin funcion\n"

int main(void) {
    /* Camino feliz: cada builtin que itera consume el generador. */
    {
        char out[512];
        int rc = ejecutar_capturando(
            GEN3
            "imprimir(\"L\", lista(g3()))\n"
            "imprimir(\"T\", tupla(g3()))\n"
            "imprimir(\"S\", suma(g3()))\n"
            "imprimir(\"MX\", maximo(g3()))\n"
            "imprimir(\"MN\", minimo(g3()))\n"
            "imprimir(\"O\", ordenado(g3()))\n"
            "imprimir(\"E\", enumerar(g3()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "feliz_rc");
        AFIRMAR(strstr(out, "L [3, 1, 2]") != NULL, "lista");
        AFIRMAR(strstr(out, "T (3, 1, 2)") != NULL, "tupla");
        AFIRMAR(strstr(out, "S 6") != NULL, "suma");
        AFIRMAR(strstr(out, "MX 3") != NULL, "maximo");
        AFIRMAR(strstr(out, "MN 1") != NULL, "minimo");
        AFIRMAR(strstr(out, "O [1, 2, 3]") != NULL, "ordenado");
        AFIRMAR(strstr(out, "E [(0, 3), (1, 1), (2, 2)]") != NULL, "enumerar");
    }

    /* cualquiera/todos/juntar/diccionario sobre generador. */
    {
        char out[512];
        int rc = ejecutar_capturando(
            GEN3
            "imprimir(\"C\", cualquiera(g3()))\n"
            "imprimir(\"A\", todos(g3()))\n"
            "imprimir(\"J\", juntar(g3(), [\"a\", \"b\"]))\n"
            "funcion pares():\n"
            "    producir (\"x\", 1)\n"
            "    producir (\"y\", 2)\n"
            "fin funcion\n"
            "imprimir(\"D\", diccionario(pares()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "varios_rc");
        AFIRMAR(strstr(out, "C verdadero") != NULL, "cualquiera");
        AFIRMAR(strstr(out, "A verdadero") != NULL, "todos");
        AFIRMAR(strstr(out, "J [(3, \"a\"), (1, \"b\")]") != NULL, "juntar");
        AFIRMAR(strstr(out, "D {\"x\": 1, \"y\": 2}") != NULL, "diccionario");
    }

    /* Generador inline (genex) como argumento. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "imprimir(suma((nx * nx para nx en rango(4))))\n"
            "imprimir(lista((nx para nx en rango(3))))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "genex_rc");
        AFIRMAR(strstr(out, "14") != NULL, "genex_suma");
        AFIRMAR(strstr(out, "[0, 1, 2]") != NULL, "genex_lista");
    }

    /* Generador vacio: [] y 0, sin error. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion vacio():\n"
            "    si falso:\n"
            "        producir 1\n"
            "    fin si\n"
            "fin funcion\n"
            "imprimir(lista(vacio()))\n"
            "imprimir(suma(vacio()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "vacio_rc");
        AFIRMAR(strstr(out, "[]") != NULL, "vacio_lista");
        AFIRMAR(strstr(out, "\n0") != NULL || strstr(out, "[]\n0") != NULL,
                "vacio_suma");
    }

    /* Generador reusado: el segundo consumo lo ve agotado. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            GEN3
            "g = g3()\n"
            "imprimir(longitud(lista(g)))\n"
            "imprimir(longitud(lista(g)))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "reuso_rc");
        AFIRMAR(strstr(out, "3\n0") != NULL, "reuso_agotado");
    }

    /* Propagacion de error: el generador lanza a mitad; suma propaga. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion boom():\n"
            "    producir 10\n"
            "    lanzar ErrorDeValor(\"boom\")\n"
            "fin funcion\n"
            "intentar:\n"
            "    imprimir(suma(boom()))\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"prop:\", cadena(e))\n"
            "fin intentar\n"
            "imprimir(\"fin\")\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "prop_rc");
        AFIRMAR(strstr(out, "prop: ErrorDeValor: boom") != NULL, "prop_suma");
        AFIRMAR(strstr(out, "fin") != NULL, "prop_post");
    }

    /* Error ANTES del primer producir: maximo/reducir NO lo enmascaran
     * con "iterable vacio" — propagan el error real. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion boom0():\n"
            "    lanzar ErrorDeValor(\"antes\")\n"
            "    producir 1\n"
            "fin funcion\n"
            "intentar:\n"
            "    maximo(boom0())\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"M:\", cadena(e))\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "mask_rc");
        AFIRMAR(strstr(out, "M: ErrorDeValor: antes") != NULL, "no_enmascara");
        AFIRMAR(strstr(out, "vacio") == NULL, "no_dice_vacio");
    }

    /* Regresion del UAF de GC (v1.200): un generador que dispara el GC
     * (recolectar()) mientras un builtin lo consume. El Iterador y el
     * acumulador de la nativa son C-locals NO enraizados; antes del fix
     * el sweep los barria -> heap corruption / RC=127 determinista.
     * Tras el fix: el GC se difiere durante el sub-dispatch y el
     * generador en la pila se marca (case VAL_GENERADOR). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion gen():\n"
            "    recolectar()\n"
            "    producir 1\n"
            "    recolectar()\n"
            "    producir 2\n"
            "    recolectar()\n"
            "    producir 3\n"
            "fin funcion\n"
            "imprimir(lista(gen()))\n"
            "imprimir(suma(gen()))\n"
            "imprimir(ordenado(gen()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "gc_uaf_rc");
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "gc_uaf_lista");
        AFIRMAR(strstr(out, "\n6\n") != NULL || strstr(out, "6") != NULL,
                "gc_uaf_suma");
    }

    /* Regresion del UAF pre-existente: `para x en gen()` donde el
     * generador dispara GC y produce un objeto heap. El generador vivia
     * en el slot del bucle sin que gc_marcar_valor lo marcara. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion gen():\n"
            "    ax = [111, 222, 333]\n"
            "    recolectar()\n"
            "    producir ax\n"
            "fin funcion\n"
            "para vx en gen():\n"
            "    imprimir(\"got:\", vx)\n"
            "fin para\n"
            "imprimir(\"done\")\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "gc_para_rc");
        AFIRMAR(strstr(out, "[111, 222, 333]") != NULL, "gc_para_obj");
        AFIRMAR(strstr(out, "done") != NULL, "gc_para_done");
    }

    if (fallos == 0) {
        printf("builtins_generador: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "builtins_generador: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
