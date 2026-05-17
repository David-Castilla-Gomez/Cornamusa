#ifndef CORNAMUSA_HASHING_H
#define CORNAMUSA_HASHING_H

#include <stddef.h>
#include <stdint.h>

/*
 * Hashing nativo (v1.60).
 *
 * Implementaciones puras en C (sin dependencias externas) de:
 *   - SHA-256 (FIPS 180-4 / RFC 6234).
 *   - MD5     (RFC 1321).
 *
 * Las funciones devuelven el digest como cadena hexadecimal en
 * minusculas, NUL-terminated. Output:
 *   - SHA-256: 64 chars hex + 1 NUL → buffer de >=65.
 *   - MD5:     32 chars hex + 1 NUL → buffer de >=33.
 *
 * Notas de seguridad: MD5 esta criptograficamente roto desde 2004
 * (colisiones practicas). Sigue siendo util para integridad casual
 * o compatibilidad con sistemas legacy, no para firmas o passwords.
 * Para passwords usa scrypt/argon2 (no provistos por Cornamusa).
 */

void hashing_sha256_hex(const uint8_t *datos, size_t len, char out_hex[65]);
void hashing_md5_hex(const uint8_t *datos, size_t len, char out_hex[33]);

/*
 * HMAC (v1.65) — RFC 2104 / RFC 4231.
 *
 * HMAC(K, m) = H((K' XOR opad) || H((K' XOR ipad) || m))
 * donde K' = K si |K| <= B, sino H(K), padded con zeros a B.
 * B (block size) = 64 para SHA-256 y MD5.
 *
 * Output: digest hexadecimal en minusculas, NUL-terminated.
 *   HMAC-SHA-256: 64 chars + 1 NUL → buffer >= 65.
 *   HMAC-MD5:     32 chars + 1 NUL → buffer >= 33.
 */
void hashing_hmac_sha256_hex(const uint8_t *clave, size_t clave_len,
                              const uint8_t *mensaje, size_t mensaje_len,
                              char out_hex[65]);

void hashing_hmac_md5_hex(const uint8_t *clave, size_t clave_len,
                           const uint8_t *mensaje, size_t mensaje_len,
                           char out_hex[33]);

/*
 * v1.67: HMAC-SHA-256 que devuelve los 32 bytes raw (sin hex). Usado
 * por JWT y otros protocolos que codifican el digest en base64-url.
 */
void hashing_hmac_sha256_bytes(const uint8_t *clave, size_t clave_len,
                                 const uint8_t *mensaje, size_t mensaje_len,
                                 uint8_t out_bytes[32]);

#endif /* CORNAMUSA_HASHING_H */
