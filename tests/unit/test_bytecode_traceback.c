/*
 * Tests del traceback multi-frame (v1.38).
 *
 * Cuando un error de runtime fatal (no atrapado) sale del dispatch,
 * la VM captura la cadena de llamadas en `vm->traceback`. Estos tests
 * ejecutan código que falla en funciones anidadas y verifican el
 * contenido del traceback.
 */

#include <stdio.h>
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

/* Ejecuta `fuente`, espera error de runtime, y copia el traceback a
   `tb_out`. Retorna true si hubo error. */
static bool ejecutar_con_traceback(const char *fuente,
                                     char *tb_out, size_t tb_cap) {
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (!prog || p.tuvo_error) {
        snprintf(tb_out, tb_cap, "<error de parseo>");
        arena_destruir(&a);
        return false;
    }
    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, prog, n)) {
        snprintf(tb_out, tb_cap, "<error de compilacion>");
        chunk_destruir(&chunk); arena_destruir(&a);
        return false;
    }
    VM vm; vm_iniciar(&vm);
    Valor resultado = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &resultado);
    bool hubo_error = (rc != VM_OK);
    snprintf(tb_out, tb_cap, "%s", vm.traceback);
    valor_destruir(&resultado);
    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
    return hubo_error;
}

static void verificar_tb_contiene(const char *desc, const char *fuente,
                                    const char *esperado) {
    char tb[2048];
    bool hubo = ejecutar_con_traceback(fuente, tb, sizeof(tb));
    if (!hubo) {
        fprintf(stderr, "FALLO [%s]: esperaba error de runtime\n", desc);
        fallos++;
        return;
    }
    if (strstr(tb, esperado) == NULL) {
        fprintf(stderr, "FALLO [%s]: traceback no contiene '%s'\n  tb: %s\n",
                desc, esperado, tb);
        fallos++;
    }
}

static void verificar_tb_vacio(const char *desc, const char *fuente) {
    char tb[2048];
    bool hubo = ejecutar_con_traceback(fuente, tb, sizeof(tb));
    if (!hubo) {
        fprintf(stderr, "FALLO [%s]: esperaba error de runtime\n", desc);
        fallos++;
        return;
    }
    if (tb[0] != '\0') {
        fprintf(stderr, "FALLO [%s]: traceback debería estar vacío, es '%s'\n",
                desc, tb);
        fallos++;
    }
}

/* ───── Traceback de funciones anidadas ───── */

static void test_traceback_tres_niveles(void) {
    const char *fuente =
        "funcion nivel_c(x):\n"
        "  retornar x + indefinido\n"
        "fin funcion\n"
        "funcion nivel_b(x):\n"
        "  retornar nivel_c(x)\n"
        "fin funcion\n"
        "funcion nivel_a():\n"
        "  retornar nivel_b(10)\n"
        "fin funcion\n"
        "nivel_a()\n";
    verificar_tb_contiene("traza incluye nivel_c", fuente, "en nivel_c");
    verificar_tb_contiene("traza incluye nivel_b", fuente, "en nivel_b");
    verificar_tb_contiene("traza incluye nivel_a", fuente, "en nivel_a");
    verificar_tb_contiene("traza incluye <programa>", fuente, "en <programa>");
}

static void test_traceback_un_nivel(void) {
    const char *fuente =
        "funcion fallar():\n"
        "  retornar 1 / 0\n"
        "fin funcion\n"
        "fallar()\n";
    verificar_tb_contiene("traza incluye la función", fuente, "en fallar");
    verificar_tb_contiene("traza incluye <programa>", fuente, "en <programa>");
    verificar_tb_contiene("encabezado de traza", fuente, "traza");
}

/* ───── Error en top-level: sin traceback ───── */

static void test_error_top_level_sin_traceback(void) {
    /* Error directo en top-level (1 solo frame) → traceback vacío,
       la línea del mensaje basta. */
    verificar_tb_vacio("error top-level no genera traceback",
        "x = variable_inexistente\n");
}

/* ───── Error atrapado: sin traceback ───── */

static void test_error_atrapado_sin_traceback(void) {
    /* Si el error se atrapa, no es fatal → no hay traceback. */
    char tb[2048];
    bool hubo = ejecutar_con_traceback(
        "funcion f():\n"
        "  intentar:\n"
        "    retornar 1 / 0\n"
        "  atrapar ErrorAritmetico como e:\n"
        "    retornar -1\n"
        "  fin intentar\n"
        "fin funcion\n"
        "x = f()\n",
        tb, sizeof(tb));
    if (hubo) {
        fprintf(stderr, "FALLO [error atrapado]: no debería ser error fatal\n");
        fallos++;
    }
    if (tb[0] != '\0') {
        fprintf(stderr, "FALLO [error atrapado]: traceback no debería existir\n");
        fallos++;
    }
}

int main(void) {
    test_traceback_tres_niveles();
    test_traceback_un_nivel();
    test_error_top_level_sin_traceback();
    test_error_atrapado_sin_traceback();

    if (fallos == 0) {
        printf("traceback: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "traceback: %d fallo(s)\n", fallos);
    return 1;
}
