/*
 * Tests de patrones estructurales en `coincidir` (v1.16).
 *
 * Extiende v1.15 con:
 *   - Patrón tupla `(p1, p2, ...)`: matchea VAL_TUPLA con misma aridad.
 *   - Patrón lista `[p1, p2, ...]`: matchea VAL_LISTA con misma longitud.
 *   - Anidación arbitraria.
 *   - Sub-patrones pueden ser literal/bind/wildcard/estructural.
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

/* ───── Tuplas básicas ───── */

static void test_tupla_origen(void) {
    verificar_var(
        "p = (0, 0)\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando (0, 0):\n"
        "    _r = \"origen\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "origen");
}

static void test_tupla_bind_segundo(void) {
    verificar_var(
        "p = (5, 0)\n"
        "_r = nulo\n"
        "coincidir p:\n"
        "  cuando (a, 0):\n"
        "    _r = a\n"
        "  cuando _:\n"
        "    _r = -1\n"
        "fin coincidir\n"
        "x = _r",
        "x", "5");
}

static void test_tupla_dos_binds(void) {
    verificar_var(
        "p = (10, 20)\n"
        "_r = 0\n"
        "coincidir p:\n"
        "  cuando (a, b):\n"
        "    _r = a + b\n"
        "fin coincidir\n"
        "x = _r",
        "x", "30");
}

static void test_tupla_aridad_no_coincide(void) {
    /* Patron de 2 elementos no matchea tupla de 3. */
    verificar_var(
        "p = (1, 2, 3)\n"
        "_r = \"sin match\"\n"
        "coincidir p:\n"
        "  cuando (a, b):\n"
        "    _r = \"par\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "otro");
}

static void test_tupla_no_es_lista(void) {
    /* Patron tupla NO matchea lista (tipos distintos). */
    verificar_var(
        "p = [1, 2]\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando (a, b):\n"
        "    _r = \"tupla\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "otro");
}

/* ───── Listas ───── */

static void test_lista_vacia(void) {
    verificar_var(
        "xs = []\n"
        "_r = \"\"\n"
        "coincidir xs:\n"
        "  cuando []:\n"
        "    _r = \"vacia\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "vacia");
}

static void test_lista_singleton(void) {
    verificar_var(
        "xs = [42]\n"
        "_r = -1\n"
        "coincidir xs:\n"
        "  cuando [unico]:\n"
        "    _r = unico\n"
        "fin coincidir\n"
        "x = _r",
        "x", "42");
}

static void test_lista_no_es_tupla(void) {
    /* Patron lista NO matchea tupla. */
    verificar_var(
        "xs = (1, 2)\n"
        "_r = \"\"\n"
        "coincidir xs:\n"
        "  cuando [a, b]:\n"
        "    _r = \"lista\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "otro");
}

/* ───── Anidación ───── */

static void test_anidacion_dos_niveles(void) {
    /* (etiqueta, (a, b)) extrae correctamente. */
    verificar_var(
        "p = (\"punto\", (3, 4))\n"
        "_r = 0\n"
        "coincidir p:\n"
        "  cuando (etiq, (a, b)):\n"
        "    _r = a + b\n"
        "fin coincidir\n"
        "x = _r",
        "x", "7");
}

static void test_anidacion_falla_segundo_nivel(void) {
    /* Si la sub-tupla no coincide en aridad, fallthrough. */
    verificar_var(
        "p = (\"punto\", (3, 4, 5))\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando (etiq, (a, b)):\n"
        "    _r = \"dos\"\n"
        "  cuando (etiq, otro):\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "otro");
}

static void test_lista_dentro_de_tupla(void) {
    /* (etiqueta, [a, b]) — anidación heterogénea. */
    verificar_var(
        "p = (\"datos\", [10, 20])\n"
        "_r = 0\n"
        "coincidir p:\n"
        "  cuando (etiq, [a, b]):\n"
        "    _r = a + b\n"
        "fin coincidir\n"
        "x = _r",
        "x", "30");
}

/* ───── Guardas + estructurales ───── */

static void test_guarda_con_tupla(void) {
    verificar_var(
        "p = (5, 5)\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando (a, b) si a == b:\n"
        "    _r = \"diagonal\"\n"
        "  cuando (a, b):\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "diagonal");
}

static void test_guarda_falla_continua(void) {
    verificar_var(
        "p = (5, 6)\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando (a, b) si a == b:\n"
        "    _r = \"diagonal\"\n"
        "  cuando (a, b):\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "otro");
}

/* ───── Tupla vacía ───── */

static void test_tupla_vacia(void) {
    verificar_var(
        "p = ()\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando ():\n"
        "    _r = \"vacia\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "vacia");
}

/* ───── Integración: secuencia de cláusulas ───── */

static void test_secuencia_completa(void) {
    /* Verifica que múltiples cláusulas no contaminan stack entre sí. */
    verificar_var(
        "casos = [(0, 0), (5, 0), (0, 3), (4, 4), (1, 2)]\n"
        "salida = []\n"
        "para p en casos:\n"
        "  coincidir p:\n"
        "    cuando (0, 0):\n"
        "      agregar(salida, \"origen\")\n"
        "    cuando (a, 0):\n"
        "      agregar(salida, a)\n"
        "    cuando (0, b):\n"
        "      agregar(salida, b)\n"
        "    cuando (a, b) si a == b:\n"
        "      agregar(salida, \"diag\")\n"
        "    cuando (a, b):\n"
        "      agregar(salida, a + b)\n"
        "  fin coincidir\n"
        "fin para\n"
        "x = salida",
        "x", "[\"origen\", 5, 3, \"diag\", 3]");
}

int main(void) {
    test_tupla_origen();
    test_tupla_bind_segundo();
    test_tupla_dos_binds();
    test_tupla_aridad_no_coincide();
    test_tupla_no_es_lista();

    test_lista_vacia();
    test_lista_singleton();
    test_lista_no_es_tupla();

    test_anidacion_dos_niveles();
    test_anidacion_falla_segundo_nivel();
    test_lista_dentro_de_tupla();

    test_guarda_con_tupla();
    test_guarda_falla_continua();

    test_tupla_vacia();

    test_secuencia_completa();

    if (fallos == 0) {
        printf("coincidir-estructural: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "coincidir-estructural: %d fallo(s)\n", fallos);
    return 1;
}
