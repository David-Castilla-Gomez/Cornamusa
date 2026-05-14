/*
 * Tests de generadores (v1.31).
 *
 * Cubre `producir` como sentencia que convierte la función en
 * generador. Generadores son iterables via `para...en...`. Soportan
 * pausa/resume con estado preservado entre yields.
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

/* ───── Casos básicos ───── */

static void test_llamar_devuelve_generador(void) {
    verificar_var("llamada a generadora retorna VAL_GENERADOR",
        "funcion g():\n"
        "  producir 1\n"
        "fin funcion\n"
        "x = tipo(g())",
        "x", "generador");
}

static void test_iterar_simple(void) {
    verificar_var("iterar generador con tres yields",
        "funcion g():\n"
        "  producir 1\n"
        "  producir 2\n"
        "  producir 3\n"
        "fin funcion\n"
        "total = 0\n"
        "para v en g():\n"
        "  total = total + v\n"
        "fin para\n"
        "x = total",
        "x", "6");
}

static void test_generador_con_estado(void) {
    verificar_var("generador con bucle + variable local",
        "funcion contar(n):\n"
        "  i = 0\n"
        "  mientras i < n:\n"
        "    producir i\n"
        "    i = i + 1\n"
        "  fin mientras\n"
        "fin funcion\n"
        "ls = []\n"
        "para v en contar(5):\n"
        "  agregar(ls, v)\n"
        "fin para\n"
        "x = ls",
        "x", "[0, 1, 2, 3, 4]");
}

static void test_generador_con_args(void) {
    verificar_var("generador con args y expresiones",
        "funcion dobles(xs):\n"
        "  para x en xs:\n"
        "    producir x * 2\n"
        "  fin para\n"
        "fin funcion\n"
        "ls = []\n"
        "para v en dobles([1, 2, 3, 4]):\n"
        "  agregar(ls, v)\n"
        "fin para\n"
        "x = ls",
        "x", "[2, 4, 6, 8]");
}

/* ───── Generador infinito con romper ───── */

static void test_infinito_con_romper(void) {
    verificar_var("fib infinito iterado con romper",
        "funcion fib():\n"
        "  a = 0\n"
        "  b = 1\n"
        "  mientras verdadero:\n"
        "    producir a\n"
        "    c = a + b\n"
        "    a = b\n"
        "    b = c\n"
        "  fin mientras\n"
        "fin funcion\n"
        "ls = []\n"
        "i = 0\n"
        "para n en fib():\n"
        "  si i >= 8:\n"
        "    romper\n"
        "  fin si\n"
        "  agregar(ls, n)\n"
        "  i = i + 1\n"
        "fin para\n"
        "x = ls",
        "x", "[0, 1, 1, 2, 3, 5, 8, 13]");
}

/* ───── Generador agotado retorna nada ───── */

static void test_agotado(void) {
    verificar_var("iterar dos veces el mismo generador",
        "funcion uno():\n"
        "  producir 42\n"
        "fin funcion\n"
        "g = uno()\n"
        "total = 0\n"
        "para v en g:\n"
        "  total = total + v\n"
        "fin para\n"
        /* segunda iteración: gen ya agotado */
        "para v en g:\n"
        "  total = total + 999\n"
        "fin para\n"
        "x = total",
        "x", "42");
}

/* ───── Generador vacío ───── */

static void test_vacio(void) {
    verificar_var("generador que nunca produce",
        "funcion vacio():\n"
        "  si falso:\n"
        "    producir 1\n"
        "  fin si\n"
        "fin funcion\n"
        "total = 0\n"
        "para v en vacio():\n"
        "  total = total + v\n"
        "fin para\n"
        "x = total",
        "x", "0");
}

/* ───── Producir fuera de función → error compilación ───── */

static void test_producir_top_level(void) {
    const char *err = NULL;
    const char *res = ejecutar("producir 1", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO [producir top-level]: esperaba error\n");
        fallos++;
    }
}

int main(void) {
    test_llamar_devuelve_generador();
    test_iterar_simple();
    test_generador_con_estado();
    test_generador_con_args();
    test_infinito_con_romper();
    test_agotado();
    test_vacio();
    test_producir_top_level();

    if (fallos == 0) {
        printf("generadores: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "generadores: %d fallo(s)\n", fallos);
    return 1;
}
