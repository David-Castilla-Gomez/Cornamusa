/*
 * Tests del bytecode con control de flujo — Fase 6 sesión 4.
 *
 * Cubre:
 *   - EXPR_LOGICA (`y` y `o`) con cortocircuito real (verificable
 *     porque sub-expresión "ruidosa" no se evalúa).
 *   - SENT_SI con cadenas si/sino si/sino.
 *   - SENT_MIENTRAS clásico, con romper/continuar y cláusula sino.
 *   - SENT_ASIGNAR_AUG: x += y, x *= y, etc.
 *   - Programas realistas: factorial iterativo, suma de pares, fizzbuzz.
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

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n",                            \
                    __FILE__, __LINE__, #cond);                                \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

static const char *ejecutar_y_leer(const char *fuente, const char *nombre_var,
                                    const char **error_out) {
    static char buffer[2048];

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
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
    const char *res = ejecutar_y_leer(fuente, var, &err);
    if (!res) {
        fprintf(stderr, "FALLO en programa:\n%s\n  error: %s\n",
                fuente, err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO en programa:\n%s\n  esperaba %s=%s\n  obtuvo: %s\n",
                fuente, var, esperado, res);
        fallos++;
    }
}

/* ───── Lógica con cortocircuito ───── */

static void test_logica(void) {
    verificar_var("x = verdadero y verdadero", "x", "verdadero");
    verificar_var("x = verdadero y falso",     "x", "falso");
    verificar_var("x = falso y verdadero",     "x", "falso");
    verificar_var("x = verdadero o falso",     "x", "verdadero");
    verificar_var("x = falso o falso",         "x", "falso");

    /* Devuelve el valor decisor original (no booleano), Python style. */
    verificar_var("x = 0 o 42", "x", "42");
    verificar_var("x = 1 y 2",  "x", "2");
    verificar_var("x = nulo o \"hola\"", "x", "hola");

    /* Cortocircuito real: `1 // 0` daría error si se evaluara. */
    verificar_var("x = verdadero o (1 // 0)", "x", "verdadero");
    verificar_var("x = falso y (1 // 0)",     "x", "falso");
}

/* ───── si / sino si / sino ───── */

static void test_si(void) {
    verificar_var(
        "x = 0\n"
        "si verdadero:\n"
        "    x = 1\n"
        "fin si",
        "x", "1");

    verificar_var(
        "x = 0\n"
        "si falso:\n"
        "    x = 1\n"
        "sino:\n"
        "    x = 2\n"
        "fin si",
        "x", "2");

    verificar_var(
        "n = 7\n"
        "si n < 5:\n"
        "    cat = \"pequenio\"\n"
        "sino si n < 10:\n"
        "    cat = \"mediano\"\n"
        "sino:\n"
        "    cat = \"grande\"\n"
        "fin si",
        "cat", "mediano");

    /* One-liner. */
    verificar_var("x = 0\nsi verdadero: x = 99", "x", "99");
}

/* ───── mientras + romper/continuar/sino ───── */

static void test_mientras(void) {
    verificar_var(
        "i = 1\n"
        "total = 0\n"
        "mientras i <= 10:\n"
        "    total += i\n"
        "    i += 1\n"
        "fin mientras",
        "total", "55");

    /* Romper. */
    verificar_var(
        "i = 0\n"
        "mientras verdadero:\n"
        "    i += 1\n"
        "    si i == 5:\n"
        "        romper\n"
        "    fin si\n"
        "fin mientras",
        "i", "5");

    /* Continuar — sumar solo pares hasta 10. */
    verificar_var(
        "i = 0\n"
        "total = 0\n"
        "mientras i < 10:\n"
        "    i += 1\n"
        "    si i % 2 == 1:\n"
        "        continuar\n"
        "    fin si\n"
        "    total += i\n"
        "fin mientras",
        "total", "30");

    /* Cláusula sino: condición se vuelve falsa → ejecuta sino. */
    verificar_var(
        "i = 0\n"
        "ok = falso\n"
        "mientras i < 3:\n"
        "    i += 1\n"
        "sino:\n"
        "    ok = verdadero\n"
        "fin mientras",
        "ok", "verdadero");

    /* Cláusula sino: NO se ejecuta tras romper. */
    verificar_var(
        "i = 0\n"
        "ok = falso\n"
        "mientras i < 3:\n"
        "    i += 1\n"
        "    romper\n"
        "sino:\n"
        "    ok = verdadero\n"
        "fin mientras",
        "ok", "falso");

    /* Errores: romper/continuar fuera de bucle. */
    /* Estos los rechaza el compilador con "fuera de un bucle". */
}

/* ───── Asignación aumentada ───── */

static void test_asignacion_aug(void) {
    verificar_var("x = 10\nx += 5", "x", "15");
    verificar_var("x = 10\nx -= 3", "x", "7");
    verificar_var("x = 6\nx *= 7", "x", "42");
    verificar_var("x = 100\nx //= 7", "x", "14");
    verificar_var("x = 10\nx %= 3", "x", "1");
    verificar_var("x = 2\nx **= 10", "x", "1024");
    verificar_var("x = 7\nx /= 2", "x", "3.5");
    verificar_var("s = \"hola \"\ns += \"mundo\"", "s", "hola mundo");
}

/* ───── Programas realistas end-to-end ───── */

static void test_factorial_iterativo(void) {
    /* 25! = 15_511_210_043_330_985_984_000_000 (bignum). */
    verificar_var(
        "n = 25\n"
        "resultado = 1\n"
        "i = 1\n"
        "mientras i <= n:\n"
        "    resultado *= i\n"
        "    i += 1\n"
        "fin mientras",
        "resultado", "15511210043330985984000000");
}

static void test_fibonacci_iterativo(void) {
    verificar_var(
        "a = 0\n"
        "b = 1\n"
        "n = 30\n"
        "i = 0\n"
        "mientras i < n:\n"
        "    t = a + b\n"
        "    a = b\n"
        "    b = t\n"
        "    i += 1\n"
        "fin mientras",
        "a", "832040");
}

static void test_potencia_de_2(void) {
    /* 2^64 = 18_446_744_073_709_551_616 (bignum). */
    verificar_var(
        "p = 1\n"
        "i = 0\n"
        "mientras i < 64:\n"
        "    p *= 2\n"
        "    i += 1\n"
        "fin mientras",
        "p", "18446744073709551616");
}

static void test_anidamiento(void) {
    /* Matriz de pares en [0..10]. */
    verificar_var(
        "i = 0\n"
        "pares = 0\n"
        "mientras i < 10:\n"
        "    si i % 2 == 0:\n"
        "        pares += 1\n"
        "    fin si\n"
        "    i += 1\n"
        "fin mientras",
        "pares", "5");

    /* mientras anidado en mientras. */
    verificar_var(
        "i = 0\n"
        "total = 0\n"
        "mientras i < 3:\n"
        "    j = 0\n"
        "    mientras j < 3:\n"
        "        total += 1\n"
        "        j += 1\n"
        "    fin mientras\n"
        "    i += 1\n"
        "fin mientras",
        "total", "9");
}

int main(void) {
    test_logica();
    test_si();
    test_mientras();
    test_asignacion_aug();
    test_factorial_iterativo();
    test_fibonacci_iterativo();
    test_potencia_de_2();
    test_anidamiento();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode con control de flujo pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
