/*
 * Tests del bytecode con iteración (`para`) — v0.6.1.
 *
 * Cubre:
 *   - `para X en cadena|lista|tupla|rango|conjunto|diccionario`.
 *   - Iteración en top-level y dentro de función.
 *   - `romper` y `continuar` dentro de `para`.
 *   - Cláusula `sino` ejecutada al agotar el iterador.
 *   - Asignación aumentada con destino índice (`dicc[k] += 1`).
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

/* ───── para sobre cadena ───── */

static void test_para_cadena(void) {
    /* "abc" → 3 iteraciones. */
    verificar_var(
        "n = 0\n"
        "para letra en \"abc\":\n"
        "    n += 1\n"
        "fin para",
        "n", "3");

    /* UTF-8: niño → 4 code points. */
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
}

/* ───── para sobre rango ───── */

static void test_para_rango(void) {
    /* Suma 1..10. */
    verificar_var(
        "total = 0\n"
        "para i en rango(1, 11):\n"
        "    total += i\n"
        "fin para",
        "total", "55");

    /* Paso negativo. */
    verificar_var(
        "ultimo = -1\n"
        "para i en rango(10, 0, -1):\n"
        "    ultimo = i\n"
        "fin para",
        "ultimo", "1");

    /* rango(n) → 0..n-1. */
    verificar_var(
        "total = 0\n"
        "para i en rango(5):\n"
        "    total += i\n"
        "fin para",
        "total", "10");
}

/* ───── para sobre lista/tupla ───── */

static void test_para_lista_tupla(void) {
    verificar_var(
        "total = 0\n"
        "para x en [1, 2, 3, 4, 5]:\n"
        "    total += x\n"
        "fin para",
        "total", "15");

    verificar_var(
        "total = 0\n"
        "para x en (10, 20, 30):\n"
        "    total += x\n"
        "fin para",
        "total", "60");
}

/* ───── para sobre dicc/conjunto ───── */

static void test_para_dicc_conjunto(void) {
    /* Iteración sobre claves del dicc. */
    verificar_var(
        "d = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
        "total = 0\n"
        "para k en d:\n"
        "    total += d[k]\n"
        "fin para",
        "total", "6");

    /* Iteración sobre conjunto: orden indeterminado, sumar es seguro. */
    verificar_var(
        "s = {1, 2, 3, 4, 5}\n"
        "total = 0\n"
        "para x en s:\n"
        "    total += x\n"
        "fin para",
        "total", "15");
}

/* ───── romper / continuar / sino ───── */

static void test_para_romper(void) {
    verificar_var(
        "encontrado = -1\n"
        "para x en [3, 1, 4, 1, 5, 9, 2, 6]:\n"
        "    si x == 4:\n"
        "        encontrado = x\n"
        "        romper\n"
        "    fin si\n"
        "fin para",
        "encontrado", "4");
}

static void test_para_continuar(void) {
    /* Sumar solo pares de 1..10. */
    verificar_var(
        "total = 0\n"
        "para i en rango(1, 11):\n"
        "    si i % 2 == 1:\n"
        "        continuar\n"
        "    fin si\n"
        "    total += i\n"
        "fin para",
        "total", "30");
}

static void test_para_sino(void) {
    /* Sino se ejecuta al agotar. */
    verificar_var(
        "ok = falso\n"
        "para x en [1, 2, 3]:\n"
        "    pasar\n"
        "sino:\n"
        "    ok = verdadero\n"
        "fin para",
        "ok", "verdadero");

    /* Sino NO se ejecuta tras `romper`. */
    verificar_var(
        "ok = falso\n"
        "para x en [1, 2, 3]:\n"
        "    romper\n"
        "sino:\n"
        "    ok = verdadero\n"
        "fin para",
        "ok", "falso");
}

/* ───── para dentro de función ───── */

static void test_para_dentro_de_funcion(void) {
    verificar_var(
        "funcion suma_pares(limite):\n"
        "    total = 0\n"
        "    para i en rango(1, limite + 1):\n"
        "        si i % 2 == 0:\n"
        "            total += i\n"
        "        fin si\n"
        "    fin para\n"
        "    retornar total\n"
        "fin funcion\n"
        "x = suma_pares(10)",
        "x", "30");

    /* Conteo de vocales (mismo programa que examples/14). */
    verificar_var(
        "funcion contar_vocales(texto):\n"
        "    total = 0\n"
        "    para letra en texto:\n"
        "        si letra == \"a\" o letra == \"e\" o letra == \"i\" o letra == \"o\" o letra == \"u\":\n"
        "            total += 1\n"
        "        fin si\n"
        "    fin para\n"
        "    retornar total\n"
        "fin funcion\n"
        "x = contar_vocales(\"murcielago\")",
        "x", "5");
}

/* ───── Asignación aumentada con destino índice (OP_DUP_2) ───── */

static void test_asignar_aug_indice(void) {
    verificar_var(
        "d = {\"a\": 1}\n"
        "d[\"a\"] += 10\n"
        "x = d[\"a\"]",
        "x", "11");

    /* Frecuencia de letras en "abracadabra". */
    verificar_var(
        "conteo = {}\n"
        "para c en \"abracadabra\":\n"
        "    si c en conteo:\n"
        "        conteo[c] += 1\n"
        "    sino:\n"
        "        conteo[c] = 1\n"
        "    fin si\n"
        "fin para\n"
        "x = conteo[\"a\"]",
        "x", "5");

    /* Lista mutación con +=. */
    verificar_var(
        "xs = [10, 20, 30]\n"
        "xs[1] += 5\n"
        "x = xs[1]",
        "x", "25");
}

/* ───── Programa realista: factorial iterativo ───── */

static void test_factorial_iterativo(void) {
    verificar_var(
        "n = 25\n"
        "resultado = 1\n"
        "para i en rango(1, n + 1):\n"
        "    resultado *= i\n"
        "fin para",
        "resultado", "15511210043330985984000000");
}

int main(void) {
    test_para_cadena();
    test_para_rango();
    test_para_lista_tupla();
    test_para_dicc_conjunto();
    test_para_romper();
    test_para_continuar();
    test_para_sino();
    test_para_dentro_de_funcion();
    test_asignar_aug_indice();
    test_factorial_iterativo();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode con iteracion pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
