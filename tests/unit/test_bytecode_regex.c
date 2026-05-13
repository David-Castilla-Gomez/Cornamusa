/*
 * Tests de regex (v1.28) — motor backtracking propio.
 *
 * Cubre:
 *   - Literales y fullmatch vs partial.
 *   - Cualquier (.), quantifiers (*, +, ?), greedy.
 *   - Anchors ^ y $.
 *   - Clases [abc], [^abc], [a-z], \d \w \s.
 *   - Alternancia a|b.
 *   - Grupos no-captura (?:...).
 *   - regex_buscar (primera ocurrencia).
 *   - regex_todos (todos no-solapantes).
 *   - regex_reemplazar.
 *   - Errores de sintaxis atrapables.
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
        if (error_out) *error_out = c.error.mensaje;
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

static void verificar_var(const char *desc, const char *fuente,
                           const char *var, const char *esperado) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, var, &err);
    if (!res) {
        fprintf(stderr, "FALLO [%s]: error %s\n", desc,
                err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO [%s]: %s=%s (esperaba %s)\n",
                desc, var, res, esperado);
        fallos++;
    }
}

/* ───── Coincidencia: fullmatch ───── */

static void test_literal_full(void) {
    verificar_var("literal coincide todo",
        "x = regex_coincide(\"hola\", \"hola\")",
        "x", "verdadero");
}

static void test_literal_no_full(void) {
    verificar_var("literal no fullmatch si sobra",
        "x = regex_coincide(\"hola\", \"hola mundo\")",
        "x", "falso");
}

static void test_punto(void) {
    verificar_var(". matches cualquiera",
        "x = regex_coincide(\"h.la\", \"hxla\")",
        "x", "verdadero");
}

static void test_estrella(void) {
    verificar_var("* greedy",
        "x = regex_coincide(\"a*b\", \"aaaab\")",
        "x", "verdadero");
}

static void test_mas(void) {
    verificar_var("+ uno o más",
        "x = regex_coincide(\"a+\", \"\")",
        "x", "falso");
}

static void test_interrogacion(void) {
    verificar_var("? opcional",
        "x = regex_coincide(\"colou?r\", \"color\")",
        "x", "verdadero");
}

/* ───── Clases ───── */

static void test_clase_simple(void) {
    verificar_var("[abc]",
        "x = regex_coincide(\"[abc]+\", \"abccba\")",
        "x", "verdadero");
}

static void test_clase_negada(void) {
    verificar_var("[^abc]",
        "x = regex_coincide(\"[^abc]+\", \"xyz\")",
        "x", "verdadero");
}

static void test_rango(void) {
    verificar_var("[a-z]",
        "x = regex_coincide(\"[a-z]+\", \"hola\")",
        "x", "verdadero");
}

static void test_clase_d(void) {
    verificar_var("\\d números",
        "x = regex_coincide(\"\\\\d+\", \"12345\")",
        "x", "verdadero");
}

static void test_clase_w(void) {
    verificar_var("\\w identificador",
        "x = regex_coincide(\"\\\\w+\", \"hola_99\")",
        "x", "verdadero");
}

/* ───── Anchors ───── */

static void test_ancla_inicio(void) {
    verificar_var("^ y fullmatch",
        "x = regex_coincide(\"^hola$\", \"hola\")",
        "x", "verdadero");
}

/* ───── Alternancia ───── */

static void test_alternancia(void) {
    verificar_var("a|b|c",
        "x = regex_coincide(\"rojo|verde|azul\", \"verde\")",
        "x", "verdadero");
}

/* ───── Buscar ───── */

static void test_buscar_encuentra(void) {
    verificar_var("buscar dígitos",
        "r = regex_buscar(\"\\\\d+\", \"abc 123 def\")\n"
        "x = r",
        "x", "(4, 7)");
}

static void test_buscar_no_encuentra(void) {
    verificar_var("buscar sin match",
        "r = regex_buscar(\"\\\\d+\", \"sin numeros\")\n"
        "x = r",
        "x", "nulo");
}

/* ───── Todos ───── */

static void test_todos(void) {
    verificar_var("todos números",
        "x = regex_todos(\"\\\\d+\", \"a 1 b 22 c 333\")",
        "x", "[\"1\", \"22\", \"333\"]");
}

static void test_todos_palabras(void) {
    verificar_var("todos palabras",
        "x = regex_todos(\"\\\\w+\", \"hola mundo cornamusa\")",
        "x", "[\"hola\", \"mundo\", \"cornamusa\"]");
}

/* ───── Reemplazar ───── */

static void test_reemplazar(void) {
    verificar_var("reemplazar dígitos",
        "x = regex_reemplazar(\"\\\\d+\", \"a 1 b 22 c\", \"N\")",
        "x", "a N b N c");
}

static void test_reemplazar_vocales(void) {
    verificar_var("reemplazar vocales",
        "x = regex_reemplazar(\"[aeiou]\", \"Hola Mundo\", \"*\")",
        "x", "H*l* M*nd*");
}

/* ───── Errores ───── */

static void test_error_patron_invalido(void) {
    verificar_var("patron invalido",
        "intentar:\n"
        "  regex_coincide(\"[abc\", \"abc\")\n"
        "atrapar ErrorDeValor como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_grupos_no_captura(void) {
    verificar_var("grupos (?:...)",
        "x = regex_coincide(\"(?:ab)+\", \"abab\")",
        "x", "verdadero");
}

int main(void) {
    test_literal_full();
    test_literal_no_full();
    test_punto();
    test_estrella();
    test_mas();
    test_interrogacion();
    test_clase_simple();
    test_clase_negada();
    test_rango();
    test_clase_d();
    test_clase_w();
    test_ancla_inicio();
    test_alternancia();
    test_buscar_encuentra();
    test_buscar_no_encuentra();
    test_todos();
    test_todos_palabras();
    test_reemplazar();
    test_reemplazar_vocales();
    test_error_patron_invalido();
    test_grupos_no_captura();

    if (fallos == 0) {
        printf("regex: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "regex: %d fallo(s)\n", fallos);
    return 1;
}
