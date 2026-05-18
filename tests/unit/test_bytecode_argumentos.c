/*
 * Tests de stdlib/argumentos.cor (v1.93).
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
        "test_arg_out.txt";
#else
        "/tmp/test_arg_out.txt";
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
    /* Parseo basico: posicional + opcion + bandera */
    {
        char out[2048];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"test\", \"prueba\")\n"
            "p.posicional(\"archivo\", \"\", nulo, nulo)\n"
            "p.opcion(\"--max\", \"-m\", \"\", \"entero\", 10)\n"
            "p.bandera(\"--verboso\", \"-v\", \"\")\n"
            "args = p.parsear([\"prog\", \"x.txt\", \"--max\", \"50\", \"-v\"])\n"
            "imprimir(args[\"archivo\"])\n"
            "imprimir(args[\"--max\"])\n"
            "imprimir(args[\"--verboso\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "x.txt") != NULL, "parseo_posicional");
        AFIRMAR(strstr(out, "50") != NULL, "parseo_opcion_entero");
        AFIRMAR(strstr(out, "verdadero") != NULL, "parseo_bandera");
    }

    /* Defaults cuando no se pasan opciones */
    {
        char out[2048];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"t\", \"\")\n"
            "p.posicional(\"f\", \"\", nulo, nulo)\n"
            "p.opcion(\"--max\", nulo, \"\", \"entero\", 100)\n"
            "p.bandera(\"--v\", nulo, \"\")\n"
            "args = p.parsear([\"prog\", \"in.txt\"])\n"
            "imprimir(args[\"f\"])\n"
            "imprimir(args[\"--max\"])\n"
            "imprimir(args[\"--v\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "in.txt") != NULL, "default_pos");
        AFIRMAR(strstr(out, "100") != NULL, "default_opcion");
        AFIRMAR(strstr(out, "falso") != NULL, "default_bandera");
    }

    /* Forma corta -m equivale a --max */
    {
        char out[1024];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"t\", \"\")\n"
            "p.posicional(\"f\", \"\", nulo, nulo)\n"
            "p.opcion(\"--max\", \"-m\", \"\", \"entero\", 10)\n"
            "args = p.parsear([\"prog\", \"x\", \"-m\", \"77\"])\n"
            "imprimir(args[\"--max\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "77") != NULL, "forma_corta");
    }

    /* Tipo decimal */
    {
        char out[1024];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"t\", \"\")\n"
            "p.opcion(\"--ratio\", \"-r\", \"\", \"decimal\", 0.5)\n"
            "args = p.parsear([\"prog\", \"--ratio\", \"3.14\"])\n"
            "imprimir(args[\"--ratio\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "3.14") != NULL, "tipo_decimal");
    }

    /* Tipo booleano */
    {
        char out[1024];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"t\", \"\")\n"
            "p.opcion(\"--activo\", \"-a\", \"\", \"booleano\", falso)\n"
            "args = p.parsear([\"prog\", \"--activo\", \"verdadero\"])\n"
            "imprimir(args[\"--activo\"])\n"
            "args2 = p.parsear([\"prog\", \"-a\", \"no\"])\n"
            "imprimir(args2[\"--activo\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "bool_verdadero");
        AFIRMAR(strstr(out, "falso") != NULL, "bool_falso");
    }

    /* Error: opcion desconocida */
    {
        char out[2048];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"t\", \"\")\n"
            "p.posicional(\"f\", \"\", nulo, nulo)\n"
            "intentar:\n"
            "    p.parsear([\"prog\", \"x\", \"--no-existe\"])\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err:\", e)\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "opcion desconocida") != NULL, "err_opcion_desconocida");
        AFIRMAR(strstr(out, "--no-existe") != NULL, "err_menciona_flag");
    }

    /* Error: posicional obligatorio ausente */
    {
        char out[2048];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"t\", \"\")\n"
            "p.posicional(\"archivo\", \"\", nulo, nulo)\n"
            "intentar:\n"
            "    p.parsear([\"prog\"])\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err:\", e)\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "obligatorio") != NULL, "err_posicional_ausente");
        AFIRMAR(strstr(out, "archivo") != NULL, "err_menciona_nombre");
    }

    /* Error: tipo invalido */
    {
        char out[2048];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"t\", \"\")\n"
            "p.opcion(\"--n\", \"-n\", \"\", \"entero\", 1)\n"
            "intentar:\n"
            "    p.parsear([\"prog\", \"--n\", \"abc\"])\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err:\", e)\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "no es un entero") != NULL, "err_tipo_invalido");
    }

    /* Error: opcion sin valor */
    {
        char out[2048];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"t\", \"\")\n"
            "p.opcion(\"--n\", \"-n\", \"\", \"entero\", 1)\n"
            "intentar:\n"
            "    p.parsear([\"prog\", \"--n\"])\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err:\", e)\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "requiere un valor") != NULL, "err_sin_valor");
    }

    /* Posicional con defecto (no obligatorio) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"t\", \"\")\n"
            "p.posicional(\"f\", \"\", \"cadena\", \"defecto.txt\")\n"
            "args = p.parsear([\"prog\"])\n"
            "imprimir(args[\"f\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "defecto.txt") != NULL, "posicional_opcional");
    }

    /* Ayuda incluye nombre + opciones */
    {
        char out[2048];
        ejecutar_capturando(
            "importar argumentos\n"
            "p = argumentos.Parser(\"miprog\", \"Hace algo\")\n"
            "p.posicional(\"entrada\", \"archivo entrada\", nulo, nulo)\n"
            "p.opcion(\"--max\", \"-m\", \"limite\", \"entero\", 10)\n"
            "p.bandera(\"--v\", nulo, \"verboso\")\n"
            "imprimir(p.ayuda())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "miprog") != NULL, "ayuda_nombre");
        AFIRMAR(strstr(out, "Hace algo") != NULL, "ayuda_descripcion");
        AFIRMAR(strstr(out, "entrada") != NULL, "ayuda_posicional");
        AFIRMAR(strstr(out, "--max") != NULL, "ayuda_opcion_larga");
        AFIRMAR(strstr(out, "-m") != NULL, "ayuda_opcion_corta");
        AFIRMAR(strstr(out, "--ayuda") != NULL, "ayuda_menciona_ayuda");
    }

    if (fallos == 0) {
        printf("argumentos: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "argumentos: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
