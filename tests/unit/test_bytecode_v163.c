/*
 * Tests de v1.16.3: type-match (`cuando Foo():`) y `como nombre` en
 * cláusulas de `coincidir`.
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

/* Plantilla: jerarquía de clases simple. */
static const char *CLASES_ANIMAL =
    "clase Animal:\n"
    "  funcion __iniciar__(yo, n):\n"
    "    yo.nombre = n\n"
    "  fin funcion\n"
    "fin clase\n"
    "clase Perro extiende Animal:\n"
    "  funcion __iniciar__(yo, n):\n"
    "    yo.nombre = n\n"
    "  fin funcion\n"
    "fin clase\n"
    "clase Gato extiende Animal:\n"
    "  funcion __iniciar__(yo, n):\n"
    "    yo.nombre = n\n"
    "  fin funcion\n"
    "fin clase\n";

/* ───── Type-match ───── */

static void test_type_match_basico(void) {
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p = Perro(\"Toby\")\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando Perro():\n"
        "    _r = \"perro\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        CLASES_ANIMAL);
    verificar_var(fuente, "x", "perro");
}

static void test_type_match_herencia(void) {
    /* Perro hereda Animal: Perro() matchea pero tambien Animal(). */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p = Perro(\"Toby\")\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando Animal():\n"
        "    _r = \"animal\"\n"
        "fin coincidir\n"
        "x = _r",
        CLASES_ANIMAL);
    verificar_var(fuente, "x", "animal");
}

static void test_type_match_orden_importa(void) {
    /* Si Perro() está antes que Animal(), matchea Perro. */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p = Perro(\"Toby\")\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando Perro():\n"
        "    _r = \"perro\"\n"
        "  cuando Animal():\n"
        "    _r = \"animal\"\n"
        "fin coincidir\n"
        "x = _r",
        CLASES_ANIMAL);
    verificar_var(fuente, "x", "perro");
}

static void test_type_match_no_es_instancia(void) {
    /* Un primitivo no matchea ningún type-match. */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "_r = \"\"\n"
        "coincidir 42:\n"
        "  cuando Animal():\n"
        "    _r = \"animal\"\n"
        "  cuando _:\n"
        "    _r = \"no\"\n"
        "fin coincidir\n"
        "x = _r",
        CLASES_ANIMAL);
    verificar_var(fuente, "x", "no");
}

/* ───── `como nombre` ───── */

static void test_como_bindea_sujeto(void) {
    /* `cuando Foo() como v:` bindea v al sujeto. */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p = Perro(\"Toby\")\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando Perro() como x:\n"
        "    _r = x.nombre\n"
        "fin coincidir\n"
        "x_out = _r",
        CLASES_ANIMAL);
    verificar_var(fuente, "x_out", "Toby");
}

static void test_como_con_literal(void) {
    /* `como` también funciona con patrones literales. */
    verificar_var(
        "_r = 0\n"
        "coincidir 5:\n"
        "  cuando 5 como n:\n"
        "    _r = n * 2\n"
        "fin coincidir\n"
        "x = _r",
        "x", "10");
}

static void test_como_con_guarda(void) {
    /* `como` + guarda en la misma cláusula. */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p = Perro(\"R\")\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando Perro() como x si longitud(x.nombre) == 1:\n"
        "    _r = \"corto\"\n"
        "  cuando Perro():\n"
        "    _r = \"largo\"\n"
        "fin coincidir\n"
        "x_out = _r",
        CLASES_ANIMAL);
    verificar_var(fuente, "x_out", "corto");
}

static void test_como_no_match_no_bindea(void) {
    /* Si el patrón falla, el `como` NO bindea. La siguiente cláusula
       puede usar el nombre sin conflicto. */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p = Gato(\"M\")\n"
        "_r = \"\"\n"
        "coincidir p:\n"
        "  cuando Perro() como x:\n"
        "    _r = \"P\"\n"
        "  cuando Gato() como x:\n"
        "    _r = f\"G{x.nombre}\"\n"
        "fin coincidir\n"
        "x_out = _r",
        CLASES_ANIMAL);
    verificar_var(fuente, "x_out", "GM");
}

/* ───── Integración ───── */

static void test_type_match_en_bucle(void) {
    /* Múltiples llamadas en bucle no contaminan stack. */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "salida = []\n"
        "para a en [Perro(\"a\"), Gato(\"b\"), Animal(\"c\")]:\n"
        "  coincidir a:\n"
        "    cuando Perro():\n"
        "      agregar(salida, \"P\")\n"
        "    cuando Gato():\n"
        "      agregar(salida, \"G\")\n"
        "    cuando Animal():\n"
        "      agregar(salida, \"A\")\n"
        "  fin coincidir\n"
        "fin para\n"
        "x = salida",
        CLASES_ANIMAL);
    verificar_var(fuente, "x", "[\"P\", \"G\", \"A\"]");
}

/* ───── Sintaxis ───── */

static void test_type_match_con_args_error(void) {
    /* v1.16.3 no soporta Foo(x, y) — destructuring posicional. */
    const char *err = NULL;
    const char *res = ejecutar(
        "coincidir 5:\n"
        "  cuando Foo(x, y):\n"
        "    pasar\n"
        "fin coincidir\n", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO: Foo(x, y) deberia fallar\n");
        fallos++;
    }
}

int main(void) {
    test_type_match_basico();
    test_type_match_herencia();
    test_type_match_orden_importa();
    test_type_match_no_es_instancia();

    test_como_bindea_sujeto();
    test_como_con_literal();
    test_como_con_guarda();
    test_como_no_match_no_bindea();

    test_type_match_en_bucle();
    test_type_match_con_args_error();

    if (fallos == 0) {
        printf("v163: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "v163: %d fallo(s)\n", fallos);
    return 1;
}
