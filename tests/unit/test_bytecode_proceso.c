/*
 * Tests de proceso (v1.27) — built-in `proceso_ejecutar`.
 *
 * Estos tests dependen de tener `cmd` (Windows) o `sh` (POSIX) en PATH.
 * En Windows usamos `cmd /c echo ...` y `cmd /c exit N`.
 * En POSIX usamos `sh -c 'echo ...; exit N'`.
 *
 * El módulo `stdlib/proceso.cor` se prueba vía `bc_run_52_proceso`.
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

/* ───── Forma del dict resultado ───── */

#if defined(_WIN32)
#  define SHELL_NAME "cmd"
#  define ARG_C "/c"
#else
#  define SHELL_NAME "sh"
#  define ARG_C "-c"
#endif

static void test_dict_tres_claves(void) {
    /* El dict resultado debe tener stdout, stderr, codigo. */
    verificar_var("dict tres claves",
        "r = proceso_ejecutar(\"" SHELL_NAME "\", [\"" SHELL_NAME "\", \"" ARG_C "\", \"exit 0\"])\n"
        "x = longitud(r)",
        "x", "3");
}

static void test_codigo_cero(void) {
    verificar_var("exit 0 → codigo 0",
        "r = proceso_ejecutar(\"" SHELL_NAME "\", [\"" SHELL_NAME "\", \"" ARG_C "\", \"exit 0\"])\n"
        "x = r[\"codigo\"]",
        "x", "0");
}

static void test_codigo_no_cero(void) {
    verificar_var("exit 7 → codigo 7",
        "r = proceso_ejecutar(\"" SHELL_NAME "\", [\"" SHELL_NAME "\", \"" ARG_C "\", \"exit 7\"])\n"
        "x = r[\"codigo\"]",
        "x", "7");
}

static void test_stdout_es_cadena(void) {
    verificar_var("stdout es cadena",
        "r = proceso_ejecutar(\"" SHELL_NAME "\", [\"" SHELL_NAME "\", \"" ARG_C "\", \"exit 0\"])\n"
        "x = tipo(r[\"stdout\"])",
        "x", "cadena");
}

static void test_stderr_es_cadena(void) {
    verificar_var("stderr es cadena",
        "r = proceso_ejecutar(\"" SHELL_NAME "\", [\"" SHELL_NAME "\", \"" ARG_C "\", \"exit 0\"])\n"
        "x = tipo(r[\"stderr\"])",
        "x", "cadena");
}

/* ───── Errores ───── */

static void test_error_programa_inexistente(void) {
    verificar_var("ejecutable que no existe",
        "intentar:\n"
        "  proceso_ejecutar(\"este_programa_no_existe_xyz_2026\", [\"este_programa_no_existe_xyz_2026\"])\n"
        "atrapar ErrorDeSistema como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_argv_no_lista(void) {
    verificar_var("argv debe ser lista",
        "intentar:\n"
        "  proceso_ejecutar(\"cmd\", \"no es lista\")\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_error_programa_no_cadena(void) {
    verificar_var("programa debe ser cadena",
        "intentar:\n"
        "  proceso_ejecutar(42, [\"cmd\"])\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

int main(void) {
    test_dict_tres_claves();
    test_codigo_cero();
    test_codigo_no_cero();
    test_stdout_es_cadena();
    test_stderr_es_cadena();
    test_error_programa_inexistente();
    test_error_argv_no_lista();
    test_error_programa_no_cadena();

    if (fallos == 0) {
        printf("proceso: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "proceso: %d fallo(s)\n", fallos);
    return 1;
}
