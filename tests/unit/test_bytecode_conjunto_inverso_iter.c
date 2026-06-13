/*
 * Tests: conjunto() e inverso() aceptan cualquier iterable nativo
 * (v1.204).
 *
 * Antes conjunto() solo aceptaba lista/tupla y inverso() rechazaba
 * rango (con sugerencia de workaround). Ahora ambos usan el iterador
 * genérico (iter_nuevo/iter_siguiente), así que aceptan cadena,
 * diccionario (sus claves), conjunto, rango y generador — reutilizando
 * el soporte de generadores del iterador (v1.200), incluida la
 * propagación de error si el generador lanza a mitad.
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
        "test_conj_inv_out.txt";
#else
        "/tmp/test_conj_inv_out.txt";
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
    /* conjunto() sobre cadena, rango, dict, generador (ordenado para
     * salida determinista). */
    {
        char out[512];
        int rc = ejecutar_capturando(
            "imprimir(\"C\", ordenado(conjunto(\"hola\")))\n"
            "imprimir(\"R\", ordenado(conjunto(rango(4))))\n"
            "imprimir(\"D\", ordenado(conjunto({\"a\": 1, \"b\": 2})))\n"
            "funcion g():\n"
            "    producir 1\n"
            "    producir 2\n"
            "    producir 1\n"
            "fin funcion\n"
            "imprimir(\"G\", ordenado(conjunto(g())))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "conj_rc");
        AFIRMAR(strstr(out, "C [\"a\", \"h\", \"l\", \"o\"]") != NULL, "conj_cadena");
        AFIRMAR(strstr(out, "R [0, 1, 2, 3]") != NULL, "conj_rango");
        AFIRMAR(strstr(out, "D [\"a\", \"b\"]") != NULL, "conj_dict_claves");
        AFIRMAR(strstr(out, "G [1, 2]") != NULL, "conj_gen_dedup");
    }

    /* inverso() sobre rango, dict (claves), generador. */
    {
        char out[512];
        int rc = ejecutar_capturando(
            "imprimir(\"R\", inverso(rango(5)))\n"
            "imprimir(\"D\", inverso({\"x\": 1, \"y\": 2, \"z\": 3}))\n"
            "funcion g():\n"
            "    producir 10\n"
            "    producir 20\n"
            "    producir 30\n"
            "fin funcion\n"
            "imprimir(\"G\", inverso(g()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "inv_rc");
        AFIRMAR(strstr(out, "R [4, 3, 2, 1, 0]") != NULL, "inv_rango");
        AFIRMAR(strstr(out, "D [\"z\", \"y\", \"x\"]") != NULL, "inv_dict");
        AFIRMAR(strstr(out, "G [30, 20, 10]") != NULL, "inv_gen");
    }

    /* Regresión: los tipos que ya funcionaban siguen igual. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "imprimir(\"L\", inverso([1, 2, 3]))\n"
            "imprimir(\"S\", inverso(\"abc\"))\n"
            "imprimir(\"CL\", ordenado(conjunto([5, 5, 1])))\n"
            "imprimir(\"CT\", ordenado(conjunto((2, 2, 9))))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "reg_rc");
        AFIRMAR(strstr(out, "L [3, 2, 1]") != NULL, "reg_inv_lista");
        AFIRMAR(strstr(out, "S [\"c\", \"b\", \"a\"]") != NULL, "reg_inv_cadena");
        AFIRMAR(strstr(out, "CL [1, 5]") != NULL, "reg_conj_lista");
        AFIRMAR(strstr(out, "CT [2, 9]") != NULL, "reg_conj_tupla");
    }

    /* conjunto() con elemento no hashable → ErrorDeTipo atrapable. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    conjunto([[1, 2], [3]])\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"NOHASH\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "nohash_rc");
        AFIRMAR(strstr(out, "NOHASH") != NULL, "nohash_atrapado");
    }

    /* conjunto()/inverso() vacíos y de un solo elemento. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "imprimir(\"E1\", conjunto())\n"
            "imprimir(\"E2\", conjunto(\"\"))\n"
            "imprimir(\"E3\", inverso(rango(0)))\n"
            "imprimir(\"E4\", inverso(rango(1)))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "vacio_rc");
        AFIRMAR(strstr(out, "E3 []") != NULL, "inv_vacio");
        AFIRMAR(strstr(out, "E4 [0]") != NULL, "inv_uno");
    }

    /* Propagación de error: un generador que lanza a mitad, consumido
     * por conjunto(), propaga el error (no lo enmascara ni lo traga). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion boom():\n"
            "    producir 1\n"
            "    lanzar ErrorDeValor(\"boom\")\n"
            "fin funcion\n"
            "intentar:\n"
            "    conjunto(boom())\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"PROP\", cadena(e))\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "prop_rc");
        AFIRMAR(strstr(out, "PROP ErrorDeValor: boom") != NULL, "prop_conjunto");
    }

    if (fallos == 0) {
        printf("conjunto_inverso_iter: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "conjunto_inverso_iter: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
