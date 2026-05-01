/*
 * Tests de los métodos especiales (dunders) v1.2.
 *
 * Cubre:
 *   - Aritméticos binarios: __sumar__, __restar__, __multiplicar__,
 *     __dividir__, __dividir_entero__, __modulo__, __potencia__.
 *   - Comparación: __igual__, __menor__, __mayor__, etc.
 *   - Coerción: __cadena__ (en f-strings y `imprimir`).
 *   - Indexación: __indice__ y __asignar_indice__.
 *   - Validación: __cadena__ que retorna no-cadena → ErrorDeTipo.
 *   - Coexistencia con IC F10: degradar a slow path para invocar
 *     dunder cuando aparece una instancia tras un site INT_INT.
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
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", c.error.mensaje);
            *error_out = errbuf;
        }
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

static void verificar_var(const char *fuente, const char *var,
                           const char *esperado) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, var, &err);
    if (!res) {
        fprintf(stderr, "FALLO: %s\n  -> error: %s\n", fuente,
                err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO: %s\n  -> %s=%s (esperaba %s)\n",
                fuente, var, res, esperado);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, "_n", &err);
    if (res) {
        fprintf(stderr, "FALLO: '%s' debia dar error pero ejecuto\n", fuente);
        fallos++;
        return;
    }
    if (!err || !strstr(err, substring)) {
        fprintf(stderr, "FALLO: '%s' dio '%s' (esperaba '%s')\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

#define DEFINE_VEC \
    "clase V:\n" \
    "  funcion __iniciar__(yo, a, b):\n" \
    "    yo.a = a\n" \
    "    yo.b = b\n" \
    "  fin funcion\n"

/* ───── Aritméticos binarios ───── */

static void test_sumar(void) {
    verificar_var(DEFINE_VEC
                  "  funcion __sumar__(yo, otro):\n"
                  "    retornar V(yo.a + otro.a, yo.b + otro.b)\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "v1 = V(1, 2)\n"
                  "v2 = V(3, 4)\n"
                  "r = v1 + v2\n"
                  "x = r.a + r.b * 10",
                  "x", "64");  /* 4 + 6*10 = 64 */
}

static void test_restar_multiplicar_dividir(void) {
    verificar_var(DEFINE_VEC
                  "  funcion __restar__(yo, otro):\n"
                  "    retornar V(yo.a - otro.a, yo.b - otro.b)\n"
                  "  fin funcion\n"
                  "  funcion __multiplicar__(yo, k):\n"
                  "    retornar V(yo.a * k, yo.b * k)\n"
                  "  fin funcion\n"
                  "  funcion __dividir__(yo, k):\n"
                  "    retornar V(yo.a // k, yo.b // k)\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "v = V(10, 20)\n"
                  "r1 = v - V(1, 1)\n"
                  "r2 = v * 3\n"
                  "r3 = v / 2\n"
                  "x = r1.a + r2.a + r3.a",
                  "x", "44");  /* 9 + 30 + 5 */
}

static void test_modulo_potencia(void) {
    verificar_var(DEFINE_VEC
                  "  funcion __modulo__(yo, otro):\n"
                  "    retornar V(yo.a % otro.a, yo.b % otro.b)\n"
                  "  fin funcion\n"
                  "  funcion __potencia__(yo, n):\n"
                  "    retornar V(yo.a ** n, yo.b ** n)\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "v = V(10, 7)\n"
                  "r1 = v % V(3, 4)\n"
                  "r2 = v ** 2\n"
                  "x = r1.a * 100 + r2.b",
                  "x", "149");  /* 10%3=1 → 1*100=100; 7**2=49 → 100+49=149 */
}

/* ───── Comparación ───── */

#define CMP_BASE \
    DEFINE_VEC \
    "  funcion __igual__(yo, otro):\n" \
    "    retornar yo.a == otro.a y yo.b == otro.b\n" \
    "  fin funcion\n" \
    "  funcion __menor__(yo, otro):\n" \
    "    retornar yo.a < otro.a\n" \
    "  fin funcion\n" \
    "  funcion __mayor__(yo, otro):\n" \
    "    retornar yo.a > otro.a\n" \
    "  fin funcion\n" \
    "fin clase\n"

static void test_comparacion(void) {
    verificar_var(CMP_BASE "x = V(1, 2) == V(1, 2)", "x", "verdadero");
    verificar_var(CMP_BASE "x = V(1, 2) == V(1, 3)", "x", "falso");
    verificar_var(CMP_BASE "x = V(1, 2) < V(2, 2)",  "x", "verdadero");
    verificar_var(CMP_BASE "x = V(2, 2) > V(1, 2)",  "x", "verdadero");
}

/* ───── __cadena__ ───── */

#define STR_BASE \
    DEFINE_VEC \
    "  funcion __cadena__(yo):\n" \
    "    retornar \"<\" + cadena(yo.a) + \",\" + cadena(yo.b) + \">\"\n" \
    "  fin funcion\n" \
    "fin clase\n"

static void test_cadena_dunder(void) {
    verificar_var(STR_BASE "v = V(3, 4)\nx = f\"v={v}\"", "x", "v=<3,4>");
    verificar_var(STR_BASE "x = f\"a={V(1, 2)} b={V(3, 4)}\"",
                  "x", "a=<1,2> b=<3,4>");
}

static void test_cadena_dunder_error(void) {
    /* __cadena__ que retorna no-cadena debe dar ErrorDeTipo claro. */
    verificar_error(DEFINE_VEC
                    "  funcion __cadena__(yo):\n"
                    "    retornar 42\n"
                    "  fin funcion\n"
                    "fin clase\n"
                    "x = f\"{V(1, 2)}\"",
                    "__cadena__ debe retornar cadena");
}

/* ───── Indexación ───── */

static void test_indice(void) {
    verificar_var(DEFINE_VEC
                  "  funcion __indice__(yo, k):\n"
                  "    si k == 0:\n"
                  "      retornar yo.a\n"
                  "    fin si\n"
                  "    retornar yo.b\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "v = V(7, 11)\n"
                  "x = v[0] + v[1]",
                  "x", "18");
}

static void test_asignar_indice(void) {
    verificar_var(DEFINE_VEC
                  "  funcion __asignar_indice__(yo, k, val):\n"
                  "    si k == 0:\n"
                  "      yo.a = val\n"
                  "    sino:\n"
                  "      yo.b = val\n"
                  "    fin si\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "v = V(0, 0)\n"
                  "v[0] = 99\n"
                  "v[1] = 88\n"
                  "x = v.a * 100 + v.b",
                  "x", "9988");
}

/* ───── Coexistencia con IC F10 ───── */

static void test_ic_no_se_rompe(void) {
    /* Site polimorfico: int+int promueve a INT_INT, luego degrade
       cuando aparece una instancia, finalmente re-promueve. */
    verificar_var(DEFINE_VEC
                  "  funcion __sumar__(yo, otro):\n"
                  "    retornar V(yo.a + otro.a, yo.b + otro.b)\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "funcion s(a, b):\n"
                  "  retornar a + b\n"
                  "fin funcion\n"
                  "r1 = s(1, 2)\n"               /* int+int */
                  "v = s(V(3, 4), V(5, 6))\n"     /* dunder degrade */
                  "r2 = s(10, 20)\n"             /* int+int re-promote */
                  "x = r1 + v.a + r2",
                  "x", "41");  /* 3 + 8 + 30 = 41 */
}

/* ───── Sin dunder definido ───── */

static void test_sin_dunder_da_error(void) {
    verificar_error(DEFINE_VEC "fin clase\n"
                    "x = V(1, 2) + V(3, 4)",
                    "operador");
}

int main(void) {
    test_sumar();
    test_restar_multiplicar_dividir();
    test_modulo_potencia();
    test_comparacion();
    test_cadena_dunder();
    test_cadena_dunder_error();
    test_indice();
    test_asignar_indice();
    test_ic_no_se_rompe();
    test_sin_dunder_da_error();

    if (fallos == 0) {
        printf("dunders: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "dunders: %d fallo(s)\n", fallos);
    return 1;
}
