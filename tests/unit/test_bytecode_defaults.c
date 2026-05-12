/*
 * Tests de argumentos por defecto en bytecode (v1.17).
 *
 * Cubre:
 *   - Default evaluado al crear la función (Python-like).
 *   - Múltiples defaults consecutivos.
 *   - Mezcla de args con y sin default.
 *   - Lambdas con defaults.
 *   - Errores: parámetro sin default tras uno con default; aridad mal;
 *     errores de aridad atrapables.
 *   - Defaults complejos (referencia a variables del scope de def).
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

/* ───── Basic ───── */

static void test_default_simple(void) {
    verificar_var(
        "funcion saludar(n, s=\"Hola\"):\n"
        "  retornar s + \", \" + n\n"
        "fin funcion\n"
        "x = saludar(\"Ana\")",
        "x", "Hola, Ana");
}

static void test_default_explicito(void) {
    verificar_var(
        "funcion saludar(n, s=\"Hola\"):\n"
        "  retornar s + \", \" + n\n"
        "fin funcion\n"
        "x = saludar(\"Ana\", \"Buenas\")",
        "x", "Buenas, Ana");
}

static void test_multiples_defaults(void) {
    verificar_var(
        "funcion f(a, b=10, c=100, d=1000):\n"
        "  retornar a + b + c + d\n"
        "fin funcion\n"
        "x = [f(1), f(1, 2), f(1, 2, 3), f(1, 2, 3, 4)]",
        "x", "[1111, 1103, 1006, 10]");
}

/* ───── Captura de scope al def ───── */

static void test_default_captura_global(void) {
    /* El default debe evaluarse al CREAR la función, no al llamarla. */
    verificar_var(
        "N = 10\n"
        "funcion f(z, base=N):\n"
        "  retornar z + base\n"
        "fin funcion\n"
        "N = 999\n"
        "x = f(5)",
        "x", "15");
}

static void test_default_expresion_compleja(void) {
    /* Default puede ser cualquier expresión. */
    verificar_var(
        "funcion f(a, b=2*3+4):\n"
        "  retornar a + b\n"
        "fin funcion\n"
        "x = f(1)",
        "x", "11");
}

static void test_default_lista_compartida(void) {
    /* CUIDADO: defaults mutables son compartidos entre llamadas.
       Mismo gotcha que Python. Documentamos comportamiento. */
    verificar_var(
        "funcion f(item, acc=[]):\n"
        "  agregar(acc, item)\n"
        "  retornar acc\n"
        "fin funcion\n"
        "a = f(1)\n"
        "b = f(2)\n"
        "x = [a, b]",
        "x", "[[1, 2], [1, 2]]");
}

/* ───── Lambda ───── */

static void test_lambda_default(void) {
    verificar_var(
        "f = lambda a, b=100: a + b\n"
        "x = [f(5), f(5, 10)]",
        "x", "[105, 15]");
}

/* ───── Errores ───── */

static void test_default_no_default_despues(void) {
    /* Error de compilación: param sin default tras uno con default. */
    const char *err = NULL;
    const char *res = ejecutar(
        "funcion f(a=1, b):\n"
        "  retornar a + b\n"
        "fin funcion\n", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO: param sin default tras default no detectado\n");
        fallos++;
    }
}

static void test_aridad_insuficiente_atrapable(void) {
    /* v1.17: error de aridad ahora atrapable. */
    verificar_var(
        "funcion f(a, b):\n"
        "  retornar a + b\n"
        "fin funcion\n"
        "_msg = \"\"\n"
        "intentar:\n"
        "  f(1)\n"
        "atrapar ErrorDeTipo:\n"
        "  _msg = \"atrapado\"\n"
        "fin intentar\n"
        "x = _msg",
        "x", "atrapado");
}

static void test_aridad_excesiva_atrapable(void) {
    verificar_var(
        "funcion f(a, b=2):\n"
        "  retornar a + b\n"
        "fin funcion\n"
        "_msg = \"\"\n"
        "intentar:\n"
        "  f(1, 2, 3)\n"
        "atrapar ErrorDeTipo:\n"
        "  _msg = \"atrapado\"\n"
        "fin intentar\n"
        "x = _msg",
        "x", "atrapado");
}

/* ───── Métodos con default ───── */

static void test_metodo_con_default(void) {
    verificar_var(
        "clase Saludador:\n"
        "  funcion __iniciar__(yo, base=\"Hola\"):\n"
        "    yo.base = base\n"
        "  fin funcion\n"
        "  funcion saludar(yo, nombre=\"mundo\"):\n"
        "    retornar yo.base + \", \" + nombre\n"
        "  fin funcion\n"
        "fin clase\n"
        "s1 = Saludador()\n"
        "s2 = Saludador(\"Buenas\")\n"
        "x = [s1.saludar(), s1.saludar(\"Ana\"), s2.saludar(\"Bob\")]",
        "x", "[\"Hola, mundo\", \"Hola, Ana\", \"Buenas, Bob\"]");
}

/* ───── Sin regresión: funciones sin default siguen igual ───── */

static void test_sin_defaults_sigue_funcionando(void) {
    verificar_var(
        "funcion f(a, b, c):\n"
        "  retornar a + b + c\n"
        "fin funcion\n"
        "x = f(1, 2, 3)",
        "x", "6");
}

int main(void) {
    test_default_simple();
    test_default_explicito();
    test_multiples_defaults();
    test_default_captura_global();
    test_default_expresion_compleja();
    test_default_lista_compartida();
    test_lambda_default();
    test_default_no_default_despues();
    test_aridad_insuficiente_atrapable();
    test_aridad_excesiva_atrapable();
    test_metodo_con_default();
    test_sin_defaults_sigue_funcionando();

    if (fallos == 0) {
        printf("defaults: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "defaults: %d fallo(s)\n", fallos);
    return 1;
}
