/*
 * Tests del caso patologico residual de v1.122 fase 1, cerrado en v1.123:
 *
 *   funcion f():
 *       a = 0
 *       b = 0
 *       para i en rango(0, N):
 *           a, b = i, i + 1   # variables NUEVAS por iter
 *       fin para
 *
 * El fix de v1.122 fase 1 dejo el caso "destinos pre-existentes" verde
 * (Fibonacci), pero documento la limitacion para destinos NUEVOS dentro
 * de un bucle: cada iter empujaba +N slots fantasma al stack y la lectura
 * OP_OBTENER_LOCAL slot_iter siempre devolvia la tupla de la PRIMERA iter.
 *
 * Fix v1.123: extender pre_reservar_locales para reconocer SENT_ASIGNAR
 * con destino EXPR_TUPLA/EXPR_LISTA (no solo EXPR_IDENT). Recorre los
 * destinos y pre-reserva sus slots IDENT antes del bucle. Cuando
 * emitir_destructuring se ejecuta dentro del bucle, los destinos ya son
 * locales "existentes" y reusan sus slots — n_nuevos_slots = 0 y la
 * rama que descarta slot_iter limpia el stack en cada iter.
 *
 * Tentativa descartada: hacer que pre_reservar_locales del cuerpo de la
 * funcion descendiera en bucles. Eso movia las variables al scope de la
 * funcion para que sobreviviesen al bucle (mas parecido a Python), pero
 * rompia `nolocal n` porque pre-declaraba n como local antes de procesar
 * la declaracion. Decision: mantener el scoping clasico (variables del
 * cuerpo del bucle no sobreviven al bucle, igual que Lua) y limitarse a
 * arreglar el bug del stack-crece.
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
        "test_destr_buc_out.txt";
#else
        "/tmp/test_destr_buc_out.txt";
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
    /* Caso 1: destructuring de NUEVAS variables dentro de un bucle
       sin pre-declararlas — el cuerpo del bucle DEBE actualizar a y b
       correctamente en cada iter (el bug previo: a y b se quedaban
       siempre en 0). */
    {
        char out[512];
        ejecutar_capturando(
            "funcion f():\n"
            "    para i en rango(0, 5):\n"
            "        a, b = i, i * 2\n"
            "        imprimir(a, b)\n"
            "    fin para\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 0") != NULL, "iter_0");
        AFIRMAR(strstr(out, "1 2") != NULL, "iter_1");
        AFIRMAR(strstr(out, "4 8") != NULL, "iter_4");
    }

    /* Caso 2: estres - 1000 iteraciones no deben acumular stack.
       Con pre-declarar a y b la lectura final debe dar [999, 1000]. */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    a = 0\n"
            "    b = 0\n"
            "    para i en rango(0, 1000):\n"
            "        a, b = i, i + 1\n"
            "    fin para\n"
            "    retornar [a, b]\n"
            "fin funcion\n"
            "imprimir(f())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[999, 1000]") != NULL, "estres_1000");
    }

    /* Caso 3: destructuring anidado a, (b, c) = ... dentro de bucle. */
    {
        char out[512];
        ejecutar_capturando(
            "funcion f():\n"
            "    para i en rango(0, 3):\n"
            "        a, (b, c) = i, (i * 2, i * 3)\n"
            "        imprimir(a, b, c)\n"
            "    fin para\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 0 0") != NULL, "anidado_0");
        AFIRMAR(strstr(out, "1 2 3") != NULL, "anidado_1");
        AFIRMAR(strstr(out, "2 4 6") != NULL, "anidado_2");
    }

    /* Caso 4: regresion - nolocal sigue funcionando (la primera
       tentativa de v1.123 lo rompia). */
    {
        char out[256];
        ejecutar_capturando(
            "funcion contador(inicial):\n"
            "    n = inicial\n"
            "    funcion inc():\n"
            "        nolocal n\n"
            "        n = n + 1\n"
            "        retornar n\n"
            "    fin funcion\n"
            "    retornar inc\n"
            "fin funcion\n"
            "c = contador(10)\n"
            "imprimir(c())\n"
            "imprimir(c())\n"
            "imprimir(c())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "11") != NULL, "nolocal_11");
        AFIRMAR(strstr(out, "12") != NULL, "nolocal_12");
        AFIRMAR(strstr(out, "13") != NULL, "nolocal_13");
    }

    /* Caso 5: destructuring dentro de si/sino dentro de para.
       Verifica que la pre-reserva recurre correctamente por SENT_SI. */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    para i en rango(0, 4):\n"
            "        si i % 2 == 0:\n"
            "            a, b = \"par\", i\n"
            "        sino:\n"
            "            a, b = \"impar\", i\n"
            "        fin si\n"
            "        imprimir(a, b)\n"
            "    fin para\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "par 0") != NULL, "rama_par_0");
        AFIRMAR(strstr(out, "impar 1") != NULL, "rama_impar_1");
        AFIRMAR(strstr(out, "par 2") != NULL, "rama_par_2");
    }

    if (fallos == 0) {
        printf("destr_bucle_nuevas: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "destr_bucle_nuevas: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
