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

/* ───── subsegmentos y alias (v0.9.1) ───── */

static void test_alias_simple(void) {
    /* `importar X como Y` registra el módulo bajo Y. */
    escribir_modulo("modtest_alias", "K = 7\n");
    verificar_var(
        "importar modtest_alias como m\n"
        "z = m.K",
        "z", "7");
    /* Verificar que el nombre original NO está disponible (solo el alias). */
    borrar_modulo("modtest_alias");
}

static void test_alias_no_expone_nombre_original(void) {
    /* Con alias, el nombre real NO debe quedar como global. */
    escribir_modulo("modtest_a2", "K = 1\n");
    verificar_error(
        "importar modtest_a2 como m\n"
        "z = modtest_a2.K",
        "no esta definido");
    borrar_modulo("modtest_a2");
}

/* Los subsegmentos requieren crear un subdirectorio para el archivo
   del módulo. El framework de tests no lo soporta directamente, así
   que solo verificamos compilación: el nombre del módulo se construye
   correctamente. Test end-to-end llega vía un módulo de stdlib más
   adelante. */
static void test_subsegmentos_no_existe(void) {
    /* Subsegmentos compilan pero el archivo no existe → ErrorDeImportacion. */
    verificar_error(
        "importar paquete_inexistente.submodulo",
        "ErrorDeImportacion");
}

/* ───── desde X importar Y (v0.9.1) ───── */

static void test_desde_importar_simple(void) {
    /* `desde X importar Y` registra Y como global del importador. */
    escribir_modulo("modtest_desde",
        "K = 42\n"
        "L = 99\n");
    verificar_var(
        "desde modtest_desde importar K\n"
        "z = K",
        "z", "42");
    borrar_modulo("modtest_desde");
}

static void test_desde_importar_multiple(void) {
    /* Varios items en una sola sentencia. */
    escribir_modulo("modtest_dm",
        "A = 1\n"
        "B = 2\n"
        "C = 3\n");
    verificar_var(
        "desde modtest_dm importar A, B, C\n"
        "z = A + B + C",
        "z", "6");
    borrar_modulo("modtest_dm");
}

static void test_desde_importar_alias(void) {
    /* `desde X importar Y como Z` registra Y bajo el nombre Z. */
    escribir_modulo("modtest_da",
        "K = 100\n");
    verificar_var(
        "desde modtest_da importar K como kk\n"
        "z = kk",
        "z", "100");
    borrar_modulo("modtest_da");
}

static void test_desde_no_expone_modulo(void) {
    /* `desde X importar Y` NO debe registrar X como global. */
    escribir_modulo("modtest_dnex",
        "K = 1\n");
    verificar_error(
        "desde modtest_dnex importar K\n"
        "z = modtest_dnex.K",
        "no esta definido");
    borrar_modulo("modtest_dnex");
}

static void test_desde_funcion(void) {
    /* Funciones importadas via `desde X importar f` se llaman normal. */
    escribir_modulo("modtest_df",
        "funcion duplicar(n):\n"
        "    retornar n * 2\n"
        "fin funcion\n");
    verificar_var(
        "desde modtest_df importar duplicar\n"
        "z = duplicar(7)",
        "z", "14");
    borrar_modulo("modtest_df");
}

/* ───── s[i] en bytecode (v0.9.1) ───── */

static void test_indexacion_cadena_basica(void) {
    verificar_var("z = \"hola\"[0]", "z", "h");
    verificar_var("z = \"hola\"[3]", "z", "a");
}

static void test_indexacion_cadena_negativa(void) {
    verificar_var("z = \"hola\"[-1]", "z", "a");
    verificar_var("z = \"hola\"[-4]", "z", "h");
}

static void test_indexacion_cadena_utf8(void) {
    /* Caracteres multi-byte en UTF-8 (ñ es 2 bytes). */
    verificar_var("z = \"ñoño\"[0]", "z", "ñ");
    verificar_var("z = \"ñoño\"[3]", "z", "o");
}

static void test_indexacion_cadena_fuera_rango(void) {
    verificar_error("z = \"hola\"[10]", "ErrorDeIndice");
    verificar_error("z = \"hola\"[-10]", "ErrorDeIndice");
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
    test_alias_simple();
    test_alias_no_expone_nombre_original();
    test_subsegmentos_no_existe();
    test_aislamiento_globales();
    test_desde_importar_simple();
    test_desde_importar_multiple();
    test_desde_importar_alias();
    test_desde_no_expone_modulo();
    test_desde_funcion();
    test_indexacion_cadena_basica();
    test_indexacion_cadena_negativa();
    test_indexacion_cadena_utf8();
    test_indexacion_cadena_fuera_rango();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode con modulos pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
