/*
 * Tests de atributos dinamicos (v1.86):
 *   tiene_atributo(obj, nombre) → booleano
 *   obtener_atributo(obj, nombre, defecto=nulo) → valor o defecto
 *   asignar_atributo(obj, nombre, valor) → nulo (muta)
 *
 * Verifica con instancias, clases y modulos. Comprueba los caminos
 * de error tambien.
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
        "test_attr_out.txt";
#else
        "/tmp/test_attr_out.txt";
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
    /* Test 1: tiene_atributo en instancia. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = P(1)\n"
            "imprimir(tiene_atributo(p, \"n\"))\n"
            "imprimir(tiene_atributo(p, \"nope\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso") != NULL ||
                strstr(out, "verdadero\r\nfalso") != NULL, "tiene_inst_basico");
    }

    /* Test 2: tiene_atributo encuentra metodos heredados de la clase. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "    funcion saludar(yo):\n"
            "        retornar \"hola\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = P()\n"
            "imprimir(tiene_atributo(p, \"saludar\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "tiene_metodo_via_inst");
    }

    /* Test 3: tiene_atributo en clase. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "    funcion bar(yo):\n"
            "        retornar 1\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(tiene_atributo(C, \"bar\"))\n"
            "imprimir(tiene_atributo(C, \"baz\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso") != NULL ||
                strstr(out, "verdadero\r\nfalso") != NULL, "tiene_en_clase");
    }

    /* Test 4: tiene_atributo en modulo. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar matematicas\n"
            "imprimir(tiene_atributo(matematicas, \"PI\"))\n"
            "imprimir(tiene_atributo(matematicas, \"nope_qqq\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "tiene_modulo_si");
        AFIRMAR(strstr(out, "falso") != NULL, "tiene_modulo_no");
    }

    /* Test 5: obtener_atributo retorna valor o defecto. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.n = 42\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = P()\n"
            "imprimir(obtener_atributo(p, \"n\"))\n"
            "imprimir(obtener_atributo(p, \"x\"))\n"
            "imprimir(obtener_atributo(p, \"x\", \"defecto-aqui\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "obtener_existente");
        AFIRMAR(strstr(out, "nulo") != NULL, "obtener_sin_defecto");
        AFIRMAR(strstr(out, "defecto-aqui") != NULL, "obtener_con_defecto");
    }

    /* Test 6: asignar_atributo muta instancia. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = P()\n"
            "asignar_atributo(p, \"nuevo\", 99)\n"
            "imprimir(p.nuevo)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "99") != NULL, "asignar_anade_atributo");
    }

    /* Test 7: asignar_atributo sobre no-instancia lanza ErrorDeTipo. */
    {
        char out[1024];
        ejecutar_capturando(
            "intentar:\n"
            "    asignar_atributo(42, \"x\", 1)\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(\"rechazado\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "rechazado") != NULL, "asignar_no_instancia_falla");
    }

    /* Test 8: tiene_atributo sobre tipos sin atributos retorna falso
     * silenciosamente (no lanza). */
    {
        char out[1024];
        ejecutar_capturando(
            "imprimir(tiene_atributo(42, \"foo\"))\n"
            "imprimir(tiene_atributo(\"texto\", \"foo\"))\n"
            "imprimir(tiene_atributo([1,2,3], \"foo\"))\n", out, sizeof(out));
        /* Los 3 deben dar falso. */
        int n_falso = 0;
        const char *p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_falso++; p++; }
        AFIRMAR(n_falso == 3, "tiene_no_aplica_silencio");
    }

    /* Test 9: nombre no-cadena lanza ErrorDeTipo. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    tiene_atributo(P(), 42)\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(\"rechazado\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "rechazado") != NULL, "nombre_no_cadena_falla");
    }

    /* Test 10: patron generico — itera atributos por nombre dinamico. */
    {
        char out[1024];
        ejecutar_capturando(
            "clase Producto:\n"
            "    funcion __iniciar__(yo, nombre, precio):\n"
            "        yo.nombre = nombre\n"
            "        yo.precio = precio\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = Producto(\"libro\", 25)\n"
            "campos = [\"nombre\", \"precio\", \"inexistente\"]\n"
            "para campo en campos:\n"
            "    si tiene_atributo(p, campo):\n"
            "        imprimir(f\"{campo}: {obtener_atributo(p, campo)}\")\n"
            "    fin si\n"
            "fin para\n", out, sizeof(out));
        AFIRMAR(strstr(out, "nombre: libro") != NULL, "patron_dinamico_nombre");
        AFIRMAR(strstr(out, "precio: 25") != NULL, "patron_dinamico_precio");
        AFIRMAR(strstr(out, "inexistente") == NULL, "patron_dinamico_skipea");
    }

    if (fallos == 0) {
        printf("atributo_dinamico: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "atributo_dinamico: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
