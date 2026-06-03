/*
 * Tests de anotaciones de tipo opcionales (v1.114).
 *
 * Cornamusa permite anotaciones de tipo SIN verificacion runtime
 * (estilo Python type hints). El parser las acepta y el compilador
 * las ignora. Util para documentacion y futuras herramientas de
 * tipos.
 *
 * Sintaxis:
 *   funcion f(x: tipo, n: tipo) -> tipo:    # params + retorno
 *   nombre: tipo = valor                     # asignacion v1.114
 *
 * Las anotaciones pueden ser cualquier expresion (identificador,
 * llamada, indice, etc.). No se valida que el tipo exista o que
 * los valores coincidan.
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
        "test_anot_out.txt";
#else
        "/tmp/test_anot_out.txt";
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
    /* Anotaciones en parametros */
    {
        char out[512];
        ejecutar_capturando(
            "funcion saludar(nombre: cadena) -> cadena:\n"
            "    retornar \"Hola, \" + nombre\n"
            "fin funcion\n"
            "imprimir(saludar(\"Ana\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "Hola, Ana") != NULL, "params_basico");
    }

    /* Anotacion + valor por defecto */
    {
        char out[512];
        ejecutar_capturando(
            "funcion contar(n: entero = 10) -> entero:\n"
            "    retornar n * 2\n"
            "fin funcion\n"
            "imprimir(contar())\n"
            "imprimir(contar(5))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "20") != NULL, "anot_con_default");
        AFIRMAR(strstr(out, "10") != NULL, "anot_default_override");
    }

    /* Mezclando anotados y sin anotar */
    {
        char out[512];
        ejecutar_capturando(
            "funcion mixto(x, n: entero, c: cadena = \"x\"):\n"
            "    imprimir(x, n, c)\n"
            "fin funcion\n"
            "mixto(\"a\", 42)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "a 42 x") != NULL, "mixto_anotado_sin");
    }

    /* Anotacion en variable top-level */
    {
        char out[256];
        ejecutar_capturando(
            "nombre: cadena = \"Ana\"\n"
            "edad: entero = 30\n"
            "imprimir(nombre)\n"
            "imprimir(edad)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "Ana") != NULL, "var_anot_cadena");
        AFIRMAR(strstr(out, "30") != NULL, "var_anot_entero");
    }

    /* Anotacion en variable dentro de funcion */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    x: entero = 42\n"
            "    imprimir(x)\n"
            "fin funcion\n"
            "f()\n", out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "var_anot_local");
    }

    /* Anotacion con tipo compuesto (cualquier expresion) */
    {
        char out[256];
        ejecutar_capturando(
            "items: lista = [1, 2, 3]\n"
            "imprimir(items)\n"
            "imprimir(longitud(items))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "anot_tipo_compuesto");
    }

    /* La anotacion NO se valida en runtime: tipos NO coinciden */
    {
        char out[256];
        /* Anotamos como entero pero asignamos cadena — no debe fallar */
        ejecutar_capturando(
            "x: entero = \"realmente cadena\"\n"
            "imprimir(x)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "realmente cadena") != NULL, "anot_no_valida_runtime");
    }

    /* Anotacion en funcion con todos los elementos */
    {
        char out[512];
        ejecutar_capturando(
            "funcion area_rectangulo(b: decimal = 1.0, h: decimal = 1.0) -> decimal:\n"
            "    return_val: decimal = b * h\n"
            "    retornar return_val\n"
            "fin funcion\n"
            "imprimir(area_rectangulo(3, 4))\n"
            "imprimir(area_rectangulo())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "12") != NULL, "funcion_completa_anotada");
    }

    /* Codigo SIN anotaciones sigue compilando */
    {
        char out[256];
        ejecutar_capturando(
            "funcion suma(a, b):\n"
            "    retornar a + b\n"
            "fin funcion\n"
            "imprimir(suma(2, 3))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "compat_sin_anotaciones");
    }

    /* Anotacion como llamada (estilo `lista[entero]`) */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f(items: longitud) -> cadena:\n"
            /* Aqui `longitud` se usa como anotacion — el parser solo
             * la guarda y descarta, no se invoca. Es un identificador
             * en posicion de tipo. */
            "    retornar cadena(items)\n"
            "fin funcion\n"
            "imprimir(f(42))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "anot_es_solo_documentacion");
    }

    if (fallos == 0) {
        printf("anotaciones: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "anotaciones: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
