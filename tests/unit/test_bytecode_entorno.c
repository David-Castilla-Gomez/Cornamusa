/*
 * Tests de variables de entorno (v1.104): obtener_variable_entorno,
 * establecer_variable_entorno, variables_entorno, directorio_inicio.
 * Wrappers en stdlib/sistema.
 *
 * Importante: cada test usa nombres de variable con prefijo
 * `_CORNAMUSA_TEST_` para no chocar con env vars reales del sistema.
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
        "test_env_out.txt";
#else
        "/tmp/test_env_out.txt";
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
    /* obtener_variable_entorno: variable inexistente devuelve nulo */
    {
        char out[1024];
        ejecutar_capturando(
            "imprimir(obtener_variable_entorno(\"_CORNAMUSA_TEST_INEXISTENTE_XYZ\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "nulo") != NULL, "inexistente_devuelve_nulo");
    }

    /* establecer_variable_entorno + obtener: round-trip */
    {
        char out[1024];
        ejecutar_capturando(
            "establecer_variable_entorno(\"_CORNAMUSA_TEST_RT\", \"valor_rt\")\n"
            "imprimir(obtener_variable_entorno(\"_CORNAMUSA_TEST_RT\"))\n"
            /* cleanup */
            "establecer_variable_entorno(\"_CORNAMUSA_TEST_RT\", nulo)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "valor_rt") != NULL, "round_trip");
    }

    /* Borrar pasando nulo */
    {
        char out[1024];
        ejecutar_capturando(
            "establecer_variable_entorno(\"_CORNAMUSA_TEST_BORRA\", \"a\")\n"
            "imprimir(obtener_variable_entorno(\"_CORNAMUSA_TEST_BORRA\"))\n"
            "establecer_variable_entorno(\"_CORNAMUSA_TEST_BORRA\", nulo)\n"
            "imprimir(obtener_variable_entorno(\"_CORNAMUSA_TEST_BORRA\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "a\n") != NULL || strstr(out, "a\r\n") != NULL,
                "borrar_antes_existe");
        AFIRMAR(strstr(out, "nulo") != NULL, "borrar_despues_nulo");
    }

    /* variables_entorno devuelve dict no vacío con PATH (o equivalente) */
    {
        char out[2048];
        ejecutar_capturando(
            "todas = variables_entorno()\n"
            "imprimir(tipo(todas))\n"
            "imprimir(longitud(todas) > 0)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "diccionario") != NULL, "variables_es_dict");
        AFIRMAR(strstr(out, "verdadero") != NULL, "variables_no_vacio");
    }

    /* variables_entorno refleja cambios despues de establecer */
    {
        char out[1024];
        ejecutar_capturando(
            "establecer_variable_entorno(\"_CORNAMUSA_TEST_REFL\", \"presente\")\n"
            "todas = variables_entorno()\n"
            "imprimir(\"_CORNAMUSA_TEST_REFL\" en todas)\n"
            "imprimir(todas[\"_CORNAMUSA_TEST_REFL\"])\n"
            "establecer_variable_entorno(\"_CORNAMUSA_TEST_REFL\", nulo)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "variables_refleja_set");
        AFIRMAR(strstr(out, "presente") != NULL, "variables_valor_correcto");
    }

    /* directorio_inicio devuelve cadena no vacia */
    {
        char out[1024];
        ejecutar_capturando(
            "h = directorio_inicio()\n"
            "imprimir(tipo(h))\n"
            "imprimir(longitud(h) > 0)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "cadena") != NULL, "inicio_es_cadena");
        AFIRMAR(strstr(out, "verdadero") != NULL, "inicio_no_vacio");
    }

    /* directorio_inicio normaliza separadores (no contiene \\) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar cadenas\n"
            "h = directorio_inicio()\n"
            "imprimir(cadenas.contiene(h, \"\\\\\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "falso") != NULL, "inicio_sin_backslash");
    }

    /* Wrappers en stdlib/sistema */
    {
        char out[1024];
        ejecutar_capturando(
            "importar sistema\n"
            "sistema.establecer_variable(\"_CORNAMUSA_TEST_WRAP\", \"wrapper\")\n"
            "imprimir(sistema.obtener_variable(\"_CORNAMUSA_TEST_WRAP\"))\n"
            "imprimir(tipo(sistema.inicio()))\n"
            "imprimir(tipo(sistema.variables()))\n"
            "sistema.establecer_variable(\"_CORNAMUSA_TEST_WRAP\", nulo)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "wrapper") != NULL, "wrap_obtener");
        AFIRMAR(strstr(out, "cadena") != NULL, "wrap_inicio");
        AFIRMAR(strstr(out, "diccionario") != NULL, "wrap_variables");
    }

    /* Errores de tipo */
    {
        char out[1024];
        ejecutar_capturando(
            "intentar:\n"
            "    obtener_variable_entorno(42)\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(\"err tipo\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err tipo") != NULL, "obtener_arg_no_cadena_lanza");
    }

    if (fallos == 0) {
        printf("entorno: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "entorno: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
