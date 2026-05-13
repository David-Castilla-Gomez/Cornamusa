/*
 * Cornamusa v1.28 — Motor regex acotado.
 *
 * Implementación: parser-to-AST + matcher backtracking en C. Sin
 * dependencias externas. Pensado para los patrones comunes (email,
 * número, identificador, validación de formato) sin pretender ser
 * un superset de PCRE.
 *
 * Sintaxis soportada en v1.28.0:
 *   - Literales y escapes: \. \\ \n \t \r \( \) \[ \] \{ \} \| \^ \$ \?
 *     \+ \* (cualquier escape no-clase es literal).
 *   - Cualquier carácter: `.` (excluye '\n' por defecto).
 *   - Quantifiers (greedy): `*`, `+`, `?`.
 *   - Anchors: `^` (inicio del texto), `$` (fin del texto).
 *   - Clases predefinidas: \d \D \w \W \s \S.
 *   - Clases de carácter: [abc], [^abc], [a-z], [a-zA-Z0-9_].
 *   - Alternancia: `a|b`. Vinculación más débil.
 *   - Grupos no-captura: `(?:...)`. Y grupos de captura `(...)` que se
 *     comportan como no-captura en v1.28.0 (la API no expone grupos).
 *
 * NO soportado en v1.28.0:
 *   - Backreferences (\1, \2...).
 *   - Lookahead/lookbehind.
 *   - Boundary `\b`.
 *   - Quantifiers explícitos `{n}`, `{n,m}`.
 *   - Lazy quantifiers `*?`, `+?`.
 *   - Captura de grupos en la API pública.
 *
 * Cualquier patrón sintácticamente inválido produce un error con
 * mensaje describiendo la posición.
 */
#ifndef CORNAMUSA_REGEX_H
#define CORNAMUSA_REGEX_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Intenta matchear `patron` contra `texto` desde el inicio. Si tiene
 * éxito, `*fin_out` recibe la posición (en bytes) inmediatamente
 * después del match, y devuelve true. Si no matchea, devuelve false.
 *
 * Si el patrón es sintácticamente inválido, devuelve false y escribe
 * el motivo en `err_buf` (si no es NULL).
 */
bool regex_coincidir(const char *patron, const char *texto, int texto_len,
                       int *fin_out,
                       char *err_buf, size_t err_cap);

/*
 * Busca la primera ocurrencia del patrón en `texto`. Si la encuentra,
 * `*inicio_out` y `*fin_out` reciben las posiciones del match
 * (en bytes), y devuelve true. Si no, false.
 */
bool regex_buscar(const char *patron, const char *texto, int texto_len,
                    int *inicio_out, int *fin_out,
                    char *err_buf, size_t err_cap);

/*
 * Llama `callback` por cada ocurrencia no-solapante del patrón en
 * `texto`. La función recibe (inicio, fin, datos_usuario).
 * Retorna el número de matches encontrados, o -1 si error de patrón.
 */
typedef bool (*RegexCallback)(int inicio, int fin, void *datos);
int regex_todos(const char *patron, const char *texto, int texto_len,
                 RegexCallback cb, void *datos,
                 char *err_buf, size_t err_cap);

#endif /* CORNAMUSA_REGEX_H */
