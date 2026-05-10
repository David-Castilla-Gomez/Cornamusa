/*
 * Tests de pattern matching `coincidir`/`cuando` (v1.15).
 *
 * Patrones soportados en v1.15:
 *   - Wildcard `_`
 *   - Literal: entero, decimal, cadena, booleano, nulo (con `-` opcional)
 *   - Bind: identificador
 *   - Guarda: `cuando <patron> si <expr>:`
 *
 * Pendientes de versiones futuras: tuplas/listas estructurales,
 * OR-patterns, type-match.
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

/* ───── Patrones literales ───── */

static void test_literal_entero(void) {
    verificar_var(
        "n = 0\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando 0:\n"
        "    _r = \"cero\"\n"
        "  cuando 1:\n"
        "    _r = \"uno\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "cero");
}

static void test_literal_segundo_match(void) {
    verificar_var(
        "n = 1\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando 0:\n"
        "    _r = \"cero\"\n"
        "  cuando 1:\n"
        "    _r = \"uno\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "uno");
}

static void test_literal_no_match_default(void) {
    verificar_var(
        "n = 99\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando 0:\n"
        "    _r = \"cero\"\n"
        "  cuando 1:\n"
        "    _r = \"uno\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "otro");
}

static void test_literal_negativo(void) {
    verificar_var(
        "n = -5\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando -5:\n"
        "    _r = \"menos cinco\"\n"
        "  cuando _:\n"
        "    _r = \"otro\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "menos cinco");
}

static void test_literal_cadena(void) {
    verificar_var(
        "cmd = \"salir\"\n"
        "_r = \"\"\n"
        "coincidir cmd:\n"
        "  cuando \"ayuda\":\n"
        "    _r = \"H\"\n"
        "  cuando \"salir\":\n"
        "    _r = \"S\"\n"
        "  cuando _:\n"
        "    _r = \"?\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "S");
}

static void test_literal_booleano(void) {
    verificar_var(
        "b = verdadero\n"
        "_r = 0\n"
        "coincidir b:\n"
        "  cuando verdadero:\n"
        "    _r = 1\n"
        "  cuando falso:\n"
        "    _r = 2\n"
        "fin coincidir\n"
        "x = _r",
        "x", "1");
}

static void test_literal_nulo(void) {
    verificar_var(
        "v = nulo\n"
        "_r = \"\"\n"
        "coincidir v:\n"
        "  cuando nulo:\n"
        "    _r = \"vacio\"\n"
        "  cuando _:\n"
        "    _r = \"valor\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "vacio");
}

/* ───── Bind ───── */

static void test_bind_simple(void) {
    verificar_var(
        "n = 42\n"
        "_r = 0\n"
        "coincidir n:\n"
        "  cuando 0:\n"
        "    _r = -1\n"
        "  cuando v:\n"
        "    _r = v * 2\n"
        "fin coincidir\n"
        "x = _r",
        "x", "84");
}

/* ───── Guardas ───── */

static void test_guarda_pasa(void) {
    verificar_var(
        "n = -5\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando v si v < 0:\n"
        "    _r = \"neg\"\n"
        "  cuando v si v > 100:\n"
        "    _r = \"grande\"\n"
        "  cuando _:\n"
        "    _r = \"normal\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "neg");
}

static void test_guarda_falla_y_continua(void) {
    /* Cuando la guarda falla, debe continuar a la siguiente cláusula. */
    verificar_var(
        "n = 50\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando v si v < 0:\n"
        "    _r = \"neg\"\n"
        "  cuando v si v > 100:\n"
        "    _r = \"grande\"\n"
        "  cuando _:\n"
        "    _r = \"normal\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "normal");
}

static void test_guarda_con_literal(void) {
    /* Guarda sobre patron literal: el literal matchea, la guarda decide. */
    verificar_var(
        "n = 5\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando 5 si verdadero:\n"
        "    _r = \"cinco si\"\n"
        "  cuando 5:\n"
        "    _r = \"cinco no\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "cinco si");
}

/* ───── Wildcard ───── */

static void test_wildcard_atrapa_todo(void) {
    verificar_var(
        "n = 999\n"
        "_r = \"\"\n"
        "coincidir n:\n"
        "  cuando _:\n"
        "    _r = \"todo\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "todo");
}

static void test_sin_match_no_ejecuta_nada(void) {
    /* Si ningún patrón matchea, no se ejecuta cuerpo (no error). */
    verificar_var(
        "n = 5\n"
        "_r = \"intacto\"\n"
        "coincidir n:\n"
        "  cuando 0:\n"
        "    _r = \"cero\"\n"
        "  cuando 1:\n"
        "    _r = \"uno\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "intacto");
}

/* ───── En contexto de función ───── */

static void test_coincidir_dentro_de_funcion(void) {
    verificar_var(
        "funcion clasificar(n):\n"
        "  coincidir n:\n"
        "    cuando 0:\n"
        "      retornar \"cero\"\n"
        "    cuando v si v > 0:\n"
        "      retornar \"positivo\"\n"
        "    cuando _:\n"
        "      retornar \"negativo\"\n"
        "  fin coincidir\n"
        "fin funcion\n"
        "x = clasificar(7)",
        "x", "positivo");
}

static void test_coincidir_anidado(void) {
    /* coincidir dentro de cuerpo de otro coincidir. */
    verificar_var(
        "tipo = \"int\"\n"
        "valor = 0\n"
        "_r = \"\"\n"
        "coincidir tipo:\n"
        "  cuando \"int\":\n"
        "    coincidir valor:\n"
        "      cuando 0:\n"
        "        _r = \"int cero\"\n"
        "      cuando _:\n"
        "        _r = \"int otro\"\n"
        "    fin coincidir\n"
        "  cuando _:\n"
        "    _r = \"otro tipo\"\n"
        "fin coincidir\n"
        "x = _r",
        "x", "int cero");
}

/* ───── Sintaxis ───── */

static void test_coincidir_vacio_error(void) {
    /* Un coincidir sin cláusulas debe fallar (error de parseo). El
       parser imprime el detalle a stderr; aquí solo verificamos que
       NO produce un programa ejecutable. */
    const char *err = NULL;
    const char *res = ejecutar(
        "coincidir 5:\n"
        "fin coincidir\n", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO: coincidir vacio deberia fallar\n");
        fallos++;
    }
}

int main(void) {
    test_literal_entero();
    test_literal_segundo_match();
    test_literal_no_match_default();
    test_literal_negativo();
    test_literal_cadena();
    test_literal_booleano();
    test_literal_nulo();

    test_bind_simple();

    test_guarda_pasa();
    test_guarda_falla_y_continua();
    test_guarda_con_literal();

    test_wildcard_atrapa_todo();
    test_sin_match_no_ejecuta_nada();

    test_coincidir_dentro_de_funcion();
    test_coincidir_anidado();

    test_coincidir_vacio_error();

    if (fallos == 0) {
        printf("coincidir: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "coincidir: %d fallo(s)\n", fallos);
    return 1;
}
