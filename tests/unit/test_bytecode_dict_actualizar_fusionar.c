/*
 * Tests de `dicc.actualizar(otro)` y `fusionar(*dicts)` (v1.150).
 *
 * Python tiene `dict.update(otro)` (muta in-place) y la sintaxis
 * `{**d1, **d2}` o el operador `d1 | d2` (3.9+) para fusionar
 * creando un nuevo dict. Cornamusa tenia `d[k] = v` (set por
 * clave) pero NO una forma de copiar TODAS las claves de otro
 * dict de una sola vez.
 *
 * v1.150 anade ambas:
 *
 *   d.actualizar(otro)        nativa que muta d con (k, v) de otro.
 *                             Claves repetidas toman el valor de otro.
 *
 *   fusionar(*dicts)          en stdlib funcionales — crea dict nuevo
 *                             a partir de N dicts. NO muta los
 *                             originales. Las claves repetidas toman
 *                             el valor del ultimo. fusionar() = {}.
 *
 * Sin tocar bytecode ni VM (la nativa dict_actualizar usa la API
 * dicc_asignar existente; fusionar usa *args sobre funcion libre).
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
        "test_dict_act_out.txt";
#else
        "/tmp/test_dict_act_out.txt";
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
    /* actualizar: caso basico — claves repetidas toman valor de otro */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1, \"b\": 2}\n"
            "d.actualizar({\"b\": 20, \"c\": 30})\n"
            "imprimir(d[\"a\"])\n"
            "imprimir(d[\"b\"])\n"
            "imprimir(d[\"c\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "act_a");
        AFIRMAR(strstr(out, "20") != NULL, "act_b_sobreescrito");
        AFIRMAR(strstr(out, "30") != NULL, "act_c_nueva");
    }

    /* actualizar con dict vacio no cambia */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1, \"b\": 2}\n"
            "d.actualizar({})\n"
            "imprimir(longitud(d))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "act_vacio_no_cambia");
    }

    /* actualizar sobre dict vacio */
    {
        char out[256];
        ejecutar_capturando(
            "d = {}\n"
            "d.actualizar({\"x\": 1, \"y\": 2})\n"
            "imprimir(longitud(d))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "act_a_vacio");
    }

    /* actualizar rechaza no-diccionario */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1}\n"
            "intentar:\n"
            "    d.actualizar(\"no dict\")\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err tipo\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err tipo") != NULL, "act_rechaza_no_dict");
    }

    /* fusionar: caso basico de 3 dicts, claves repetidas */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar fusionar\n"
            "d1 = {\"a\": 1, \"b\": 2}\n"
            "d2 = {\"b\": 20, \"c\": 30}\n"
            "d3 = {\"c\": 300, \"d\": 400}\n"
            "f = fusionar(d1, d2, d3)\n"
            "imprimir(f[\"a\"])\n"
            "imprimir(f[\"b\"])\n"
            "imprimir(f[\"c\"])\n"
            "imprimir(f[\"d\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "fus_a");
        AFIRMAR(strstr(out, "20") != NULL, "fus_b_segundo");
        AFIRMAR(strstr(out, "300") != NULL, "fus_c_tercero");
        AFIRMAR(strstr(out, "400") != NULL, "fus_d");
    }

    /* fusionar NO muta los originales */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar fusionar\n"
            "d1 = {\"a\": 1}\n"
            "d2 = {\"a\": 99}\n"
            "_ = fusionar(d1, d2)\n"
            "imprimir(d1[\"a\"])\n"
            "imprimir(d2[\"a\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "fus_no_muta_d1");
        AFIRMAR(strstr(out, "99") != NULL, "fus_no_muta_d2");
    }

    /* fusionar() sin args devuelve dict vacio */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar fusionar\n"
            "imprimir(longitud(fusionar()))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "fus_sin_args");
    }

    /* fusionar(d) con un solo dict — copia */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar fusionar\n"
            "d = {\"a\": 1, \"b\": 2}\n"
            "f = fusionar(d)\n"
            "imprimir(longitud(f))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "fus_un_arg");
    }

    if (fallos == 0) {
        printf("dict_act: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "dict_act: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
