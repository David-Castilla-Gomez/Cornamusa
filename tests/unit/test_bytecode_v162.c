/*
 * Tests de v1.16.2: OR-patterns y star-pattern en `coincidir`.
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

/* ───── OR-patterns ───── */

static void test_or_basico(void) {
    verificar_var(
        "n = 2\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando 1 | 2 | 3:\n"
        "    _r = \"pequeño\"\n"
        "  cuando _:\n"
        "    _r = \"grande\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "pequeño");
}

static void test_or_cadenas(void) {
    verificar_var(
        "d = \"sábado\"\n"
        "_r = \"\"\n"
        "coincidir d:\n"
        "  cuando \"sábado\" | \"domingo\":\n"
        "    _r = \"fin\"\n"
        "  cuando _:\n"
        "    _r = \"laboral\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "fin");
}

static void test_or_negativos(void) {
    verificar_var(
        "n = -2\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando -1 | -2 | -3:\n"
        "    _r = \"neg\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "neg");
}

static void test_or_no_match(void) {
    verificar_var(
        "n = 99\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando 1 | 2 | 3:\n"
        "    _r = \"in\"\n"
        "  cuando _:\n"
        "    _r = \"out\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "out");
}

static void test_or_rechaza_bind(void) {
    /* OR con bind debe ser error de parseo. */
    const char *err = NULL;
    const char *res = ejecutar(
        "coincidir 5:\n"
        "  cuando a | 2:\n"
        "    pasar\n"
        "  cuando _:\n"
        "    pasar\n"
        "fin coincidir\n", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO: OR con bind no detectado\n");
        fallos++;
    }
}

/* ───── Star-pattern ───── */

static void test_star_head(void) {
    verificar_var(
        "xs = [1, 2, 3, 4]\n"
        "_r = nulo\n"
        "coincidir xs:\n"
        "  cuando [primero, *resto]:\n"
        "    _r = [primero, resto]\n"
        "fin coincidir\n"
        "x = _r",
        "x", "[1, [2, 3, 4]]");
}

static void test_star_tail(void) {
    verificar_var(
        "xs = [1, 2, 3, 4]\n"
        "_r = nulo\n"
        "coincidir xs:\n"
        "  cuando [*inicio, ultimo]:\n"
        "    _r = [inicio, ultimo]\n"
        "fin coincidir\n"
        "x = _r",
        "x", "[[1, 2, 3], 4]");
}

static void test_star_medio(void) {
    verificar_var(
        "xs = [1, 2, 3, 4, 5]\n"
        "_r = nulo\n"
        "coincidir xs:\n"
        "  cuando [primero, *medio, ultimo]:\n"
        "    _r = [primero, medio, ultimo]\n"
        "fin coincidir\n"
        "x = _r",
        "x", "[1, [2, 3, 4], 5]");
}

static void test_star_captura_vacio(void) {
    /* Star matchea con 0 elementos en el medio. */
    verificar_var(
        "xs = [1, 2]\n"
        "_r = nulo\n"
        "coincidir xs:\n"
        "  cuando [primero, *medio, ultimo]:\n"
        "    _r = [primero, medio, ultimo]\n"
        "fin coincidir\n"
        "x = _r",
        "x", "[1, [], 2]");
}

static void test_star_lista_corta(void) {
    /* Si la lista no tiene suficientes fijos, no matchea. */
    verificar_var(
        "xs = [1]\n"
        "_r = \"\"\n"
        "coincidir xs:\n"
        "  cuando [primero, *medio, ultimo]:\n"
        "    _r = \"tres\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "otro");
}

static void test_star_solo_vacia(void) {
    /* [*todo] matchea cualquier lista. */
    verificar_var(
        "xs = [1, 2, 3]\n"
        "_r = nulo\n"
        "coincidir xs:\n"
        "  cuando [*todo]:\n"
        "    _r = todo\n"
        "fin coincidir\n"
        "x = _r",
        "x", "[1, 2, 3]");
}

static void test_star_rechaza_tupla(void) {
    /* Star solo permitido en lista. */
    const char *err = NULL;
    const char *res = ejecutar(
        "coincidir (1, 2):\n"
        "  cuando (primero, *resto):\n"
        "    pasar\n"
        "fin coincidir\n", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO: star en tupla no detectado\n");
        fallos++;
    }
}

static void test_star_rechaza_dos(void) {
    /* Solo un star permitido. */
    const char *err = NULL;
    const char *res = ejecutar(
        "coincidir [1, 2]:\n"
        "  cuando [*a, *b]:\n"
        "    pasar\n"
        "fin coincidir\n", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO: dos stars no detectado\n");
        fallos++;
    }
}

/* ───── Integración ───── */

static void test_star_en_bucle(void) {
    /* Múltiples llamadas en bucle no contaminan stack. */
    verificar_var(
        "salida = []\n"
        "para xs en [[1, 2], [1, 2, 3], [1, 2, 3, 4]]:\n"
        "  coincidir xs:\n"
        "    cuando [primero, *resto]:\n"
        "      agregar(salida, longitud(resto))\n"
        "  fin coincidir\n"
        "fin para\n"
        "x = salida",
        "x", "[1, 2, 3]");
}

int main(void) {
    test_or_basico();
    test_or_cadenas();
    test_or_negativos();
    test_or_no_match();
    test_or_rechaza_bind();

    test_star_head();
    test_star_tail();
    test_star_medio();
    test_star_captura_vacio();
    test_star_lista_corta();
    test_star_solo_vacia();
    test_star_rechaza_tupla();
    test_star_rechaza_dos();

    test_star_en_bucle();

    if (fallos == 0) {
        printf("v162: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "v162: %d fallo(s)\n", fallos);
    return 1;
}
