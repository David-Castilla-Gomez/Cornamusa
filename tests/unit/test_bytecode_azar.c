/*
 * Tests de azar (v1.26) — built-ins primitivos.
 *
 * El módulo stdlib/azar.cor no se prueba aquí (los tests unitarios
 * no tienen acceso a stdlib). Solo verificamos las primitivas:
 *
 *   - azar_decimal() ∈ [0, 1) (chequeo de rango sobre muestras).
 *   - azar_entero(a, b) ∈ [a, b].
 *   - azar_semilla(n) hace el PRNG reproducible.
 *   - Distribución aproximadamente uniforme (chi-square ligero).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "compilador.h"
#include "lexer.h"
#include "parser.h"
#include "valor.h"
#include "vm.h"

static int fallos = 0;

static const char *ejecutar(const char *fuente, const char *nombre_var,
                              const char **error_out) {
    static char buffer[4096];
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (!prog || p.tuvo_error) {
        if (error_out) *error_out = "<error de parseo>";
        arena_destruir(&a);
        return NULL;
    }
    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, prog, n)) {
        if (error_out) *error_out = c.error.mensaje;
        chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    VM vm; vm_iniciar(&vm);
    Valor resultado = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &resultado);
    if (rc != VM_OK) {
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", vm.error.mensaje);
            *error_out = errbuf;
        }
        valor_destruir(&resultado);
        vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    Valor nombre = valor_cadena_referencia(nombre_var, (int)strlen(nombre_var));
    Valor v;
    if (!dicc_obtener(vm.globales, &nombre, &v)) {
        if (error_out) *error_out = "<variable no encontrada>";
        valor_destruir(&resultado);
        vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    valor_a_cadena(&v, buffer, sizeof(buffer));
    valor_destruir(&v);
    valor_destruir(&resultado);
    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
    if (error_out) *error_out = NULL;
    return buffer;
}

static void verificar_var(const char *desc, const char *fuente,
                           const char *var, const char *esperado) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, var, &err);
    if (!res) {
        fprintf(stderr, "FALLO [%s]: error %s\n", desc,
                err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO [%s]: %s=%s (esperaba %s)\n",
                desc, var, res, esperado);
        fallos++;
    }
}

/* ───── Reproducibilidad con semilla ───── */

static void test_semilla_reproducible(void) {
    /* Misma semilla → mismas dos secuencias. */
    verificar_var("semilla reproducible",
        "azar_semilla(42)\n"
        "a = azar_decimal()\n"
        "b = azar_decimal()\n"
        "azar_semilla(42)\n"
        "c = azar_decimal()\n"
        "d = azar_decimal()\n"
        "x = (a == c y b == d)",
        "x", "verdadero");
}

static void test_semilla_distinta(void) {
    verificar_var("semillas distintas",
        "azar_semilla(1)\n"
        "a = azar_decimal()\n"
        "azar_semilla(2)\n"
        "b = azar_decimal()\n"
        "x = (a != b)",
        "x", "verdadero");
}

/* ───── Rango de decimal ───── */

static void test_decimal_rango(void) {
    verificar_var("decimal en [0, 1) en 1000 muestras",
        "azar_semilla(7)\n"
        "ok = verdadero\n"
        "i = 0\n"
        "mientras i < 1000:\n"
        "  d = azar_decimal()\n"
        "  si d < 0.0 o d >= 1.0:\n"
        "    ok = falso\n"
        "  fin si\n"
        "  i = i + 1\n"
        "fin mientras\n"
        "x = ok",
        "x", "verdadero");
}

/* ───── Rango de entero ───── */

static void test_entero_rango(void) {
    verificar_var("entero ∈ [1, 6] en 500 muestras",
        "azar_semilla(99)\n"
        "ok = verdadero\n"
        "i = 0\n"
        "mientras i < 500:\n"
        "  n = azar_entero(1, 6)\n"
        "  si n < 1 o n > 6:\n"
        "    ok = falso\n"
        "  fin si\n"
        "  i = i + 1\n"
        "fin mientras\n"
        "x = ok",
        "x", "verdadero");
}

static void test_entero_extremos(void) {
    verificar_var("entero a==b retorna a",
        "azar_semilla(1)\n"
        "x = azar_entero(7, 7)",
        "x", "7");
}

/* ───── Distribución aproximada (chi-square ligero) ───── */

static void test_distribucion_dado(void) {
    /* Tirar un d6 6000 veces. Cada cara debería salir ~1000 veces.
       Chequeo laxo: cuenta de cada cara debe estar en [800, 1200]. */
    verificar_var("d6 6000 tiradas, sin sesgo evidente",
        "azar_semilla(12345)\n"
        "cuentas = [0, 0, 0, 0, 0, 0]\n"
        "i = 0\n"
        "mientras i < 6000:\n"
        "  c = azar_entero(0, 5)\n"
        "  cuentas[c] = cuentas[c] + 1\n"
        "  i = i + 1\n"
        "fin mientras\n"
        "ok = verdadero\n"
        "para v en cuentas:\n"
        "  si v < 800 o v > 1200:\n"
        "    ok = falso\n"
        "  fin si\n"
        "fin para\n"
        "x = ok",
        "x", "verdadero");
}

/* ───── Errores ───── */

static void test_error_entero_a_mayor_que_b(void) {
    verificar_var("a > b da error",
        "intentar:\n"
        "  azar_entero(10, 5)\n"
        "atrapar ErrorDeValor como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_decimal_con_args(void) {
    verificar_var("azar_decimal() rechaza args",
        "intentar:\n"
        "  azar_decimal(1, 2)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

int main(void) {
    test_semilla_reproducible();
    test_semilla_distinta();
    test_decimal_rango();
    test_entero_rango();
    test_entero_extremos();
    test_distribucion_dado();
    test_error_entero_a_mayor_que_b();
    test_error_decimal_con_args();

    if (fallos == 0) {
        printf("azar: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "azar: %d fallo(s)\n", fallos);
    return 1;
}
