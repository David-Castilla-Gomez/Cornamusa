/*
 * Tests de f-string format specifiers (v1.45).
 *
 * Sintaxis Python-paritaria: `{expr:[fill][align][width][.precision][type]}`.
 * Tipos: d (entero), f (decimal), e (científica), x/X (hex), b (binario),
 * s (cadena explícita). Defaults: align '>' para numéricos, '<' para
 * cadenas. Prefijo '0' implica zero-padding y `>` alineado.
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

/* ─── Ancho y alineación ─── */

static void test_ancho_default_numerico_es_derecha(void) {
    verificar_var("r = f\"{42:5}\"\n", "r", "   42");
}
static void test_ancho_default_cadena_es_izquierda(void) {
    verificar_var("r = f\"{'hi':5}\"\n", "r", "hi   ");
}
static void test_alineacion_explicita(void) {
    verificar_var("r = f\"{42:<5}\"\n", "r", "42   ");
    verificar_var("r = f\"{42:>5}\"\n", "r", "   42");
    verificar_var("r = f\"{42:^7}\"\n", "r", "  42   ");
}
static void test_relleno_custom(void) {
    verificar_var("r = f\"{'hi':-^8}\"\n", "r", "---hi---");
    verificar_var("r = f\"{42:*>5}\"\n", "r", "***42");
}
static void test_zero_padding(void) {
    verificar_var("r = f\"{42:05}\"\n", "r", "00042");
}

/* ─── Tipos numéricos ─── */

static void test_tipo_d(void) {
    verificar_var("r = f\"{42:d}\"\n", "r", "42");
    verificar_var("r = f\"{-7:d}\"\n", "r", "-7");
    /* bignum funciona */
    verificar_var("r = f\"{2 ** 100:d}\"\n",
                  "r", "1267650600228229401496703205376");
}

static void test_tipo_f_con_precision(void) {
    verificar_var("r = f\"{3.14159:.2f}\"\n", "r", "3.14");
    verificar_var("r = f\"{3.14159:.0f}\"\n", "r", "3");
    verificar_var("r = f\"{3.14159:8.2f}\"\n", "r", "    3.14");
    /* precision default = 6 */
    verificar_var("r = f\"{0.5:f}\"\n", "r", "0.500000");
}

static void test_tipo_e_cientifica(void) {
    verificar_var("r = f\"{1234.5:.2e}\"\n", "r", "1.23e+03");
}

static void test_tipo_x_hex(void) {
    verificar_var("r = f\"{255:x}\"\n", "r", "ff");
    verificar_var("r = f\"{255:X}\"\n", "r", "FF");
    verificar_var("r = f\"{255:04x}\"\n", "r", "00ff");
}

static void test_tipo_b_binario(void) {
    verificar_var("r = f\"{5:b}\"\n", "r", "101");
    verificar_var("r = f\"{5:08b}\"\n", "r", "00000101");
}

/* ─── Tipo s con precisión (truncar) ─── */

static void test_tipo_s_truncar(void) {
    verificar_var("r = f\"{'hola':.2}\"\n", "r", "ho");
    verificar_var("r = f\"{'hola':.10}\"\n", "r", "hola");
}

/* ─── Spec vacío equivale a sin spec ─── */

static void test_spec_vacio(void) {
    verificar_var("r = f\"{42:}\"\n", "r", "42");
}

/* ─── Coexistencia con `:` de slicing/dict ─── */

static void test_slicing_no_se_confunde_con_spec(void) {
    /* `xs[1:3]` dentro de f-cadena — el `:` está dentro de `[]`,
       NO se interpreta como inicio de format spec. */
    verificar_var("xs = [10, 20, 30, 40]\n"
                  "r = f\"{xs[1:3]}\"\n",
                  "r", "[20, 30]");
}

static void test_dict_no_se_confunde_con_spec(void) {
    /* `{ {1: 2} }` — el `:` está en una llave anidada. */
    verificar_var("r = f\"{ {1: 2} }\"\n",
                  "r", "{1: 2}");
}

/* ─── Errores ─── */

static void test_tipo_d_requiere_entero(void) {
    verificar_error("_n = f\"{3.14:d}\"\n", "formato 'd' requiere entero");
}

static void test_spec_invalido(void) {
    verificar_error("_n = f\"{42:z}\"\n", "tipo de formato 'z' no soportado");
}

int main(void) {
    test_ancho_default_numerico_es_derecha();
    test_ancho_default_cadena_es_izquierda();
    test_alineacion_explicita();
    test_relleno_custom();
    test_zero_padding();

    test_tipo_d();
    test_tipo_f_con_precision();
    test_tipo_e_cientifica();
    test_tipo_x_hex();
    test_tipo_b_binario();

    test_tipo_s_truncar();
    test_spec_vacio();

    test_slicing_no_se_confunde_con_spec();
    test_dict_no_se_confunde_con_spec();

    test_tipo_d_requiere_entero();
    test_spec_invalido();

    if (fallos == 0) {
        printf("fstring_spec: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "fstring_spec: %d fallo(s)\n", fallos);
    return 1;
}
