/*
 * Tests de red (v1.29) — cliente HTTP.
 *
 * NOTA: estos tests verifican el manejo de errores y argumentos del
 * built-in `red_http_obtener` sin requerir conexión real a internet.
 *
 * Pruebas de HTTP real están en `examples/54_red.cor` y se ejecutan
 * solo si la variable de entorno CORNAMUSA_TEST_RED está activada
 * (configurable en CI).
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

/* ───── Validación de argumentos ───── */

static void test_url_no_http(void) {
    verificar_var("URL no http://",
        "intentar:\n"
        "  red_http_obtener(\"ftp://servidor/archivo\")\n"
        "atrapar ErrorDeSistema como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_url_https(void) {
    verificar_var("HTTPS no soportado",
        "intentar:\n"
        "  red_http_obtener(\"https://example.com/\")\n"
        "atrapar ErrorDeSistema como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_url_no_cadena(void) {
    verificar_var("url debe ser cadena",
        "intentar:\n"
        "  red_http_obtener(42)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_sin_args(void) {
    verificar_var("sin args",
        "intentar:\n"
        "  red_http_obtener()\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_dns_fallo(void) {
    verificar_var("host inexistente",
        "intentar:\n"
        "  red_http_obtener(\"http://host.que.no.existe.cornamusa.xyz/\")\n"
        "atrapar ErrorDeSistema como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

static void test_timeout_invalido(void) {
    verificar_var("timeout debe ser entero positivo",
        "intentar:\n"
        "  red_http_obtener(\"http://example.com/\", nulo, -1)\n"
        "atrapar ErrorDeTipo como e:\n"
        "  x = \"atrapado\"\n"
        "fin intentar",
        "x", "atrapado");
}

int main(void) {
    test_url_no_http();
    test_url_https();
    test_url_no_cadena();
    test_sin_args();
    test_dns_fallo();
    test_timeout_invalido();

    if (fallos == 0) {
        printf("red: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "red: %d fallo(s)\n", fallos);
    return 1;
}
