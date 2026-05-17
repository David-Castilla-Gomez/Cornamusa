#include "hashing.h"

#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────
 * Helpers comunes.
 * ────────────────────────────────────────────────────────────────── */

static void escribir_hex(const uint8_t *datos, size_t n, char *out) {
    static const char HEX[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[2 * i]     = HEX[(datos[i] >> 4) & 0xF];
        out[2 * i + 1] = HEX[datos[i] & 0xF];
    }
    out[2 * n] = '\0';
}

static uint32_t leer_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24)
         | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)
         |  (uint32_t)p[3];
}

static void escribir_be32(uint32_t v, uint8_t *p) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t leer_le32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void escribir_le32(uint32_t v, uint8_t *p) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t rotr32(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}

static uint32_t rotl32(uint32_t x, unsigned n) {
    return (x << n) | (x >> (32 - n));
}

/* ──────────────────────────────────────────────────────────────────
 * SHA-256 (FIPS 180-4).
 *
 * Procesa bloques de 64 bytes. Padding: 0x80, ceros, longitud en bits
 * big-endian 64-bit al final. Output: 32 bytes big-endian.
 * ────────────────────────────────────────────────────────────────── */

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_procesar_bloque(uint32_t H[8], const uint8_t bloque[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = leer_be32(bloque + i * 4);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
    uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + SHA256_K[i] + w[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    H[0] += a; H[1] += b; H[2] += c; H[3] += d;
    H[4] += e; H[5] += f; H[6] += g; H[7] += h;
}

/* Computa SHA-256 raw (32 bytes, no hex). Usado tanto por
 * `hashing_sha256_hex` como por HMAC-SHA-256. */
static void sha256_raw(const uint8_t *datos, size_t len, uint8_t digest[32]) {
    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    /* Procesar bloques completos. */
    size_t bloques = len / 64;
    for (size_t i = 0; i < bloques; i++) {
        sha256_procesar_bloque(H, datos + i * 64);
    }
    size_t resto = len % 64;
    const uint8_t *cola = datos + bloques * 64;

    /* Padding final. Necesitamos: cola + 0x80 + zeros + 8 bytes length.
     * Si cola + 1 + 8 > 64, requiere 2 bloques de padding. */
    uint8_t pad[128] = {0};
    memcpy(pad, cola, resto);
    pad[resto] = 0x80;

    /* Bits totales en big-endian 64-bit al final. */
    uint64_t bits = (uint64_t)len * 8;
    int total_pad_blocks = (resto + 1 + 8 > 64) ? 2 : 1;
    int total_pad_bytes = total_pad_blocks * 64;
    /* Longitud va en los ultimos 8 bytes. */
    for (int i = 0; i < 8; i++) {
        pad[total_pad_bytes - 8 + i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    for (int i = 0; i < total_pad_blocks; i++) {
        sha256_procesar_bloque(H, pad + i * 64);
    }

    /* Output 32 bytes big-endian. */
    for (int i = 0; i < 8; i++) escribir_be32(H[i], digest + i * 4);
}

void hashing_sha256_hex(const uint8_t *datos, size_t len, char out_hex[65]) {
    uint8_t digest[32];
    sha256_raw(datos, len, digest);
    escribir_hex(digest, 32, out_hex);
}

/* ──────────────────────────────────────────────────────────────────
 * MD5 (RFC 1321).
 *
 * Procesa bloques de 64 bytes. Padding: 0x80, ceros, longitud en bits
 * little-endian 64-bit al final. Output: 16 bytes little-endian.
 * ────────────────────────────────────────────────────────────────── */

static const uint32_t MD5_K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const unsigned MD5_S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static void md5_procesar_bloque(uint32_t H[4], const uint8_t bloque[64]) {
    uint32_t M[16];
    for (int i = 0; i < 16; i++) M[i] = leer_le32(bloque + i * 4);

    uint32_t A = H[0], B = H[1], C = H[2], D = H[3];
    for (int i = 0; i < 64; i++) {
        uint32_t F;
        unsigned g;
        if (i < 16) {
            F = (B & C) | ((~B) & D);
            g = (unsigned)i;
        } else if (i < 32) {
            F = (D & B) | ((~D) & C);
            g = (5 * (unsigned)i + 1) % 16;
        } else if (i < 48) {
            F = B ^ C ^ D;
            g = (3 * (unsigned)i + 5) % 16;
        } else {
            F = C ^ (B | (~D));
            g = (7 * (unsigned)i) % 16;
        }
        F = F + A + MD5_K[i] + M[g];
        A = D;
        D = C;
        C = B;
        B = B + rotl32(F, MD5_S[i]);
    }
    H[0] += A; H[1] += B; H[2] += C; H[3] += D;
}

/* Computa MD5 raw (16 bytes, no hex). Usado tanto por hashing_md5_hex
 * como por HMAC-MD5. */
static void md5_raw(const uint8_t *datos, size_t len, uint8_t digest[16]) {
    uint32_t H[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };

    size_t bloques = len / 64;
    for (size_t i = 0; i < bloques; i++) {
        md5_procesar_bloque(H, datos + i * 64);
    }
    size_t resto = len % 64;
    const uint8_t *cola = datos + bloques * 64;

    /* Padding. Igual que SHA-256 pero la longitud va little-endian. */
    uint8_t pad[128] = {0};
    memcpy(pad, cola, resto);
    pad[resto] = 0x80;

    uint64_t bits = (uint64_t)len * 8;
    int total_pad_blocks = (resto + 1 + 8 > 64) ? 2 : 1;
    int total_pad_bytes = total_pad_blocks * 64;
    /* Bits en los ultimos 8 bytes, little-endian. */
    for (int i = 0; i < 8; i++) {
        pad[total_pad_bytes - 8 + i] = (uint8_t)(bits >> (8 * i));
    }
    for (int i = 0; i < total_pad_blocks; i++) {
        md5_procesar_bloque(H, pad + i * 64);
    }

    /* Output 16 bytes little-endian. */
    for (int i = 0; i < 4; i++) escribir_le32(H[i], digest + i * 4);
}

void hashing_md5_hex(const uint8_t *datos, size_t len, char out_hex[33]) {
    uint8_t digest[16];
    md5_raw(datos, len, digest);
    escribir_hex(digest, 16, out_hex);
}

/* ──────────────────────────────────────────────────────────────────
 * HMAC (RFC 2104 / RFC 4231).
 *
 * HMAC(K, m) = H((K' XOR opad) || H((K' XOR ipad) || m))
 *
 *   K' = K si |K| <= B
 *   K' = H(K) si |K| > B  (digest_len < B, padded con zeros)
 *   ipad = 0x36 byte * B
 *   opad = 0x5C byte * B
 *
 * B (block size) = 64 para SHA-256 y MD5.
 * digest_len: 32 para SHA-256, 16 para MD5.
 * ────────────────────────────────────────────────────────────────── */

#define HMAC_BLOCK_SIZE 64

/* Computa K' (clave ajustada a block size) en `out`. Out tiene
 * tamanio HMAC_BLOCK_SIZE bytes. Si la clave excede B, primero se
 * hashea con `hash_fn` que escribe `digest_len` bytes en out. Si la
 * clave es <= B, se copia tal cual y el resto se rellena con zeros. */
static void hmac_preparar_clave(const uint8_t *clave, size_t clave_len,
                                  void (*hash_fn)(const uint8_t *, size_t, uint8_t *),
                                  int digest_len,
                                  uint8_t out[HMAC_BLOCK_SIZE]) {
    memset(out, 0, HMAC_BLOCK_SIZE);
    if (clave_len > HMAC_BLOCK_SIZE) {
        hash_fn(clave, clave_len, out);
        /* El resto de out (digest_len .. B) queda en cero por memset. */
        (void)digest_len;
    } else {
        memcpy(out, clave, clave_len);
    }
}

/* Wrappers que adaptan sha256_raw / md5_raw a la firma generica
 * (uint8_t*) -> (uint8_t*). */
static void sha256_raw_adapter(const uint8_t *in, size_t len, uint8_t *out) {
    sha256_raw(in, len, out);
}
static void md5_raw_adapter(const uint8_t *in, size_t len, uint8_t *out) {
    md5_raw(in, len, out);
}

void hashing_hmac_sha256_bytes(const uint8_t *clave, size_t clave_len,
                                 const uint8_t *mensaje, size_t mensaje_len,
                                 uint8_t out_bytes[32]) {
    uint8_t k_prime[HMAC_BLOCK_SIZE];
    hmac_preparar_clave(clave, clave_len, sha256_raw_adapter, 32, k_prime);

    /* inner = H((K' XOR ipad) || mensaje). */
    uint8_t k_xor_ipad[HMAC_BLOCK_SIZE];
    for (int i = 0; i < HMAC_BLOCK_SIZE; i++) k_xor_ipad[i] = k_prime[i] ^ 0x36;

    /* Buffer concatenado: (K' XOR ipad) || mensaje. */
    uint8_t *buf = (uint8_t *)malloc(HMAC_BLOCK_SIZE + mensaje_len);
    if (!buf) { memset(out_bytes, 0, 32); return; }
    memcpy(buf, k_xor_ipad, HMAC_BLOCK_SIZE);
    if (mensaje_len > 0) memcpy(buf + HMAC_BLOCK_SIZE, mensaje, mensaje_len);

    uint8_t inner_digest[32];
    sha256_raw(buf, HMAC_BLOCK_SIZE + mensaje_len, inner_digest);
    free(buf);

    /* outer = H((K' XOR opad) || inner). */
    uint8_t k_xor_opad[HMAC_BLOCK_SIZE];
    for (int i = 0; i < HMAC_BLOCK_SIZE; i++) k_xor_opad[i] = k_prime[i] ^ 0x5C;

    uint8_t outer_buf[HMAC_BLOCK_SIZE + 32];
    memcpy(outer_buf, k_xor_opad, HMAC_BLOCK_SIZE);
    memcpy(outer_buf + HMAC_BLOCK_SIZE, inner_digest, 32);

    sha256_raw(outer_buf, HMAC_BLOCK_SIZE + 32, out_bytes);
}

void hashing_hmac_sha256_hex(const uint8_t *clave, size_t clave_len,
                              const uint8_t *mensaje, size_t mensaje_len,
                              char out_hex[65]) {
    uint8_t bytes[32];
    hashing_hmac_sha256_bytes(clave, clave_len, mensaje, mensaje_len, bytes);
    escribir_hex(bytes, 32, out_hex);
}

void hashing_hmac_md5_hex(const uint8_t *clave, size_t clave_len,
                           const uint8_t *mensaje, size_t mensaje_len,
                           char out_hex[33]) {
    uint8_t k_prime[HMAC_BLOCK_SIZE];
    hmac_preparar_clave(clave, clave_len, md5_raw_adapter, 16, k_prime);

    uint8_t k_xor_ipad[HMAC_BLOCK_SIZE];
    for (int i = 0; i < HMAC_BLOCK_SIZE; i++) k_xor_ipad[i] = k_prime[i] ^ 0x36;

    uint8_t *buf = (uint8_t *)malloc(HMAC_BLOCK_SIZE + mensaje_len);
    if (!buf) { out_hex[0] = '\0'; return; }
    memcpy(buf, k_xor_ipad, HMAC_BLOCK_SIZE);
    if (mensaje_len > 0) memcpy(buf + HMAC_BLOCK_SIZE, mensaje, mensaje_len);

    uint8_t inner_digest[16];
    md5_raw(buf, HMAC_BLOCK_SIZE + mensaje_len, inner_digest);
    free(buf);

    uint8_t k_xor_opad[HMAC_BLOCK_SIZE];
    for (int i = 0; i < HMAC_BLOCK_SIZE; i++) k_xor_opad[i] = k_prime[i] ^ 0x5C;

    uint8_t outer_buf[HMAC_BLOCK_SIZE + 16];
    memcpy(outer_buf, k_xor_opad, HMAC_BLOCK_SIZE);
    memcpy(outer_buf + HMAC_BLOCK_SIZE, inner_digest, 16);

    uint8_t final_digest[16];
    md5_raw(outer_buf, HMAC_BLOCK_SIZE + 16, final_digest);
    escribir_hex(final_digest, 16, out_hex);
}
