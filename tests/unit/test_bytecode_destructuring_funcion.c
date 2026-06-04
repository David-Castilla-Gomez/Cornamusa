/*
 * Tests de destructuring/asignacion multiple dentro de funciones.
 *
 * Bug detectado en fase 1 del plan de correccion del corpus:
 *   - x, z = 3, 4 dentro de funcion funcionaba (slots nuevos
 *     correctamente reservados desde v1.28).
 *   - a, b = b, a swap simple dentro de funcion: aparentemente OK
 *     en una sola iteracion.
 *   - a, b = b, a + b DENTRO DE UN BUCLE en funcion: solo se
 *     ejecutaba la primera iteracion. Causa: slot_iter (la tupla
 *     RHS) se reservaba en compile time pero NUNCA se descartaba,
 *     asi que la pila crecia +1 por iter y OP_OBTENER_LOCAL
 *     slot_iter seguia leyendo la tupla de la PRIMERA iter.
 *     Sintoma observable: examples/03_fibonacci.cor imprimia
 *     fib(n) = 0 para todo n.
 *
 * Fix en src/compilador.c: cuando los destinos del destructuring
 * son TODOS variables existentes (local/upvalue), reusar el slot
 * en vez de pre-reservar otro, y descartar slot_iter del stack
 * tras las asignaciones. Caso patologico restante (destructurar
 * variables NUEVAS dentro de un bucle) documentado en el codigo.
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
        "test_destr_func_out.txt";
#else
        "/tmp/test_destr_func_out.txt";
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
    /* Destructuring de literal dentro de funcion (variables nuevas) */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    x, z = 3, 4\n"
            "    imprimir(x, z)\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3 4") != NULL, "literal_nuevas_en_funcion");
    }

    /* Swap simple dentro de funcion (variables existentes) */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    a = 1\n"
            "    b = 2\n"
            "    a, b = b, a\n"
            "    imprimir(a, b)\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2 1") != NULL, "swap_funcion");
    }

    /* Fibonacci con destructuring en bucle dentro de funcion */
    {
        char out[512];
        ejecutar_capturando(
            "funcion fib(n):\n"
            "    a = 0\n"
            "    b = 1\n"
            "    para _ en rango(n):\n"
            "        a, b = b, a + b\n"
            "    fin para\n"
            "    retornar a\n"
            "fin funcion\n"
            "imprimir(fib(0))\n"
            "imprimir(fib(1))\n"
            "imprimir(fib(5))\n"
            "imprimir(fib(10))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL,  "fib_0");
        AFIRMAR(strstr(out, "1") != NULL,  "fib_1");
        AFIRMAR(strstr(out, "5") != NULL,  "fib_5");
        AFIRMAR(strstr(out, "55") != NULL, "fib_10");
    }

    /* Swap en bucle muchas iteraciones - el stack NO debe crecer */
    {
        char out[256];
        ejecutar_capturando(
            "funcion oscilar(n):\n"
            "    a = 0\n"
            "    b = 1\n"
            "    para _ en rango(n):\n"
            "        a, b = b, a\n"
            "    fin para\n"
            "    retornar a + b * 10\n"
            "fin funcion\n"
            "imprimir(oscilar(1))\n"
            "imprimir(oscilar(2))\n"
            "imprimir(oscilar(1000))\n",
            out, sizeof(out));
        /* iter par: 0,1 -> 1,0 -> 0,1; a+b*10 = 0+10 = 10
           iter impar: 0,1 -> 1,0; a+b*10 = 1+0 = 1 */
        AFIRMAR(strstr(out, "1") != NULL, "swap_loop_1");
        AFIRMAR(strstr(out, "10") != NULL, "swap_loop_2");
    }

    /* Destructuring desde funcion que devuelve tupla */
    {
        char out[256];
        ejecutar_capturando(
            "funcion par():\n"
            "    retornar (10, 20)\n"
            "fin funcion\n"
            "funcion usar():\n"
            "    a, b = par()\n"
            "    imprimir(a, b)\n"
            "fin funcion\n"
            "usar()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10 20") != NULL, "destr_de_funcion");
    }

    /* Asignar a upvalue capturado: destructuring debe llegar al
       OP_ASIGNAR_UPVALUE de la outer var. */
    {
        char out[256];
        ejecutar_capturando(
            "funcion outer():\n"
            "    a = 0\n"
            "    b = 0\n"
            "    funcion inner():\n"
            "        a, b = 100, 200\n"
            "    fin funcion\n"
            "    inner()\n"
            "    imprimir(a, b)\n"
            "fin funcion\n"
            "outer()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "100 200") != NULL, "destr_upvalue");
    }

    if (fallos == 0) {
        printf("destr_funcion: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "destr_funcion: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
