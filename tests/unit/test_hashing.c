/*
 * Tests de SHA-256 y MD5 (v1.60).
 *
 * Valida los test vectors canonicos (FIPS 180-4 §B.1 / RFC 6234
 * para SHA-256; RFC 1321 §A.5 para MD5). Incluye el caso de
 * 1 millon de "a" (FIPS B.3) que ejercita streaming de bloques.
 *
 * Llama directamente a las funciones C `hashing_sha256_hex` y
 * `hashing_md5_hex` (no via VM) — son puramente computacionales,
 * sin interaccion con el runtime.
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

static void chk_sha256(const char *in, const char *esperado, const char *etiq) {
    char out[65];
    hashing_sha256_hex((const uint8_t *)in, strlen(in), out);
    bool ok = strcmp(out, esperado) == 0;
    if (!ok) {
        fprintf(stderr, "  %s: esperado=%s got=%s\n", etiq, esperado, out);
    }
    AFIRMAR(ok, etiq);
}

static void chk_md5(const char *in, const char *esperado, const char *etiq) {
    char out[33];
    hashing_md5_hex((const uint8_t *)in, strlen(in), out);
    bool ok = strcmp(out, esperado) == 0;
    if (!ok) {
        fprintf(stderr, "  %s: esperado=%s got=%s\n", etiq, esperado, out);
    }
    AFIRMAR(ok, etiq);
}

int main(void) {
    /* ─── SHA-256 test vectors ─── */

    /* FIPS 180-4 §B.1: "abc" */
    chk_sha256("abc",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "sha256_abc");

    /* Empty string */
    chk_sha256("",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "sha256_empty");

    /* FIPS 180-4 §B.2: 56 bytes (testa edge case 2-block padding). */
    chk_sha256(
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        "sha256_56bytes");

    /* Common test: "The quick brown fox..." */
    chk_sha256("The quick brown fox jumps over the lazy dog",
        "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592",
        "sha256_fox");

    /* Mismo input pero con un caracter cambiado (avalanche test). */
    chk_sha256("The quick brown fox jumps over the lazy cog",
        "e4c4d8f3bf76b692de791a173e05321150f7a345b46484fe427f6acc7ecc81be",
        "sha256_fox_cog");

    /* FIPS B.3: 1 millon de "a". Single-block algorithm con streaming. */
    {
        size_t n = 1000000;
        char *buf = (char *)malloc(n);
        if (buf) {
            memset(buf, 'a', n);
            char out[65];
            hashing_sha256_hex((const uint8_t *)buf, n, out);
            bool ok = strcmp(out,
                "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0;
            if (!ok) {
                fprintf(stderr, "  sha256_1M_a: got=%s\n", out);
            }
            AFIRMAR(ok, "sha256_1M_a");
            free(buf);
        }
    }

    /* ─── MD5 test vectors (RFC 1321 §A.5) ─── */

    chk_md5("",                              "d41d8cd98f00b204e9800998ecf8427e", "md5_empty");
    chk_md5("a",                             "0cc175b9c0f1b6a831c399e269772661", "md5_a");
    chk_md5("abc",                           "900150983cd24fb0d6963f7d28e17f72", "md5_abc");
    chk_md5("message digest",                "f96b697d7cb7938d525a2f31aaf161d0", "md5_msg");
    chk_md5("abcdefghijklmnopqrstuvwxyz",    "c3fcd3d76192e4007dfb496cca67e13b", "md5_alpha");
    chk_md5("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
            "d174ab98d277d9f5a5611c2c9f419d9f",
            "md5_alpha_num");
    chk_md5("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
            "57edf4a22be3c955ac49da2e2107b67a",
            "md5_80digits");

    /* Edge: 56-byte input para MD5 (mismo bytes que SHA-256). */
    chk_md5("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "8215ef0796a20bcaaae116d3876c664a",
            "md5_56bytes");

    if (fallos == 0) {
        printf("hashing: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "hashing: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
