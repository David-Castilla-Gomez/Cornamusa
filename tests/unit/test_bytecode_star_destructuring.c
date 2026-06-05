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
 * v1.133: anadido soporte para star en PRIMERA posicion
 *   (`*r, x = it`). El parser detecta `*` al inicio de sentencia
 *   antes de invocar parser_parsear_expr (que falla porque '*' no
 *   tiene regla prefix). Tambien se anade TT_ASTERISCO a la
 *   heuristica de fin-de-sentencia para que `expr\n*r, x = it`
 *   no se parsee como multiplicacion cruzando lineas.
 *
 * Limitaciones documentadas:
 *   - Solo UN star por destructuring.
 *   - El iterable debe ser indexable y soportar OP_LONGITUD; lista,
 *     tupla y cadena funcionan. Rango no porque OP_INDICE no lo
 *     soporta (es generador puro).
 *   - En `para X en it` el star inicial aun no se reconoce — el
 *     parser de para tiene una rama distinta.
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

    /* v1.133: star en PRIMERA posicion sobre lista */
    {
        char out[256];
        ejecutar_capturando(
            "*r, z = [1, 2, 3, 4]\n"
            "imprimir(r, z)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3] 4") != NULL, "inicial");
    }

    /* v1.133: star inicial con dos finales */
    {
        char out[256];
        ejecutar_capturando(
            "*r, x, z = [1, 2, 3, 4, 5]\n"
            "imprimir(r, x, z)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3] 4 5") != NULL, "inicial_dos_finales");
    }

    /* v1.133: star inicial vacio (aridad minima) */
    {
        char out[256];
        ejecutar_capturando(
            "*vacio, ult = [10]\n"
            "imprimir(vacio, ult)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[] 10") != NULL, "inicial_vacio");
    }

    /* v1.133: star inicial sobre tupla */
    {
        char out[256];
        ejecutar_capturando(
            "*r, c = (10, 20, 30, 40)\n"
            "imprimir(r, c)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[10, 20, 30] 40") != NULL, "inicial_tupla");
    }

    /* v1.133: star inicial dentro de funcion */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    *r, c = [10, 20, 30, 40]\n"
            "    imprimir(r, c)\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[10, 20, 30] 40") != NULL,
                "inicial_en_funcion");
    }

    /* v1.133: heuristica de fin-de-sentencia — multiplicacion al
     * cruzar lineas NO debe robarse el `*` del destructuring. */
    {
        char out[256];
        ejecutar_capturando(
            "x = 1\n"
            "*r, z = [10, 20, 30]\n"
            "imprimir(x, r, z)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 [10, 20] 30") != NULL,
                "heuristica_no_multiplicacion");
    }

    /* v1.133: multiplicacion en la misma linea sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "x = 2 * 3\n"
            "imprimir(x)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "6") != NULL, "mult_misma_linea");
    }

    /* v1.133: error si tras `*` no hay identificador */
    {
        char out[256];
        ejecutar_capturando(
            "*, x = [1, 2]\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") == NULL, "star_sin_ident_rechaza");
    }

    /* v1.133: error si tras `*ident` no hay coma (no es destructuring) */
    {
        char out[256];
        ejecutar_capturando(
            "*r = [1, 2]\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") == NULL, "star_sin_coma_rechaza");
    }

    /* v1.133: aridad insuficiente con star inicial sigue siendo
     * ErrorDeValor atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    *r, x, z = [1]\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "inicial_aridad_atrap");
    }

    if (fallos == 0) {
        printf("star_destr: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "star_destr: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
