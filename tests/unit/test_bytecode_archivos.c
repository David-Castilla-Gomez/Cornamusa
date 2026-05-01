/*
 * Tests de los built-ins de I/O de archivos (v1.8).
 *
 * Usa archivos temporales en `tmpnam()`/equivalente, los crea y
 * limpia en cada test. Cubre:
 *   - escribir + leer round-trip.
 *   - lineas split correcto.
 *   - existe true/false.
 *   - agregar (append) acumula.
 *   - errores de tipo (ruta no cadena, etc.).
 *
 * NO cubre errores de I/O (archivo no existe, permisos) porque las
 * nativas reportan errores de runtime no atrapables — el programa
 * termina. Validación manual en `examples/31_archivos.cor`.
 */

#include <stdio.h>
#include <stdlib.h>
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

/* Genera ruta de archivo temporal único para este proceso + counter. */
static int test_counter = 0;
static const char *ruta_tmp(void) {
    static char buf[256];
    snprintf(buf, sizeof(buf), "/tmp/cornamusa_test_%d_%d.txt",
             (int)getpid(), test_counter++);
    return buf;
}

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
    const char *res = ejecutar(fuente, "_x", &err);
    if (res) {
        fprintf(stderr, "FALLO: '%s' debia dar error pero ejecuto\n", fuente);
        fallos++;
        return;
    }
    if (!err || !strstr(err, substring)) {
        fprintf(stderr, "FALLO: '%s' dio '%s' (esperaba '%s')\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

static void test_round_trip(void) {
    const char *ruta = ruta_tmp();
    char fuente[1024];
    snprintf(fuente, sizeof(fuente),
        "archivo_escribir(\"%s\", \"hola mundo\")\n"
        "x = archivo_leer(\"%s\")", ruta, ruta);
    verificar_var(fuente, "x", "hola mundo");
    remove(ruta);
}

static void test_existe(void) {
    const char *ruta = ruta_tmp();
    char fuente[1024];
    /* Antes de escribir: no existe. */
    snprintf(fuente, sizeof(fuente),
        "x = archivo_existe(\"%s\")", ruta);
    verificar_var(fuente, "x", "falso");
    /* Después de escribir: existe. */
    snprintf(fuente, sizeof(fuente),
        "archivo_escribir(\"%s\", \"\")\n"
        "x = archivo_existe(\"%s\")", ruta, ruta);
    verificar_var(fuente, "x", "verdadero");
    remove(ruta);
}

static void test_lineas(void) {
    const char *ruta = ruta_tmp();
    char fuente[1024];
    snprintf(fuente, sizeof(fuente),
        "archivo_escribir(\"%s\", \"a\\nb\\nc\")\n"
        "_l = archivo_lineas(\"%s\")\n"
        "x = longitud(_l)", ruta, ruta);
    verificar_var(fuente, "x", "3");
    /* Línea final con \n: split incluye solo 3 (no agrega vacía al final). */
    snprintf(fuente, sizeof(fuente),
        "archivo_escribir(\"%s\", \"a\\nb\\nc\\n\")\n"
        "_l = archivo_lineas(\"%s\")\n"
        "x = longitud(_l)", ruta, ruta);
    verificar_var(fuente, "x", "3");
    remove(ruta);
}

static void test_agregar(void) {
    const char *ruta = ruta_tmp();
    char fuente[1024];
    snprintf(fuente, sizeof(fuente),
        "archivo_escribir(\"%s\", \"a\")\n"
        "archivo_agregar(\"%s\", \"b\")\n"
        "archivo_agregar(\"%s\", \"c\")\n"
        "x = archivo_leer(\"%s\")", ruta, ruta, ruta, ruta);
    verificar_var(fuente, "x", "abc");
    remove(ruta);
}

static void test_errores_tipo(void) {
    verificar_error("x = archivo_leer(42)", "espera una cadena");
    verificar_error("x = archivo_escribir(\"a\", 42)", "contenido como cadena");
    verificar_error("x = archivo_existe()", "requiere 1 argumento");
    verificar_error("x = archivo_lineas([])", "espera una cadena");
}

/*
 * test_modulo retirado: cuando ctest ejecuta el binario de tests,
 * el working directory es `build/tests/` y `importar archivos`
 * busca relativo a CWD. El módulo se valida end-to-end vía
 * `bc_run_31_archivos` integración (que sí corre con WORKING_DIRECTORY
 * apuntando al repo root).
 */

int main(void) {
    test_round_trip();
    test_existe();
    test_lineas();
    test_agregar();
    test_errores_tipo();

    if (fallos == 0) {
        printf("archivos: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "archivos: %d fallo(s)\n", fallos);
    return 1;
}
