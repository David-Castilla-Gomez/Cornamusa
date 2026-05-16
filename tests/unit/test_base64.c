/*
 * Tests unitarios de base64 (v1.59).
 *
 * Valida los test vectors de RFC 4648 §10 mas casos edge.
 * Ejecuta `r = expr` y compara la global `r` con el valor esperado.
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

/* Ejecuta `fuente`, extrae la global `r` (debe ser cadena) y la
 * copia a `out_buf`. Devuelve longitud o -1 en error. */
static int ejecutar_r(const char *fuente, char *out_buf, int out_cap) {
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 4096);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    if (p.tuvo_error) { arena_destruir(&a); return -1; }
    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, sents, n)) {
        chunk_destruir(&chunk); arena_destruir(&a); return -1;
    }
    VM vm; vm_iniciar(&vm);
    Valor resultado = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &resultado);
    int len = -1;
    if (rc == VM_OK) {
        Valor nombre = valor_cadena_referencia("r", 1);
        Valor v;
        if (dicc_obtener(vm.globales, &nombre, &v)) {
            if (v.tipo == VAL_CADENA) {
                int l_str = v.como.cadena.longitud;
                if (l_str < out_cap) {
                    memcpy(out_buf, v.como.cadena.texto, (size_t)l_str);
                    out_buf[l_str] = '\0';
                    len = l_str;
                }
            }
            valor_destruir(&v);
        }
    }
    valor_destruir(&resultado);
    vm_destruir(&vm);
    chunk_destruir(&chunk);
    arena_destruir(&a);
    return len;
}

/* Verifica que `base64_codificar(in) == esperado`. */
static void chequear_cod(const char *in, const char *esperado, const char *etiqueta) {
    char prog[512];
    snprintf(prog, sizeof(prog), "r = base64_codificar(\"%s\")\n", in);
    char out[512];
    int n = ejecutar_r(prog, out, sizeof(out));
    if (n < 0) {
        fprintf(stderr, "  '%s': ejecutar fallo\n", etiqueta);
        AFIRMAR(false, etiqueta);
        return;
    }
    bool ok = (n == (int)strlen(esperado))
            && memcmp(out, esperado, strlen(esperado)) == 0;
    if (!ok) {
        fprintf(stderr, "  '%s': esperado='%s' got='%s'\n", etiqueta, esperado, out);
    }
    AFIRMAR(ok, etiqueta);
}

/* Verifica que `base64_decodificar(in) == esperado`. */
static void chequear_dec(const char *in, const char *esperado, const char *etiqueta) {
    char prog[512];
    snprintf(prog, sizeof(prog), "r = base64_decodificar(\"%s\")\n", in);
    char out[512];
    int n = ejecutar_r(prog, out, sizeof(out));
    if (n < 0) {
        fprintf(stderr, "  '%s': ejecutar fallo\n", etiqueta);
        AFIRMAR(false, etiqueta);
        return;
    }
    bool ok = (n == (int)strlen(esperado))
            && memcmp(out, esperado, strlen(esperado)) == 0;
    if (!ok) {
        fprintf(stderr, "  '%s': esperado='%s' got='%s' (len %d vs %d)\n",
                 etiqueta, esperado, out, n, (int)strlen(esperado));
    }
    AFIRMAR(ok, etiqueta);
}

int main(void) {
    /* RFC 4648 §10 test vectors. */
    chequear_cod("",       "",         "rfc_empty");
    chequear_cod("f",      "Zg==",     "rfc_f");
    chequear_cod("fo",     "Zm8=",     "rfc_fo");
    chequear_cod("foo",    "Zm9v",     "rfc_foo");
    chequear_cod("foob",   "Zm9vYg==", "rfc_foob");
    chequear_cod("fooba",  "Zm9vYmE=", "rfc_fooba");
    chequear_cod("foobar", "Zm9vYmFy", "rfc_foobar");

    chequear_dec("",         "",       "rfc_empty_dec");
    chequear_dec("Zg==",     "f",      "rfc_f_dec");
    chequear_dec("Zm8=",     "fo",     "rfc_fo_dec");
    chequear_dec("Zm9v",     "foo",    "rfc_foo_dec");
    chequear_dec("Zm9vYg==", "foob",   "rfc_foob_dec");
    chequear_dec("Zm9vYmE=", "fooba",  "rfc_fooba_dec");
    chequear_dec("Zm9vYmFy", "foobar", "rfc_foobar_dec");

    /* Tolerancia a whitespace (MIME-style). */
    chequear_dec("Zm9v YmFy",       "foobar", "ws_space");
    chequear_dec("Zm9v\\nYmFy",     "foobar", "ws_newline");

    /* HTTP Basic Auth. */
    chequear_cod("user:pass", "dXNlcjpwYXNz", "basic_auth_cod");
    chequear_dec("dXNlcjpwYXNz", "user:pass", "basic_auth_dec");

    if (fallos == 0) {
        printf("base64: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "base64: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
