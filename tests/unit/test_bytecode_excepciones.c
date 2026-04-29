/*
 * Tests del bytecode con excepciones — v0.6.3.
 *
 * Cubre:
 *   - VAL_EXCEPCION: construcción via Excepcion(clase, mensaje) y
 *     atajos (ErrorAritmetico, ErrorDeTipo, etc.).
 *   - `lanzar` excepción con valor.
 *   - `intentar`/`atrapar` con alias (`atrapar Tipo como e:`).
 *   - Excepciones lanzadas dentro de funciones, propagación al
 *     llamador.
 *   - Excepciones no atrapadas → error en VM.
 *   - Anidamiento de `intentar`.
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

/* ───── Construcción de excepciones ───── */

static void test_construir_excepciones(void) {
    /* Excepcion(clase, mensaje) genérica. */
    verificar_var("e = Excepcion(\"MiError\", \"algo fallo\")\nx = tipo(e)",
                  "x", "excepcion");

    /* ErrorAritmetico construye con clase prerrellenada. */
    verificar_var(
        "e = ErrorAritmetico(\"div por cero\")\n"
        "x = e",
        "x", "ErrorAritmetico: div por cero");

    /* Otros tipos. */
    verificar_var(
        "x = ErrorDeTipo(\"tipo malo\")",
        "x", "ErrorDeTipo: tipo malo");
}

/* ───── lanzar + intentar/atrapar ───── */

static void test_atrapar_simple(void) {
    /* lanzar ... atrapar (con alias) ... fin intentar. */
    verificar_var(
        "atrapado = falso\n"
        "intentar:\n"
        "    lanzar ErrorAritmetico(\"oops\")\n"
        "atrapar Excepcion como e:\n"
        "    atrapado = verdadero\n"
        "fin intentar",
        "atrapado", "verdadero");
}

static void test_atrapar_con_mensaje(void) {
    /* El alias `e` se asigna correctamente y lleva la excepción. */
    verificar_var(
        "intentar:\n"
        "    lanzar ErrorAritmetico(\"div por cero\")\n"
        "atrapar Excepcion como e:\n"
        "    msg = e\n"
        "fin intentar",
        "msg", "ErrorAritmetico: div por cero");
}

static void test_no_se_lanza(void) {
    /* Si el cuerpo no lanza, el atrapar no se ejecuta. */
    verificar_var(
        "atrapado = falso\n"
        "ejecutado = falso\n"
        "intentar:\n"
        "    ejecutado = verdadero\n"
        "atrapar Excepcion como e:\n"
        "    atrapado = verdadero\n"
        "fin intentar",
        "atrapado", "falso");
}

/* ───── Excepciones en funciones ───── */

static void test_excepcion_en_funcion(void) {
    /* La excepción lanzada dentro de una función propaga al
       llamador y es atrapable allí. */
    verificar_var(
        "funcion arriesgar():\n"
        "    lanzar ErrorAritmetico(\"falla en func\")\n"
        "fin funcion\n"
        "atrapado = falso\n"
        "intentar:\n"
        "    arriesgar()\n"
        "atrapar Excepcion como e:\n"
        "    atrapado = verdadero\n"
        "fin intentar",
        "atrapado", "verdadero");
}

static void test_dividir_robusto(void) {
    /* Programa típico: función que valida y lanza si error. */
    verificar_var(
        "funcion dividir(a, b):\n"
        "    intentar:\n"
        "        si b == 0:\n"
        "            lanzar ErrorAritmetico(\"div por cero\")\n"
        "        fin si\n"
        "        retornar a / b\n"
        "    atrapar Excepcion como e:\n"
        "        retornar nulo\n"
        "    fin intentar\n"
        "fin funcion\n"
        "x = dividir(10, 0)",
        "x", "nulo");

    verificar_var(
        "funcion dividir(a, b):\n"
        "    intentar:\n"
        "        si b == 0:\n"
        "            lanzar ErrorAritmetico(\"div por cero\")\n"
        "        fin si\n"
        "        retornar a / b\n"
        "    atrapar Excepcion como e:\n"
        "        retornar nulo\n"
        "    fin intentar\n"
        "fin funcion\n"
        "x = dividir(10, 4)",
        "x", "2.5");
}

/* ───── Excepciones no atrapadas ───── */

static void test_no_atrapada(void) {
    /* Una excepción sin atrapador propaga al cliente como error. */
    verificar_error(
        "lanzar ErrorAritmetico(\"sin atrapar\")",
        "ErrorAritmetico: sin atrapar");

    /* Excepción dentro de función sin intentar también propaga. */
    verificar_error(
        "funcion fallar():\n"
        "    lanzar ErrorDeValor(\"fallo\")\n"
        "fin funcion\n"
        "fallar()",
        "ErrorDeValor: fallo");
}

/* ───── Anidamiento de intentar ───── */

static void test_anidamiento(void) {
    /* Un atrapar interno atrapa. */
    verificar_var(
        "atrapado_interno = falso\n"
        "atrapado_externo = falso\n"
        "intentar:\n"
        "    intentar:\n"
        "        lanzar ErrorAritmetico(\"x\")\n"
        "    atrapar Excepcion como e:\n"
        "        atrapado_interno = verdadero\n"
        "    fin intentar\n"
        "atrapar Excepcion como e:\n"
        "    atrapado_externo = verdadero\n"
        "fin intentar",
        "atrapado_interno", "verdadero");

    /* Si el interno no atrapa (re-lanza), el externo sí. */
    verificar_var(
        "atrapado_externo = falso\n"
        "intentar:\n"
        "    intentar:\n"
        "        lanzar ErrorAritmetico(\"x\")\n"
        "    atrapar Excepcion como e:\n"
        "        lanzar ErrorDeTipo(\"otro\")\n"
        "    fin intentar\n"
        "atrapar Excepcion como e:\n"
        "    atrapado_externo = verdadero\n"
        "fin intentar",
        "atrapado_externo", "verdadero");
}

/* ───── Lanzar cadena (azúcar) ───── */

static void test_lanzar_cadena(void) {
    /* `lanzar "msg"` sin clase específica se envuelve como
       Excepcion("Excepcion", "msg"). */
    verificar_var(
        "intentar:\n"
        "    lanzar \"algo\"\n"
        "atrapar Excepcion como e:\n"
        "    msg = e\n"
        "fin intentar",
        "msg", "Excepcion: algo");
}

/* ───── v0.8.3: atrapar Tipo, sino, finalmente, lanzar re-raise ───── */

static void test_atrapar_por_tipo(void) {
    /* atrapar Tipo solo coincide si excepcion.clase == Tipo. */
    verificar_var(
        "intentar:\n"
        "    lanzar ErrorAritmetico(\"div\")\n"
        "atrapar ErrorDeTipo como e:\n"
        "    msg = \"tipo\"\n"
        "atrapar ErrorAritmetico como e:\n"
        "    msg = \"aritmetico\"\n"
        "fin intentar",
        "msg", "aritmetico");
}

static void test_atrapar_excepcion_atrapa_todo(void) {
    /* `atrapar Excepcion` atrapa cualquier tipo (genérico). */
    verificar_var(
        "intentar:\n"
        "    lanzar ErrorDeTipo(\"x\")\n"
        "atrapar Excepcion como e:\n"
        "    msg = \"atrapado\"\n"
        "fin intentar",
        "msg", "atrapado");
}

static void test_atrapar_sin_match_propaga(void) {
    /* Si ningún atrapador coincide, la excepción se propaga. */
    verificar_error(
        "intentar:\n"
        "    lanzar ErrorAritmetico(\"x\")\n"
        "atrapar ErrorDeTipo como e:\n"
        "    msg = \"tipo\"\n"
        "fin intentar",
        "ErrorAritmetico");
}

static void test_sino(void) {
    /* sino solo se ejecuta si NO hubo excepción. */
    verificar_var(
        "ejecutado = falso\n"
        "intentar:\n"
        "    x = 42\n"
        "atrapar Excepcion como e:\n"
        "    pasar\n"
        "sino:\n"
        "    ejecutado = verdadero\n"
        "fin intentar",
        "ejecutado", "verdadero");

    /* sino NO se ejecuta si hubo excepción. */
    verificar_var(
        "ejecutado = falso\n"
        "intentar:\n"
        "    lanzar ErrorAritmetico(\"x\")\n"
        "atrapar Excepcion como e:\n"
        "    pasar\n"
        "sino:\n"
        "    ejecutado = verdadero\n"
        "fin intentar",
        "ejecutado", "falso");
}

static void test_finalmente(void) {
    /* finalmente se ejecuta tras salida limpia. */
    verificar_var(
        "ejecutado = falso\n"
        "intentar:\n"
        "    x = 1\n"
        "finalmente:\n"
        "    ejecutado = verdadero\n"
        "fin intentar",
        "ejecutado", "verdadero");

    /* finalmente se ejecuta tras un atrapar exitoso. */
    verificar_var(
        "ejecutado = falso\n"
        "intentar:\n"
        "    lanzar ErrorAritmetico(\"x\")\n"
        "atrapar Excepcion como e:\n"
        "    pasar\n"
        "finalmente:\n"
        "    ejecutado = verdadero\n"
        "fin intentar",
        "ejecutado", "verdadero");
}

static void test_lanzar_reraise(void) {
    /* Dentro de un atrapar con alias, `lanzar` sin valor re-emite la
       excepción capturada para que el siguiente nivel la atrape. */
    verificar_var(
        "funcion fallar():\n"
        "    intentar:\n"
        "        lanzar ErrorDeTipo(\"profunda\")\n"
        "    atrapar Excepcion como e:\n"
        "        lanzar\n"
        "    fin intentar\n"
        "fin funcion\n"
        "intentar:\n"
        "    fallar()\n"
        "atrapar Excepcion como e:\n"
        "    msg = e\n"
        "fin intentar",
        "msg", "ErrorDeTipo: profunda");
}

static void test_lanzar_reraise_sin_alias_es_error(void) {
    /* lanzar sin valor sin un alias activo es error de compilación. */
    verificar_error(
        "lanzar",
        "solo es valido dentro de");
}

static void test_intentar_blocks_repetidos(void) {
    /* Múltiples bloques intentar en top-level no deben contaminarse:
       el segundo bloque debe ver SU propia excepción, no la primera. */
    verificar_var(
        "intentar:\n"
        "    lanzar ErrorAritmetico(\"primera\")\n"
        "atrapar Excepcion como e:\n"
        "    msg1 = e\n"
        "fin intentar\n"
        "intentar:\n"
        "    lanzar ErrorDeTipo(\"segunda\")\n"
        "atrapar Excepcion como e:\n"
        "    msg2 = e\n"
        "fin intentar\n"
        "x = msg2",
        "x", "ErrorDeTipo: segunda");
}

int main(void) {
    test_construir_excepciones();
    test_atrapar_simple();
    test_atrapar_con_mensaje();
    test_no_se_lanza();
    test_excepcion_en_funcion();
    test_dividir_robusto();
    test_no_atrapada();
    test_anidamiento();
    test_lanzar_cadena();
    test_atrapar_por_tipo();
    test_atrapar_excepcion_atrapa_todo();
    test_atrapar_sin_match_propaga();
    test_sino();
    test_finalmente();
    test_lanzar_reraise();
    test_lanzar_reraise_sin_alias_es_error();
    test_intentar_blocks_repetidos();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode con excepciones pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
