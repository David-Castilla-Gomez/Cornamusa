/*
 * Tests de stdlib/pruebas.cor (v1.96).
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
        "test_pru_out.txt";
#else
        "/tmp/test_pru_out.txt";
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
    /* Asserts standalone pasando */
    {
        char out[1024];
        ejecutar_capturando(
            "importar pruebas\n"
            "pruebas.aseverar(2 + 2 == 4, \"\")\n"
            "pruebas.aseverar_igual(3, 3)\n"
            "pruebas.aseverar_distinto(1, 2)\n"
            "pruebas.aseverar_verdadero(verdadero)\n"
            "pruebas.aseverar_falso(falso)\n"
            "pruebas.aseverar_nulo(nulo)\n"
            "pruebas.aseverar_no_nulo(42)\n"
            "pruebas.aseverar_contiene([1, 2, 3], 2)\n"
            "pruebas.aseverar_no_contiene([1, 2, 3], 99)\n"
            "imprimir(\"OK\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "OK") != NULL, "asserts_basicos_pasan");
    }

    /* Aseverar igual fallando lanza ErrorDeValor */
    {
        char out[1024];
        ejecutar_capturando(
            "importar pruebas\n"
            "intentar:\n"
            "    pruebas.aseverar_igual(1, 2)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err:\", e)\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "esperado 2") != NULL, "aseverar_igual_fallo_msg");
        AFIRMAR(strstr(out, "obtenido 1") != NULL, "aseverar_igual_obtenido");
    }

    /* Aseverar aproximado pasa con tolerancia */
    {
        char out[1024];
        ejecutar_capturando(
            "importar pruebas\n"
            "pruebas.aseverar_aproximado(0.1 + 0.2, 0.3, 0.001)\n"
            "imprimir(\"OK\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "OK") != NULL, "aproximado_pasa");
    }

    /* Aseverar aproximado falla si tolerancia muy estricta */
    {
        char out[1024];
        ejecutar_capturando(
            "importar pruebas\n"
            "intentar:\n"
            "    pruebas.aseverar_aproximado(1.0, 2.0, 0.001)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "aproximado_falla_si_lejos");
    }

    /* aseverar_lanza con nombre cadena */
    {
        char out[1024];
        ejecutar_capturando(
            "importar pruebas\n"
            "funcion f():\n"
            "    x = 1 / 0\n"
            "fin funcion\n"
            "pruebas.aseverar_lanza(f, \"ErrorAritmetico\")\n"
            "imprimir(\"OK\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "OK") != NULL, "aseverar_lanza_nombre_correcto");
    }

    /* aseverar_lanza con tipo mismatch */
    {
        char out[1024];
        ejecutar_capturando(
            "importar pruebas\n"
            "funcion f():\n"
            "    lanzar ErrorDeValor(\"x\")\n"
            "fin funcion\n"
            "intentar:\n"
            "    pruebas.aseverar_lanza(f, \"ErrorDeTipo\")\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"mismatch\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "mismatch") != NULL, "aseverar_lanza_mismatch");
    }

    /* aseverar_lanza con nulo (cualquier excepcion) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar pruebas\n"
            "funcion f():\n"
            "    lanzar ErrorDeValor(\"x\")\n"
            "fin funcion\n"
            "pruebas.aseverar_lanza(f, nulo)\n"
            "imprimir(\"OK\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "OK") != NULL, "aseverar_lanza_nulo_acepta");
    }

    /* aseverar_lanza falla si callable no lanza */
    {
        char out[1024];
        ejecutar_capturando(
            "importar pruebas\n"
            "funcion f():\n"
            "    retornar 1\n"
            "fin funcion\n"
            "intentar:\n"
            "    pruebas.aseverar_lanza(f, nulo)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"no lanzo\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "no lanzo") != NULL, "aseverar_lanza_falla_si_no_lanza");
    }

    /* Suite con casos pasando */
    {
        char out[2048];
        ejecutar_capturando(
            "importar pruebas\n"
            "funcion t1():\n"
            "    pruebas.aseverar_igual(1, 1)\n"
            "fin funcion\n"
            "funcion t2():\n"
            "    pruebas.aseverar_igual(2, 2)\n"
            "fin funcion\n"
            "s = pruebas.Suite(\"test\")\n"
            "s.caso(\"primero\", t1)\n"
            "s.caso(\"segundo\", t2)\n"
            "r = s.ejecutar()\n"
            "imprimir(\"pasados:\", r[\"pasados\"])\n"
            "imprimir(\"total:\", r[\"total\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[OK]") != NULL, "suite_imprime_ok");
        AFIRMAR(strstr(out, "pasados: 2") != NULL, "suite_pasados_2");
        AFIRMAR(strstr(out, "total: 2") != NULL, "suite_total_2");
    }

    /* Suite con caso que falla */
    {
        char out[2048];
        ejecutar_capturando(
            "importar pruebas\n"
            "funcion t1():\n"
            "    pruebas.aseverar_igual(1, 1)\n"
            "fin funcion\n"
            "funcion t2():\n"
            "    pruebas.aseverar_igual(1, 99)\n"
            "fin funcion\n"
            "s = pruebas.Suite(\"mixto\")\n"
            "s.caso(\"pasa\", t1)\n"
            "s.caso(\"falla\", t2)\n"
            "r = s.ejecutar()\n"
            "imprimir(\"fallados:\", r[\"fallados\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[FAIL]") != NULL, "suite_imprime_fail");
        AFIRMAR(strstr(out, "fallados: 1") != NULL, "suite_fallados_1");
    }

    /* ejecutar_casos (wrapper funcional) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar pruebas\n"
            "funcion t1():\n"
            "    pruebas.aseverar_igual(2 + 2, 4)\n"
            "fin funcion\n"
            "r = pruebas.ejecutar_casos([[\"test_t1\", t1]])\n"
            "imprimir(\"pasados:\", r[\"pasados\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "pasados: 1") != NULL, "ejecutar_casos_funcional");
    }

    if (fallos == 0) {
        printf("pruebas: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "pruebas: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
