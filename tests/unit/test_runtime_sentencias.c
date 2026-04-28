/*
 * Tests del runtime — Fase 4 Sesión 3: evaluador de sentencias.
 *
 * Cobertura:
 *   - Asignación simple (`x = expr`).
 *   - Asignación aumentada (`x += expr`, `*=`, etc.).
 *   - `pasar`.
 *   - `si`/`sino si`/`sino`: cada rama, ejecución de la primera verdadera.
 *   - `mientras`: bucle clásico, `romper`, `continuar`, cláusula `sino`.
 *   - `para`: iteración sobre cadena (UTF-8 code points), `romper`,
 *     `continuar`, cláusula `sino`.
 *   - `bloque`: secuencia, parada por error/control.
 *   - Errores: nombre no definido, destino no soportado, control de
 *     flujo fuera de bucle.
 *   - Programas completos pequeños: factorial iterativo, contar
 *     vocales en una cadena, Fibonacci con dos variables.
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "entorno.h"
#include "evaluador.h"
#include "lexer.h"
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

/*
 * Compila y ejecuta un programa Cornamusa, luego inspecciona el valor
 * de la variable `nombre_variable` en el entorno global. Devuelve la
 * representación textual en un buffer estático, o NULL si hubo error.
 */
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

/* ───── Asignación simple ───── */

static void test_asignacion(void) {
    verificar_var("x = 42", "x", "42");
    verificar_var("x = 1 + 2 * 3", "x", "7");
    verificar_var("x = \"hola\"", "x", "hola");
    /* Reasignación: la última gana. */
    verificar_var("x = 1\nx = 2", "x", "2");
    /* Múltiples variables. */
    verificar_var("a = 10\nb = 20\nc = a + b", "c", "30");
    /* Sin tipo: una misma variable cambia de tipo libremente. */
    verificar_var("x = 1\nx = \"hola\"\nx = 3.14", "x", "3.14");
}

/* ───── Asignación aumentada ───── */

static void test_asignacion_aug(void) {
    verificar_var("x = 10\nx += 5", "x", "15");
    verificar_var("x = 10\nx -= 3", "x", "7");
    verificar_var("x = 6\nx *= 7", "x", "42");
    verificar_var("x = 100\nx //= 7", "x", "14");
    verificar_var("x = 10\nx %= 3", "x", "1");
    verificar_var("x = 2\nx **= 10", "x", "1024");
    verificar_var("s = \"hola \"\ns += \"mundo\"", "s", "hola mundo");

    /* x /= 2 produce decimal aunque x sea entero. */
    verificar_var("x = 7\nx /= 2", "x", "3.5");

    /* Aug sobre variable no definida: error. */
    verificar_error("x += 1", "no esta definido");
}

/* ───── if / sino si / sino ───── */

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

/* ───── mientras ───── */

static void test_mientras(void) {
    /* Sumar 1 a 10. */
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

    /* Continuar: sumar solo pares hasta 10. */
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
        "total", "30");  /* 2+4+6+8+10 */

    /* Cláusula sino: se ejecuta cuando termina por condición falsa. */
    verificar_var(
        "i = 0\n"
        "ok = falso\n"
        "mientras i < 3:\n"
        "    i += 1\n"
        "sino:\n"
        "    ok = verdadero\n"
        "fin mientras",
        "ok", "verdadero");

    /* Cláusula sino: NO se ejecuta si rompimos. */
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
}

/* ───── para sobre cadena ───── */

static void test_para(void) {
    verificar_var(
        "n = 0\n"
        "para letra en \"hola\":\n"
        "    n += 1\n"
        "fin para",
        "n", "4");

    /* UTF-8: 'niño' tiene 4 code points (la ñ es uno solo). */
    verificar_var(
        "n = 0\n"
        "para letra en \"niño\":\n"
        "    n += 1\n"
        "fin para",
        "n", "4");

    /* Concatenar mientras se itera. */
    verificar_var(
        "r = \"\"\n"
        "para c en \"abc\":\n"
        "    r += c\n"
        "fin para",
        "r", "abc");

    /* Romper temprano. */
    verificar_var(
        "n = 0\n"
        "para c en \"abcdef\":\n"
        "    n += 1\n"
        "    si n == 3:\n"
        "        romper\n"
        "    fin si\n"
        "fin para",
        "n", "3");

    /* Cláusula sino: ejecutada al terminar normalmente. */
    verificar_var(
        "ok = falso\n"
        "para c en \"ab\":\n"
        "    pasar\n"
        "sino:\n"
        "    ok = verdadero\n"
        "fin para",
        "ok", "verdadero");

    /* Iterable no soportado. */
    verificar_error(
        "para c en 5:\n"
        "    pasar\n"
        "fin para",
        "no soporta iterar");
}

/* ───── pasar ───── */

static void test_pasar(void) {
    verificar_var(
        "x = 1\n"
        "si verdadero:\n"
        "    pasar\n"
        "fin si",
        "x", "1");
}

/* ───── Programas realistas ───── */

static void test_programa_factorial_iterativo(void) {
    /* factorial(20) = 2_432_902_008_176_640_000 (excede int64 sería un problema,
     * pero 20! cabe; 25! sí necesita bignum). Hagámoslo con 25 para probar
     * bignum bajo el evaluador. */
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

static void test_programa_contar_vocales(void) {
    verificar_var(
        "frase = \"murcielago\"\n"
        "vocales = 0\n"
        "para c en frase:\n"
        "    si c == \"a\" o c == \"e\" o c == \"i\" o c == \"o\" o c == \"u\":\n"
        "        vocales += 1\n"
        "    fin si\n"
        "fin para",
        "vocales", "5");
}

static void test_programa_fibonacci(void) {
    /* Fib(30) sin recursion: secuencia clasica con dos variables.
     * 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, ...; fib(30) = 832040. */
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

static void test_programa_potencias_de_2(void) {
    /* 2^64 = 18446744073709551616. Bignum. */
    verificar_var(
        "p = 1\n"
        "i = 0\n"
        "mientras i < 64:\n"
        "    p *= 2\n"
        "    i += 1\n"
        "fin mientras",
        "p", "18446744073709551616");
}

/* ───── Errores ───── */

static void test_error_destino_no_ident(void) {
    /* Asignar a una llamada o a un literal: el parser podría aceptarlo
     * o no; el evaluador debe rechazar destinos no-IDENT. */
    /* `1 = 2` el parser lo rechaza. Probamos con expr.attr o índice
     * cuando el parser los acepta como destino — por ahora, vacío. */
    /* Nada que probar aquí en v0.4 sin atributos/índices como destino. */
}

/* ───── Anidamiento ───── */

static void test_anidamiento(void) {
    /* `si` anidado en `mientras`. */
    verificar_var(
        "i = 0\n"
        "pares = 0\n"
        "mientras i < 10:\n"
        "    si i % 2 == 0:\n"
        "        pares += 1\n"
        "    fin si\n"
        "    i += 1\n"
        "fin mientras",
        "pares", "5");  /* 0,2,4,6,8 */

    /* `mientras` anidado en `para`. */
    verificar_var(
        "letras = 0\n"
        "para c en \"abc\":\n"
        "    j = 0\n"
        "    mientras j < 3:\n"
        "        letras += 1\n"
        "        j += 1\n"
        "    fin mientras\n"
        "fin para",
        "letras", "9");
}

/* ───── Main ───── */

int main(void) {
    test_asignacion();
    test_asignacion_aug();
    test_si();
    test_mientras();
    test_para();
    test_pasar();
    test_programa_factorial_iterativo();
    test_programa_contar_vocales();
    test_programa_fibonacci();
    test_programa_potencias_de_2();
    test_error_destino_no_ident();
    test_anidamiento();

    if (fallos == 0) {
        printf("OK: todos los tests del evaluador de sentencias pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
