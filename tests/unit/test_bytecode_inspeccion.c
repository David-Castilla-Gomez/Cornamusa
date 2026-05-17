/*
 * Tests de inspeccion / reflexion (v1.91):
 *   - Nativas: clase_de, nombre_clase, metodos_de, atributos_de.
 *   - stdlib: obtener_clase, obtener_nombre, listar_metodos,
 *     listar_atributos, es_callable, es_clase, es_instancia, describir.
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
        "test_insp_out.txt";
#else
        "/tmp/test_insp_out.txt";
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
    /* clase_de: instancia → clase; otros → nulo. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = P()\n"
            "imprimir(clase_de(p))\n"
            "imprimir(clase_de(42))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "<clase P>") != NULL, "clase_de_instancia");
        AFIRMAR(strstr(out, "nulo") != NULL, "clase_de_no_instancia");
    }

    /* nombre_clase: instancia y clase devuelven mismo nombre. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase Persona:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = Persona()\n"
            "imprimir(nombre_clase(p))\n"
            "imprimir(nombre_clase(Persona))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "Persona") != NULL, "nombre_clase_basico");
    }

    /* nombre_clase rechaza otros tipos. */
    {
        char out[1024];
        ejecutar_capturando(
            "intentar:\n"
            "    nombre_clase(42)\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(\"rechazado\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "rechazado") != NULL, "nombre_clase_rechaza");
    }

    /* metodos_de: lista nombres de metodos. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase M:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "    funcion uno(yo):\n"
            "        retornar 1\n"
            "    fin funcion\n"
            "    funcion dos(yo):\n"
            "        retornar 2\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(metodos_de(M))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "uno") != NULL, "metodos_uno");
        AFIRMAR(strstr(out, "dos") != NULL, "metodos_dos");
        AFIRMAR(strstr(out, "__iniciar__") != NULL, "metodos_iniciar");
    }

    /* metodos_de funciona tambien con instancia. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase M:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "    funcion saludar(yo):\n"
            "        retornar 1\n"
            "    fin funcion\n"
            "fin clase\n"
            "i = M()\n"
            "imprimir(metodos_de(i))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "saludar") != NULL, "metodos_via_inst");
    }

    /* atributos_de: solo instancia. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo, n, e):\n"
            "        yo.n = n\n"
            "        yo.e = e\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = P(1, 2)\n"
            "imprimir(atributos_de(p))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "\"n\"") != NULL, "atributos_n");
        AFIRMAR(strstr(out, "\"e\"") != NULL, "atributos_e");
    }

    /* atributos_de rechaza no-instancia. */
    {
        char out[1024];
        ejecutar_capturando(
            "intentar:\n"
            "    atributos_de(42)\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(\"rechazado\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "rechazado") != NULL, "atributos_rechaza");
    }

    /* stdlib helpers via importar. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar inspeccion\n"
            "clase X:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "x = X()\n"
            "imprimir(inspeccion.es_clase(X))\n"
            "imprimir(inspeccion.es_clase(x))\n"
            "imprimir(inspeccion.es_instancia(x))\n"
            "imprimir(inspeccion.es_instancia(X))\n"
            "imprimir(inspeccion.es_callable(X))\n"
            "imprimir(inspeccion.es_callable(42))\n", out, sizeof(out));
        int n_verdaderos = 0, n_falsos = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_verdaderos++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_falsos++; p++; }
        /* esperados: X es_clase=v, x es_clase=f, x es_inst=v, X es_inst=f,
         * X es_callable=v, 42 es_callable=f. 3 verdaderos, 3 falsos. */
        AFIRMAR(n_verdaderos == 3, "stdlib_verdaderos_3");
        AFIRMAR(n_falsos == 3, "stdlib_falsos_3");
    }

    /* describir: dict con estructura completa. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar inspeccion\n"
            "clase Algo:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "fin clase\n"
            "a = Algo(42)\n"
            "d = inspeccion.describir(a)\n"
            "imprimir(d[\"tipo\"])\n"
            "imprimir(d[\"clase\"])\n"
            "imprimir(d[\"atributos\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "instancia") != NULL, "describir_tipo");
        AFIRMAR(strstr(out, "Algo") != NULL, "describir_clase");
        AFIRMAR(strstr(out, "\"n\"") != NULL, "describir_atributos");
    }

    if (fallos == 0) {
        printf("inspeccion: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "inspeccion: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
