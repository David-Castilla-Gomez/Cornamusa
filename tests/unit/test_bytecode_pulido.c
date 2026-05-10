/*
 * Tests del pulido v1.14:
 *
 *   - Re-raise automático (`lanzar` sin valor en `atrapar`).
 *   - Slicing de cadenas (`s[i:j]`, `s[i:j:k]`, UTF-8).
 *   - F-cadenas triples (`f"""..."""`, `f'''...'''`).
 *   - Cadenas triples literales (`"""..."""`, `'''...'''`).
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

/* ───── Re-raise ───── */

static void test_reraise_con_alias(void) {
    /* `lanzar` sin valor dentro de `atrapar X como e:` propaga la
       excepción atrapada al handler exterior. */
    verificar_var(
        "_outer = nulo\n"
        "intentar:\n"
        "  intentar:\n"
        "    lanzar ErrorDeValor(\"orig\")\n"
        "  atrapar ErrorDeValor como e:\n"
        "    lanzar\n"
        "  fin intentar\n"
        "atrapar ErrorDeValor como e2:\n"
        "  _outer = e2\n"
        "fin intentar\n"
        "x = _outer",
        "x", "ErrorDeValor: orig");
}

static void test_reraise_sin_alias(void) {
    /* v1.14: `lanzar` sin valor también funciona en `atrapar` sin alias. */
    verificar_var(
        "_outer = nulo\n"
        "intentar:\n"
        "  intentar:\n"
        "    lanzar ErrorDeTipo(\"bork\")\n"
        "  atrapar ErrorDeTipo:\n"
        "    lanzar\n"
        "  fin intentar\n"
        "atrapar Excepcion como e:\n"
        "  _outer = e\n"
        "fin intentar\n"
        "x = _outer",
        "x", "ErrorDeTipo: bork");
}

static void test_reraise_fuera_de_atrapar(void) {
    /* Error de compilación si `lanzar` sin valor está fuera de un atrapar. */
    const char *err = NULL;
    const char *res = ejecutar("lanzar\n", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO: lanzar sin valor fuera de atrapar deberia fallar\n");
        fallos++;
        return;
    }
    if (!err || !strstr(err, "lanzar")) {
        fprintf(stderr, "FALLO: error '%s' no menciona 'lanzar'\n",
                err ? err : "<null>");
        fallos++;
    }
}

/* ───── Slicing de cadenas ───── */

static void test_slice_cadena_basico(void) {
    verificar_var("x = \"Hola mundo\"[0:4]", "x", "Hola");
    verificar_var("x = \"Hola mundo\"[5:]", "x", "mundo");
    verificar_var("x = \"Hola mundo\"[:4]", "x", "Hola");
    verificar_var("x = \"Hola mundo\"[:]", "x", "Hola mundo");
}

static void test_slice_cadena_negativo(void) {
    verificar_var("x = \"abcdef\"[-3:]", "x", "def");
    verificar_var("x = \"abcdef\"[:-2]", "x", "abcd");
    verificar_var("x = \"abcdef\"[::-1]", "x", "fedcba");
}

static void test_slice_cadena_paso(void) {
    verificar_var("x = \"abcdef\"[::2]", "x", "ace");
    verificar_var("x = \"abcdef\"[1::2]", "x", "bdf");
    verificar_var("x = \"abcdef\"[::-2]", "x", "fdb");
}

static void test_slice_cadena_utf8(void) {
    /* Con caracteres multi-byte: índices son code points, no bytes. */
    verificar_var("x = \"caf\xc3\xa9\"[0:3]", "x", "caf");
    verificar_var("x = \"caf\xc3\xa9\"[3:]", "x", "\xc3\xa9");
    verificar_var("x = \"caf\xc3\xa9\"[::-1]", "x", "\xc3\xa9""fac");
}

static void test_slice_cadena_fuera_rango(void) {
    /* Fuera de rango → cadena vacía (clamp Python-style). */
    verificar_var("x = \"abc\"[10:20]", "x", "");
    verificar_var("x = \"abc\"[2:1]", "x", "");
}

static void test_slice_cadena_vacia(void) {
    verificar_var("x = \"\"[0:5]", "x", "");
    verificar_var("x = \"\"[::]", "x", "");
}

/* ───── Slicing de listas (sin regresión) ───── */

static void test_slice_lista_sigue_funcionando(void) {
    verificar_var("x = [1, 2, 3, 4, 5][1:4]", "x", "[2, 3, 4]");
    verificar_var("x = [1, 2, 3, 4, 5][::-1]", "x", "[5, 4, 3, 2, 1]");
}

static void test_slice_paso_cero_error(void) {
    /* Paso = 0 produce ErrorDeValor atrapable. */
    verificar_var(
        "_atrapado = falso\n"
        "intentar:\n"
        "  _y = \"abc\"[0:3:0]\n"
        "atrapar Excepcion como e:\n"
        "  _atrapado = verdadero\n"
        "fin intentar\n"
        "x = _atrapado",
        "x", "verdadero");
}

/* ───── F-cadenas triples ───── */

static void test_ftriple_basico(void) {
    verificar_var(
        "n = 5\n"
        "x = f\"\"\"valor={n}\"\"\"",
        "x", "valor=5");
}

static void test_ftriple_multilinea(void) {
    /* Saltos de línea preservados. */
    verificar_var(
        "x = f\"\"\"linea1\nlinea2\"\"\"",
        "x", "linea1\nlinea2");
}

static void test_ftriple_simples(void) {
    /* Triple con comillas simples. */
    verificar_var(
        "n = 7\n"
        "x = f'''val={n}'''",
        "x", "val=7");
}

static void test_ftriple_comillas_internas(void) {
    /* Triple permite comillas dobles sueltas dentro. */
    verificar_var(
        "x = f\"\"\"con \"comillas\" dentro\"\"\"",
        "x", "con \"comillas\" dentro");
}

/* ───── Handler leak fix: retornar dentro de intentar ───── */

static void test_handler_no_leak_tras_retornar(void) {
    /* Bug preexistente expuesto en v1.14: si una función `retornar`
       desde dentro de un `intentar` (sin pasar por INTENTAR_FIN), el
       handler quedaba registrado y atrapaba excepciones del caller.
       Repro: dos llamadas, primera OK, segunda lanza — el warn debe
       imprimirse UNA sola vez (no dos). Usamos una lista (mutable
       compartida) en vez de global+global decl para simplicidad. */
    verificar_var(
        "_warns = []\n"
        "funcion f(x):\n"
        "  intentar:\n"
        "    retornar x[0:0]\n"
        "  atrapar Excepcion:\n"
        "    agregar(_warns, 1)\n"
        "    lanzar\n"
        "  fin intentar\n"
        "fin funcion\n"
        "intentar:\n"
        "  f(\"abc\")\n"      /* OK */
        "  f(42)\n"           /* 42[0:0] da ErrorDeTipo */
        "atrapar ErrorDeTipo:\n"
        "  pasar\n"
        "fin intentar\n"
        "x = longitud(_warns)",
        "x", "1");
}

/* ───── Cadenas triples literales (sin f) ───── */

static void test_triple_literal(void) {
    verificar_var(
        "x = \"\"\"linea1\nlinea2\"\"\"",
        "x", "linea1\nlinea2");
}

static void test_triple_literal_simples(void) {
    verificar_var(
        "x = '''hola\nmundo'''",
        "x", "hola\nmundo");
}

int main(void) {
    test_reraise_con_alias();
    test_reraise_sin_alias();
    test_reraise_fuera_de_atrapar();

    test_slice_cadena_basico();
    test_slice_cadena_negativo();
    test_slice_cadena_paso();
    test_slice_cadena_utf8();
    test_slice_cadena_fuera_rango();
    test_slice_cadena_vacia();
    test_slice_lista_sigue_funcionando();
    test_slice_paso_cero_error();

    test_ftriple_basico();
    test_ftriple_multilinea();
    test_ftriple_simples();
    test_ftriple_comillas_internas();

    test_triple_literal();
    test_triple_literal_simples();

    test_handler_no_leak_tras_retornar();

    if (fallos == 0) {
        printf("pulido: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "pulido: %d fallo(s)\n", fallos);
    return 1;
}
