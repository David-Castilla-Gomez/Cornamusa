# B8 — Inline caching especializado tipo PEP 659

**Estado:** ✅ Decidido.
**Fecha de propuesta:** 2026-04-29
**Fecha de decisión:** 2026-04-29
**Decisor:** David Castilla

**Decisión:** Implementar inline caching en F10 (v0.10) **antes** de v1.0, con cache slots **embebidos inline** en el flujo de bytecode (estilo PEP 659 / CPython 3.11+), no en una tabla paralela. Quickening por reescritura in-place del opcode tras el primer acierto. Foco en los hot paths conocidos por la literatura: lookup de globales, llamadas a funciones, aritmética binaria por tipo, acceso a atributos con shape caching simple. **Sin tier-2 ni tracing en F10** — eso queda como trabajo post-v1.0 cuando haya datos de uso reales.

## Contexto

Tras v0.9.2 los benchmarks baseline son:

| Benchmark             | Tiempo |
|-----------------------|--------|
| bignum_factorial      | ~25 ms |
| dicc_intensivo        | ~120 ms |
| fibonacci_recursivo   | ~1.4 s (fib(30)) |
| oo_intensivo          | ~45 ms |

`fibonacci_recursivo` cuesta ~5-7x lo que cuesta en CPython — esperable para un intérprete sin opcodes especializados, pero suficientemente lento como para dejar mala primera impresión a un usuario que pruebe el lenguaje. Las llamadas a función y el lookup de globales son los hot paths obvios.

La pregunta que disparó esta decisión fue **"qué nos da mejor rendimiento a largo plazo"** (sesión del 2026-04-29). Tres opciones:

1. **F10 antes de v1.0**: ~6-9 sesiones de trabajo, v1.0 sale con buen rendimiento de salida.
2. **F11/v1.0 directo**: ship rápido, F10 como v1.1.
3. **v0.10 hardening (sanitizers, docs, ESPEC) + v1.0**, F10 como v1.1.

## Decisión: F10 antes de v1.0

Las tres razones que pesaron:

1. **Arquitectura limpia**. Ahora compilador y VM siguen siendo manejables (~3000 líneas C combinados). Añadir slots de cache, opcodes quickened y rewriting in-place hoy son ~6-9 sesiones; con más features encima esa misma decisión cuesta el doble por las restricciones de compatibilidad.

2. **Sin compromiso de estabilidad todavía**. Tras v1.0 cualquier cambio de formato de chunk es una decisión política (migración, versionado, deprecación). Hoy podemos cambiar el layout libremente sin avisar a nadie.

3. **Las especializaciones obvias no necesitan datos reales**. `OP_OBTENER_GLOBAL`, `OP_LLAMAR`, `OP_BINARIO_*` con tipos monomórficos — sabemos por la literatura (PEP 659, V8, LuaJIT, Self) que estos son los hot paths. La optimización guiada por perfil real (tier-2, shape caching avanzado, tracing) sí necesita usuarios, y por eso queda como v1.1+ post-feedback.

Trade-off aceptado: v1.0 se retrasa ~6-9 sesiones. Mitigación: durante F10 no hay churn de features de lenguaje, así que el riesgo de sumar deuda nueva es bajo.

## Arquitectura

### Cache slots inline (no side-table)

Cada opcode "cacheable" lleva, **inmediatamente tras sus operandos normales**, N bytes de cache que la VM lee/escribe in-place. Ejemplo del layout para `OP_OBTENER_GLOBAL_CACHE`:

```
[OP_OBTENER_GLOBAL_CACHE] [name_idx u8] [version u16] [slot_idx u16]
↑ 1 byte                  ↑ 1 byte      ↑ 2 bytes    ↑ 2 bytes
                          operando      cache (filled at miss)
```

**Por qué inline y no side-table indexada por PC:**

- Locality: la cache vive donde el dispatch va a leerla. Side-table cuesta una indirección extra que mata gran parte de la ganancia.
- Inspección simple: `--ast` y `--dump-bytecode` (cuando exista) muestran el cache en el mismo flujo, no hay que cruzar arrays.
- PEP 659 lo hizo así, V8 también con bytecode handlers — está validado.
- El bookkeeping de "cuántos bytes de cache trae este opcode" se centraliza en una tabla por opcode (`oparg_size[op]` + `cache_size[op]`) que también sirve para el disassembler.

**Coste**: el iterador del compilador y el del disassembler tienen que saber el tamaño total de cada opcode. Lo manejamos con una función única `opcode_tamano_total(op)` que ambos consultan.

### Quickening: rewrite in-place del opcode

Primera ejecución de `OP_OBTENER_GLOBAL` mira el dicc, encuentra el slot, **escribe `OP_OBTENER_GLOBAL_CACHE` en `chunk->codigo[pc]`** y rellena los bytes de cache. Las ejecuciones siguientes despachan al opcode cacheado directamente sin chequear "está cacheado?".

**Por qué quickening (rewrite) en lugar de chequeo en cada hit:**

- Una rama menos por dispatch. En el hot path importa.
- El opcode "lento" original solo se ejecuta una vez (típicamente). Tras eso, todos los aciertos van por el camino especializado puro.
- Cornamusa es single-threaded → no hay race condition al escribir el byte. (Si en el futuro se añade hilos, hay que repensar — pero v1.x no lo hace.)

### Cache miss en el opcode rápido = de-quickening

Si el `OP_OBTENER_GLOBAL_CACHE` ejecuta y la versión del dicc no coincide (alguien añadió/borró globals), **revertimos** a `OP_OBTENER_GLOBAL` reescribiendo el opcode y dejando que el lento rellene el cache otra vez.

Sin tier-2: no marcamos sites "polimórficos" para tratarlos distinto. Si un site oscila entre tipos, rotará entre `OP_X_CACHE` y `OP_X` — paga el coste pero converge si se estabiliza.

### Versionado de invalidación

Cada estructura cacheada (Diccionario, Clase) lleva un `version: uint64_t` que se incrementa al mutarse:

- `Diccionario.version`: incrementa al insertar, borrar, o redimensionar la tabla.
- `Clase.version`: incrementa al añadir/redefinir métodos o cambiar superclase.

El cache slot guarda la versión que vio en el último acierto. En el hot path comparamos `cache.version == estructura.version` — si igual, fast path; si no, miss path.

Coste por cache slot: 2 bytes de versión (u16, wraparound aceptable: si llegas a 65535 mutaciones, todos los caches se invalidan una vez — irrelevante en la práctica).

### Opcodes a especializar en F10

Por orden de impacto esperado:

| Opcode original          | Opcode especializado          | Cache slot           | Sesión |
|--------------------------|-------------------------------|----------------------|--------|
| `OP_OBTENER_GLOBAL`      | `_CACHE`                      | version + slot_idx   | 2-3    |
| `OP_OBTENER_LOCAL`       | _(ya es rápido — saltarlo)_   | —                    | —      |
| `OP_LLAMAR`              | `_FN_NATIVA` / `_FN_BC` / `_CLASE` | tipo callee     | 4-5    |
| `OP_BINARIO_*` (suma, resta, mul) | `_INT_INT` / `_FLT_FLT`   | tipos esperados      | 6      |
| `OP_OBTENER_ATRIBUTO`    | `_INSTANCIA_SHAPE`            | clase + slot_idx     | 7      |

`OP_OBTENER_LOCAL` es ya un acceso a `frame->slots[i]`, una operación O(1) tipo array. No hay nada que cachear que mejore eso. Lo dejamos fuera.

### Lo que F10 NO incluye

Quedan explícitamente fuera, para post-v1.0:

- **Tier-2 / superinstrucciones**: combinar pares hot (`OP_OBTENER_LOCAL` + `OP_LLAMAR`) en un superopcode. Beneficio claro pero requiere medir qué pares dominan.
- **Tracing**: detectar bucles calientes y compilar el cuerpo a una traza linear. Es el siguiente nivel — mejor con datos reales.
- **JIT**: traducir a código máquina. Salto cualitativo, post-tracing.
- **Threaded code dispatch (computed gotos)**: 10-15% de mejora general pero requiere mantenimiento doble (GCC vs MSVC). Considerar en v1.1 si la diferencia se siente.
- **Shape caching avanzado** (hidden classes con transiciones tipo V8): en F10 hacemos shape simple = clase + slot. Transiciones requieren reescribir el modelo de instancia.

## Verificación end-to-end

Cada sesión de F10 termina con:

1. Todos los tests previos verde (78 unitarios + integración + diferenciales). Especialmente los diferenciales tree-walking ↔ bytecode — cualquier cache mal invalidado se manifiesta como divergencia de salida.
2. Los benchmarks de `benchmarks/` corren y muestran ganancia medible respecto a v0.9.2 baseline para los workloads relevantes.
3. ASan + UBSan pasan en el job de CI nuevo (especialmente importante: el rewriting in-place de bytecode es exactamente el tipo de código donde un off-by-one rompe heap silenciosamente).
4. CHANGELOG entry detallada, no genérica.

## Riesgos

- **Cache mal invalidado** → resultado incorrecto silencioso. Mitigación: tests diferenciales tree-walking vs bytecode, sanitizers en CI, tests específicos por opcode especializado que muten la estructura entre llamadas.
- **Code size growth** → más opcodes, más switch cases, más complejidad de disassembler. Mitigación: macros para definir pares lento/rápido juntos, tabla `opcode_metadata[]` que centralice tamaños y nombres.
- **Quickening race** si en el futuro se añaden hilos. Mitigación: documentado en este file, NO añadir hilos en v1.x sin replantear cache slots como atomic.

## Consecuencias

- `CORNAMUSA_BYTECODE_VERSION` pasa a `2` cuando aterrice el primer opcode con cache slot inline. Ya está documentado en chunk.h.
- El disassembler (`debug.c`) gana una columna "cache" para mostrar slots cacheados con su contenido.
- La ABI del intérprete cambia internamente — irrelevante hoy (chunks no se serializan), relevante si en post-F10 se introduce `.cornc` cache files.
