# B3 — Representación numérica de enteros

**Estado:** ✅ **Decidido**.
**Fecha de propuesta:** 2026-04-28
**Fecha de decisión:** 2026-04-28
**Decisor:** David Castilla
**Bloqueador identificado en:** [REPASO_CRITICO.md](../REPASO_CRITICO.md), problema B3.

**Decisión:** Opción **D** — polimórfico fasado. Bignum (libtommath) desde v0.4 con representación boxed; transición a tagged i63 + bignum en Fase 6 (bytecode VM); especialización en Fase 10 (inline caching). Semántica correcta desde el primer release jugable, sin breaking changes entre versiones.

## Contexto

El tipo `entero` de Cornamusa es uno de los más usados (cualquier programa hace aritmética). Su representación interna afecta:

1. **Semántica de usuario** — qué pasa cuando `factorial(21)` excede 2⁶³.
2. **Rendimiento** — un `add` de i64 son 1-2 ciclos; un `add` de bignum son 50-100 ciclos.
3. **Memoria** — 8 bytes vs 16-32 bytes por valor.
4. **Pipeline de optimización** — Fase 10 (inline caching) y Fase 12 (JIT) dependen de cómo está representado el entero.

El ESPEC borrador propuso "i64 hasta v1.0, bignum en v1.0". Esto es **un cambio breaking de semántica** entre v0.5 y v1.0: programas que en v0.5 producen overflow silencioso, en v1.0 producen el resultado correcto. Inadmisible.

## El problema fundamental

Cornamusa es **pedagógico para hispanohablantes**. La frase "principiantes encuentran que `factorial(21)` da `0`" describe el peor escenario UX posible:

```cornamusa
funcion factorial(n):
    si n == 0: retornar 1
    retornar n * factorial(n - 1)
fin funcion

imprimir(factorial(20))     # 2432902008176640000  ✓
imprimir(factorial(21))     # ¿overflow? ¿error? ¿bignum?
```

Cualquier respuesta a esa pregunta debe ser:
- **Predecible**: misma respuesta hoy, mañana, en v0.4 y en v1.0.
- **No sorprendente para el usuario sin formación en CS**.

La opción "overflow silencioso" falla ambos criterios.

## Opciones

### Opción A — i64 puro

Como Lua, Go, Rust, Java (long).

```c
typedef int64_t Entero;
```

**Pros:**
- Trivial de implementar.
- 1-2 ciclos por operación, máximo rendimiento.
- 8 bytes por valor.

**Contras decisivos para Cornamusa:**
- Overflow silencioso: `factorial(21) → -4249290049419214848`. Inaceptable pedagógicamente.
- Overflow detectado (con check): `factorial(21) → ErrorAritmetico`. Mejor, pero sigue siendo barrera arbitraria.
- "Es un lenguaje serio que limita por hardware" — Cornamusa no es serio en ese sentido, es pedagógico.

### Opción B — Bignum siempre

Como Python (3.0+), Ruby (con conversión transparente), Smalltalk, Scheme.

```c
typedef struct {
    int signo;
    size_t num_palabras;
    uint64_t *palabras;   // representación little-endian
} Entero;
```

**Pros:**
- Matemáticamente correcto sin excepción. `factorial(100)` es trivial.
- Cero sorpresa para usuario sin formación.
- Coherente con la filosofía pedagógica.
- Una sola representación, código simple.

**Contras:**
- Toda operación entera **asigna** memoria (sin pool/cache de pequeños). Presión sobre el GC.
- 50-100 ciclos por operación, **vs 1-2** del fast path de tagged.
- 16-24 bytes mínimo por valor.
- Requiere librería de bignum (vendoreada o casera).

### Opción C — Polimórfico (tagged i63 + bignum)

Como Smalltalk, Common Lisp, Ruby con `Fixnum`/`Bignum`, JavaScript V8 con `Smi`. CPython 1.x/2.x lo hizo así.

```c
// Value de 8 bytes con bit tag:
//   bit 0 = 1 → entero pequeño (i63 inline)
//   bit 0 = 0 → puntero a bignum (o a otro objeto con su propio tag)

typedef uint64_t Valor;

#define ES_ENTERO_PEQUEÑO(v)  ((v) & 1)
#define A_ENTERO_PEQUEÑO(v)   ((int64_t)(v) >> 1)
#define DE_ENTERO_PEQUEÑO(n)  (((uint64_t)(n) << 1) | 1)
```

Operación `a + b`:
1. Si ambos son enteros pequeños → suma con check de overflow (instrucción nativa).
2. Si overflow → promueve a bignum y suma.
3. Si alguno es bignum → suma con bignum.

**Pros:**
- **Mejor de ambos mundos**: i63 fast path para el 99% de casos, bignum para el 1% que lo necesita.
- Matemáticamente correcto sin excepción.
- 1-3 ciclos para fast path, ~50 ciclos para promoción.
- 8 bytes por valor en fast path.

**Contras:**
- Más complejo de implementar (~200 líneas extra para tagging + glue + promoción).
- Cada operación tiene branch de tag check (predicción del CPU lo hace gratis en práctica).
- Bugs sutiles en paths de promoción.

### Opción D — Polimórfico **fasado** (recomendada)

Misma semántica que C, pero **implementación distribuida en fases**:

| Fase | Implementación | Coste |
|---|---|---|
| **v0.4-v0.5** (tree-walking) | Bignum siempre. Boxed `Valor = {tag, ptr_bignum}`. Lento pero correcto. | ~80 líneas glue + libtommath vendoreada |
| **v0.6** (bytecode VM) | Tagged i63 + bignum. Fast path en una operación nativa, slow path con promoción. | ~200 líneas adicionales |
| **v0.10+** (inline caching) | Especialización: `BINARY_ADD` → `BINARY_ADD_SMALL_INT` cuando el profile observa solo enteros pequeños. | ya en plan de Fase 10 |

**Semántica visible al usuario:** **idéntica desde v0.4** — `factorial(100)` funciona igual desde el primer release jugable.

**Performance:** mejora monotónicamente. v0.4 lento, v0.6 rápido, v0.10 muy rápido. Sin breaking changes.

## Comparativa resumida

| Criterio | A: i64 | B: bignum | C: polimórfico | **D: polimórfico fasado** |
|---|---|---|---|---|
| Semántica predecible | 🔴 overflow | 🟢 correcta | 🟢 correcta | 🟢 correcta |
| Performance v0.6 | 🟢 máxima | 🔴 lenta | 🟢 alta | 🟢 alta |
| Performance v0.4 | 🟢 alta | 🔴 lenta | 🟢 alta | 🟡 lenta (aceptable en tree-walking) |
| Coste de implementación inicial | 🟢 bajo | 🟡 medio | 🔴 alto | 🟡 medio |
| Coste total a v1.0 | 🟢 bajo | 🟡 medio | 🔴 alto | 🟡 medio-alto |
| Sin breaking changes | 🟢 | 🟢 | 🟢 | 🟢 |
| Adecuado para pedagógico | 🔴 | 🟢 | 🟢 | 🟢 |

## Recomendación

**Opción D — polimórfico fasado.**

**Razones:**
1. **Semántica correcta desde día 1**, sin sorpresas para principiantes.
2. **Sin breaking changes** entre releases — `factorial(21)` siempre funciona, desde v0.4.
3. **Coste inicial moderado**: en v0.4 solo necesitamos boxed bignum, ~80 líneas. La complejidad de tagged pointers se aplaza a Fase 6 cuando ya estamos pensando en performance.
4. **Performance evoluciona monotónicamente**: v0.4 lento (aceptable para tree-walking, que ya es lento por diseño), v0.6 rápido, v0.10 muy rápido.
5. **Encaja con el resto del plan**: la promoción a tagged en Fase 6 va en paralelo con la introducción del bytecode; la especialización en Fase 10 ya está prevista.

**Trade-off aceptado:** v0.4 es ~10x más lento en aritmética intensiva que con i64 puro. Es aceptable porque:
- El tree-walking es ya 50-100x más lento que la VM bytecode futura.
- v0.4 es para corregir ergonomía/UX, no para benchmarks.
- Fibonacci recursivo `fib(30)` corre en ~1 segundo en v0.4 con boxed bignum. Suficiente.

## Detalles operativos

### Librería de bignum: libtommath

**Elegida:** [libtommath](https://www.libtom.net/LibTomMath/).

| Criterio | libtommath | GMP | Casero |
|---|---|---|---|
| Licencia | Public Domain / WTFPL | LGPL (linking) | — |
| Tamaño | ~5K líneas de C | ~50K líneas | ~500 líneas (limitado) |
| Madurez | 20+ años, usado en libtomcrypt, Tcl | Estándar industrial | nuevo, sin pruebas |
| Compatibilidad MIT | ✅ | ⚠️ requires LGPL accommodations | ✅ |
| Performance | Buena para nuestros tamaños | Mejor en operaciones grandes | Limitada |
| Build cross-platform | Trivial (drop-in C) | Complicado en Windows | Trivial |

**Vendoreada en `vendor/libtommath/`** y compilada como parte del binario. No dependencia externa en runtime.

### Tipo `Valor` evolutivo

#### v0.4-v0.5 — Boxed Value
```c
typedef enum {
    VAL_NULO,
    VAL_BOOLEANO,
    VAL_ENTERO,        // siempre puntero a bignum (mp_int)
    VAL_DECIMAL,
    VAL_OBJETO,
} TipoValor;

typedef struct {
    TipoValor tipo;
    union {
        bool booleano;
        double decimal;
        struct mp_int *entero;     // bignum siempre
        struct Objeto *objeto;
    } como;
} Valor;
```

#### v0.6+ — Tagged + NaN-boxing (decisión secundaria diferida a Fase 6)

Probablemente NaN-boxing estilo Crafting Interpreters cap. 30:
- Doubles: IEEE 754 directo.
- Pequeños enteros (i48 o i52, según diseño): en bits de NaN.
- Bignum: puntero a `mp_int` boxed.
- Otros tipos: en bits de NaN con tag.

Detalle exacto se decidirá al inicio de Fase 6, evaluando trade-offs específicos del compilador y arquitectura.

### Operadores y semántica

| Operador | Comportamiento con enteros |
|---|---|
| `a + b`, `a - b`, `a * b` | Resultado entero exacto. Promueve a bignum si excede i63. |
| `a / b` | **Float division** (resultado decimal), igual que Python 3. `7 / 2 → 3.5`. |
| `a // b` | **Floor division** (resultado entero exacto). `7 // 2 → 3`. |
| `a % b` | Módulo entero exacto. |
| `a ** b` | Potencia. Si `b` es entero ≥ 0, resultado entero exacto. Si `b` es negativo, resultado decimal. |
| `entero(x)` | Conversión: float → entero (truncado), cadena → entero (parsea, error si no). |
| `decimal(x)` | Conversión a IEEE 754 double. Pierde precisión si entero excede 2⁵³. |

### Comparación entero vs decimal

`1 == 1.0` es `verdadero` (igual que Python). Internamente:
- Si el decimal es exactamente representable, comparar como decimales.
- Si el entero excede 2⁵³, convertir el decimal a fracción exacta y comparar.

Detalles en Fase 6.

### Conversión a cadena

Bignum → cadena vía libtommath (`mp_to_radix`). Sin sorpresas.

```cornamusa
imprimir(factorial(50))
# 30414093201713378043612608166064768844377641568960512000000000000
```

### Edge cases documentados

1. **División por cero**: `1 / 0`, `1 // 0`, `1 % 0` → lanzan `ErrorDivisiónPorCero`.
2. **Overflow del exponente**: `2 ** 1_000_000` puede consumir GB de memoria. Documentado, no protegido (responsabilidad del usuario).
3. **Hash de enteros**: bignum y i63 con mismo valor matemático deben tener mismo hash. Implementación específica.
4. **Comparación con cadenas**: `"5" == 5` es `falso` (igual que Python).

## Decisión

**Opción D adoptada el 2026-04-28.**

Los enteros de Cornamusa son **matemáticamente correctos desde v0.4**: no hay overflow silencioso ni errores arbitrarios cuando un cálculo excede los 64 bits. La implementación se distribuye en fases:

| Fase | Mecanismo | Performance |
|---|---|---|
| v0.4-v0.5 | Boxed bignum (libtommath siempre) | ~50-100 ciclos por op |
| v0.6+ | Tagged i63 + bignum con promoción | 1-3 ciclos en fast path |
| v0.10+ | Inline caching especializa hot loops | pico de rendimiento |

La **semántica visible al usuario es idéntica desde v0.4**: solo cambia la velocidad. `factorial(100)` funciona igual en cualquier versión.

**Librería de bignum confirmada:** [libtommath](https://www.libtom.net/LibTomMath/) (Public Domain), vendoreada en `vendor/libtommath/` y compilada como parte del binario. Descartada GMP por licencia LGPL.

## Consecuencias (si se decide D)

- **ESPEC.md §3** (Tipos primitivos del runtime): actualizar `entero` con descripción "precisión arbitraria desde v0.4 (bignum boxed); fast path con tagged i63 desde v0.6".
- **ESPEC.md §2.5** (literales numéricos): añadir nota sobre que enteros son arbitrariamente grandes.
- **Plan maestro Fase 4**: añadir libtommath como dependencia; ~80 líneas de glue para `entero`.
- **Plan maestro Fase 6**: detallar transición a tagged + NaN-boxing.
- **Plan maestro Fase 10**: especialización `BINARY_ADD_SMALL_INT` queda como objetivo.
- **CMakeLists.txt**: añadir subdirectorio `vendor/libtommath/`.
- **vendor/libtommath/**: vendorear el código (commit grande, pero único).
- **Riesgos del plan**: añadir "performance v0.4-v0.5 con bignum boxed (aceptable, tree-walking ya lento)".

## Alternativas descartadas y por qué

| Opción | Motivo de descarte |
|---|---|
| A: i64 puro con overflow silencioso | Inadmisible pedagógicamente |
| A': i64 puro con error en overflow | Barrera arbitraria sin sentido para el usuario |
| B: bignum siempre, también en v0.6+ | Performance insuficiente para Fase 10/12 |
| C: tagged i63 + bignum desde día 1 | Demasiada complejidad inicial; aplazable sin perder semántica |
| Lib GMP en lugar de libtommath | Licencia LGPL complica distribución; rendimiento no necesario para nuestros tamaños |
