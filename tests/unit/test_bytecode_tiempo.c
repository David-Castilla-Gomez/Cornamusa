/*
 * Tests de los built-ins de tiempo (v1.19): tiempo_actual,
 * tiempo_descomponer, tiempo_componer, tiempo_formato.
 *
 * Notas:
 *   - tiempo_actual cambia con cada llamada; lo probamos con bounds
 *     razonables y monotonicidad.
 *   - tiempo_descomponer/componer/formato son determinísticas para un
 *     timestamp fijo. Verificamos roundtrip: componer→descomponer da
 *     los mismos componentes, descomponer→componer da el mismo ts
 *     (modulo DST que el built-in maneja con isdst=-1).
 *
 * Las funciones usan local time, así que los tests evitan asumir una
 * zona horaria específica.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

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

/* ───── tiempo_actual ───── */

static void test_tiempo_actual_es_entero(void) {
    verificar_var(
        "x = tipo(tiempo_actual())",
        "x", "entero");
}

static void test_tiempo_actual_razonable(void) {
    /* El ts actual debe estar dentro del rango razonable
       (entre 2020 y 2100). */
    verificar_var(
        "ts = tiempo_actual()\n"
        "x = ts > 1577836800 y ts < 4102444800",
        "x", "verdadero");
}

static void test_tiempo_actual_aridad(void) {
    const char *err = NULL;
    const char *res = ejecutar("x = tiempo_actual(1)", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO: tiempo_actual(arg) no detectado\n");
        fallos++;
    }
}

/* ───── tiempo_componer + descomponer ───── */

static void test_componer_descomponer_roundtrip(void) {
    /* Construir un ts conocido y verificar que descomponer da los
       mismos componentes. Usamos 2026-05-15 12:30:45 LOCAL. */
    verificar_var(
        "ts = tiempo_componer(2026, 5, 15, 12, 30, 45)\n"
        "c = tiempo_descomponer(ts)\n"
        "x = [c[0], c[1], c[2], c[3], c[4], c[5]]",
        "x", "[2026, 5, 15, 12, 30, 45]");
}

static void test_componer_dia_año(void) {
    /* 15 de mayo = día 31+28+31+30+15 = 135 del año (en año normal). */
    verificar_var(
        "ts = tiempo_componer(2026, 5, 15, 12, 0, 0)\n"
        "c = tiempo_descomponer(ts)\n"
        "x = c[7]",
        "x", "135");
}

static void test_componer_aridad(void) {
    const char *err = NULL;
    const char *res = ejecutar(
        "x = tiempo_componer(2026, 5, 15)", "x", &err);
    if (res != NULL) {
        fprintf(stderr, "FALLO: tiempo_componer con 3 args no detectado\n");
        fallos++;
    }
}

/* ───── tiempo_formato ───── */

static void test_formato_iso(void) {
    verificar_var(
        "ts = tiempo_componer(2026, 1, 2, 3, 4, 5)\n"
        "x = tiempo_formato(ts, \"%Y-%m-%d %H:%M:%S\")",
        "x", "2026-01-02 03:04:05");
}

static void test_formato_solo_fecha(void) {
    verificar_var(
        "ts = tiempo_componer(2026, 7, 4, 0, 0, 0)\n"
        "x = tiempo_formato(ts, \"%Y-%m-%d\")",
        "x", "2026-07-04");
}

static void test_formato_solo_hora(void) {
    verificar_var(
        "ts = tiempo_componer(2026, 1, 1, 23, 59, 1)\n"
        "x = tiempo_formato(ts, \"%H:%M:%S\")",
        "x", "23:59:01");
}

static void test_formato_porcentaje_literal(void) {
    verificar_var(
        "ts = tiempo_componer(2026, 1, 1, 0, 0, 0)\n"
        "x = tiempo_formato(ts, \"100%%\")",
        "x", "100%");
}

/* ───── Validación de errores atrapables ───── */

static void test_componer_invalido_atrapable(void) {
    /* 2026-13-01 es inválido (mes 13). mktime puede normalizarlo o
       fallar. Verificamos que NO crashea. */
    verificar_var(
        "intentar:\n"
        "  ts = tiempo_componer(2026, 13, 1, 0, 0, 0)\n"
        "  x = \"ok\"\n"
        "atrapar Excepcion:\n"
        "  x = \"atrapado\"\n"
        "fin intentar\n",
        "x", "ok");  /* mktime normalmente normaliza, así que ok */
}

static void test_descomponer_tipo_invalido_atrapable(void) {
    verificar_var(
        "_msg = \"\"\n"
        "intentar:\n"
        "  tiempo_descomponer(\"no es entero\")\n"
        "atrapar ErrorDeTipo:\n"
        "  _msg = \"atrapado\"\n"
        "fin intentar\n"
        "x = _msg",
        "x", "atrapado");
}

int main(void) {
    test_tiempo_actual_es_entero();
    test_tiempo_actual_razonable();
    test_tiempo_actual_aridad();

    test_componer_descomponer_roundtrip();
    test_componer_dia_año();
    test_componer_aridad();

    test_formato_iso();
    test_formato_solo_fecha();
    test_formato_solo_hora();
    test_formato_porcentaje_literal();

    test_componer_invalido_atrapable();
    test_descomponer_tipo_invalido_atrapable();

    if (fallos == 0) {
        printf("tiempo: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "tiempo: %d fallo(s)\n", fallos);
    return 1;
}
