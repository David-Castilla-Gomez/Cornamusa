/*
 * Tests del bytecode con funciones — Fase 6 sesión 5.
 *
 * Cubre:
 *   - SENT_FUNCION: definición de función top-level.
 *   - EXPR_LLAMADA general: invocación con argumentos.
 *   - SENT_RETORNAR con/sin valor.
 *   - Variables locales (parámetros + locales declaradas en cuerpo).
 *   - Recursión: factorial, fibonacci.
 *   - Aridad: error por número incorrecto de argumentos.
 *   - Errores: no invocable, retornar fuera de función.
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
    static char buffer[2048];

    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 16384);
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

    if (nombre_var == NULL) {
        buffer[0] = '\0';
    } else {
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
    }
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

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, "_no_existe_", &err);
    if (res) {
        fprintf(stderr, "FALLO: programa debería dar error pero ejecutó:\n%s\n",
                fuente);
        fallos++;
        return;
    }
    if (!err || !strstr(err, substring)) {
        fprintf(stderr, "FALLO: '%s' dio '%s' pero se esperaba '%s'\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

/* ───── Funciones básicas ───── */

static void test_funcion_basica(void) {
    verificar_var(
        "funcion saludar():\n"
        "    retornar \"hola\"\n"
        "fin funcion\n"
        "x = saludar()",
        "x", "hola");

    verificar_var(
        "funcion sumar(a, b):\n"
        "    retornar a + b\n"
        "fin funcion\n"
        "x = sumar(3, 4)",
        "x", "7");

    /* Función sin retornar explícito devuelve nulo. */
    verificar_var(
        "funcion nada():\n"
        "    pasar\n"
        "fin funcion\n"
        "x = nada()",
        "x", "nulo");
}

/* ───── Recursión ───── */

static void test_recursion(void) {
    /* factorial(10) = 3_628_800 */
    verificar_var(
        "funcion factorial(n):\n"
        "    si n <= 1:\n"
        "        retornar 1\n"
        "    fin si\n"
        "    retornar n * factorial(n - 1)\n"
        "fin funcion\n"
        "x = factorial(10)",
        "x", "3628800");

    /* factorial(20) sigue cabiendo en bignum (de hecho cabe en u64
     * pero verificamos que la VM bytecode lo computa correctamente). */
    verificar_var(
        "funcion fact(n):\n"
        "    si n == 0:\n"
        "        retornar 1\n"
        "    fin si\n"
        "    retornar n * fact(n - 1)\n"
        "fin funcion\n"
        "x = fact(20)",
        "x", "2432902008176640000");

    /* fib(10) = 55 */
    verificar_var(
        "funcion fib(n):\n"
        "    si n < 2:\n"
        "        retornar n\n"
        "    fin si\n"
        "    retornar fib(n - 1) + fib(n - 2)\n"
        "fin funcion\n"
        "x = fib(10)",
        "x", "55");
}

/* ───── Variables locales ───── */

static void test_locales(void) {
    /* Variable local declarada en el cuerpo. */
    verificar_var(
        "funcion calcular(n):\n"
        "    cuadrado = n * n\n"
        "    cubo = cuadrado * n\n"
        "    retornar cuadrado + cubo\n"
        "fin funcion\n"
        "x = calcular(3)",   /* 9 + 27 = 36 */
        "x", "36");

    /* Reasignación de local. */
    verificar_var(
        "funcion contar():\n"
        "    n = 0\n"
        "    n = n + 1\n"
        "    n = n + 10\n"
        "    retornar n\n"
        "fin funcion\n"
        "x = contar()",
        "x", "11");

    /* Local con mismo nombre que global (debe sombrear). */
    verificar_var(
        "x = 100\n"
        "funcion test():\n"
        "    x = 5\n"
        "    retornar x\n"
        "fin funcion\n"
        "y_local = test()\n"
        "y_global = x",
        "y_global", "100");
}

/* ───── Aridad ───── */

static void test_aridad(void) {
    verificar_error(
        "funcion sumar(a, b):\n"
        "    retornar a + b\n"
        "fin funcion\n"
        "x = sumar(1)",
        "esperaba 2 argumentos");

    verificar_error(
        "funcion sumar(a, b):\n"
        "    retornar a + b\n"
        "fin funcion\n"
        "x = sumar(1, 2, 3)",
        "esperaba 2 argumentos");
}

/* ───── retornar ───── */

static void test_retornar(void) {
    /* Retornar sin valor → nulo. */
    verificar_var(
        "funcion f():\n"
        "    retornar\n"
        "fin funcion\n"
        "x = f()",
        "x", "nulo");

    /* Retornar dentro de un bucle. */
    verificar_var(
        "funcion buscar_par(limite):\n"
        "    i = 1\n"
        "    mientras i <= limite:\n"
        "        si i % 2 == 0:\n"
        "            retornar i\n"
        "        fin si\n"
        "        i += 1\n"
        "    fin mientras\n"
        "    retornar nulo\n"
        "fin funcion\n"
        "x = buscar_par(7)",
        "x", "2");

    /* `retornar` fuera de función → error de compilación. */
    verificar_error("retornar 5", "fuera de una funcion");
}

/* ───── No invocable ───── */

static void test_no_invocable(void) {
    verificar_error(
        "x = 5\n"
        "z = x()",
        "no es invocable");
}

/* ───── Locales no contaminan globales ───── */

static void test_locales_aisladas(void) {
    /* Una variable definida solo dentro de la función NO aparece como
     * global tras la llamada. */
    verificar_var(
        "funcion hace_algo():\n"
        "    secreto = 42\n"
        "    retornar nulo\n"
        "fin funcion\n"
        "ignorado = hace_algo()\n"
        "ignorado = 1",
        "ignorado", "1");

    /* Verificamos que `secreto` NO esté como global. */
    const char *err = NULL;
    const char *res = ejecutar(
        "funcion hace_algo():\n"
        "    secreto = 42\n"
        "    retornar nulo\n"
        "fin funcion\n"
        "ignorado = hace_algo()\n"
        "x = secreto",
        "x", &err);
    if (res != NULL) {
        fprintf(stderr,
            "FALLO: 'secreto' filtrado al scope global (resultado: %s)\n", res);
        fallos++;
    } else if (!err || !strstr(err, "no esta definido")) {
        fprintf(stderr, "FALLO: error inesperado: %s\n", err ? err : "<null>");
        fallos++;
    }
}

/* ───── Built-ins nativas via OP_LLAMAR ───── */

static void test_nativas(void) {
    /* longitud sobre cadena. */
    verificar_var("x = longitud(\"hola\")", "x", "4");
    /* longitud sobre cadena UTF-8 (4 code points). */
    verificar_var("x = longitud(\"niño\")", "x", "4");
    /* tipo sobre varios tipos. */
    verificar_var("x = tipo(42)", "x", "entero");
    verificar_var("x = tipo(3.14)", "x", "decimal");
    verificar_var("x = tipo(\"hola\")", "x", "cadena");
    verificar_var("x = tipo(verdadero)", "x", "booleano");
    verificar_var("x = tipo(nulo)", "x", "nulo");
    /* rango() devuelve un tipo "rango"; longitud(rango(N)) = N. */
    verificar_var("x = longitud(rango(10))", "x", "10");
    verificar_var("x = longitud(rango(2, 12))", "x", "10");
    verificar_var("x = longitud(rango(0, 10, 3))", "x", "4");

    /* Composición de nativas con cálculos. */
    verificar_var(
        "funcion mide(s):\n"
        "    retornar longitud(s) * 2\n"
        "fin funcion\n"
        "x = mide(\"abc\")",
        "x", "6");
}

/* ───── Programa realista: factorial(50) bignum ───── */

static void test_factorial_grande(void) {
    /* 50! = 30414093201713378043612608166064768844377641568960512000000000000 */
    verificar_var(
        "funcion fact(n):\n"
        "    si n == 0:\n"
        "        retornar 1\n"
        "    fin si\n"
        "    retornar n * fact(n - 1)\n"
        "fin funcion\n"
        "x = fact(50)",
        "x", "30414093201713378043612608166064768844377641568960512000000000000");
}

int main(void) {
    test_funcion_basica();
    test_recursion();
    test_locales();
    test_aridad();
    test_retornar();
    test_no_invocable();
    test_locales_aisladas();
    test_nativas();
    test_factorial_grande();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode con funciones pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
