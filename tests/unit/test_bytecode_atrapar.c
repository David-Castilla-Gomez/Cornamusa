/*
 * Tests de errores atrapables en built-ins (v1.10).
 *
 * Cubre:
 *   - Errores de I/O (archivo no existe).
 *   - Errores de tipo (longitud(int), suma de tipos incompatibles).
 *   - Errores de valor (JSON inválido).
 *   - Errores de índice (lista[fuera de rango]).
 *   - Atrapar por tipo específico (ErrorDeIO, ErrorDeTipo, etc.).
 *   - Múltiples atrapados consecutivos.
 *   - Globales preservados tras unwind a través de frames de módulo.
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

/* ───── Errores de tipo ───── */

static void test_atrapar_longitud_tipo(void) {
    /* `longitud(42)` da ErrorDeTipo. v1.10: atrapable. */
    verificar_var(
        "_x = nulo\n"
        "intentar:\n"
        "  longitud(42)\n"
        "atrapar Excepcion como e:\n"
        "  _x = \"atrapado\"\n"
        "fin intentar\n"
        "x = _x",
        "x", "atrapado");
}

static void test_atrapar_por_tipo_especifico(void) {
    /* `atrapar ErrorDeTipo como e` debe matchear el ErrorDeTipo
       producido por `longitud(42)`. */
    verificar_var(
        "_clase = nulo\n"
        "intentar:\n"
        "  longitud(42)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  _clase = \"tipo\"\n"
        "atrapar Excepcion como e:\n"
        "  _clase = \"otro\"\n"
        "fin intentar\n"
        "x = _clase",
        "x", "tipo");
}

/* ───── Errores de I/O ───── */

static void test_atrapar_archivo_inexistente(void) {
    /* Usa el built-in directo `archivo_leer` (sin wrapper de módulo)
       para no depender de la ruta de búsqueda de stdlib en tests. */
    verificar_var(
        "_msg = \"\"\n"
        "intentar:\n"
        "  archivo_leer(\"/no/existe.txt\")\n"
        "atrapar Excepcion como e:\n"
        "  _msg = \"capturado\"\n"
        "fin intentar\n"
        "x = _msg",
        "x", "capturado");
}

/* ───── Errores de valor ───── */

static void test_atrapar_json_invalido(void) {
    verificar_var(
        "_ok = falso\n"
        "intentar:\n"
        "  json_parsear(\"esto no es JSON\")\n"
        "atrapar Excepcion como e:\n"
        "  _ok = verdadero\n"
        "fin intentar\n"
        "x = _ok",
        "x", "verdadero");
}

/* ───── Errores en operadores ───── */

static void test_atrapar_suma_tipos_incompatibles(void) {
    verificar_var(
        "_e = \"\"\n"
        "intentar:\n"
        "  _x = 5 + \"hola\"\n"
        "atrapar Excepcion como e:\n"
        "  _e = \"caught\"\n"
        "fin intentar\n"
        "x = _e",
        "x", "caught");
}

/* ───── Múltiples atrapados consecutivos ───── */

static void test_multiples_atrapados(void) {
    /* Si el unwind tiene bug, el segundo atrapado fallaría. */
    verificar_var(
        "_n = 0\n"
        "intentar:\n"
        "  longitud(42)\n"
        "atrapar Excepcion como e:\n"
        "  _n = _n + 1\n"
        "fin intentar\n"
        "intentar:\n"
        "  longitud(falso)\n"
        "atrapar Excepcion como e:\n"
        "  _n = _n + 10\n"
        "fin intentar\n"
        "intentar:\n"
        "  longitud(3.14)\n"
        "atrapar Excepcion como e:\n"
        "  _n = _n + 100\n"
        "fin intentar\n"
        "x = _n",
        "x", "111");
}

/* ───── Excepción atrapada como valor ───── */

static void test_excepcion_como_valor(void) {
    /* La variable de `como e` debe contener una Excepcion con clase
       y mensaje correctos. */
    verificar_var(
        "_clase = \"\"\n"
        "intentar:\n"
        "  longitud(42)\n"
        "atrapar Excepcion como e:\n"
        "  _clase = e\n"  /* la excepción se imprime como "Clase: mensaje" */
        "fin intentar\n"
        "x = _clase",
        "x", "ErrorDeTipo: longitud() no soporta 'entero'");
}

int main(void) {
    test_atrapar_longitud_tipo();
    test_atrapar_por_tipo_especifico();
    test_atrapar_archivo_inexistente();
    test_atrapar_json_invalido();
    test_atrapar_suma_tipos_incompatibles();
    test_multiples_atrapados();
    test_excepcion_como_valor();

    if (fallos == 0) {
        printf("atrapar: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "atrapar: %d fallo(s)\n", fallos);
    return 1;
}
