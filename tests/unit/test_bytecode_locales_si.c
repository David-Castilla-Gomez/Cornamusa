/*
 * Tests de regresion (v1.95): nuevos locales declarados dentro de
 * ramas de un `si` deben quedar correctamente reservados en stack
 * antes de las ramas, no dentro de una rama particular.
 *
 * Bug original (v1.94): el `OP_NULO + agregar_local` se emitia
 * dentro de la rama del `si`. Si esa rama no se ejecutaba, el slot
 * en stack quedaba desalineado y `OP_ASIGNAR_LOCAL slot` desde otra
 * rama pisaba memoria equivocada. Manifestacion tipica: dentro de
 * un metodo de clase importada, la variable post-si quedaba con un
 * valor de la pool de constantes (la cadena siguiente).
 *
 * Fix: pre-declarar todos los identificadores que se asignan en
 * cualquier rama del `si` ANTES de las ramas (un solo OP_NULO +
 * agregar_local por nombre).
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
        "test_lsi_out.txt";
#else
        "/tmp/test_lsi_out.txt";
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
    /* Caso original: si/sino con asignacion en ambas ramas, una
     * con acceso a atributo. Tomamos la rama "sino" (otro es cadena,
     * no instancia). */
    {
        char out[1024];
        ejecutar_capturando(
            "funcion m(otro):\n"
            "    si tipo(otro) == \"instancia\":\n"
            "        v = otro.s\n"
            "    sino:\n"
            "        v = otro\n"
            "    fin si\n"
            "    imprimir(\"v:\", v)\n"
            "    retornar v\n"
            "fin funcion\n"
            "imprimir(\"r:\", m(\"hello\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "v: hello") != NULL, "v_correcta_post_si");
        AFIRMAR(strstr(out, "r: hello") != NULL, "retorno_correcto");
    }

    /* Tomar la rama "si" en lugar de "sino" */
    {
        char out[1024];
        ejecutar_capturando(
            "clase Caja:\n"
            "    funcion __iniciar__(yo, s):\n"
            "        yo.s = s\n"
            "    fin funcion\n"
            "fin clase\n"
            "funcion m(otro):\n"
            "    si tipo(otro) == \"instancia\":\n"
            "        v = otro.s\n"
            "    sino:\n"
            "        v = otro\n"
            "    fin si\n"
            "    retornar v\n"
            "fin funcion\n"
            "imprimir(m(Caja(\"hola\")))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "hola") != NULL, "rama_si_correcta");
    }

    /* Mismo bug pero en metodo de clase (caso que motivo el fix) */
    {
        char out[1024];
        ejecutar_capturando(
            "clase X:\n"
            "    funcion m(yo, otro):\n"
            "        si tipo(otro) == \"instancia\":\n"
            "            v = otro.s\n"
            "        sino:\n"
            "            v = otro\n"
            "        fin si\n"
            "        retornar v\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(X().m(\"abc\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "abc") != NULL, "metodo_clase_post_si");
    }

    /* Cadena de sino si: declara variable en rama del medio */
    {
        char out[1024];
        ejecutar_capturando(
            "funcion clasificar(n):\n"
            "    si n < 0:\n"
            "        etiqueta = \"negativo\"\n"
            "    sino si n == 0:\n"
            "        etiqueta = \"cero\"\n"
            "    sino:\n"
            "        etiqueta = \"positivo\"\n"
            "    fin si\n"
            "    retornar etiqueta\n"
            "fin funcion\n"
            "imprimir(clasificar(-5))\n"
            "imprimir(clasificar(0))\n"
            "imprimir(clasificar(7))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "negativo") != NULL, "sino_si_neg");
        AFIRMAR(strstr(out, "cero") != NULL, "sino_si_cero");
        AFIRMAR(strstr(out, "positivo") != NULL, "sino_si_pos");
    }

    /* Multiples variables nuevas en distintas ramas */
    {
        char out[1024];
        ejecutar_capturando(
            "funcion m(c):\n"
            "    si c == 1:\n"
            "        a = \"uno\"\n"
            "        b = 10\n"
            "    sino:\n"
            "        a = \"otro\"\n"
            "        b = 20\n"
            "    fin si\n"
            "    retornar a + \":\" + cadena(b)\n"
            "fin funcion\n"
            "imprimir(m(1))\n"
            "imprimir(m(2))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "uno:10") != NULL, "vars_multiples_si");
        AFIRMAR(strstr(out, "otro:20") != NULL, "vars_multiples_sino");
    }

    /* Si anidado: variable declarada en sub-si */
    {
        char out[1024];
        ejecutar_capturando(
            "funcion m(a, b):\n"
            "    si a:\n"
            "        si b:\n"
            "            v = \"ab\"\n"
            "        sino:\n"
            "            v = \"a_no_b\"\n"
            "        fin si\n"
            "    sino:\n"
            "        v = \"no_a\"\n"
            "    fin si\n"
            "    retornar v\n"
            "fin funcion\n"
            "imprimir(m(verdadero, verdadero))\n"
            "imprimir(m(verdadero, falso))\n"
            "imprimir(m(falso, falso))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "ab") != NULL, "si_anidado_ab");
        AFIRMAR(strstr(out, "a_no_b") != NULL, "si_anidado_a_no_b");
        AFIRMAR(strstr(out, "no_a") != NULL, "si_anidado_no_a");
    }

    /* Variable declarada solo en una rama (sino sin asignacion) —
     * fuera del si la variable queda nulo si la rama no se ejecuto.
     * No es un bug; es comportamiento esperado. */
    {
        char out[1024];
        ejecutar_capturando(
            "funcion m(c):\n"
            "    si c:\n"
            "        v = \"asignada\"\n"
            "    fin si\n"
            "    retornar v\n"
            "fin funcion\n"
            "imprimir(m(verdadero))\n"
            "imprimir(m(falso))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "asignada") != NULL, "una_rama_asignada");
        AFIRMAR(strstr(out, "nulo") != NULL, "una_rama_no_asignada_nulo");
    }

    if (fallos == 0) {
        printf("locales_si: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "locales_si: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
