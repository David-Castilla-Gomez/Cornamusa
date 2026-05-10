/*
 * Tests de la sentencia `con` (v1.13).
 *
 * El parser desugar `con expr [como x]: ... fin con` a un bloque con
 * intentar/finalmente que invoca `__entrar__` y `__salir__` sobre el
 * context manager. Esto garantiza que `__salir__` se ejecuta sí o sí
 * (como `with` de Python).
 *
 * Cubre:
 *   - Sin alias y con alias.
 *   - El cuerpo lanza excepción → __salir__ ejecuta antes de propagar.
 *   - Anidación: orden correcto LIFO.
 *   - Variable interna no contamina el alcance del usuario.
 *   - Errores: clase sin __entrar__ o __salir__ → ErrorDeTipo.
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

/* Plantilla reutilizada: clase Cronometro con counters. */
static const char *CRONO =
    "clase Crono:\n"
    "  funcion __iniciar__(yo):\n"
    "    yo.entradas = 0\n"
    "    yo.salidas = 0\n"
    "  fin funcion\n"
    "  funcion __entrar__(yo):\n"
    "    yo.entradas = yo.entradas + 1\n"
    "    retornar yo\n"
    "  fin funcion\n"
    "  funcion __salir__(yo):\n"
    "    yo.salidas = yo.salidas + 1\n"
    "  fin funcion\n"
    "fin clase\n";

/* ───── Casos básicos ───── */

static void test_con_alias_sin_excepcion(void) {
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "c = Crono()\n"
        "_dentro = nulo\n"
        "con c como x:\n"
        "  _dentro = x.entradas\n"
        "fin con\n"
        "x = [c.entradas, c.salidas, _dentro]",
        CRONO);
    verificar_var(fuente, "x", "[1, 1, 1]");
}

static void test_con_sin_alias(void) {
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "c = Crono()\n"
        "con c:\n"
        "  _t = c.entradas + 100\n"
        "fin con\n"
        "x = [c.entradas, c.salidas, _t]",
        CRONO);
    verificar_var(fuente, "x", "[1, 1, 101]");
}

/* ───── Excepciones ───── */

static void test_con_cuerpo_lanza(void) {
    /* El cuerpo lanza → __salir__ se ejecuta antes de propagar. */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "c = Crono()\n"
        "_atrapado = falso\n"
        "intentar:\n"
        "  con c:\n"
        "    lanzar ErrorDeValor(\"falla\")\n"
        "  fin con\n"
        "atrapar ErrorDeValor como e:\n"
        "  _atrapado = verdadero\n"
        "fin intentar\n"
        "x = [c.entradas, c.salidas, _atrapado]",
        CRONO);
    /* Tanto __entrar__ como __salir__ deben haber corrido una vez. */
    verificar_var(fuente, "x", "[1, 1, verdadero]");
}

static void test_con_entrar_lanza(void) {
    /* Si __entrar__ lanza, __salir__ NO debe ejecutarse (no estábamos
       dentro del with todavía). */
    verificar_var(
        "clase Mal:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.salidas = 0\n"
        "  fin funcion\n"
        "  funcion __entrar__(yo):\n"
        "    lanzar ErrorDeValor(\"al entrar\")\n"
        "  fin funcion\n"
        "  funcion __salir__(yo):\n"
        "    yo.salidas = yo.salidas + 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "m = Mal()\n"
        "_atrapado = falso\n"
        "intentar:\n"
        "  con m como _x:\n"
        "    pasar\n"
        "  fin con\n"
        "atrapar ErrorDeValor como e:\n"
        "  _atrapado = verdadero\n"
        "fin intentar\n"
        "x = [m.salidas, _atrapado]",
        "x", "[0, verdadero]");
}

/* ───── Anidación ───── */

static void test_con_anidado(void) {
    /* Anidados: salir interno antes que el externo (LIFO). */
    verificar_var(
        "_orden = []\n"
        "clase CronoOrden:\n"
        "  funcion __iniciar__(yo, etiqueta):\n"
        "    yo.etiqueta = etiqueta\n"
        "  fin funcion\n"
        "  funcion __entrar__(yo):\n"
        "    agregar(_orden, f\"E:{yo.etiqueta}\")\n"
        "    retornar yo\n"
        "  fin funcion\n"
        "  funcion __salir__(yo):\n"
        "    agregar(_orden, f\"S:{yo.etiqueta}\")\n"
        "  fin funcion\n"
        "fin clase\n"
        "ext = CronoOrden(\"ext\")\n"
        "intr = CronoOrden(\"int\")\n"
        "con ext:\n"
        "  con intr:\n"
        "    agregar(_orden, \"cuerpo\")\n"
        "  fin con\n"
        "fin con\n"
        "x = _orden",
        "x",
        "[\"E:ext\", \"E:int\", \"cuerpo\", \"S:int\", \"S:ext\"]");
}

/* ───── Errores: dunders ausentes ───── */

static void test_con_sin_entrar(void) {
    verificar_var(
        "clase SinEntrar:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "  funcion __salir__(yo):\n"
        "    yo.x = 2\n"
        "  fin funcion\n"
        "fin clase\n"
        "_atrapado = falso\n"
        "intentar:\n"
        "  con SinEntrar() como _:\n"
        "    pasar\n"
        "  fin con\n"
        "atrapar Excepcion como e:\n"
        "  _atrapado = verdadero\n"
        "fin intentar\n"
        "x = _atrapado",
        "x", "verdadero");
}

static void test_con_sin_salir(void) {
    /* Sin __salir__: el cuerpo del finalmente lanzará al intentar
       invocar el método inexistente. La excepción se propaga
       atrapable. */
    verificar_var(
        "clase SinSalir:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "  funcion __entrar__(yo):\n"
        "    retornar yo\n"
        "  fin funcion\n"
        "fin clase\n"
        "_atrapado = falso\n"
        "intentar:\n"
        "  con SinSalir():\n"
        "    pasar\n"
        "  fin con\n"
        "atrapar Excepcion como e:\n"
        "  _atrapado = verdadero\n"
        "fin intentar\n"
        "x = _atrapado",
        "x", "verdadero");
}

/* ───── Sintaxis ───── */

static void test_con_expresion_compleja(void) {
    /* La expresión del context manager puede ser cualquier expresión:
       una llamada, un acceso a atributo, etc. */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "funcion crear():\n"
        "  retornar Crono()\n"
        "fin funcion\n"
        "con crear() como c:\n"
        "  _e = c.entradas\n"
        "fin con\n"
        "x = _e",
        CRONO);
    verificar_var(fuente, "x", "1");
}

int main(void) {
    test_con_alias_sin_excepcion();
    test_con_sin_alias();
    test_con_cuerpo_lanza();
    test_con_entrar_lanza();
    test_con_anidado();
    test_con_sin_entrar();
    test_con_sin_salir();
    test_con_expresion_compleja();

    if (fallos == 0) {
        printf("con: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "con: %d fallo(s)\n", fallos);
    return 1;
}
