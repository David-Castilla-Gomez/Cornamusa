/*
 * Tests del bytecode con closures + lambdas — v0.6.2.
 *
 * Cubre:
 *   - Captura simple de local desde función anidada.
 *   - Captura con escritura (`n += 1` dentro de la closure).
 *   - Closure independiente: dos llamadas a la factoría producen
 *     contadores con estado separado.
 *   - Captura indirecta a través de scopes intermedios (upvalue de
 *     upvalue).
 *   - Lambdas con y sin captura.
 *   - Slicing (resto del paquete v0.6.2).
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

/* ───── Closures ───── */

static void test_closure_lectura(void) {
    /* Captura simple: leer una local del padre desde la función
     * anidada. */
    verificar_var(
        "funcion fuera():\n"
        "    n = 5\n"
        "    funcion dentro():\n"
        "        retornar n\n"
        "    fin funcion\n"
        "    retornar dentro()\n"
        "fin funcion\n"
        "x = fuera()",
        "x", "5");
}

static void test_closure_contador(void) {
    /* Contador clásico: la closure mantiene estado tras retornar. */
    verificar_var(
        "funcion crear_contador():\n"
        "    n = 0\n"
        "    funcion incrementar():\n"
        "        n += 1\n"
        "        retornar n\n"
        "    fin funcion\n"
        "    retornar incrementar\n"
        "fin funcion\n"
        "c = crear_contador()\n"
        "x1 = c()\n"
        "x2 = c()\n"
        "x = c()",      /* tercera llamada */
        "x", "3");
}

static void test_closure_independientes(void) {
    /* Dos llamadas a la factoría producen closures con estado
       independiente. */
    verificar_var(
        "funcion crear_contador():\n"
        "    n = 0\n"
        "    funcion inc():\n"
        "        n += 1\n"
        "        retornar n\n"
        "    fin funcion\n"
        "    retornar inc\n"
        "fin funcion\n"
        "a = crear_contador()\n"
        "b = crear_contador()\n"
        "a()\n"
        "a()\n"
        "x = a()\n"      /* a debe ir 1, 2, 3 */
        "y_test = b()",  /* b empieza desde 1 (independiente) */
        "x", "3");

    verificar_var(
        "funcion crear_contador():\n"
        "    n = 0\n"
        "    funcion inc():\n"
        "        n += 1\n"
        "        retornar n\n"
        "    fin funcion\n"
        "    retornar inc\n"
        "fin funcion\n"
        "a = crear_contador()\n"
        "a()\n"
        "a()\n"
        "b = crear_contador()\n"
        "x = b()",        /* b empieza desde 1 */
        "x", "1");
}

static void test_closure_dos_niveles(void) {
    /* Captura desde dos niveles arriba: dentro2 captura `x` de
       fuera vía dentro1 (upvalue → upvalue). */
    verificar_var(
        "funcion fuera():\n"
        "    a = 100\n"
        "    funcion dentro1():\n"
        "        funcion dentro2():\n"
        "            retornar a\n"
        "        fin funcion\n"
        "        retornar dentro2()\n"
        "    fin funcion\n"
        "    retornar dentro1()\n"
        "fin funcion\n"
        "x = fuera()",
        "x", "100");
}

/* ───── Lambdas ───── */

static void test_lambda_sin_captura(void) {
    /* Lambda invocada inmediatamente. */
    verificar_var(
        "f = lambda z: z * 2\n"
        "x = f(7)",
        "x", "14");
}

static void test_lambda_con_captura(void) {
    /* Factoría que devuelve lambda capturando un parámetro. */
    verificar_var(
        "funcion crear_mult(factor):\n"
        "    retornar lambda z: z * factor\n"
        "fin funcion\n"
        "doblar = crear_mult(2)\n"
        "triplicar = crear_mult(3)\n"
        "x = doblar(5) + triplicar(5)",  /* 10 + 15 = 25 */
        "x", "25");
}

/* ───── Slicing ───── */

static void test_slicing(void) {
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[1:4]", "z", "[2, 3, 4]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[:3]", "z", "[1, 2, 3]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[2:]", "z", "[3, 4, 5]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[::2]", "z", "[1, 3, 5]");
    verificar_var("xs = [1, 2, 3, 4, 5]\nz = xs[::-1]", "z", "[5, 4, 3, 2, 1]");
    verificar_var("xs = [1, 2, 3]\nz = xs[5:10]", "z", "[]");
}

int main(void) {
    test_closure_lectura();
    test_closure_contador();
    test_closure_independientes();
    test_closure_dos_niveles();
    test_lambda_sin_captura();
    test_lambda_con_captura();
    test_slicing();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode con closures + lambdas + slicing pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
