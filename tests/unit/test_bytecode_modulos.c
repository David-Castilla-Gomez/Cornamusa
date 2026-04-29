/*
 * Tests del sistema de módulos (v0.9.0).
 *
 * Cubre:
 *   - `importar X` carga el archivo y lo registra como global.
 *   - Acceso a atributos: `X.f()` invoca función del módulo, `X.K`
 *     accede a constante.
 *   - Cache: importar el mismo módulo dos veces no re-ejecuta.
 *   - Errores: módulo inexistente, atributo inexistente.
 *   - Limitaciones documentadas: subsegmentos / alias rechazados.
 *
 * Los archivos de los módulos de prueba se crean en disco antes de
 * ejecutar (en la cwd de los tests, que es build_v2/tests/) y se
 * borran al final con remove().
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

static const char *ejecutar(const char *fuente, const char *nombre_var,
                              const char **error_out) {
    static char buffer[2048];
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 16384);
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
    if (nombre_var == NULL) {
        buffer[0] = '\0';
    } else {
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
    }
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
        fprintf(stderr, "FALLO en programa:\n%s\n  error: %s\n",
                fuente, err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO en programa:\n%s\n  esperaba %s=%s\n  obtuvo: %s\n",
                fuente, var, esperado, res);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, NULL, &err);
    if (res) {
        fprintf(stderr, "FALLO: programa debería dar error pero ejecutó:\n%s\n",
                fuente);
        fallos++;
        return;
    }
    if (!err || !strstr(err, substring)) {
        fprintf(stderr, "FALLO: '%s' dio '%s' pero se esperaba '%s'\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

/* Crea un archivo .cor temporal para usarlo como módulo. */
static void escribir_modulo(const char *nombre, const char *contenido) {
    char path[256];
    snprintf(path, sizeof(path), "%s.cor", nombre);
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "FALLO: no se pudo crear %s\n", path);
        fallos++;
        return;
    }
    fputs(contenido, f);
    fclose(f);
}

static void borrar_modulo(const char *nombre) {
    char path[256];
    snprintf(path, sizeof(path), "%s.cor", nombre);
    remove(path);
}

/* ───── importar simple ───── */

static void test_importar_constante(void) {
    escribir_modulo("modtest_const", "K = 42\n");
    verificar_var(
        "importar modtest_const\n"
        "z = modtest_const.K",
        "z", "42");
    borrar_modulo("modtest_const");
}

static void test_importar_funcion(void) {
    escribir_modulo("modtest_fn",
        "funcion suma(a, b):\n"
        "    retornar a + b\n"
        "fin funcion\n");
    verificar_var(
        "importar modtest_fn\n"
        "z = modtest_fn.suma(3, 4)",
        "z", "7");
    borrar_modulo("modtest_fn");
}

static void test_importar_modulo_no_existe(void) {
    verificar_error(
        "importar modulo_que_no_existe_jamas_xyz",
        "ErrorDeImportacion");
}

static void test_atributo_inexistente(void) {
    escribir_modulo("modtest_attr", "X = 1\n");
    verificar_error(
        "importar modtest_attr\n"
        "z = modtest_attr.no_existe",
        "ErrorDeAtributo");
    borrar_modulo("modtest_attr");
}

/* ───── tipo() de modulo ───── */

static void test_tipo_modulo(void) {
    escribir_modulo("modtest_tipo", "X = 1\n");
    verificar_var(
        "importar modtest_tipo\n"
        "z = tipo(modtest_tipo)",
        "z", "modulo");
    borrar_modulo("modtest_tipo");
}

/* ───── cache (importar dos veces no re-ejecuta) ───── */

static void test_cache(void) {
    /* Importar el mismo módulo dos veces en el mismo programa: el
       segundo debe pegar al cache y registrar la misma instancia. */
    escribir_modulo("modtest_cache", "K = 100\n");
    verificar_var(
        "importar modtest_cache\n"
        "importar modtest_cache\n"
        "z = modtest_cache.K",
        "z", "100");
    borrar_modulo("modtest_cache");
}

/* ───── limitaciones documentadas ───── */

static void test_subsegmentos_rechazado(void) {
    verificar_error(
        "importar mat.geometria",
        "subsegmentos");
}

static void test_alias_rechazado(void) {
    verificar_error(
        "importar mat como m",
        "alias de modulo");
}

/* ───── globales del módulo no contaminan importador ───── */

static void test_aislamiento_globales(void) {
    /* El módulo define `secreto`. El importador no debe verlo
       directamente — solo via modulo.secreto. */
    escribir_modulo("modtest_aislado", "secreto = 999\n");
    verificar_error(
        "importar modtest_aislado\n"
        "z = secreto",   /* sin prefijo: ErrorDeNombre */
        "no esta definido");
    borrar_modulo("modtest_aislado");
}

int main(void) {
    test_importar_constante();
    test_importar_funcion();
    test_importar_modulo_no_existe();
    test_atributo_inexistente();
    test_tipo_modulo();
    test_cache();
    test_subsegmentos_rechazado();
    test_alias_rechazado();
    test_aislamiento_globales();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode con modulos pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
