/*
 * Tests del compilador + VM para f-cadenas con interpolación real (v1.1).
 *
 * Cubre:
 *   - Literal puro (sin interpolación) → cadena exacta.
 *   - Interpolación simple (un identificador).
 *   - Interpolación con aritmética y llamadas.
 *   - Mezclas de literales y expresiones.
 *   - Llaves dobles `{{` `}}` como literal.
 *   - F-cadena vacía.
 *   - Anidación (`f"...{f'...{x}'}..."`).
 *   - Tipos coercionados a cadena: entero (small + bignum), decimal,
 *     booleano, nulo, lista.
 *   - Errores de parser: `{` sin cerrar, `}` huérfano, expresión vacía,
 *     tokens sobrantes.
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
        fprintf(stderr, "FALLO: %s -> error: %s\n", fuente,
                err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO: %s -> %s=%s (esperaba %s)\n",
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

/* ───── Literales sin interpolación ───── */

static void test_literal_puro(void) {
    verificar_var("x = f\"hola\"",       "x", "hola");
    verificar_var("x = f''",             "x", "");
    verificar_var("x = f\"\"",           "x", "");
    verificar_var("x = f'una sola comilla'", "x", "una sola comilla");
}

static void test_escapes_literales(void) {
    verificar_var("x = f\"a\\nb\"",      "x", "a\nb");
    verificar_var("x = f\"\\t\"",        "x", "\t");
    verificar_var("x = f\"\\\\\"",       "x", "\\");
}

static void test_llaves_dobles(void) {
    verificar_var("x = f\"{{literal}}\"",   "x", "{literal}");
    verificar_var("x = f\"{{a}}\"",         "x", "{a}");
    verificar_var("a = 1\nx = f\"{{{a}}}\"", "x", "{1}");
}

/* ───── Interpolación simple ───── */

static void test_interp_ident(void) {
    verificar_var("a = 7\nx = f\"{a}\"",          "x", "7");
    verificar_var("n = \"Ana\"\nx = f\"hola {n}\"", "x", "hola Ana");
}

static void test_interp_aritmetica(void) {
    verificar_var("x = f\"{1 + 2}\"",        "x", "3");
    verificar_var("x = f\"{2 ** 10}\"",      "x", "1024");
    verificar_var("a = 5\nx = f\"{a * a}\"", "x", "25");
}

static void test_interp_llamada(void) {
    verificar_var("funcion dob(n):\n  retornar n * 2\nfin funcion\nx = f\"{dob(7)}\"",
                  "x", "14");
    verificar_var("x = f\"{longitud([1,2,3])}\"", "x", "3");
}

/* ───── Mezclas ───── */

static void test_mezcla_literal_expr(void) {
    verificar_var("a=1\nb=2\nx = f\"a={a} b={b}\"", "x", "a=1 b=2");
    verificar_var("a=1\nx = f\"x{a}y{a}z\"",        "x", "x1y1z");
    verificar_var("a=1\nx = f\"{a}-medio-{a}\"",    "x", "1-medio-1");
}

/* ───── Tipos coercionados ───── */

static void test_coercion_tipos(void) {
    verificar_var("x = f\"{2 ** 100}\"", "x",
                  "1267650600228229401496703205376");
    verificar_var("x = f\"{3.14}\"",    "x", "3.14");
    verificar_var("x = f\"{verdadero}\"", "x", "verdadero");
    verificar_var("x = f\"{falso}\"",   "x", "falso");
    verificar_var("x = f\"{nulo}\"",    "x", "nulo");
    verificar_var("x = f\"{[1, 2, 3]}\"", "x", "[1, 2, 3]");
}

/* ───── Anidación ───── */

static void test_anidacion(void) {
    verificar_var("a = 5\nx = f\"out-{f'in-{a}'}\"", "x", "out-in-5");
    verificar_var("a = \"x\"\nx = f\"<{f'[{a}]'}>\"", "x", "<[x]>");
}

/* ───── Errores ─────
 *
 * El helper `verificar_error` solo captura errores de runtime y
 * compilación; los errores del parser van a stderr. Aquí verificamos
 * solo que el caso falla (no se ejecuta) — el mensaje exacto se
 * verifica manualmente en el example de la sesión. */

static void test_errores_parser(void) {
    verificar_error("x = f\"{sin_cerrar\"",     "<error de parseo>");
    verificar_error("x = f\"sin abrir}\"",      "<error de parseo>");
    verificar_error("x = f\"{}\"",              "<error de parseo>");
    verificar_error("x = f\"{a + }\"",          "<error de parseo>");
    verificar_error("x = f\"{1 2}\"",           "<error de parseo>");
}

int main(void) {
    test_literal_puro();
    test_escapes_literales();
    test_llaves_dobles();
    test_interp_ident();
    test_interp_aritmetica();
    test_interp_llamada();
    test_mezcla_literal_expr();
    test_coercion_tipos();
    test_anidacion();
    test_errores_parser();

    if (fallos == 0) {
        printf("fstrings: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "fstrings: %d fallo(s)\n", fallos);
    return 1;
}
