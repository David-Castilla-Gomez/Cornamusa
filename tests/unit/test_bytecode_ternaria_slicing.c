/*
 * Tests de v1.44: expresión ternaria `A si C sino B` y slicing
 * assignment `xs[i:j:k] = nuevo`.
 *
 * Ternaria: precedencia más baja que `o`; asociativa derecha; vive
 * en una sola línea (un `si` que abre línea se interpreta como
 * statement, no como ternaria).
 *
 * Slicing assignment: solo listas. Con `paso == 1` la lista crece o
 * encoge; con `paso != 1` los tamaños deben coincidir exactamente.
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
    if (substring && (!err || !strstr(err, substring))) {
        fprintf(stderr, "FALLO: error %s no contiene '%s'\n",
                err ? err : "<vacio>", substring);
        fallos++;
    }
}

/* ─── Ternaria ─── */

static void test_ternaria_basica(void) {
    verificar_var("r = \"si\" si verdadero sino \"no\"\n", "r", "si");
    verificar_var("r = \"si\" si falso sino \"no\"\n", "r", "no");
}

static void test_ternaria_con_expresion_compleja(void) {
    verificar_var("x = 10\n"
                  "r = \"positivo\" si x > 0 sino \"no positivo\"\n",
                  "r", "positivo");
}

static void test_ternaria_anidada_asociativa_derecha(void) {
    /* `a si c1 sino b si c2 sino d` = `a si c1 sino (b si c2 sino d)` */
    verificar_var("x = 0\n"
                  "r = \"cero\" si x == 0 sino \"pos\" si x > 0 sino \"neg\"\n",
                  "r", "cero");
    verificar_var("x = 5\n"
                  "r = \"cero\" si x == 0 sino \"pos\" si x > 0 sino \"neg\"\n",
                  "r", "pos");
    verificar_var("x = -5\n"
                  "r = \"cero\" si x == 0 sino \"pos\" si x > 0 sino \"neg\"\n",
                  "r", "neg");
}

static void test_ternaria_dentro_de_comprehension(void) {
    /* La ternaria aparece en la expr ELEMENTO de la comprehension. */
    verificar_var("xs = [n si n > 0 sino 0 para n en [-2, -1, 0, 1, 2]]\n"
                  "r = cadena(xs)\n",
                  "r", "[0, 0, 0, 1, 2]");
}

static void test_si_de_linea_nueva_es_statement(void) {
    /* El `si` que abre línea NO es ternaria — es sentencia `si`. */
    verificar_var("x = 10\n"
                  "si x > 0:\n"
                  "  r = \"pos\"\n"
                  "fin si\n",
                  "r", "pos");
}

static void test_ternaria_en_args_de_llamada(void) {
    verificar_var("funcion mas(a, b):\n"
                  "  retornar a + b\n"
                  "fin funcion\n"
                  "r = mas(1 si verdadero sino 99, 2)\n",
                  "r", "3");
}

/* ─── Slicing assignment ─── */

static void test_slicing_reemplazo_mismo_tamano(void) {
    verificar_var("xs = [1, 2, 3, 4, 5]\n"
                  "xs[1:4] = [99, 99, 99]\n"
                  "r = cadena(xs)\n",
                  "r", "[1, 99, 99, 99, 5]");
}

static void test_slicing_crecer(void) {
    verificar_var("xs = [1, 2, 3]\n"
                  "xs[1:2] = [10, 20, 30, 40]\n"
                  "r = cadena(xs)\n",
                  "r", "[1, 10, 20, 30, 40, 3]");
}

static void test_slicing_encoger(void) {
    verificar_var("xs = [1, 2, 3, 4, 5]\n"
                  "xs[1:4] = [99]\n"
                  "r = cadena(xs)\n",
                  "r", "[1, 99, 5]");
}

static void test_slicing_borrar_con_lista_vacia(void) {
    verificar_var("xs = [1, 2, 3, 4, 5]\n"
                  "xs[1:4] = []\n"
                  "r = cadena(xs)\n",
                  "r", "[1, 5]");
}

static void test_slicing_inicio_omitido(void) {
    verificar_var("xs = [1, 2, 3, 4, 5]\n"
                  "xs[:2] = [100]\n"
                  "r = cadena(xs)\n",
                  "r", "[100, 3, 4, 5]");
}

static void test_slicing_fin_omitido(void) {
    verificar_var("xs = [1, 2, 3, 4, 5]\n"
                  "xs[3:] = [100, 200, 300]\n"
                  "r = cadena(xs)\n",
                  "r", "[1, 2, 3, 100, 200, 300]");
}

static void test_slicing_paso_no_uno(void) {
    verificar_var("xs = [1, 2, 3, 4, 5, 6]\n"
                  "xs[::2] = [10, 30, 50]\n"
                  "r = cadena(xs)\n",
                  "r", "[10, 2, 30, 4, 50, 6]");
}

static void test_slicing_paso_no_uno_tamano_distinto_falla(void) {
    verificar_error("xs = [1, 2, 3, 4]\n"
                    "xs[::2] = [99, 99, 99]\n"
                    "_n = 1\n",
                    "rebanada con paso");
}

static void test_slicing_desde_tupla(void) {
    /* El valor de asignación puede ser tupla, no solo lista. */
    verificar_var("xs = [0, 0, 0]\n"
                  "xs[1:3] = (10, 20)\n"
                  "r = cadena(xs)\n",
                  "r", "[0, 10, 20]");
}

static void test_slicing_solo_listas(void) {
    /* Las cadenas son inmutables — slicing assignment falla. */
    verificar_error("s = \"hola\"\n"
                    "s[1:3] = \"x\"\n"
                    "_n = 1\n",
                    "asignacion por rebanada solo en listas");
}

int main(void) {
    test_ternaria_basica();
    test_ternaria_con_expresion_compleja();
    test_ternaria_anidada_asociativa_derecha();
    test_ternaria_dentro_de_comprehension();
    test_si_de_linea_nueva_es_statement();
    test_ternaria_en_args_de_llamada();

    test_slicing_reemplazo_mismo_tamano();
    test_slicing_crecer();
    test_slicing_encoger();
    test_slicing_borrar_con_lista_vacia();
    test_slicing_inicio_omitido();
    test_slicing_fin_omitido();
    test_slicing_paso_no_uno();
    test_slicing_paso_no_uno_tamano_distinto_falla();
    test_slicing_desde_tupla();
    test_slicing_solo_listas();

    if (fallos == 0) {
        printf("ternaria_slicing: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "ternaria_slicing: %d fallo(s)\n", fallos);
    return 1;
}
