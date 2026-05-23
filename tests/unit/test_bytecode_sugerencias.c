/*
 * Tests de sugerencias en ErrorDeNombre (v1.35).
 *
 * Cuando un nombre global no existe, la VM busca el más cercano por
 * distancia de Levenshtein y sugiere "¿quisiste decir X?". Estos
 * tests verifican el mensaje de error capturado.
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

/* Ejecuta `fuente` y captura el mensaje de error (si lo hay) en
   `err_out`. Retorna true si hubo error de runtime. */
static bool ejecutar_capturando_error(const char *fuente, char *err_out,
                                        size_t err_cap) {
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (!prog || p.tuvo_error) {
        snprintf(err_out, err_cap, "<error de parseo>");
        arena_destruir(&a);
        return true;
    }
    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, prog, n)) {
        snprintf(err_out, err_cap, "%s", c.error.mensaje);
        chunk_destruir(&chunk); arena_destruir(&a);
        return true;
    }
    VM vm; vm_iniciar(&vm);
    Valor resultado = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &resultado);
    bool hubo_error = (rc != VM_OK);
    if (hubo_error) {
        snprintf(err_out, err_cap, "%s", vm.error.mensaje);
    } else {
        err_out[0] = '\0';
    }
    valor_destruir(&resultado);
    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
    return hubo_error;
}

/* Verifica que el error contiene la subcadena `esperada`. */
static void verificar_error_contiene(const char *desc, const char *fuente,
                                       const char *esperada) {
    char err[512];
    bool hubo = ejecutar_capturando_error(fuente, err, sizeof(err));
    if (!hubo) {
        fprintf(stderr, "FALLO [%s]: esperaba error, no hubo\n", desc);
        fallos++;
        return;
    }
    if (strstr(err, esperada) == NULL) {
        fprintf(stderr, "FALLO [%s]: error '%s' no contiene '%s'\n",
                desc, err, esperada);
        fallos++;
    }
}

/* Verifica que el error NO contiene la subcadena. */
static void verificar_error_no_contiene(const char *desc, const char *fuente,
                                          const char *prohibida) {
    char err[512];
    bool hubo = ejecutar_capturando_error(fuente, err, sizeof(err));
    if (!hubo) {
        fprintf(stderr, "FALLO [%s]: esperaba error, no hubo\n", desc);
        fallos++;
        return;
    }
    if (strstr(err, prohibida) != NULL) {
        fprintf(stderr, "FALLO [%s]: error '%s' contiene '%s' (no debería)\n",
                desc, err, prohibida);
        fallos++;
    }
}

/* ───── Sugerencias sobre built-ins ───── */

static void test_sugiere_builtin(void) {
    /* `longutud` → `longitud` (1 transposición). */
    verificar_error_contiene("typo en built-in longitud",
        "imprimir(longutud([1, 2, 3]))",
        "quisiste decir 'longitud'");
}

static void test_sugiere_builtin_imprimir(void) {
    /* `imprmir` → `imprimir`. */
    verificar_error_contiene("typo en imprimir",
        "x = imprmir",
        "quisiste decir 'imprimir'");
}

/* ───── Sugerencias sobre variables del usuario ───── */

static void test_sugiere_variable(void) {
    verificar_error_contiene("typo en variable de usuario",
        "mi_contador = 10\n"
        "imprimir(mi_contadr)",
        "quisiste decir 'mi_contador'");
}

static void test_sugiere_funcion(void) {
    verificar_error_contiene("typo en nombre de función",
        "funcion calcular_total():\n"
        "  retornar 42\n"
        "fin funcion\n"
        "imprimir(calcular_totl())",
        "quisiste decir 'calcular_total'");
}

/* ───── Sin sugerencia cuando no hay nada cercano ───── */

static void test_sin_sugerencia_lejano(void) {
    /* `xyzzy` no se parece a ningún global → sin sugerencia. */
    verificar_error_no_contiene("nombre sin candidato cercano",
        "imprimir(xyzzy)",
        "quisiste decir");
}

static void test_error_sigue_presente(void) {
    /* Aunque no haya sugerencia, el ErrorDeNombre base sigue. */
    verificar_error_contiene("error base presente sin sugerencia",
        "imprimir(qqqqqqqq)",
        "no esta definido");
}

/* ───── No sugiere nombres internos ($...) ───── */

static void test_no_sugiere_internos(void) {
    /* Un nombre como `iter` no debe sugerir `$iter` (interno). */
    verificar_error_no_contiene("no sugiere internos con $",
        "imprimir(iter)",
        "$");
}

/* ───── v1.36: sugerencias en atributos ───── */

static void test_sugiere_atributo_instancia(void) {
    verificar_error_contiene("typo en atributo de instancia",
        "clase Punto:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.coord_x = 1\n"
        "    yo.coord_y = 2\n"
        "  fin funcion\n"
        "fin clase\n"
        "p = Punto()\n"
        "imprimir(p.coord_z)",
        "quisiste decir 'coord_");
}

static void test_sugiere_metodo(void) {
    verificar_error_contiene("typo en nombre de método",
        "clase Caja:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "  funcion abrir(yo):\n"
        "    retornar yo.x\n"
        "  fin funcion\n"
        "fin clase\n"
        "c = Caja()\n"
        "imprimir(c.abrr())",
        "quisiste decir 'abrir'");
}

/* NOTA: la sugerencia sobre símbolos de módulo (`mod.simbol`) se
   verifica vía el ejemplo `examples/`, no aquí — los unit tests no
   tienen `stdlib/` en su working directory. */

static void test_atributo_sin_sugerencia(void) {
    verificar_error_no_contiene("atributo sin candidato cercano",
        "clase X:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.algo = 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "x = X()\n"
        "imprimir(x.zzzzzzz)",
        "quisiste decir");
}

/* ───── v1.107: case-insensitive ASCII ───── */

static void test_case_insensitive_builtin_mayusculas(void) {
    /* IMPRIMIR (todo mayusculas) debe sugerir imprimir. Sin el fix,
     * Levenshtein normal da distancia 8 y no se sugiere. */
    verificar_error_contiene("case-insensitive IMPRIMIR",
        "IMPRIMIR(\"x\")",
        "quisiste decir 'imprimir'");
}

static void test_case_insensitive_longitud(void) {
    verificar_error_contiene("case-insensitive LONGITUD",
        "x = LONGITUD([])",
        "quisiste decir 'longitud'");
}

static void test_case_mixto(void) {
    /* Capitalize estilo Pascal: aunque Levenshtein con 1 char ya lo
     * cogeria por umbral, confirmamos que tambien funciona con la
     * ruta case-insensitive. */
    verificar_error_contiene("case mixto Longitud",
        "x = Longitud([])",
        "quisiste decir 'longitud'");
}

static void test_case_variable_usuario(void) {
    verificar_error_contiene("case insensitive en variable usuario",
        "mi_contador = 10\n"
        "imprimir(MI_CONTADOR)",
        "quisiste decir 'mi_contador'");
}

/* ───── v1.107: no sugerir identico ───── */

static void test_no_sugiere_identico(void) {
    /* Cuando ya tenemos otra variable cercana, no debe sugerirse a
     * si mismo. Caso indirecto: si hay dos variables muy similares,
     * la sugerencia debe ser la otra, no la misma. */
    char err[512];
    bool hubo = ejecutar_capturando_error(
        "foobar2026 = 42\n"
        "imprimir(foobar2027)\n", err, sizeof(err));
    if (!hubo) {
        fprintf(stderr, "FALLO [no_sugiere_identico]: no hubo error\n");
        fallos++;
        return;
    }
    /* foobar2027 no existe; el unico candidato cercano es foobar2026.
     * NO debe sugerir foobar2027 a si mismo. */
    if (strstr(err, "quisiste decir 'foobar2027'") != NULL) {
        fprintf(stderr, "FALLO [no_sugiere_identico]: se sugirio a si mismo: %s\n", err);
        fallos++;
    }
    if (strstr(err, "quisiste decir 'foobar2026'") == NULL) {
        fprintf(stderr, "FALLO [no_sugiere_identico]: no sugirio foobar2026: %s\n", err);
        fallos++;
    }
}

int main(void) {
    test_sugiere_builtin();
    test_sugiere_builtin_imprimir();
    test_sugiere_variable();
    test_sugiere_funcion();
    test_sin_sugerencia_lejano();
    test_error_sigue_presente();
    test_no_sugiere_internos();
    test_sugiere_atributo_instancia();
    test_sugiere_metodo();
    test_atributo_sin_sugerencia();
    /* v1.107 */
    test_case_insensitive_builtin_mayusculas();
    test_case_insensitive_longitud();
    test_case_mixto();
    test_case_variable_usuario();
    test_no_sugiere_identico();

    if (fallos == 0) {
        printf("sugerencias: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "sugerencias: %d fallo(s)\n", fallos);
    return 1;
}
