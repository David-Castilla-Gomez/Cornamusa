/*
 * Tests del modulo stdlib/tiempo.cor (v1.73).
 *
 * Verifica via VM real:
 *   - epoch_segundos() y epoch_ms() devuelven enteros razonables (post-2020).
 *   - epoch_ms() es ~1000 * epoch_segundos() (margen 2s).
 *   - monotonic() es monotonicamente creciente.
 *   - dormir(s) bloquea al menos ~s segundos.
 *   - dormir(0) retorna inmediato.
 *   - cronometro: leer() crece, reiniciar() resetea, instancias independientes.
 *
 * El otro test_bytecode_tiempo.c cubre las nativas v1.19
 * (tiempo_actual/descomponer/componer/formato).
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
        "test_tiempo_std_out.txt";
#else
        "/tmp/test_tiempo_std_out.txt";
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
    /* Test 1: epoch_segundos retorna un timestamp post-2020. */
    {
        char out[256];
        ejecutar_capturando(
            "importar tiempo\n"
            "ts = tiempo.epoch_segundos()\n"
            "imprimir(ts > 1577836800)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "epoch_segundos_post_2020");
    }

    /* Test 2: epoch_ms es ~1000x epoch_segundos. */
    {
        char out[256];
        ejecutar_capturando(
            "importar tiempo\n"
            "s = tiempo.epoch_segundos()\n"
            "ms = tiempo.epoch_ms()\n"
            "diff = ms - s * 1000\n"
            "imprimir(diff >= 0 y diff < 2000)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "epoch_ms_coherente");
    }

    /* Test 3: monotonic crece. */
    {
        char out[256];
        ejecutar_capturando(
            "importar tiempo\n"
            "a = tiempo.monotonic()\n"
            "b = tiempo.monotonic()\n"
            "imprimir(b >= a)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "monotonic_no_decreciente");
    }

    /* Test 4: dormir(0) inmediato. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "importar tiempo\n"
            "tiempo.dormir(0)\n"
            "imprimir(\"ok\")\n", out, sizeof(out));
        AFIRMAR(rc == 0, "dormir_0_ok");
        AFIRMAR(strstr(out, "ok") != NULL, "dormir_0_continua");
    }

    /* Test 5: dormir(s) bloquea (margen amplio para Sleep windows). */
    {
        char out[256];
        ejecutar_capturando(
            "importar tiempo\n"
            "a = tiempo.monotonic()\n"
            "tiempo.dormir(0.05)\n"
            "b = tiempo.monotonic()\n"
            "transcurrido = b - a\n"
            "imprimir(transcurrido >= 0.03 y transcurrido < 1.0)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "dormir_bloqueante");
    }

    /* Test 6: cronometro acumula tras dormir. */
    {
        char out[256];
        ejecutar_capturando(
            "importar tiempo\n"
            "c = tiempo.cronometro()\n"
            "tiempo.dormir(0.05)\n"
            "imprimir(c.leer() > 0.03)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "cronometro_acumula");
    }

    /* Test 7: cronometro.reiniciar(). */
    {
        char out[256];
        ejecutar_capturando(
            "importar tiempo\n"
            "c = tiempo.cronometro()\n"
            "tiempo.dormir(0.05)\n"
            "antes = c.leer()\n"
            "c.reiniciar()\n"
            "despues = c.leer()\n"
            "imprimir(despues < antes)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "cronometro_reinicia");
    }

    /* Test 8: cronometros independientes. */
    {
        char out[256];
        ejecutar_capturando(
            "importar tiempo\n"
            "c1 = tiempo.cronometro()\n"
            "tiempo.dormir(0.05)\n"
            "c2 = tiempo.cronometro()\n"
            "imprimir(c1.leer() > c2.leer())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "cronometros_independientes");
    }

    if (fallos == 0) {
        printf("tiempo_stdlib: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "tiempo_stdlib: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
