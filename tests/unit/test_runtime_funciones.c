/*
 * Tests del runtime — Fase 4 Sesión 4: funciones y built-ins.
 *
 * Cobertura:
 *   - Definición y llamada a funciones top-level (sin closures, B2).
 *   - Recursión: factorial, fibonacci.
 *   - Parámetros con valor por defecto.
 *   - Aridad: argumentos faltantes (con/sin defaults), exceso, función
 *     no invocable.
 *   - `retornar` con/sin valor, retornar dentro de bucles.
 *   - Built-ins: `imprimir` (vía longitud para no comparar stdout),
 *     `longitud` (cadena UTF-8 + rango), `tipo`, `rango` (1/2/3 args).
 *   - `para` sobre `rango()`: ascendente, descendente, paso != 1, cero
 *     iteraciones cuando paso/dirección no progresan.
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "entorno.h"
#include "evaluador.h"
#include "lexer.h"
#include "nativos.h"
#include "parser.h"
#include "valor.h"

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
    static char buffer[1024];

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");

    Arena a;
    arena_iniciar(&a, 4096);

    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");

    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (prog == NULL || p.tuvo_error) {
        if (error_out) *error_out = "<error de parseo>";
        arena_destruir(&a);
        return NULL;
    }

    Entorno globales;
    entorno_iniciar(&globales, NULL);
    nativos_registrar(&globales);

    Evaluador ev;
    evaluador_iniciar(&ev, &globales);

    evaluador_ejecutar_programa(&ev, prog, n);

    if (ev.error.tuvo_error) {
        if (error_out) {
            static char errbuf[1024];
            snprintf(errbuf, sizeof(errbuf), "%s", ev.error.mensaje);
            *error_out = errbuf;
        }
        valor_destruir(&ev.valor_retorno);
        entorno_destruir(&globales);
        arena_destruir(&a);
        return NULL;
    }

    Valor v;
    if (!entorno_obtener(&globales, nombre_var, (int)strlen(nombre_var), &v)) {
        if (error_out) *error_out = "<variable no encontrada>";
        valor_destruir(&ev.valor_retorno);
        entorno_destruir(&globales);
        arena_destruir(&a);
        return NULL;
    }

    valor_a_cadena(&v, buffer, sizeof(buffer));
    valor_destruir(&v);
    valor_destruir(&ev.valor_retorno);
    entorno_destruir(&globales);
    arena_destruir(&a);
    if (error_out) *error_out = NULL;
    return buffer;
}

static void verificar_var(const char *fuente, const char *var,
                          const char *esperado) {
    const char *err = NULL;
    const char *resultado = ejecutar_y_leer(fuente, var, &err);
    if (resultado == NULL) {
        fprintf(stderr, "FALLO en programa:\n%s\n  error: %s\n",
                fuente, err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(resultado, esperado) != 0) {
        fprintf(stderr, "FALLO en programa:\n%s\n  esperaba %s=%s\n  obtuvo: %s\n",
                fuente, var, esperado, resultado);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *resultado = ejecutar_y_leer(fuente, "x", &err);
    if (resultado != NULL) {
        fprintf(stderr, "FALLO: programa debería dar error pero ejecutó:\n%s\n  resultado: %s\n",
                fuente, resultado);
        fallos++;
        return;
    }
    if (err == NULL || strstr(err, substring) == NULL) {
        fprintf(stderr, "FALLO: programa\n%s\n  dio error '%s' pero se esperaba '%s'\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

/* ───── Definición y llamada simple ───── */

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

    /* factorial(50) = 30414093201713378043612608166064768844377641568960512000000000000 */
    verificar_var(
        "funcion factorial(n):\n"
        "    si n <= 1:\n"
        "        retornar 1\n"
        "    fin si\n"
        "    retornar n * factorial(n - 1)\n"
        "fin funcion\n"
        "x = factorial(50)",
        "x", "30414093201713378043612608166064768844377641568960512000000000000");

    /* fibonacci recursivo. */
    verificar_var(
        "funcion fib(n):\n"
        "    si n < 2:\n"
        "        retornar n\n"
        "    fin si\n"
        "    retornar fib(n - 1) + fib(n - 2)\n"
        "fin funcion\n"
        "x = fib(15)",
        "x", "610");
}

/* ───── Parámetros con default ───── */

static void test_parametros_default(void) {
    verificar_var(
        "funcion saludar(nombre=\"mundo\"):\n"
        "    retornar \"hola \" + nombre\n"
        "fin funcion\n"
        "x = saludar()",
        "x", "hola mundo");

    verificar_var(
        "funcion saludar(nombre=\"mundo\"):\n"
        "    retornar \"hola \" + nombre\n"
        "fin funcion\n"
        "x = saludar(\"David\")",
        "x", "hola David");

    /* Mezcla: req + default. */
    verificar_var(
        "funcion potencia(base, exp=2):\n"
        "    retornar base ** exp\n"
        "fin funcion\n"
        "a = potencia(5)\n"     /* 25 */
        "b = potencia(2, 8)",   /* 256 */
        "a", "25");

    verificar_var(
        "funcion potencia(base, exp=2):\n"
        "    retornar base ** exp\n"
        "fin funcion\n"
        "a = potencia(5)\n"
        "b = potencia(2, 8)",
        "b", "256");
}

/* ───── Aridad / errores ───── */

static void test_aridad(void) {
    verificar_error(
        "funcion sumar(a, b):\n"
        "    retornar a + b\n"
        "fin funcion\n"
        "x = sumar(1)",
        "esperaba 2 argumentos, recibio 1");

    verificar_error(
        "funcion sumar(a, b):\n"
        "    retornar a + b\n"
        "fin funcion\n"
        "x = sumar(1, 2, 3)",
        "esperaba 2 argumentos, recibio 3");

    /* No invocable. */
    verificar_error(
        "x = 5\n"
        "z = x()",
        "no es invocable");
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

    /* Retornar dentro de bucle. */
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
}

/* ───── built-ins: tipo() ───── */

static void test_tipo(void) {
    verificar_var("x = tipo(42)", "x", "entero");
    verificar_var("x = tipo(3.14)", "x", "decimal");
    verificar_var("x = tipo(\"hola\")", "x", "cadena");
    verificar_var("x = tipo(verdadero)", "x", "booleano");
    verificar_var("x = tipo(nulo)", "x", "nulo");
    verificar_var("x = tipo(rango(5))", "x", "rango");
    verificar_var("x = tipo(tipo)", "x", "funcion");
}

/* ───── built-ins: longitud() ───── */

static void test_longitud(void) {
    verificar_var("x = longitud(\"hola\")", "x", "4");
    verificar_var("x = longitud(\"\")", "x", "0");
    /* UTF-8: "niño" = 4 code points (no 5 bytes). */
    verificar_var("x = longitud(\"niño\")", "x", "4");
    /* Rango. */
    verificar_var("x = longitud(rango(10))", "x", "10");
    verificar_var("x = longitud(rango(2, 12))", "x", "10");
    verificar_var("x = longitud(rango(0, 10, 3))", "x", "4");  /* 0,3,6,9 */
    verificar_var("x = longitud(rango(10, 0, -1))", "x", "10");
    verificar_var("x = longitud(rango(0, 0))", "x", "0");
    /* Tipo no soportado. */
    verificar_error("x = longitud(42)", "no soporta");
}

/* ───── built-ins: rango() y para sobre rango ───── */

static void test_rango_y_para(void) {
    /* Ascendente clásico. */
    verificar_var(
        "total = 0\n"
        "para i en rango(1, 11):\n"
        "    total += i\n"
        "fin para",
        "total", "55");  /* suma 1..10 */

    /* rango(n) → 0..n-1 */
    verificar_var(
        "total = 0\n"
        "para i en rango(5):\n"
        "    total += i\n"
        "fin para",
        "total", "10");  /* 0+1+2+3+4 */

    /* Paso > 1 */
    verificar_var(
        "total = 0\n"
        "para i en rango(0, 20, 3):\n"
        "    total += i\n"
        "fin para",
        "total", "63");  /* 0+3+6+9+12+15+18 */

    /* Paso negativo. */
    verificar_var(
        "ultimo = 99\n"
        "para i en rango(10, 0, -1):\n"
        "    ultimo = i\n"
        "fin para",
        "ultimo", "1");

    /* Cero iteraciones (paso no permite avanzar). */
    verificar_var(
        "total = 0\n"
        "para i en rango(0, 10, -1):\n"
        "    total += 1\n"
        "fin para",
        "total", "0");

    /* paso == 0 → error. */
    verificar_error(
        "para i en rango(0, 10, 0):\n"
        "    pasar\n"
        "fin para",
        "no admite paso 0");
}

/* ───── Combinación: imprimir no se rompe (sin verificar stdout) ───── */

static void test_imprimir_no_rompe(void) {
    /* No comparamos stdout, pero verificamos que no haya error. */
    verificar_var(
        "imprimir(\"silencio\")\n"
        "imprimir(1, 2, 3)\n"
        "imprimir()\n"
        "x = 42",
        "x", "42");
}

/* ───── Programas realistas ───── */

static void test_programa_pares(void) {
    verificar_var(
        "funcion es_par(n):\n"
        "    retornar n % 2 == 0\n"
        "fin funcion\n"
        "pares = 0\n"
        "para i en rango(1, 21):\n"
        "    si es_par(i):\n"
        "        pares += 1\n"
        "    fin si\n"
        "fin para",
        "pares", "10");  /* 2,4,...,20 */
}

static void test_programa_factorial_grande_recursivo(void) {
    /* factorial(100) recursivo: 158 dígitos. */
    verificar_var(
        "funcion fact(n):\n"
        "    si n <= 1:\n"
        "        retornar 1\n"
        "    fin si\n"
        "    retornar n * fact(n - 1)\n"
        "fin funcion\n"
        "x = fact(100)",
        "x",
        "93326215443944152681699238856266700490715968264381621468592963895217599993229915608941463976156518286253697920827223758251185210916864000000000000000000000000");
}

/* ───── Main ───── */

int main(void) {
    test_funcion_basica();
    test_recursion();
    test_parametros_default();
    test_aridad();
    test_retornar();
    test_tipo();
    test_longitud();
    test_rango_y_para();
    test_imprimir_no_rompe();
    test_programa_pares();
    test_programa_factorial_grande_recursivo();

    if (fallos == 0) {
        printf("OK: todos los tests de funciones y built-ins pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
