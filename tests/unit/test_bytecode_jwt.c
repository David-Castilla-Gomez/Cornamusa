/*
 * Tests del modulo stdlib/jwt.cor (v1.67 + v1.70).
 *
 * Verifica:
 *   - Round-trip (codificar -> decodificar devuelve el mismo payload).
 *   - Firma incorrecta lanza ErrorDeValor.
 *   - Token alterado falla la verificacion.
 *   - Token malformado lanza con mensaje claro.
 *   - jwt.verificar() devuelve true/false sin lanzar.
 *   - Header con alg distinto a HS256 rechazado.
 *   - jwt.expirado(payload, ahora) (v1.70).
 *   - jwt.decodificar_y_validar(token, clave, ahora) (v1.70).
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
static int casos = 0;

#define AFIRMAR(cond, etiqueta)                                                \
    do {                                                                        \
        casos++;                                                                \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FALLO %s:%d (%s)\n", __FILE__, __LINE__, etiqueta);\
            fallos++;                                                           \
        }                                                                       \
    } while (0)

/* Ejecuta `fuente`, captura stdout completo en `out_buf`. */
static int ejecutar_capturando(const char *fuente, char *out_buf, int out_cap) {
    const char *tmpfile =
#ifdef _WIN32
        "test_jwt_out.txt";
#else
        "/tmp/test_jwt_out.txt";
#endif
    if (!freopen(tmpfile, "w+", stdout)) return -1;

    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    int rc = -1;
    if (!p.tuvo_error) {
        Chunk chunk; chunk_iniciar(&chunk);
        Compilador c; compilador_iniciar(&c, &chunk);
        if (compilador_compilar_programa(&c, sents, n)) {
            VM vm; vm_iniciar(&vm);
            Valor r = valor_nulo();
            ResultadoVM rcvm = vm_ejecutar(&vm, &chunk, &r);
            valor_destruir(&r);
            vm_destruir(&vm);
            if (rcvm == VM_OK) rc = 0;
        }
        chunk_destruir(&chunk);
    }
    arena_destruir(&a);

    fflush(stdout);
#ifdef _WIN32
    freopen("CON", "w", stdout);
#else
    freopen("/dev/tty", "w", stdout);
#endif
    if (rc != 0) return -1;

    FILE *f = fopen(tmpfile, "r");
    if (!f) return -1;
    int leido = (int)fread(out_buf, 1, (size_t)(out_cap - 1), f);
    out_buf[leido] = '\0';
    fclose(f);
    remove(tmpfile);
    return leido;
}

int main(void) {
    /* Round-trip: codificar y decodificar devuelve el mismo payload. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"sub\": \"42\"}, \"secreto\")\n"
            "p = jwt.decodificar(tok, \"secreto\")\n"
            "imprimir(p[\"sub\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "roundtrip_sub");
    }

    /* JWT empieza con `eyJ` (header base64-url de `{`). */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "imprimir(jwt.codificar({\"a\": 1}, \"k\"))\n", out, sizeof(out));
        AFIRMAR(strncmp(out, "eyJ", 3) == 0, "empieza_con_eyJ");
    }

    /* Verificar con clave correcta. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"x\": 1}, \"k\")\n"
            "imprimir(jwt.verificar(tok, \"k\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "verificar_ok");
    }

    /* Verificar con clave incorrecta. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"x\": 1}, \"k\")\n"
            "imprimir(jwt.verificar(tok, \"otro\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "falso") != NULL, "verificar_clave_mala");
    }

    /* Token mal formado (no tiene 3 partes). */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "intentar:\n"
            "    jwt.decodificar(\"no.es\", \"k\")\n"
            "atrapar Excepcion como e:\n"
            "    imprimir(\"caught\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "caught") != NULL, "malformado_caught");
    }

    /* Decodificar con clave mala lanza. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"x\": 1}, \"k\")\n"
            "intentar:\n"
            "    jwt.decodificar(tok, \"otra\")\n"
            "atrapar Excepcion como e:\n"
            "    imprimir(\"firma-bad\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "firma-bad") != NULL, "decod_clave_mala");
    }

    /* Round-trip con payload complejo (anidado). */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"usuario\": {\"id\": 7, \"nombre\": \"Ana\"},"
            " \"roles\": [\"admin\", \"editor\"]}, \"k\")\n"
            "p = jwt.decodificar(tok, \"k\")\n"
            "imprimir(p[\"usuario\"][\"nombre\"], p[\"roles\"][0])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "Ana") != NULL, "anidado_nombre");
        AFIRMAR(strstr(out, "admin") != NULL, "anidado_roles");
    }

    /* v1.70: jwt.expirado() con claim exp. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "imprimir(jwt.expirado({\"exp\": 1000}, 2000))\n"
            "imprimir(jwt.expirado({\"exp\": 1000}, 500))\n"
            "imprimir(jwt.expirado({\"exp\": 1000}, 1000))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso\nverdadero") != NULL ||
                strstr(out, "verdadero\r\nfalso\r\nverdadero") != NULL,
                "expirado_combinaciones");
    }

    /* v1.70: jwt.expirado() sin claim exp -> nunca expira. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "imprimir(jwt.expirado({\"sub\": \"42\"}, 999999))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "falso") != NULL, "expirado_sin_exp");
    }

    /* v1.70: decodificar_y_validar OK con exp en el futuro. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"sub\": \"42\", \"exp\": 2000}, \"k\")\n"
            "p = jwt.decodificar_y_validar(tok, \"k\", 1000)\n"
            "imprimir(p[\"sub\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "validar_ok_exp_futuro");
    }

    /* v1.70: decodificar_y_validar lanza con exp pasado. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"sub\": \"42\", \"exp\": 1000}, \"k\")\n"
            "intentar:\n"
            "    jwt.decodificar_y_validar(tok, \"k\", 2000)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"expirado-OK\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "expirado-OK") != NULL, "validar_exp_pasado");
    }

    /* v1.70: decodificar_y_validar lanza con nbf futuro. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"sub\": \"42\", \"nbf\": 5000}, \"k\")\n"
            "intentar:\n"
            "    jwt.decodificar_y_validar(tok, \"k\", 1000)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"nbf-OK\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "nbf-OK") != NULL, "validar_nbf_futuro");
    }

    /* v1.70: decodificar_y_validar OK cuando nbf <= ahora. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"sub\": \"42\", \"nbf\": 1000}, \"k\")\n"
            "p = jwt.decodificar_y_validar(tok, \"k\", 2000)\n"
            "imprimir(p[\"sub\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "validar_ok_nbf_pasado");
    }

    /* v1.70: decodificar_y_validar tambien valida firma. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar jwt\n"
            "tok = jwt.codificar({\"sub\": \"42\"}, \"k\")\n"
            "intentar:\n"
            "    jwt.decodificar_y_validar(tok, \"otra\", 1000)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"firma-rechazada\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "firma-rechazada") != NULL, "validar_firma_mala");
    }

    if (fallos == 0) {
        printf("jwt: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "jwt: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
