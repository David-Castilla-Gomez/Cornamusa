/*
 * Tests de metodos sobre tipos nativos (lista, cadena, dict) - fase 2.
 *
 * Antes: `xs.añadir(4)`, `"Hola".minusculas()`, `d.claves()` etc.
 * lanzaban "ErrorDeTipo: 'lista'/'cadena'/'diccionario' no tiene
 * atributos accesibles". Solo existian funciones globales.
 *
 * v1.122: nuevo VAL_METODO_NATIVO_LIGADO + tabla
 * METODOS_NATIVOS en nativos.c. OP_OBTENER_ATRIBUTO sobre receptor
 * nativo busca en la tabla; OP_LLAMAR sobre MetodoNativoLigado
 * prepend el receptor a los args y delega a la FnNativa subyacente.
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
        "test_met_nat_out.txt";
#else
        "/tmp/test_met_nat_out.txt";
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
    /* lista.añadir(x) */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "xs.agregar(4)\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4]") != NULL, "lista_agregar");
    }

    /* lista.insertar(i, x) */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [2, 3, 4]\n"
            "xs.insertar(0, 1)\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4]") != NULL, "lista_insertar");
    }

    /* lista.ordenar() */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [3, 1, 2]\n"
            "xs.ordenar()\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "lista_ordenar");
    }

    /* lista.invertir() */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "xs.invertir()\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 2, 1]") != NULL, "lista_invertir");
    }

    /* lista.quitar() / lista.quitar(i) */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3, 4]\n"
            "xs.quitar()\n"
            "imprimir(xs)\n"
            "xs.quitar(0)\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "lista_quitar_default");
        AFIRMAR(strstr(out, "[2, 3]") != NULL, "lista_quitar_idx");
    }

    /* cadena.minusculas() / mayusculas() */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"HOLA\".minusculas())\n"
            "imprimir(\"hola\".mayusculas())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "hola") != NULL, "cadena_minusculas");
        AFIRMAR(strstr(out, "HOLA") != NULL, "cadena_mayusculas");
    }

    /* cadena.empieza_con / termina_con */
    {
        char out[256];
        ejecutar_capturando(
            "s = \"buenos dias\"\n"
            "imprimir(s.empieza_con(\"buenos\"))\n"
            "imprimir(s.termina_con(\"dias\"))\n"
            "imprimir(s.empieza_con(\"adios\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "cad_empieza_si");
        AFIRMAR(strstr(out, "falso") != NULL, "cad_empieza_no");
    }

    /* cadena.indice_de */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola mundo\".indice_de(\"mu\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "cad_indice_de");
    }

    /* dict.claves / valores */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1, \"b\": 2}\n"
            "imprimir(d.claves())\n"
            "imprimir(d.valores())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "\"a\"") != NULL, "dict_claves_a");
        AFIRMAR(strstr(out, "\"b\"") != NULL, "dict_claves_b");
        AFIRMAR(strstr(out, "1") != NULL, "dict_valores_1");
        AFIRMAR(strstr(out, "2") != NULL, "dict_valores_2");
    }

    /* Metodo encadenado en expresion compleja: literal . metodo() */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([3, 1, 2].agregar)\n",
            out, sizeof(out));
        /* Solo verificar que NO crash y muestra algo razonable */
        AFIRMAR(strstr(out, "metodo-nativo") != NULL ||
                strstr(out, "agregar") != NULL,
                "metodo_no_invocado_es_callable");
    }

    /* Metodo desconocido sobre tipo nativo lanza ErrorDeTipo atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    [1, 2].xyz_no_existe()\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "metodo_inexistente_lanza");
    }

    if (fallos == 0) {
        printf("met_nativos: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "met_nativos: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
