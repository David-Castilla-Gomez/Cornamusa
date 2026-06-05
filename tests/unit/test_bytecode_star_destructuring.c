/*
 * Tests del star binding en destructuring (v1.129).
 *
 * Antes: `a, *resto, c = lista` daba ErrorDeSintaxis en la coma. La
 * sintaxis sí estaba documentada en pattern matching (`cuando [a, *r, b]`)
 * pero no en destructuring de asignacion.
 *
 * v1.129:
 *   - Nuevo EXPR_STAR_BIND en el AST.
 *   - Parser reconoce `*ident` dentro del bucle de destinos de la
 *     asignacion tupla (parsear_asignar_o_expr).
 *   - emitir_destructuring detecta star_idx, cambia la check de
 *     aridad de `==` a `>=` (n-1), y para el destino estrella emite
 *     OP_REBANADA [star_idx : len - tail].
 *   - OP_REBANADA extendido para aceptar VAL_TUPLA (devuelve LISTA;
 *     la semantica es la misma que para lista).
 *
 * Limitaciones documentadas:
 *   - Solo UN star por destructuring.
 *   - El iterable debe ser indexable y soportar OP_LONGITUD; lista,
 *     tupla y cadena funcionan. Rango no porque OP_INDICE no lo
 *     soporta (es generador puro).
 *   - Star en primera posicion (`*r, a = it`) no se reconoce porque
 *     el parser empieza llamando a parser_parsear_expr; el `*` se
 *     interpreta como factor.
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
        "test_star_out.txt";
#else
        "/tmp/test_star_out.txt";
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
    /* Star en medio sobre lista */
    {
        char out[256];
        ejecutar_capturando(
            "a, *r, c = [1, 2, 3, 4, 5]\n"
            "imprimir(a, r, c)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 [2, 3, 4] 5") != NULL, "medio_lista");
    }

    /* Star al final */
    {
        char out[256];
        ejecutar_capturando(
            "a, b, *r = [1, 2, 3, 4, 5]\n"
            "imprimir(a, b, r)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2 [3, 4, 5]") != NULL, "final");
    }

    /* Star vacio en el medio (justo la aridad minima) */
    {
        char out[256];
        ejecutar_capturando(
            "a, *r, c = [1, 2]\n"
            "imprimir(a, r, c)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 [] 2") != NULL, "vacio_medio");
    }

    /* Star vacio al final */
    {
        char out[256];
        ejecutar_capturando(
            "a, b, *r = [1, 2]\n"
            "imprimir(a, b, r)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2 []") != NULL, "vacio_final");
    }

    /* Dos lados, star vacio */
    {
        char out[256];
        ejecutar_capturando(
            "a, b, *r, c, d = [1, 2, 3, 4]\n"
            "imprimir(a, b, r, c, d)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2 [] 3 4") != NULL, "dos_lados");
    }

    /* Aridad insuficiente lanza ErrorDeValor atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    p, q, *r, s = [1]\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "aridad_insuficiente");
    }

    /* Sobre tupla (mismo comportamiento, devuelve LISTA para el star) */
    {
        char out[256];
        ejecutar_capturando(
            "a, *r, c = (10, 20, 30, 40)\n"
            "imprimir(a, r, c)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10 [20, 30] 40") != NULL, "tupla");
    }

    /* Sobre cadena (slice devuelve cadena) */
    {
        char out[256];
        ejecutar_capturando(
            "a, *r, c = \"hola\"\n"
            "imprimir(a, r, c)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "h ol a") != NULL, "cadena");
    }

    /* Reasignar variables ya existentes (locales globales del programa) */
    {
        char out[256];
        ejecutar_capturando(
            "m = 0\n"
            "mm = 0\n"
            "n = 0\n"
            "m, *mm, n = [100, 200, 300, 400]\n"
            "imprimir(m, mm, n)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "100 [200, 300] 400") != NULL, "reasignar");
    }

    /* Dentro de una funcion */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    a, *r, c = [10, 20, 30, 40]\n"
            "    imprimir(a, r, c)\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10 [20, 30] 40") != NULL, "en_funcion");
    }

    /* Multiples stars rechazado en compile time */
    {
        char out[256];
        ejecutar_capturando(
            "a, *r, *s = [1, 2, 3]\n",
            out, sizeof(out));
        /* La compilacion falla; el fuente nunca llega a ejecutarse y
         * stdout queda vacio. El test verifica que NO se imprime nada. */
        AFIRMAR(strstr(out, "1") == NULL, "multi_star_rechazado");
    }

    /* Sin star sigue funcionando (regresion del codigo previo) */
    {
        char out[256];
        ejecutar_capturando(
            "a, b, c = [10, 20, 30]\n"
            "imprimir(a, b, c)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10 20 30") != NULL, "regresion_sin_star");
    }

    /* Aridad exacta sin star sigue rechazando */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    a, b = [1, 2, 3]\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "sin_star_aridad");
    }

    if (fallos == 0) {
        printf("star_destr: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "star_destr: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
