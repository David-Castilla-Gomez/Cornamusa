/*
 * Tests de metodos sobre conjunto y tupla (v1.128).
 *
 * v1.122 dejo VAL_CONJUNTO y VAL_TUPLA sin entradas en la tabla
 * METODOS_NATIVOS. v1.128 anade:
 *   conjunto: agregar/añadir, quitar, union, interseccion, diferencia,
 *             es_subconjunto, contiene, copiar.
 *   tupla:    contar, contiene, indice_de.
 *   lista:    indice_de (faltaba complementar contar/contiene de v1.122).
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
        "test_met_conj_tup_out.txt";
#else
        "/tmp/test_met_conj_tup_out.txt";
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
    /* conjunto.union: cardinalidad correcta y elementos esperados. */
    {
        char out[256];
        ejecutar_capturando(
            "a = {1, 2, 3}\n"
            "b = {3, 4, 5}\n"
            "u = a.union(b)\n"
            "imprimir(longitud(u))\n"
            "imprimir(1 en u)\n"
            "imprimir(5 en u)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "union_card");
        AFIRMAR(strstr(out, "verdadero") != NULL, "union_contiene");
    }

    /* conjunto.interseccion: solo el comun. */
    {
        char out[256];
        ejecutar_capturando(
            "a = {1, 2, 3}\n"
            "b = {2, 3, 4}\n"
            "i = a.interseccion(b)\n"
            "imprimir(longitud(i))\n"
            "imprimir(2 en i)\n"
            "imprimir(1 en i)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "inter_card");
        /* 1 no debe estar -> falso entre los outputs */
        AFIRMAR(strstr(out, "falso") != NULL, "inter_excluye");
    }

    /* conjunto.diferencia: a - b. */
    {
        char out[256];
        ejecutar_capturando(
            "a = {1, 2, 3}\n"
            "b = {2, 3, 4}\n"
            "d = a.diferencia(b)\n"
            "imprimir(longitud(d))\n"
            "imprimir(1 en d)\n"
            "imprimir(2 en d)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "diff_card");
        AFIRMAR(strstr(out, "verdadero") != NULL, "diff_contiene_1");
        AFIRMAR(strstr(out, "falso") != NULL, "diff_excluye_2");
    }

    /* conjunto.es_subconjunto */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir({1, 2}.es_subconjunto({1, 2, 3}))\n"
            "imprimir({1, 9}.es_subconjunto({1, 2, 3}))\n"
            "imprimir({}.es_subconjunto({1, 2}))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "subconj_si");
        AFIRMAR(strstr(out, "falso") != NULL, "subconj_no");
    }

    /* conjunto.contiene */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2, 3}\n"
            "imprimir(s.contiene(2))\n"
            "imprimir(s.contiene(99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "conj_cont_si");
        AFIRMAR(strstr(out, "falso") != NULL, "conj_cont_no");
    }

    /* conjunto.copiar es independiente del original */
    {
        char out[256];
        ejecutar_capturando(
            "a = {1, 2}\n"
            "c = a.copiar()\n"
            "a.agregar(99)\n"
            "imprimir(99 en a)\n"
            "imprimir(99 en c)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "copiar_a_tiene");
        AFIRMAR(strstr(out, "falso") != NULL, "copiar_c_no_tiene");
    }

    /* conjunto.agregar y .quitar via metodo */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2}\n"
            "s.agregar(3)\n"
            "imprimir(3 en s)\n"
            "s.quitar(3)\n"
            "imprimir(3 en s)\n"
            "s.añadir(4)\n"
            "imprimir(4 en s)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "conj_agr_si");
        AFIRMAR(strstr(out, "falso") != NULL, "conj_quitar_no");
    }

    /* tupla.contar */
    {
        char out[256];
        ejecutar_capturando(
            "t = (1, 2, 3, 2, 1, 2)\n"
            "imprimir(t.contar(2))\n"
            "imprimir(t.contar(99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "tup_contar_3");
        AFIRMAR(strstr(out, "0") != NULL, "tup_contar_0");
    }

    /* tupla.contiene */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((1, 2, 3).contiene(2))\n"
            "imprimir((1, 2, 3).contiene(99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "tup_cont_si");
        AFIRMAR(strstr(out, "falso") != NULL, "tup_cont_no");
    }

    /* tupla.indice_de */
    {
        char out[256];
        ejecutar_capturando(
            "t = (10, 20, 30, 20)\n"
            "imprimir(t.indice_de(20))\n"
            "imprimir(t.indice_de(99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "tup_idx_1");
        AFIRMAR(strstr(out, "-1") != NULL, "tup_idx_no");
    }

    /* lista.indice_de */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [\"a\", \"b\", \"c\", \"b\"]\n"
            "imprimir(xs.indice_de(\"b\"))\n"
            "imprimir(xs.indice_de(\"z\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "lst_idx_1");
        AFIRMAR(strstr(out, "-1") != NULL, "lst_idx_no");
    }

    /* metodo no existente sobre conjunto sigue dando ErrorDeTipo atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    {1, 2}.xyz()\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "conj_metodo_inexistente");
    }

    if (fallos == 0) {
        printf("met_conj_tup: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "met_conj_tup: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
