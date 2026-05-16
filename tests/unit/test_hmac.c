/*
 * Tests de HMAC-SHA-256 y HMAC-MD5 (v1.65).
 *
 * Valida los test vectors de RFC 4231 (HMAC-SHA-256) y RFC 2104
 * (HMAC-MD5). Llama directamente a las funciones C para tener
 * control sobre el byte exacto de la clave (Cornamusa no procesa
 * `\x0b` como byte hex en cadenas).
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashing.h"

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

static void chk_hmac_sha256(const uint8_t *clave, size_t clave_len,
                              const char *mensaje, const char *esperado,
                              const char *etiq) {
    char out[65];
    hashing_hmac_sha256_hex(clave, clave_len,
                             (const uint8_t *)mensaje, strlen(mensaje), out);
    bool ok = strcmp(out, esperado) == 0;
    if (!ok) {
        fprintf(stderr, "  %s: esperado=%s got=%s\n", etiq, esperado, out);
    }
    AFIRMAR(ok, etiq);
}

static void chk_hmac_md5(const uint8_t *clave, size_t clave_len,
                          const char *mensaje, const char *esperado,
                          const char *etiq) {
    char out[33];
    hashing_hmac_md5_hex(clave, clave_len,
                          (const uint8_t *)mensaje, strlen(mensaje), out);
    bool ok = strcmp(out, esperado) == 0;
    if (!ok) {
        fprintf(stderr, "  %s: esperado=%s got=%s\n", etiq, esperado, out);
    }
    AFIRMAR(ok, etiq);
}

int main(void) {
    /* ─── RFC 4231 HMAC-SHA-256 test vectors ─── */

    /* Test Case 1: Key = 0x0b × 20, Data = "Hi There". */
    {
        uint8_t key[20];
        memset(key, 0x0b, 20);
        chk_hmac_sha256(key, 20, "Hi There",
            "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
            "rfc4231_case1");
    }

    /* Test Case 2: Key = "Jefe", Data = "what do ya want for nothing?". */
    chk_hmac_sha256((const uint8_t *)"Jefe", 4,
        "what do ya want for nothing?",
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
        "rfc4231_case2");

    /* Test Case 3: Key = 0xaa × 20, Data = 0xdd × 50. */
    {
        uint8_t key[20];
        memset(key, 0xaa, 20);
        uint8_t data[50];
        memset(data, 0xdd, 50);
        char out[65];
        hashing_hmac_sha256_hex(key, 20, data, 50, out);
        bool ok = strcmp(out,
            "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe") == 0;
        if (!ok) fprintf(stderr, "  rfc4231_case3: got=%s\n", out);
        AFIRMAR(ok, "rfc4231_case3");
    }

    /* Test Case 4: Key = 0x01..0x19, Data = 0xcd × 50. */
    {
        uint8_t key[25];
        for (int i = 0; i < 25; i++) key[i] = (uint8_t)(i + 1);
        uint8_t data[50];
        memset(data, 0xcd, 50);
        char out[65];
        hashing_hmac_sha256_hex(key, 25, data, 50, out);
        bool ok = strcmp(out,
            "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b") == 0;
        if (!ok) fprintf(stderr, "  rfc4231_case4: got=%s\n", out);
        AFIRMAR(ok, "rfc4231_case4");
    }

    /* Test Case 6: Key > B (clave de 131 bytes). */
    {
        uint8_t key[131];
        memset(key, 0xaa, 131);
        const char *data = "Test Using Larger Than Block-Size Key - Hash Key First";
        char out[65];
        hashing_hmac_sha256_hex(key, 131, (const uint8_t *)data, strlen(data), out);
        bool ok = strcmp(out,
            "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54") == 0;
        if (!ok) fprintf(stderr, "  rfc4231_case6: got=%s\n", out);
        AFIRMAR(ok, "rfc4231_case6_clave_grande");
    }

    /* ─── HMAC-MD5 vectors (RFC 2104 §2). ─── */

    /* Test: Key = 0x0b × 16, Data = "Hi There". */
    {
        uint8_t key[16];
        memset(key, 0x0b, 16);
        chk_hmac_md5(key, 16, "Hi There",
            "9294727a3638bb1c13f48ef8158bfc9d",
            "rfc2104_md5_caso1");
    }

    /* Test: Key = "Jefe", Data = "what do ya want for nothing?". */
    chk_hmac_md5((const uint8_t *)"Jefe", 4,
        "what do ya want for nothing?",
        "750c783e6ab0b503eaa86e310a5db738",
        "rfc2104_md5_caso2");

    /* Test: Key = 0xaa × 16, Data = 0xdd × 50. */
    {
        uint8_t key[16];
        memset(key, 0xaa, 16);
        uint8_t data[50];
        memset(data, 0xdd, 50);
        char out[33];
        hashing_hmac_md5_hex(key, 16, data, 50, out);
        bool ok = strcmp(out, "56be34521d144c88dbb8c733f0e8b3f6") == 0;
        if (!ok) fprintf(stderr, "  rfc2104_md5_caso3: got=%s\n", out);
        AFIRMAR(ok, "rfc2104_md5_caso3");
    }

    /* Edge: clave vacia. Resultado debe ser un HMAC valido (no crash). */
    {
        char out[65];
        hashing_hmac_sha256_hex(NULL, 0, (const uint8_t *)"x", 1, out);
        /* Solo verificamos que no crashea y produce hex de 64 chars. */
        AFIRMAR(strlen(out) == 64, "edge_clave_vacia");
    }

    /* Edge: mensaje vacio. */
    {
        char out[65];
        hashing_hmac_sha256_hex((const uint8_t *)"k", 1, NULL, 0, out);
        AFIRMAR(strlen(out) == 64, "edge_mensaje_vacio");
    }

    if (fallos == 0) {
        printf("hmac: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "hmac: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
