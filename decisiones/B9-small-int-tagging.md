# B9 — Small-int tagging para `Valor`

**Estado:** ✅ Decidido.
**Fecha de propuesta:** 2026-04-30
**Fecha de decisión:** 2026-04-30
**Decisor:** David Castilla

**Decisión:** Introducir un nuevo tag `VAL_ENTERO_SMALL` en `TipoValor` que guarda enteros que caben en `int64_t` inline en la unión `Valor.como`, eliminando la asignación de `mp_int` con `malloc` para el caso común. Los enteros grandes siguen siendo `VAL_ENTERO` (puntero a `mp_int`). Las operaciones que producen ints normalizan automáticamente: si el resultado cabe en `int64_t`, devuelven SMALL; si no, BIG. **NO** usamos tagged pointers (bit 0 del puntero) — ver §3.

## Contexto

Tras F10 (v0.10.0), `fibonacci_recursivo` mejoró solo 2% pese a especializar todos los hot opcodes (OP_OBTENER_GLOBAL, OP_LLAMAR, OP_BINARIO_*_INT_INT, OP_OBTENER_ATRIBUTO). El cuello dominante resulta ser la asignación de `mp_int` por operación: con ~1.66M llamadas recursivas y ~3 operaciones bignum por llamada, son ~5M `malloc + mp_init + mp_clear + free` que dominan los 1.4s de runtime.

CPython, V8, LuaJIT, Self, SmallTalk-80 — todos los lenguajes dinámicos serios resuelven esto con tagged values. La técnica está validada por décadas. Para Cornamusa el coste es localizado (~500-800 líneas, 5 archivos) y el rendimiento esperado en programas numéricos es 3-5x.

La pregunta original que disparó esta decisión fue del usuario tras el release v0.10.0: **"¿podemos seguir mejorando el rendimiento?"**. Con F10 quedó claro que el dispatch ya no es el cuello, pero la representación de int sí lo es.

## Decisión: nuevo tag, no tagged pointer

### Opción A (rechazada) — tagged pointer

Reusar `VAL_ENTERO`, codificar small int en el bit 0 de `como.entero`. malloc devuelve punteros 8-byte aligned, así que el bit 0 siempre está libre.

- **Pro:** cero cambios en sitios que comprueban `v.tipo == VAL_ENTERO` (72 puntos en la codebase).
- **Contra:** cualquier `mp_int *` deref que olvidemos chequear corrompe heap silenciosamente. 28 sitios de acceso a `como.entero`. Bug-density alta. Los compilers de C no avisan: `entero->dp` con un puntero tagged accede a memoria arbitraria. Imposible auditar limpiamente sin convertir todos los accesos en macros, lo que reintroduce el coste de migración.

### Opción B (elegida) — `VAL_ENTERO_SMALL` ✅

Añadir `VAL_ENTERO_SMALL` al enum `TipoValor`. SMALL guarda `int64_t` inline en `Valor.como.entero_small`.

- **Pro:** el compilador de C grita en cada `case VAL_ENTERO` que falte adaptarse (con `-Wswitch-enum` activo). El sistema de tipos hace de checklist mecánico. Auditable: cada sitio que accede a `como.entero` queda obligado a chequear primero `v.tipo == VAL_ENTERO` (BIG), no solo `valor_es_entero(v)`. Imposible cometer el bug de Opción A.
- **Contra:** los 72 sitios `v.tipo == VAL_ENTERO` deben actualizarse a `valor_es_entero(&v)` (helper inline que comprueba ambos tags). Mecánico pero predecible — `grep` cubre 100% del trabajo.

### Opción C (rechazada) — NaN-boxing

Codificar todos los `Valor` en un `uint64_t` aprovechando los bits libres de un NaN double. Es la representación más densa y la usan motores JS modernos (V8, JavaScriptCore).

- **Contra:** refactor de **toda** la representación `Valor`, no solo enteros. Cambia tamaño de struct, ABI, todos los accesos. Coste 5-10x el de Opción B. Beneficio marginal sobre Opción B para Cornamusa (no estamos compitiendo con JS engines en density). Reservado para una hipotética v2.0 si se demuestra crítico.

**Decisión:** Opción B. Coste predecible, type-safety preservada, infraestructura del refactor reusable si en el futuro se quisiera tagged pointer (la API helper sería la misma).

## Rango de SMALL: 2^62, no 2^63

```c
#define CORNAMUSA_SMALL_INT_MAX  ((int64_t)0x3FFFFFFFFFFFFFFFLL)  /* 2^62 - 1 */
#define CORNAMUSA_SMALL_INT_MIN  (-CORNAMUSA_SMALL_INT_MAX - 1)   /* -2^62 */
```

Reservamos 1 bit de margen. Razón: con SMALL en `[-2^62, 2^62)`, la suma `a + b` con ambos en rango está garantizada en `int64_t` (no UB, no overflow del C). Tras la suma comprobamos si el resultado cabe en SMALL; si no, promovemos a BIG.

Si usásemos el rango completo `int64_t` (`[-2^63, 2^63)`), `INT64_MAX + 1` sería UB en C. Tendríamos que usar `__builtin_add_overflow` (GCC/Clang) y un fallback manual en MSVC. Más complejo y portable a un coste de 1 bit de rango.

Pérdida real: enteros entre 2^62 y 2^63 (4.6 × 10^18 a 9.2 × 10^18) van por el path BIG en lugar de SMALL. En la práctica, programas normales no operan en ese rango — los que lo necesitan ya están en territorio bignum y dominan otros costes.

## Aritmética: tres caminos

`OP_SUMAR` (y similares) ahora bifurca por la combinación de tipos:

| Combinación | Path |
|---|---|
| SMALL + SMALL | suma int64 directa, check de rango, normalizar a SMALL o BIG |
| SMALL + BIG | promover SMALL a `mp_int` temporal, usar `mp_*`, normalizar resultado |
| BIG + BIG | path mp_* existente, normalizar resultado |

**Normalizar:** después de cada operación que produce un int, comprobamos si el `mp_int` resultado cabe en `int64_t` y lo demote a SMALL si sí. Esto evita fugas de BIG por programas que mezclan cálculos: `mp_add(BIG_grande, -BIG_grande) = 0` debe quedar como SMALL(0), no como BIG.

## API helper canónica

Toda interacción con enteros en código que NO sea aritmética de bajo nivel debe ir por estos helpers:

```c
bool valor_es_entero(const Valor *v);              /* Sustituye v->tipo == VAL_ENTERO */
bool valor_entero_a_i64(const Valor *v, int64_t *out);  /* false si no cabe */
mp_int *valor_entero_a_mp_int(const Valor *v, bool *propio);  /* siempre disponible */
Valor valor_entero_de_i64(int64_t n);              /* SMALL si cabe, sino BIG */
Valor valor_entero_de_mp_normalizado(mp_int *m);   /* toma posesión, demote si cabe */
```

Cualquier acceso directo a `v.como.entero` o `v.como.entero_small` queda restringido a `valor.c` (helpers internos) y a los hot paths del IC en `vm.c`. Todo lo demás usa el API.

## Casos peligrosos identificados

1. **`SMALL × SMALL` overflow** — más probable que suma. Usamos `__builtin_mul_overflow` (GCC/Clang); MSVC fallback con cast a `mp_int` temporal.
2. **`SMALL_MIN / -1`** — overflow definido del C como UB (`INT64_MIN/-1`). Detectar el caso y promover a BIG.
3. **Hash divergente** — `dicc[5]` (SMALL) y `dicc[mp_int(5)]` (BIG) deben colisionar al mismo slot. `hash_valor` para BIG comprueba si cabe en i64 y, si sí, hashea como SMALL.
4. **Igualdad cross-tag** — `valor_iguales(SMALL(5), BIG(5))` debe ser `true`. Normaliza vía `valor_entero_a_i64`.
5. **Repetición de cadena negativa** — `cadena * SMALL` con SMALL negativo es error igual que ahora. Helper `valor_entero_a_i64` + check.
6. **`mp_neg(SMALL_MIN)`** — `-SMALL_MIN` overflow. Promote a BIG (su negación cabe en BIG).

## Tests diferenciales como red de seguridad

Los 8 tests diferenciales tree-walking vs bytecode (introducidos en v0.9.2) son críticos para esta refactorización:

- Tree-walking puede mantenerse generando solo `VAL_ENTERO` (BIG) hasta que el refactor esté completo.
- Bytecode genera `VAL_ENTERO_SMALL` con la nueva API.
- Si SMALL no se comporta semánticamente igual que BIG, los tests divergen → fallan → bug detectado.
- Plan: una vez los tests diferenciales sigan pasando con SMALL en bytecode, migrar tree-walking también.

## Plan por sesiones (resumen)

1. **Andamiaje:** enum + unión + helpers stub. SMALL no se genera.
2. **`valor.c`:** clonar/destruir/hash/repr/iguales aceptan SMALL.
3. **Activar SMALL:** `valor_entero_de_i64` produce SMALL si cabe; migrar 72 sitios a `valor_es_entero`.
4. **Aritmética rápida:** `evaluador.c` SMALL+SMALL inline.
5. **IC bytecode:** `OP_*_INT_INT` con SMALL inline (la mayor parte del speedup observable).
6. **Migración remanente:** `nativos.c`, `parser.c`, `eval_repetir_cadena`.
7. **Edge cases + tests:** boundaries, hash, equality, comparaciones cross-tag.
8. **Bench + release v0.11.0.**

Detalle completo en el plan de sesión asociado (no archivado, queda en la conversación con el usuario).

## Disparadores de PAUSA

Detener el refactor y consultar si:

- Tests diferenciales empiezan a fallar tras sesión 3 sin causa raíz obvia.
- Sesión 5: speedup en `fibonacci_recursivo` < 1.5x (sugiere modelo del cuello incorrecto).
- ASan reporta corrupciones en cualquier sesión.
- Sesiones 6+ están descubriendo bugs nuevos en lugar de cerrar los conocidos.

## Consecuencias

- `CORNAMUSA_BYTECODE_VERSION` permanece en 1: el formato de chunk no cambia (solo cambia la representación de Valor en runtime).
- ABI de la API pública (.cor scripts) no cambia.
- Memoria por entero pequeño: pasa de ~64 bytes (mp_int + digits + heap header) a 16 bytes (Valor inline). Mejora también el footprint.
- GC: enteros SMALL no necesitan `GCObject` header (no son heap-allocated). Enteros BIG tampoco lo tenían antes (ya eran `mp_int *` simple con refcount manual del Valor). Sin cambio en GC.

## Riesgo principal

El risk #1 es el de Opción A revivido por descuido: hacer un cambio "obvio" en una sesión futura que asume `v.tipo == VAL_ENTERO ⇒ v.como.entero` válido. Mitigación durable: dejar comentario en `valor.h` advirtiendo, y mantener `-Wswitch-enum` activo en CI.
