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

#endif /* CORNAMUSA_HASHING_H */
