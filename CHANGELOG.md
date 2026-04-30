# Registro de cambios

Todos los cambios notables a este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto adhiere a [Versionado Semántico](https://semver.org/lang/es/).

## [No publicado]

## [0.11.6] — 2026-04-30 — FIX: stack growth en mientras con nuevo local

Bug latente derivado del fix de v0.11.5 detectado al validar
`examples/25_biblioteca_oop.cor` durante la sesión 5 del plan v1.0.
Síntoma: "OP_ITER_SIGUIENTE sin iterador en slot N" tras 2+
iteraciones de un `mientras` que asignaba un nuevo local en su cuerpo.

### Bug

```cornamusa
funcion main():
    rondas = 3
    suma = 0
    mientras rondas > 0:
        n = rondas + 10        # nuevo local 'n' en cada iter
        suma = suma + n
        rondas = rondas - 1
    fin mientras
fin funcion
main()
```

Antes de v0.11.6: tras la 2ª iteración, error de runtime sobre el
stack desincronizado.

### Causa raíz

El fix de v0.11.5 emitía OP_NULO + push valor + OP_ASIGNAR_LOCAL
para nuevos locales, lo que crece el stack +1 por iteración. Para
`para` no había problema porque SENT_PARA pre-reserva el slot de su
variable de iteración fuera del cuerpo. Para `mientras` no había
pre-reserva — cada iteración acumulaba un OP_NULO sin consumir.

### Fix

Nueva función `pre_reservar_locales(c, sent, linea)` en
`src/compilador.c` que recorre el AST del cuerpo (recursivo por
SENT_BLOQUE y SENT_SI; no desciende en SENT_FUNCION/SENT_CLASE/
sub-bucles) buscando SENT_ASIGNAR a IDENT no-local existente. Por
cada uno emite OP_NULO + `agregar_local` UNA vez antes del cuerpo.

Llamada desde `compilar_mientras` y `compilar_para` antes de
compilar el cuerpo. Las asignaciones dentro del cuerpo ahora
encuentran "local existente" y emiten plain OP_ASIGNAR_LOCAL —
stack neutro por iteración.

### Tests

- Nuevo `test_regresion_mientras_con_nuevo_local`: ejercita el
  patrón exacto. 96 tests verde.
- `examples/25_biblioteca_oop.cor` restaurado con el algoritmo de
  selección iterativa (top-3 más prestados) que descubrió el bug,
  ahora funciona.

### Lección

Patrón repetido de v0.11.5: bug en un fix anterior detectado al
validar contra programas reales (no micro-tests). El test suite
ahora incluye un caso por cada bug histórico de scoping local.

## [0.11.5] — 2026-04-30 — FIX CRÍTICO: nuevo local en bucle dentro de función

Bug serio de correctness en el bytecode VM, descubierto al validar el
tutorial de v1.0 (sesión 3 del plan B10). El tree-walking interpreter
no tenía el bug; los 8 tests diferenciales tree-walking↔bytecode
existentes no lo detectaron porque sus ejemplos no usaban el patrón.

### Bug

Cuando una asignación a un **nuevo local** ocurre dentro de un bucle
dentro de una función, el slot del local quedaba con el valor de la
primera iteración para siempre.

```cornamusa
funcion main():
    para v en [1, 2, 3]:
        a = v + 10                  # nuevo local 'a'
        imprimir("v=", v, "a=", a)
    fin para
fin funcion
main()
```

Antes de v0.11.5:
```
v= 1 a= 11
v= 2 a= 11      ← bug: siempre 11
v= 3 a= 11      ← bug: siempre 11
```

Ahora v0.11.5 (correcto):
```
v= 1 a= 11
v= 2 a= 12
v= 3 a= 13
```

El bug afecta a **cualquier** programa con asignación a local nuevo
dentro de un bucle dentro de función. Es un patrón extremadamente
común en código real.

### Causa raíz

`compilar_asignar` en `src/compilador.c` usaba la "OLD convention"
para nuevos locales: empujar el valor + `agregar_local` SIN emitir
`OP_ASIGNAR_LOCAL`, asumiendo que el push deja el valor en el slot
recién creado. Eso solo es cierto en la PRIMERA ejecución del
bytecode emitido. Dentro de un bucle, el bytecode se ejecuta
múltiples veces y en iteraciones siguientes el push va a un stack
pos distinto del slot fijado en compile-time, dejando el slot con
el valor de la primera iteración.

### Fix

Para nuevos locales en función:

1. Emitir `OP_NULO` (reserva el slot en stack).
2. `agregar_local` (registra el nombre y fija el slot index).
3. Compilar la expresión del valor (push).
4. `OP_ASIGNAR_LOCAL` al slot recién creado (pop + asign).

El push del placeholder + asignación explícita funciona en cualquier
iteración porque el stack queda con el mismo n elementos al inicio
y al fin de cada iter.

### Impacto en otras versiones

Este bug ha estado presente desde **antes de v0.11** — probablemente
desde v0.6 cuando se introdujo el bytecode VM. Los benchmarks no lo
detectaron porque usaban variables globales (no locales nuevas en
bucles). Los tests integración no lo detectaron porque no usaban el
patrón. **v0.6.0 hasta v0.11.4 inclusive contienen este bug**.

Después de la sesión, el camino crítico de programas reales
funciona correctamente. Cualquier programa que demostraba algo
"raro" (resultado constante donde debía variar) probablemente
estaba afectado.

### Tests

- Nuevo `test_regresion_local_nuevo_en_bucle` en `test_bytecode_ic.c`:
  ejecuta el patrón exacto del bug y verifica `suma = 11 + 12 + 13 = 36`.
- 92 tests verde (incluye el nuevo).
- Bench sin regresiones: globales_lookup ~218ms (igual que v0.11.4).

### Lección

Tutorial validado contra el intérprete real es una práctica esencial.
El bug llevaba meses ahí; nadie lo había detectado porque los
ejemplos test eran demasiado micro. Lo capturamos porque escribir
un programa que un usuario humano escribiría reveló el patrón
inmediatamente.

## [0.11.4] — 2026-04-30 — fix hash divergente en banda 2^62..2^63

Cierra tech-debt #6 de la revisión post-release v0.11.1: bug latente
de hash entre `VAL_ENTERO` BIG y `VAL_DECIMAL` con el mismo valor
numérico cuando ambos caían en la banda `[2^62, 2^63)`.

### Bug corregido (v0.11.4)

- `valor_a_int64_si_cabe` (en `src/valor.c`) rechazaba BIG con
  `mp_count_bits > 62`, pero un DECIMAL del mismo valor pasaba por
  el camino i64 (rango ±9.2e18). Resultado: hashes divergentes.
  Ejemplo:
  ```cornamusa
  d = {}
  d[2 ** 62] = "uno"             # clave guardada como BIG
  imprimir(d[2.0 ** 62])          # antes: ErrorDeClave (slots distintos)
                                  # ahora: "uno" (mismo slot)
  ```
- Fix de 1 línea: `mp_count_bits > 62` → `mp_count_bits >= 64`.
  Esto acepta hasta magnitud 63 bits (rango int64 completo excepto
  INT64_MIN cuya magnitud es exactamente 64). Ahora BIGs en
  `[INT64_MIN+1, INT64_MAX]` y DECIMALs equivalentes hashean al
  mismo slot, manteniendo la invariante `a == b ⇒ hash(a) == hash(b)`.

### Tests (v0.11.4)

- Nuevo `test_hash_banda_2_62` en `test_small_int.c`: construye un
  BIG con valor 2^62 y un DECIMAL con valor 2^62.0; verifica que
  `dicc_asignar(dict, BIG)` permite recuperar con clave DECIMAL.
- 14 tests boundaries totales (de 13 en v0.11.3).
- 92 tests en suite completa.

### Pendiente para v0.12+

Tras este patch, los tech-debt restantes documentados son:
- #2: helpers `valor_entero_a_mp_int` (público, no acepta bool) vs
  `como_mp_int` (privado en evaluador.c, sí acepta bool). Aún
  duplicados; razón legítima de existir, pero la convergencia hacia
  un solo helper queda pendiente.
- #4: MSVC fallback de `__builtin_mul_overflow` con cota int31.
- #8: `long → int64_t` en indexación para Windows LLP64.
- Threaded code dispatch (computed gotos): refactor masivo de
  ~200 cambios al switch del VM por solo ~10-15% de ganancia.
  ROI/coste no compensa frente a un pivote a v1.0 (Fase 11.2).

## [0.11.3] — 2026-04-30 — constant folding en compilador

Pulido del pipeline de compilación. Expresiones cuyos operandos son
todos constantes ahora se reducen a un único `OP_CONST` en compile-time
en lugar de emitir bytecode aritmético.

### Añadido (v0.11.3)

- **`evaluar_constante` en `compilador.c`**: helper recursivo que
  intenta evaluar una expresión en compile-time. Soporta:
  - Literales (`NULO`, `BOOLEANO`, `ENTERO`, `DECIMAL`, `CADENA`).
  - `EXPR_GRUPO` (paréntesis) recursivo.
  - `EXPR_UNARIO` (`-x`, `+x`, `no x`, `~x`) cuando el operando es
    constante.
  - `EXPR_BINARIO` (todas las aritméticas/comparaciones/lógicas)
    cuando ambos lados son constantes.
- Reusa `evaluador_aplicar_unario` y `evaluador_aplicar_binario` —
  el folding produce semánticamente lo mismo que el runtime.
- **No foldeamos errores**: si la operación produciría un error
  (división por cero, tipos incompatibles), el folding aborta y
  dejamos al runtime reportar el error en su línea original.

### Patrones que ahora se foldean

```cornamusa
SEGUNDOS_DIA = 60 * 60 * 24      # → OP_CONST 86400
AREA = 3.14159 * 10 * 10         # → OP_CONST 314.159
MENSAJE = "Hola, " + "mundo"     # → OP_CONST "Hola, mundo"
LIMITE = 2 ** 16                 # → OP_CONST 65536
PUEDE = 5 < 10 y 3 > 1           # → OP_CONST true
```

### Tests (v0.11.3)

- Tests del IC actualizados (`test_bytecode_ic.c`): los casos que
  antes hacían `1 + 2` para verificar `OP_SUMAR_INT_INT` ahora usan
  variables intermedias (`k0 = 1; k1 = 2; a = k0 + k1`) para evitar
  que el folding eluda el opcode bajo prueba. La especialización
  IC sigue funcionando correctamente, pero se valida con código
  realista (variables locales/globales, no literales).

92 tests verde.

### Notas (v0.11.3)

- El impacto en benchmarks `benchmarks/*.cor` es marginal porque
  esos workloads no tienen aritmética constante en hot loops. El
  beneficio real es en código de aplicación: definiciones de
  constantes nombradas, fórmulas pre-computables, configs.
- Aún no foldeamos llamadas a built-ins puros como `longitud("hola")`
  o `mat.PI * 2`. Sería natural en una sesión futura — solo requiere
  whitelist de funciones puras.

## [0.11.2] — 2026-04-30 — fast-path int64 en iterador de `rango`

Tech-debt #5 de la revisión post-release de v0.11.1 cerrado. Programas
con loops grandes (`para i en rango(N)`) eran cuello porque el
iterador alocaba un `mp_int` nuevo cada paso aunque inicio/fin/paso
cupieran en SMALL.

### Mejoras (v0.11.2)

- **Camino rápido int64 en `iter_siguiente` para `VAL_RANGO`**: si
  inicio, fin y paso caben en `int64_t` (chequeado vía `mp_count_bits
  < 64` por valor), calculamos `inicio + cursor*paso` directamente
  con aritmética nativa. Detección de overflow vía
  `__builtin_mul_overflow`/`add_overflow` en GCC/Clang; en MSVC
  fallback con cota `int31` para cursor y paso.
- En overflow o si algún componente del rango excede `int64`, fallback
  al path bignum existente. Sin pérdida funcional.
- El resultado pasa por `valor_entero_de_i64` que produce SMALL
  cuando cabe, BIG si no.

### Corregido (v0.11.2)

- **Leak preexistente**: en `iter_siguiente` para `VAL_RANGO`, si
  `mp_init(resultado)` succeeds y `mp_copy(...)` falla, antes se
  llamaba `free(resultado)` sin `mp_clear` — perdía los `digits`
  alocados por `mp_init`. Ahora hace `mp_clear + free` correctamente.
  Reportado por la revisión post-release como tech-debt 5b.

### Rendimiento (v0.11.2)

Comparación contra v0.11.1 (mediana de 3 corridas):

| Benchmark             | v0.11.1  | v0.11.2  | Mejora |
|-----------------------|----------|----------|--------|
| globales_lookup       | 391 ms   | **218 ms** | **1.79x** |
| dicc_intensivo        | 59 ms    | **50 ms**  | 1.18x |
| fibonacci_recursivo   | 222 ms   | 235 ms   | (~igual; no usa rango) |
| bignum_factorial      | 17 ms    | 27 ms    | (variabilidad) |
| oo_intensivo          | 24 ms    | 32 ms    | (variabilidad) |

**Comparación acumulada vs v0.10.0 baseline**:

| Benchmark             | v0.10  | v0.11.2 | Total |
|-----------------------|--------|---------|-------|
| globales_lookup       | 993 ms | 218 ms  | **4.55x** |
| dicc_intensivo        | 121 ms | 50 ms   | 2.42x |
| fibonacci_recursivo   | 1.33 s | 235 ms  | 5.66x |

Geomedia consolidada: ~3.0x sobre v0.10.

92 tests verde.

## [0.11.1] — 2026-04-30 — fixes post-release (revisión crítica)

Code review crítica independiente del refactor B9 detectó tres
problemas de calidad y un bug latente. Esta versión los corrige
sin cambios de comportamiento observable.

### Corregido (v0.11.1)

- **Bug latente — `valor_entero_a_mp_int` no inicializaba `*propio`**
  cuando `nuevo_mp_int()` fallaba (OOM). Los callers que leyeran
  `propio` para decidir si liberar leerían memoria sin inicializar.
  Fix: `*propio = false` al inicio de la función. Severidad baja
  en la práctica (OOM raro) pero la API pública debe ser robusta.
- **Comentarios stale en `valor.c`** que describían "sesión 1, BIG
  siempre" cuando el código ya producía SMALL. Reescritos para
  reflejar el comportamiento actual de v0.11.0.
- **Comentario engañoso en `valor_entero_a_i64`** ("comparar
  mp_count_bits con 63") cuando el código compara `< 64`.
  Reescrito explicando que `< 64` significa "magnitud ≤ 63 bits"
  y que `INT64_MIN` queda excluido a propósito (SMALL_INT_MIN =
  -2^62, así no perdemos rango útil).
- **Función no usada `evaluador_valor_entero_de_mp` eliminada** del
  API pública. La función `static valor_entero_de_mp` también
  eliminada (warning `-Wunused-function`). Toda la producción
  pasa por `valor_entero_de_mp_normalizado` ahora.

### Tests reforzados (v0.11.1)

- **Nuevo test `test_smallmin_mult_neg1`**: cubre `SMALL_MIN * -1`,
  caso peligroso B9 §4 que no estaba en la suite original.
- **Validador común `verificar_overflow_promueve`** — los tests de
  overflow ahora validan EXPLÍCITAMENTE ambas ramas (`aplic=true`
  con BIG y valor correcto, o `aplic=false` con sentinel nulo). En
  v0.11.0 los tests usaban `if (aplic) { ... }` sin else, así
  que pasaban silenciosamente con cualquier implementación que
  jamás reportara `aplic=true`.
- 13 tests boundaries en total (de 12 en v0.11.0).

### Conocido para v0.12+ (post-release)

La revisión crítica también identificó tres tech-debt no urgentes:

- Iter de `VAL_RANGO` aloca `mp_int` por cada paso aunque inicio,
  fin y paso quepan en `int64_t` — fast-path SMALL no implementado
  en `valor.c::iter_siguiente`. Beneficiaría loops grandes como
  `para i en rango(1_000_000)`.
- MSVC fallback de `__builtin_mul_overflow` en `small_op_small` es
  conservador (rechaza si cualquier operando excede int32). En
  GCC/Clang ya está bien.
- Hash divergente en banda 2^62..2^63 entre BIG y DECIMAL con mismo
  valor numérico (preexistente, no introducido por B9). El refactor
  B9 era el momento natural de armonizarlo y se dejó pasar.
- Migración `long → int64_t` en indexación para Windows (LLP64).

Ninguno bloquea el uso de v0.11.1 — son optimizaciones y limpieza
para una sesión futura.

## [0.11.0] — 2026-04-30 — Small-int tagging (Fase 11.1)

Segunda fase de optimización de rendimiento, basada en la decisión
[B9](decisiones/B9-small-int-tagging.md). Enteros que caben en 63 bits
ahora viven inline en la unión `Valor.como.entero_small` (`int64_t`),
sin alocar `mp_int` ni invocar `mp_init`/`mp_clear`. La aritmética
SMALL+SMALL es directa en `int64_t` con detección de overflow.

Resultado: **~2.7x geomedia sobre v0.10**, **~6x en programas
recursivos numéricos**.

### Cambios principales (v0.11.0)

**Representación de Valor**:
- Nuevo tag `VAL_ENTERO_SMALL` en `TipoValor`. La unión `Valor.como`
  gana un campo `int64_t entero_small` junto al `mp_int *entero`
  existente.
- Rango: `CORNAMUSA_SMALL_INT_MAX = 2^62 - 1`, `_MIN = -2^62`.
  Reservamos 1 bit de margen respecto a `int64_t` para que la suma
  de dos SMALL caben sin UB en C — necesario para el camino rápido
  sin `__builtin_add_overflow` (MSVC fallback).
- `VAL_ENTERO` (BIG) sigue siendo `mp_int *` para enteros grandes.
  Las operaciones que producen ints normalizan: si el resultado cabe
  en SMALL se devuelve como tal; si no, BIG.

**API canónica** (en [src/valor.h](src/valor.h)):
- `valor_es_entero(v)`: predicado que sustituye `v->tipo == VAL_ENTERO`.
- `valor_entero_a_i64(v, *out)`: extrae como `int64_t` si cabe.
- `valor_entero_a_mp_int(v, *propio)`: extrae como `mp_int *`,
  alocando un temporal si es SMALL (flag *propio para liberar).
- `valor_entero_de_i64(n)`: constructor canónico — SMALL si cabe,
  BIG si no.
- `valor_entero_de_mp_normalizado(m)`: constructor desde mp_int* con
  demote automático a SMALL si el valor cabe.

**Aritmética SMALL+SMALL** (en [src/evaluador.c](src/evaluador.c)):
- Helper `evaluador_small_op_small` con dispatcher para `+`, `-`,
  `*`, `//`, `%`. Usa `__builtin_*_overflow` (GCC/Clang) o detección
  manual (MSVC).
- Casos especiales manejados:
  - Overflow → reportado como no-aplicable, fallback al path BIG.
  - `SMALL_MIN / -1` (UB en C, `INT_MIN/-1`) → no-aplicable.
  - División por cero → error explícito.
  - Módulo Python-style (signo del divisor): `-7 % 3 = 2`.
- `entero_op_entero` (path BIG existente) ahora normaliza el resultado
  con `valor_entero_de_mp_normalizado` — `100000 - 99999 = 1` se
  devuelve como SMALL, no como BIG.
- `comparar_valores`: camino rápido `int64_t` inline si ambos
  operandos caben en `i64`, fallback a `mp_cmp` si alguno es BIG
  fuera de rango.

**IC bytecode con SMALL** (en [src/vm.c](src/vm.c)):
- Macros `BIN_INT_INT_ARITH` y `BIN_INT_INT_CMP` reescritas con tres
  caminos: SMALL+SMALL inline, BIG+BIG via `mp_*`, mezcla degrada al
  slow path.
- El camino SMALL+SMALL invoca `evaluador_small_op_small`. Si
  overflow, fallback a `mp_int` temporales que normalizan resultado.
- Los literales numéricos del parser (`valor_entero_de_lexema`) se
  pasan por `valor_entero_de_mp_normalizado` — los literales
  pequeños son SMALL desde el principio.

**Migración masiva** (sesión 3): 72 sitios que comprobaban
`v.tipo == VAL_ENTERO` migrados a `valor_es_entero(&v)`. 22 sitios
adicionales que leían `v.como.entero` directo migrados a usar los
helpers (`valor_entero_a_i64` o `valor_entero_a_mp_int` con cleanup
de temporal). Cubren `valor_a_doble`, indexación, rebanada,
construcción de rango, comparador de ordenamiento, repetición de
cadena, unario `-` y `~`, etc.

### Tests (v0.11.0)

- **12 tests boundaries nuevos** en [tests/unit/test_small_int.c](tests/unit/test_small_int.c):
  - Constructor en frontera (SMALL_MIN/MAX caben; ±1 promueven a BIG).
  - `valor_entero_de_mp_normalizado` demote correcto.
  - Igualdad cross-tag: `SMALL(5) == BIG(5) == 5.0 == True`.
  - Hash equivalente: `dicc[SMALL(5)]` y `dicc[BIG(5)]` acceden al
    mismo slot — invariante crítica.
  - Overflow promueve correctamente a BIG (suma, resta, mult).
  - `SMALL_MIN / -1` reportado como no-aplicable.
  - División por cero, módulo Python-style.
  - Clone preserva tipo. Helpers de extracción funcionan para ambos
    tags.
- 92 tests verde en total (91 previos + el nuevo `test_small_int`).
- Tests existentes actualizados donde asumían representación
  pre-v0.11 (`test_runtime_valor`, `test_chunk_disasm`,
  `test_runtime_evaluador`).

### Rendimiento (v0.11.0)

Mediana de 5 corridas, binario v0.10.0 desde su tag vs binario v0.11.0
HEAD, ambos en CMake Release:

| Benchmark             | v0.10.0  | v0.11.0  | Mejora |
|-----------------------|----------|----------|--------|
| bignum_factorial      | 29 ms    | 17 ms    | **1.71x** |
| dicc_intensivo        | 121 ms   | 59 ms    | **2.05x** |
| fibonacci_recursivo   | 1.33 s   | 222 ms   | **5.98x** |
| globales_lookup       | 993 ms   | 391 ms   | **2.54x** |
| oo_intensivo          | 44 ms    | 24 ms    | **1.83x** |

**Geomedia ≈ 2.7x.**

`fibonacci_recursivo` excede el plan B9 (3-5x prometido). El cuello
ya no es allocación de `mp_int` — son las llamadas recursivas y el
dispatch general (que F10 ya optimizó hasta lo razonable).

`bignum_factorial` mejora menos en absoluto porque su loop interno
hace `r * i` con `r` que crece hasta 1000 dígitos (BIG persistente):
el ahorro está en `i` (SMALL) y en evitar alocaciones temporales,
pero `mp_mul` sigue dominando.

### Decisión arquitectónica (v0.11.0)

Opción **B** del documento [B9](decisiones/B9-small-int-tagging.md):
nuevo tag explícito en `TipoValor`, no tagged pointer.

Razones documentadas:
- Type-safety: el compilador C grita en cada switch que falte
  adaptarse (gracias a `-Wswitch`). El sistema de tipos hace de
  checklist.
- Auditable: cada acceso a `como.entero` queda visible en grep, y
  cualquier sitio que lea SMALL como `mp_int *` se manifiesta como
  segfault o test failure inmediato (no corrupción silenciosa).
- Migración progresiva: la API de helpers permitió migrar el código
  base en pasos pequeños, con tests verde después de cada commit.

Opción A (tagged pointer en bit 0 del `mp_int *`) rechazada por
riesgo de bugs silenciosos. Opción C (NaN-boxing) aplazada a un
hipotético v2.0 si se demuestra necesaria.

### Notas (v0.11.0)

- API pública (.cor scripts) sin cambios. El usuario no nota
  diferencia salvo en velocidad — programas que antes corrían
  correctamente siguen corriendo correctamente con los mismos
  resultados.
- Los tests diferenciales tree-walking vs bytecode (8 ejemplos)
  fueron la red de seguridad principal del refactor: cualquier
  divergencia entre paths SMALL y BIG se manifestaría como test
  rojo. Permanecieron verde durante todo el ciclo.
- ASan + UBSan en CI (job `sanitizers`) garantizó que las
  conversiones SMALL ↔ BIG no introdujeran heap corruption ni UB
  detectables.

### Pendiente para futuro (post-v0.11)

- **Threaded code dispatch** (computed gotos): ~10-15% global en
  GCC/Clang; MSVC requiere doble path. Considerar como v0.12.
- **Constant folding en compilador**: `1 + 2` se computa en
  compile-time. Pequeño pero gratis.
- **GC generacional + tier-2 IC + tracing**: trabajo mayor para
  v1.x.

## [0.10.0] — 2026-04-30 — Inline caching especializado tipo PEP 659 (Fase 10)

Primera fase de optimización de rendimiento. Cuatro tandas de
especializaciones implementadas en 5 sesiones de trabajo
(detalladas en [decisiones/B8-inline-caching.md](decisiones/B8-inline-caching.md)).
Quickening por reescritura del byte del opcode in-place; cache slots
inline en el bytecode para los opcodes con cache versionada (PEP 659
style, no side-table).

### Especializaciones nuevas (v0.10.0)

**Lookup de globales** (sesión 2):
- `OP_OBTENER_GLOBAL` ahora ocupa 6 bytes (opcode + name_idx + 4 bytes
  de cache). Slot inline guarda los 16 bits bajos de
  `Diccionario.version` y el slot_idx en `entradas`. Tras un acierto
  promueve a `OP_OBTENER_GLOBAL_CACHE` que lee directamente
  `entradas[slot_idx]` sin hashing ni probing. Miss → degrada y
  rebobina ip.
- `Diccionario.version` (uint64_t) bumpea solo en cambios
  estructurales (insert nuevo, remove, resize). NO en sobreescritura
  — preserva el cache para `contador = contador + 1` en hot loop.

**Llamadas a función** (sesión 3-4):
- 4 variantes especializadas, sin cache slot (el byte del opcode es
  el cache):
  - `OP_LLAMAR_NATIVA` para `VAL_NATIVA`
  - `OP_LLAMAR_BC` para `VAL_FUNCION_BC` (closure)
  - `OP_LLAMAR_CLASE` para `VAL_CLASE` (instanciación + `__iniciar__`)
  - `OP_LLAMAR_METODO_LIGADO` para `VAL_METODO_LIGADO`
- Refactor: cada cuerpo de rama de `OP_LLAMAR` extraído a helper
  `static` (`ejecutar_llamar_<tipo>`). Slow path captura el chunk del
  caller antes de que el helper push'ee un frame nuevo, para
  promover el opcode en el chunk correcto.

**Aritmética y comparaciones int+int** (sesión 5-6):
- `OP_SUMAR_INT_INT`, `OP_RESTAR_INT_INT`, `OP_MULTIPLICAR_INT_INT`
  llaman `mp_add`/`mp_sub`/`mp_mul` directamente, saltándose el
  switch general de tipos de `evaluador_aplicar_binario`.
- `OP_MENOR_INT_INT`, `OP_MENOR_IGUAL_INT_INT`, `OP_MAYOR_INT_INT`,
  `OP_MAYOR_IGUAL_INT_INT` con `mp_cmp` directo.
- Helpers de bignum (`nuevo_mp`, `liberar_mp`, `valor_entero_de_mp`)
  expuestos en `evaluador.h` para que `vm.c` no duplique gestión de
  `mp_int`.

**Acceso a atributos de instancia** (sesión 7-8):
- `OP_OBTENER_ATRIBUTO` ahora 6 bytes con cache de
  (clase_hash u16, slot_idx u16). Los 16 bits bajos del puntero a la
  clase filtran cross-class; el slot_idx apunta a `instancia.atributos`.
- `OP_OBTENER_ATRIBUTO_INSTANCIA` (fast path) verifica:
  1. obj es `VAL_INSTANCIA`
  2. low16(clase) coincide con cache
  3. slot ocupado
  4. clave guardada coincide con el nombre esperado (memcmp corto)
- El check (4) es esencial: instancias de la misma clase pueden
  tener layouts distintos si fueron mutadas dinámicamente.

### Infraestructura (v0.10.0)

- **ASan + UBSan en CI** (Linux/Clang Debug): job `sanitizers` en
  `.github/workflows/build.yml` que compila con
  `-fsanitize=address,undefined -fno-omit-frame-pointer` y corre
  todos los tests. Captura corrupciones de heap y UB que serían
  invisibles en builds Release. Importante para F10 que hace
  rewriting in-place de bytecode.
- **`CORNAMUSA_BYTECODE_VERSION = 1`** en `chunk.h` (decisión I7):
  marcador de formato. Se bumpea cuando el layout binario rompa
  compatibilidad. Hoy los chunks no se serializan a disco; la
  constante prepara el terreno para futuras herramientas (`.cornc`
  cache files, inspector externo).
- **Decisión [B8-inline-caching.md](decisiones/B8-inline-caching.md)**
  documentando arquitectura, riesgos, opcodes pendientes para
  post-v1.0 (tier-2, tracing, threaded code dispatch, JIT).

### Tests (v0.10.0)

- **11 tests unitarios nuevos en `test_bytecode_ic.c`**: validan
  quickening básico, hits múltiples estables, invalidación en
  insert nuevo, no-invalidación en sobreescritura, promoción de
  `OP_LLAMAR` a `_NATIVA`/`_BC`, degradación polimórfica,
  promoción de binarios a `_INT_INT`, mezcla de tipos en mismo site,
  shape cache de atributos.
- 91 tests verde totales (incluye los 8 diferenciales tree-walking
  vs bytecode — críticos para garantizar que el quickening no
  introduce divergencia semántica).

### Rendimiento (v0.10.0)

Mediana de 5 corridas, cada benchmark contra binario v0.9.2 y v0.10.0
construidos con CMake Release:

| Benchmark             | v0.9.2  | v0.10.0 | Mejora |
|-----------------------|---------|---------|--------|
| bignum_factorial      | 33 ms   | 18 ms   | **1.83x** |
| oo_intensivo          | 50 ms   | 37 ms   | **1.35x** |
| dicc_intensivo        | 157 ms  | 130 ms  | 1.21x |
| globales_lookup       | 1.18 s  | 1.04 s  | 1.14x |
| fibonacci_recursivo   | 1.47 s  | 1.44 s  | 1.02x |

**Geomedia ≈ 1.30x** (30% más rápido).

`fibonacci_recursivo` mejora poco porque su cuello dominante es la
asignación de `mp_int` por operación bignum, no el dispatch.
Optimizar eso requeriría small-int tagging (i63 para enteros que
caben en 63 bits) o pool de `mp_int` — quedan como trabajo
post-F10, posiblemente v0.11.

### Notas (v0.10.0)

- El IC introduce riesgo de bugs por cache mal invalidado. Los tests
  diferenciales son la red de seguridad principal — cualquier
  divergencia de salida entre tree-walking (sin IC) y bytecode (con
  IC) se manifiesta como test rojo.
- Todos los cache slots son zero-init en chunks recién emitidos. El
  primer hit del slow path los rellena. Si un programa solo ejecuta
  un site UNA vez, paga 4 bytes extra de chunk sin beneficiarse —
  aceptable.
- Sites polimórficos (que oscilan entre tipos) pagan el coste de
  rewrite en cada cambio. La detección de polimorfismo y degradación
  permanente se queda como trabajo post-v1.0 (tier-2 PEP 659).

## [0.9.2] — 2026-04-29 — pulido pre-v1.0: stdlib `sistema`, tests diferenciales, benchmarks

Pasada de madurez antes de decidir entre F10 (inline caching) y F11 (v1.0
final). **Sin nueva semántica de lenguaje** — solo herramientas alrededor
del intérprete que faltaban para que un usuario externo pueda llegar al
repo y orientarse. **90 tests verde**.

### Añadido (v0.9.2)
- **Stdlib `sistema`**: nuevo módulo en `stdlib/sistema.cor` que expone
  `sistema.argv` (lista de argumentos del programa) construida sobre el
  built-in nativo `obtener_argv()`.
- **Built-in `salir(codigo)`**: termina el proceso con el código
  indicado (entero o booleano). Disponible globalmente sin importar
  nada. Implementado como nueva nativa en `src/nativos.c`.
- **`nativos_set_argv(argc, argv)`**: hook que `main.c` llama tras
  parsear los flags, pasando los argumentos del programa (a partir
  del `.cor` ejecutado) para que `obtener_argv()` los devuelva.
- **Tests diferenciales tree-walking vs bytecode** (`tests/integracion/diff_motores.cmake`):
  para los 8 ejemplos compatibles con ambos motores, ejecutamos cada
  uno con `cornamusa` y `cornamusa --bytecode`, capturamos stdout en
  ficheros, y comparamos byte a byte con `cmake -E compare_files`.
  Red de seguridad ante regresiones semánticas.
- **Benchmarks baseline** en `benchmarks/`: 4 micro-benchmarks
  (fibonacci recursivo, dicc intensivo, OO intensivo, factorial
  bignum) + scripts `run.sh` y `run.ps1` para medir tiempos. Numbers
  baseline documentados en `benchmarks/README.md` para futura
  comparación con F10.
- **Ejemplo `examples/23_sistema_jugable.cor`** demostrando
  `sistema.argv` y `salir(0)`.
- **README al día**: badge actualizado a v0.9.2, características
  reflejan la realidad (clases, GC, módulos, stdlib, tests
  diferenciales, benchmarks), roadmap actualizado.

### Corregido (v0.9.2)
- **`OP_DESCARTAR` tras `OP_IMPORTAR`**: el frame del módulo retornaba
  `nulo` al stack del importador, dejando un valor sobrante. Causaba
  errores `OP_ITER_SIGUIENTE sin iterador en slot 0` cuando el código
  posterior usaba slots por posición. El compilador ahora descarta el
  valor explícitamente. (Bug presente desde v0.9.0 / refinado en v0.9.1
  pero el fix no entró en el commit del tag v0.9.1.)

### Notas (v0.9.2)
- 12 ejemplos podrían en teoría correr en ambos motores, pero 4
  (`03_fibonacci`, `05_listas`, `06_diccionarios`, `10_quicksort`)
  usan f-strings o desempaquetado de tuplas que ningún motor soporta
  todavía — fallan idénticamente al parsear, así que no aportan al
  diferencial. Los 8 restantes cubren básicos, control de flujo,
  listas, dicc, conjuntos, tuplas.
- `sistema.argv` solo se expone via `obtener_argv()` (no como variable
  fija), porque las globals del módulo se evalúan UNA vez al cargar
  el módulo. Si en el futuro se quiere argv reactivo, habrá que
  exponer `sistema.argv` como propiedad.
- `salir()` llama a `exit()` directamente, sin oportunidad de unwind
  ni `finalmente`. Es el comportamiento de Python `sys.exit()` con
  `os._exit()`, no con `SystemExit`. Si se necesitara un cierre
  ordenado en el futuro, habría que lanzar una excepción especial.

## [0.9.1] — 2026-04-29 — módulos completos + indexación de cadenas

Cierra la deuda funcional de v0.9.0: módulos con subsegmentos, alias,
y `desde X importar Y`. Indexación de cadenas `s[i]` ahora funciona
en bytecode con UTF-8. Stdlib `cadenas.cor` ampliada con funciones que
requieren indexación. **79 tests verde**.

### Añadido (v0.9.1)
- **`importar X.Y` (subsegmentos)**: `cargar_modulo_desde_archivo`
  traduce `.` a `/` antes del lookup. `importar mat.geometria` busca
  `./mat/geometria.cor` luego `stdlib/mat/geometria.cor`.
- **`importar X como Y` (alias)**: el módulo se carga y se cachea por
  su nombre real, pero se registra como global del importador bajo el
  alias.
- **`OP_IMPORTAR` ahora toma 2 operandos** (`module_idx`, `binding_idx`)
  para soportar alias/subsegmentos: `module_idx` es el nombre real del
  módulo (cache key), `binding_idx` es el nombre de la global (alias o
  último segmento).
- **`CallFrame.modulo_binding_name`** + `modulo_binding_len`:
  buffer heap-duplicated con el nombre del binding global, liberado en
  `OP_RETORNAR` tras registrar la global.
- **`desde X importar Y, Z` (selective import)**: nuevo opcode
  `OP_IMPORTAR_PARA_DESDE [name_idx]` que carga el módulo y lo deja
  en el tope del stack (sin registrar global). Para cada item, el
  compilador emite `OP_DUP`, `OP_OBTENER_ATRIBUTO [item_idx]`,
  `OP_DEFINIR_GLOBAL [binding_idx]`. Final `OP_DESCARTAR` retira el
  módulo. Nuevo flag `CallFrame.desde_import` para que `OP_RETORNAR`
  finalice el módulo poniéndolo en stack en vez de bindeándolo.
- **`OP_DUP`**: duplica el valor en el tope del stack (clone). Nuevo.
- **`OP_INDICE` ahora soporta `VAL_CADENA`**: indexación UTF-8 con
  `utf8proc_iterate`. Devuelve cadena de 1 carácter. Soporta índices
  negativos (cuentan desde el final). `ErrorDeIndice` si fuera de
  rango. `ErrorDeTipo` si índice no es entero.
- **`stdlib/cadenas.cor` ampliada**: funciones nuevas `caracter(s, i)`,
  `empieza_con(s, prefijo)`, `termina_con(s, sufijo)`, `contar(s, sub)`.
  Antes estaban deshabilitadas porque requerían `s[i]`.
- **9 tests nuevos** en `test_bytecode_modulos.c`: alias simple, alias
  no expone nombre original, subsegmentos compilan, `desde X importar`
  simple/multiple/alias, `desde` no expone módulo, función importada
  via desde, indexación cadena básica/negativa/UTF-8/fuera-de-rango.
- **Ejemplo `examples/22_modulos_avanzado.cor`** demostrando alias,
  desde-importar, y `s[i]` con UTF-8.

### Limitaciones documentadas en v0.9.1
- **Nuevos locales declarados dentro de cuerpos de bucles** (en función)
  no funcionan correctamente: el slot se desfasa entre iteraciones.
  Workaround: declarar el local antes del bucle. La función `contar`
  en `cadenas.cor` aplica este workaround. Resolver requiere un
  preamble de OP_NULOs en el chunk de la función + emit explícito de
  OP_ASIGNAR_LOCAL para todos los nuevos locales — refactor mediano,
  aplazado a v0.9.2 o v0.9.3.

## [0.9.0] — 2026-04-29 — módulos + stdlib mínima (Fase 9)

Cornamusa gana sistema de módulos: `importar matematicas` carga un
archivo `.cor` y expone sus globales como atributos del módulo.
Stdlib inicial con `matematicas` y `cadenas`. **78 tests verde**.

### Añadido (v0.9.0)
- **Tipo `VAL_MODULO`** + `struct Modulo` en `valor.{h,c}`: nombre +
  diccionario de atributos. Pretty-printed `<modulo X>`. tipo() reporta
  `"modulo"`. Ni hashable ni iguales por valor (identidad por puntero).
- **Opcode `OP_IMPORTAR [byte name_idx]`** en bytecode:
  1. Si el módulo está en cache (`vm->cache_modulos`), solo asigna la
     global del importador.
  2. Sino, busca el archivo (`./{nombre}.cor` luego
     `stdlib/{nombre}.cor`), lex+parse+compile.
  3. Crea un nuevo `Modulo` y un nuevo `Diccionario` para sus globales,
     poblado inicialmente con las nativas (imprimir, etc.).
  4. Empuja un sub-frame con el chunk del módulo y cambia
     `vm->globales` al dicc del módulo. El frame guarda
     `globales_pre_modulo` para restaurar al retornar.
  5. Cuando el frame del módulo termina (OP_RETORNAR detecta
     `modulo_en_carga`), captura el dicc de globales en
     `mod->atributos`, restaura el dicc principal y registra el módulo
     como global del importador + en cache.
- **`Closure.globales_definicion`**: cada closure captura el dicc de
  globales del scope donde fue creada. Crítico para módulos: una
  función definida en un módulo, cuando se invoca desde fuera, sigue
  viendo las globales del módulo (no las del importador). Sin esto,
  `mat.cuadrado(5)` daría `ErrorDeNombre` al intentar resolver `n`,
  `cuadrado`, etc., desde el contexto del importador.
- **`CallFrame.globales_pre_llamada`**: `OP_LLAMAR` (en sus tres
  variantes: closure, constructor, bound method) detecta si la closure
  tiene una `globales_definicion` distinta a la actual; si es así,
  guarda la actual y cambia. `OP_RETORNAR` restaura.
- **OP_OBTENER_ATRIBUTO** ahora despacha sobre `VAL_MODULO`: lookup en
  `modulo.atributos` con `ErrorDeAtributo` si no existe.
- **Compilación de `SENT_IMPORTAR`**: emite `OP_IMPORTAR [name_idx]`.
  Limitaciones documentadas: solo `importar X` simple (1 segmento, sin
  `como`); `importar X.Y` y `importar X como Y` rechazados con error
  claro (a cubrir en v0.9.x).
- **`stdlib/matematicas.cor`**: `PI`, `E`, `cuadrado`, `cubo`,
  `absoluto`, `maximo`, `minimo`, `signo`, `factorial`, `suma_rango`,
  `es_par`, `es_impar`, `mcd`. Funciones que se llaman entre sí
  (e.g. `mcd` usa `absoluto`) demuestran el cierre de globales.
- **`stdlib/cadenas.cor`**: `repetir`, `es_vacia`, `unir`. Operaciones
  que requieren indexación por carácter (`s[i]`) están aplazadas
  porque el bytecode no soporta indexación de cadenas en v0.9.0.
- **Cache global de módulos**: `VM.cache_modulos` evita re-cargar el
  mismo archivo en imports repetidos del mismo programa.
- **`tests/unit/test_bytecode_modulos.c`** con 9 tests cubriendo:
  importar constante, importar función, módulo no existe, atributo
  inexistente, `tipo()` reporta "modulo", cache (doble import en mismo
  programa registra una sola vez), subsegmentos rechazados, alias
  rechazado, aislamiento (las globales del módulo no son visibles sin
  prefijo).
- **`examples/21_modulos_jugable.cor`** que importa `matematicas` y
  `cadenas`, ejercita constantes, funciones simples, funciones que
  llaman a otras del mismo módulo, y `tipo()` sobre un módulo.

### Correcciones
- **Bug crítico en OP_IMPORTAR**: el frame del módulo no inicializaba
  `globales_pre_llamada`, que en OP_RETORNAR es leído como puntero
  para restaurar globales. Memoria sin inicializar contenía a veces
  basura no-NULL → vm->globales se sobrescribía con un puntero
  inválido → crash en heap corruption. Detectado por iteración
  infinita en `dicc_liberar` durante `vm_destruir`.

### Limitaciones documentadas (a resolver en v0.9.x)
- Sin subsegmentos en path: `importar mat.geometria` no busca
  `mat/geometria.cor`. Llega en v0.9.1.
- Sin alias: `importar mat como m` rechazado. Llega en v0.9.1.
- Sin `desde X importar Y`: la sentencia se reconoce en parser pero el
  compilador rechaza con error explícito. Llega en v0.9.1.
- `cadenas.cor` está limitado por la falta de `s[i]` en bytecode (que
  funciona en tree-walking pero no en bytecode). Resolver requiere
  añadir VAL_CADENA al case OP_INDICE.

## [0.8.3] — 2026-04-29 — excepciones polish

Completa el modelo de excepciones que llevaba postergado desde
v0.6.3: discriminación por tipo, `sino`, `finalmente`, y `lanzar`
re-raise. **75 tests verde**.

### Añadido (v0.8.3)
- **`atrapar Tipo como e:`** discriminado: el atrapador solo coincide
  si la clase de la excepción coincide con el nombre del tipo
  (comparación por cadena del identificador). `atrapar Excepcion`
  funciona como tipo genérico (atrapa cualquier excepción).
- **Múltiples atrapadores** en un mismo `intentar` ahora se compilan
  todos: el primero que coincide se ejecuta; si ninguno coincide, la
  excepción se re-lanza al handler exterior.
- **`sino:`** ejecuta solo si el cuerpo del `intentar` terminó sin
  excepción.
- **`finalmente:`** ejecuta SIEMPRE: tras salida limpia, tras cada
  atrapar exitoso, y antes del re-lanzar si ningún atrapador
  coincide. (Limitación: NO se ejecuta cuando hay `retornar`,
  `romper` o `continuar` que sale del intentar — llega en una versión
  posterior si se necesita.)
- **`lanzar` sin valor (re-raise)**: dentro de un `atrapar Tipo como e:`
  re-emite la excepción capturada. El compilador rastrea aliases de
  atrapadores activos en una pila (`Compilador.atrapador_alias_slots`)
  para que `lanzar` sin valor compile correctamente. Fuera de un
  atrapar con alias, error de compilación claro.
- **Opcode nuevo `OP_COMPROBAR_TIPO_EXC [byte name_idx]`**: peek la
  excepción top, compara su `clase` con la cadena en
  constantes[name_idx], empuja un bool sin descartar la excepción.
  Permite que el handler chequee el tipo antes de decidir si atrapa
  o re-lanza.

### Correcciones críticas en compilar_intentar/compilar_para
- **Bug del aliasing en `OP_ASIGNAR_LOCAL`**: cuando el slot de
  destino y el slot que se acababa de pop coincidían (caso muy común
  en top-level cuando el handler asignaba la excepción al slot 0),
  `valor_destruir(destino)` liberaba el valor que `nuevo` aún
  apuntaba — use-after-free. Detectamos `destino == vm->tope` tras
  `sacar` y saltamos el destruir.
- **Bug del scope persistente**: el compilador acumulaba locals en el
  scope top-level entre bloques `intentar`/`para`, pero el runtime
  stack es transitorio. Ahora `compilar_intentar` y `compilar_para`
  guardan `n_locales` al entrar y emiten `OP_DESCARTAR` por cada
  local introducido al salir (cuerpo, alias, $iter, target). Sin
  esto, bloques posteriores leían valores stale del slot 0.
- **Bug de aliasing entre atrapadores**: si dos atrapadores usaban el
  mismo nombre de alias (`e` por convención), el segundo encontraba
  el slot del primero via `buscar_local` y emitía `ASIGNAR_LOCAL`
  que dropeaba `tope` por debajo del local. Ahora cada atrapador
  añade un local fresco; entre atrapadores se resetea `n_locales` al
  valor de entrada al handler para que cada alias caiga en el mismo
  slot consistente con la posición real en stack.

### Tests nuevos
- `test_atrapar_por_tipo`, `test_atrapar_excepcion_atrapa_todo`,
  `test_atrapar_sin_match_propaga`: discriminación por tipo y
  fallback a re-raise.
- `test_sino`: ejecuta solo si no hubo excepción (positivo y negativo).
- `test_finalmente`: tras salida limpia y tras atrapar exitoso.
- `test_lanzar_reraise`: re-raise dentro de función propaga al
  llamador.
- `test_lanzar_reraise_sin_alias_es_error`: `lanzar` sin valor sin
  contexto → error de compilación.
- `test_intentar_blocks_repetidos`: bloques intentar consecutivos en
  top-level no se contaminan (test del bug de scope persistente).

## [0.8.2] — 2026-04-29 — super multinivel correcto

Resuelve la limitación de `super` que llevaba arrastrándose desde
v0.7.1: ahora `super.metodo()` funciona correctamente con cualquier
profundidad de herencia, no solo 1 nivel.

### Añadido (v0.8.2)
- **Campo `Clase *clase_definicion` en `Closure`**: la clase donde el
  closure fue registrado como método. NULL si no es método (función
  top-level, lambda, función anidada).
  - Antes de v0.8.2 (refcount sin GC), este campo crearía un ciclo
    `Clase → metodos[m] → Closure → clase_definicion → Clase` que el
    refcount no podía romper. Ahora con GC mark-sweep (v0.8.0+), el
    ciclo se rompe automáticamente cuando la clase deja de ser
    alcanzable. La razón por la que esta sesión es post-v0.8.1.
- **`OP_METODO`** ahora set `closure->clase_definicion = clase` (con
  retención) tras meter la closure en `clase.metodos`. Cada
  declaración o redefinición de método actualiza este campo en el
  closure correspondiente.
- **`OP_HEREDAR`** preserva el `clase_definicion` heredado: cuando el
  hijo hereda un método del padre via copia, el closure compartido
  mantiene `clase_definicion = Padre`. Esto es lo que queremos —
  `super` dentro de un método heredado busca en el padre original,
  no en la clase del hijo.
- **`OP_SUPER_INVOCAR`** ahora resuelve
  `frame->closure->clase_definicion->superclase` en lugar de
  `yo.clase.superclase`. Resultado correcto para varios niveles:
  - `Abuelo → Padre → Nieto`. Si el método actual fue declarado en
    Padre, super busca en Abuelo, **incluso si `yo` es un Nieto**.
  - Si `yo.clase` se usara (v0.7.1), `super` desde Padre.metodo en una
    instancia de Nieto resolvería incorrectamente a Padre, causando
    recursión infinita.
  - Fallback al esquema antiguo (`yo.clase.superclase`) si el closure
    no tiene `clase_definicion` set (caso edge: función llamada como
    método sin pasar por una declaración de clase).
- **`gc_marcar_objeto` para closure** ahora también propaga la marca a
  `clase_definicion` para que la clase no sea barrida mientras el
  método siga vivo.
- **`closure_liberar`** decrementa el refcount de `clase_definicion`
  además de la plantilla y los upvalues.
- **Tests nuevos** en `test_bytecode_clases.c`:
  - `test_super_multinivel`: Abuelo → Padre → Nieto. Padre.via_super()
    llama super.m(); con un Nieto como receptor, el resultado es
    `"abuelo"` (no `"padre"` como sería con la implementación
    incorrecta).
  - `test_super_multinivel_constructor`: Cadena de constructores
    Nieto→Padre→Abuelo via super.__iniciar__(), cada uno añadiendo un
    atributo. Verificar que los tres atributos se asignan.
- **Versión** bump a `0.8.2`.

### Limitaciones que aún quedan
- `__cadena__` y otros dunders runtime aún sin implementar — llegan
  en v0.8.3 si se sigue por esta línea, o se aplazan a Fase 9.

## [0.8.1] — 2026-04-29 — GC automático + recolectar() built-in

Activa el trigger automático del recolector que en v0.8.0 quedó
diferido por el problema de las factories anidadas. Añade
`recolectar()` como built-in callable desde código Cornamusa.
**75 tests verde**, **incluido test_bytecode_gc.c bajo `--gc-stress`**.

### Añadido (v0.8.1)
- **Modelo "deferred-to-opcode-boundary"** en `gc_alocar`: cuando
  detecta que el GC debería correr (umbral cruzado o `gc_stress`),
  no ejecuta la recolección inmediatamente — solo marca un flag
  `Memoria.trigger_pendiente`. El dispatch loop de la VM lo chequea
  al inicio de cada iteración (cuando el stack está consistente entre
  opcodes) y ejecuta la recolección en ese punto seguro. Resuelve el
  problema de las factories anidadas (clase_nueva → dicc_nuevo, etc.)
  porque cualquier alocación dentro de un opcode termina antes del
  trigger.
- **Built-in `recolectar()`**: ejecuta un ciclo de mark-sweep manual
  desde código Cornamusa. Devuelve el número de objetos heap liberados
  durante la pasada (entero ≥ 0). Acepta 0 args; aridad incorrecta
  produce `ErrorDeTipo` claro. Funciona también para limpiar ciclos
  intencionalmente desde el usuario.
- **Flag `--gc-stress` ahora funcional**: compilando con
  `cmake -DCORNAMUSA_GC_STRESS=ON` hace que cada `gc_alocar` marque el
  flag pendiente, que el siguiente opcode dispatch dispara. Útil para
  validar que cada alocación es segura y todas las raíces se marcan
  correctamente.
- **`gc_marcar_raices(VM*)`** ahora también marca las constantes del
  chunk de cada frame activo (incluido el frame top-level cuyo closure
  es NULL). Las constantes incluyen plantillas y cadenas dueñas que
  deben sobrevivir.
- **`tests/unit/test_bytecode_gc.c`** con 7 tests end-to-end:
  `recolectar()` devuelve entero, aridad incorrecta, libera ciclo de
  diccionarios, libera ciclo de instancias, no toca objetos vivos,
  carga pesada (50 listas en bucle) bajo gc_stress no explota,
  métodos en bucle (20 instancias + dispatch) funcionan.
- **Refactor `vm_ejecutar` → `vm_ejecutar_dispatch` interno + wrapper
  público**: el wrapper activa `gc_habilitado=true` al entrar y
  `false` al salir, garantizando que el trigger automático solo opere
  durante la ejecución (no durante la fase de compilación entre
  `vm_iniciar` y `vm_ejecutar`).
- **Versión** bump a `0.8.1`.

### Limitaciones que aún quedan (a resolver en v0.8.2+)
- `super` multinivel sigue restringido a 1 nivel (limitación de v0.7.1).
  Resolver requiere `clase_definicion` en Closure que ahora con GC es
  posible sin leak; el cambio se aplaza a v0.8.2.
- `__cadena__` y otros dunders runtime aún sin implementar.

## [0.8.0] — 2026-04-29 — GC mark-sweep tri-color (Fase 7)

Sustituye al refcount como fundamento del modelo de memoria, sin
eliminarlo todavía: refcount sigue siendo el liberador primario y el
GC complementa para limpiar ciclos. **74 tests verde**.

Esta versión es principalmente infraestructura — no hay cambios
visibles al usuario en el lenguaje. Habilita correcciones futuras
(super multinivel, `__cadena__`, etc.) que requieren ciclos seguros
en el modelo de memoria.

### Añadido (v0.8.0)
- **`src/memoria.{h,c}`** con la infraestructura completa de GC
  mark-sweep tri-color simplificado (white/black, sin gris explícito):
  - `GCObject` header (siguiente, marcado, tipo): primer campo de cada
    struct heap-rastreado.
  - `Memoria` con linked-list `cabeza` de objetos vivos + estadísticas
    (total_alocado, total_objetos, umbral_gc) + flag `gc_stress`.
  - `gc_alocar(size, tipo)`: alocator central que enlaza el objeto a
    la lista de la `Memoria` global instalada via `gc_instalar`.
  - `gc_desenlazar(GCObject *)`: usado por los `*_liberar` de refcount
    para sacar el objeto de la lista cuando el refcount los libera.
  - `gc_marcar_valor(Valor *)` y `gc_marcar_objeto(GCObject *)`:
    propagación recursiva idempotente (corta ciclos via flag marcado).
  - `gc_barrer(Memoria *)`: recorre la lista, libera no-marcados con
    un destructor "no recursivo" que solo libera partes propietarias
    no-GC (mp_int, char* dueño, buffers de tablas hash) y la struct
    misma. NO decrementa refcounts de hijos heap-rastreados — esos se
    procesan en la misma pasada cuando el barrido los alcance.
  - `gc_recolectar(Memoria *, FnMarcarRaices, void *ctx)`: orquesta el
    ciclo completo (desmarcar + marcar raíces + barrer).
  - `gc_set_marcador_raices(Memoria *, FnMarcarRaices, void *ctx)`:
    registra el callback que `gc_alocar` usaría para gatillar
    recolección automática (deshabilitado en v0.8.0 — ver limitaciones).
- **Migración de los 12 tipos heap del runtime** a usar `GCObject obj`
  como primer campo: Lista, Diccionario, Conjunto, Tupla, FuncionBC,
  Closure, Upvalue, Iterador, Excepcion, Clase, Instancia,
  MetodoLigado. Sus factory functions usan `gc_alocar`; sus liberadores
  llaman `gc_desenlazar` antes de `free`.
- **VM con `Memoria` propia**: `vm_iniciar` la inicializa e instala
  como global. `vm_destruir` la barre y desinstala — defensa contra
  ciclos refcount cuando el cliente destruye la VM sin haber
  recolectado manualmente.
- **`gc_marcar_raices(VM *)`** en `src/vm.c`: marca el stack
  (pila..tope), las globales (Diccionario), los closures de cada frame
  + las constantes del chunk activo (incluido el frame top-level cuyo
  closure es NULL), y los open_upvalues.
- **Flag `--gc-stress` (CMake `CORNAMUSA_GC_STRESS=ON`)** activable en
  build para habilitar el trigger automático en cada `gc_alocar`.
  Compila pero NO funciona correctamente todavía (ver limitaciones).
- **`tests/unit/test_memoria.c`** con 17 tests cubriendo: alocación con
  y sin Memoria instalada, enlace y desenlace correctos, destrucción
  masiva, integración con cada tipo migrado, mark de valores planos
  (no-op), recursión via lista anidada, dicc clave/valor, clase +
  instancia + superclase + atributos, idempotencia, ciclos sin
  recursión infinita, raíces de la VM real, sweep libera no marcados,
  recolección rompe ciclos refcount, recolección preserva marcados,
  destrucción de Memoria sin leaks visibles.

### Limitaciones conocidas en v0.8.0 (a resolver en v0.8.x)
- **Trigger automático del GC deshabilitado**. La razón: muchas factory
  functions anidan llamadas a `gc_alocar` (ej. `clase_nueva` aloca la
  Clase y luego un Diccionario para sus métodos; `instancia_nueva`
  igual). Tras la primera alocación el objeto está en la lista pero
  todavía no es alcanzable desde ninguna raíz; un trigger interno lo
  barrería incorrectamente. La solución limpia es añadir paréntesis
  `gc_pausar/gc_reanudar` en cada factory, o un modelo de trigger a
  nivel de opcode-boundary. En v0.8.0 el GC se invoca solo manualmente
  via `gc_recolectar` desde C; el built-in `recolectar()` para código
  Cornamusa llega en v0.8.1.
- **Refcount sigue siendo primario**. El GC limpia solo lo que el
  refcount no liberó (típicamente ciclos). Eliminar el refcount por
  completo requiere primero arreglar el trigger automático.
- **`super` multinivel sigue restringido a 1 nivel** (limitación
  documentada de v0.7.1). Resolver requiere `clase_definicion` en
  Closure que crea un ciclo refcount; ahora con GC es posible, pero el
  cambio se aplaza a v0.8.x junto con la activación automática.
- **`__cadena__` y otros dunders runtime** siguen sin implementar —
  llegan en v0.8.x ahora que GC permite invocar métodos durante
  `imprimir()` sin riesgo de leaks.

### Cambios internos
- `chunk.c` ahora `#include "memoria.h"` para gc_alocar/gc_desenlazar
  en `funcion_bc_nueva`/`closure_nuevo`/`upvalue_nuevo` y sus
  liberadores.
- `valor.h` `#include "memoria.h"` para que cada struct heap pueda
  tener `GCObject obj` como primer campo.
- Refactor `vm_ejecutar` → `vm_ejecutar_dispatch` (interno) +
  `vm_ejecutar` (wrapper público que activaría el flag `gc_habilitado`
  cuando el trigger automático esté disponible).

## [0.7.1] — 2026-04-29 — super en bytecode

Cierra el ciclo OOP en bytecode añadiendo `super.metodo(args)` para
herencia simple. Ejemplos como `examples/20_clases_jugable.cor` ahora
encadenan constructores hijo/padre. **73 tests verde**.

### Añadido (v0.7.1)
- **Palabra clave `super`** activa en el parser (el lexer ya tenía `TT_SUPER` desde Fase 2). Solo válida en la forma `super.metodo(args)`.
- **Nuevo nodo AST `EXPR_SUPER`**: guarda el nombre del método tras el punto. El receptor (`yo`) es implícito (slot 1 del frame del método actual).
- **`parsear_super`** registrada como prefix-rule en `obtener_regla(TT_SUPER)`. Espera `super.identificador`; si no hay `.` o no hay identificador tras el punto, error de sintaxis claro.
- **Compilación de `EXPR_LLAMADA(EXPR_SUPER, args)`**: emite `OP_OBTENER_LOCAL 1` (push `yo`) + args + `OP_SUPER_INVOCAR [name_idx] [n_args]`. Validación: solo dentro de un scope de función (método); fuera de un método o sin llamada inmediata, error claro de compilación.
- **Nuevo opcode `OP_SUPER_INVOCAR [byte name_idx] [byte n_args]`**:
  - Stack al ejecutar: `[..., yo, arg1, ..., argN]`.
  - Resuelve `yo.clase.superclase.metodos[name]`, valida aridad incluyendo `yo` (error reporta cifras sin el receptor).
  - Despacha igual que un bound method: `memmove` args un slot arriba, reemplaza el callee con la closure y pone receptor (clonado) en slot 1.
  - Errores claros: `'super' solo puede usarse en metodos de instancia`, `la clase '...' no tiene superclase`, `ErrorDeAtributo: la superclase '...' no tiene metodo '...'`.
- **Limitación documentada**: la búsqueda de super usa `yo.clase.superclase`, no la clase donde el método actual fue definido. Para herencia de un solo nivel (Padre → Hijo) coincide; para varios niveles (Padre → Hijo → Nieto, `super` dentro de un método de Hijo) se requiere almacenar `clase_definicion` en `Closure`, lo que crea un ciclo refcount → llega en v0.8.0 con GC mark-sweep.
- **Tests nuevos** (en `test_bytecode_clases.c`): `super.metodo()` simple, `super.__iniciar__(args)` en constructor del hijo (con campo extra propio), aridad incorrecta vía super, super sin superclase, método inexistente en superclase, super fuera de método (error compilación), super sin punto (error sintaxis).
- **Ejemplo `20_clases_jugable.cor`** actualizado: `Perro` ahora tiene su propio `__iniciar__(yo, nombre, edad, raza)` que llama a `super.__iniciar__(nombre, edad)` antes de asignar `yo.raza`.
- **Versión** bump a `0.7.1`.

## [0.7.0] — 2026-04-29 — clases, métodos, herencia (Fase 8)

Cornamusa pasa a ser un lenguaje OOP completo: clases definibles por
el usuario con atributos mutables, métodos con `yo` autoinyectado,
constructor `__iniciar__`, y herencia simple por copia de métodos.
**71 tests verde**.

### Añadido (v0.7.0)
- **Tipos nuevos en `valor.{h,c}`**:
  - **`VAL_CLASE`** + `struct Clase`: nombre heap-duplicado, `metodos` (Diccionario cadena → VAL_FUNCION_BC), `superclase` opcional, refcount. Pretty-printed `<clase Foo>`.
  - **`VAL_INSTANCIA`** + `struct Instancia`: referencia compartida a su `Clase`, `atributos` (Diccionario propio modificable), refcount. Pretty-printed `<instancia de Foo>`.
  - **`VAL_METODO_LIGADO`** + `struct MetodoLigado`: receptor (Valor con refcount) + método (Closure con refcount). Construido al acceder a `instancia.metodo` cuando el nombre está en `clase.metodos`. Pretty-printed `<metodo nombre>`. `valor_nombre_tipo` lo reporta como `"funcion"`.
  - Lifecycle (destruir/clonar/iguales/es_verdadero/es_hashable/nombre_tipo) wired para los tres tipos. Identidad por puntero; ninguno hashable.
- **5 opcodes nuevos** en bytecode:
  - **`OP_CLASE [byte name_idx]`**: crea `Clase` con el nombre indicado y la empuja.
  - **`OP_OBTENER_ATRIBUTO [byte name_idx]`**: lookup de instancia con fallback. Primero busca en `instancia.atributos` (override); si no está, busca en `instancia.clase.metodos` y, si encuentra una closure, crea un `MetodoLigado(instancia, closure)`. `ErrorDeAtributo` si no existe en ninguno; `ErrorDeTipo` si el objeto no es instancia.
  - **`OP_ASIGNAR_ATRIBUTO [byte name_idx]`**: pop valor, pop instancia, set `atributos[nombre] = valor`, push nulo (la sentencia descarta).
  - **`OP_METODO [byte name_idx]`**: con stack `[..., clase, closure]`, pop closure y guardarla en `clase.metodos[name]`; clase queda en el tope para más métodos.
  - **`OP_HEREDAR`** (sin operando): con stack `[..., clase, super]`, pop super, copia `super.metodos → clase.metodos` (los OP_METODO posteriores sobrescriben para implementar override) y enlaza `clase.superclase = super`.
- **`OP_LLAMAR` despacha sobre tres nuevos tipos de callee**:
  - **`VAL_CLASE`**: instancia la clase. Si tiene `__iniciar__`, lo invoca como método con la instancia recién creada como receptor; aridad chequeada incluyendo `yo` (el error reporta cifras sin el receptor). Sin `__iniciar__` y `n_args > 0` → error claro. La llamada `Foo(args)` siempre devuelve la instancia, no lo que `__iniciar__` retorne.
  - **`VAL_METODO_LIGADO`**: inserta el receptor como primer argumento del frame (`memmove` los args un slot arriba, reemplaza el callee con la closure y pone el receptor en slot 1). Aridad chequeada con receptor incluido; el error reporta cifras sin él.
- **Nuevo flag `CallFrame.es_constructor`**: marca el frame de `__iniciar__` para que `OP_RETORNAR` descarte el valor de retorno y devuelva la instancia (slot 1) en su lugar.
- **Compilación de `SENT_CLASE`** completa:
  - Cuerpo admite `SENT_PASAR` y `SENT_FUNCION` (métodos); cualquier otra sentencia produce error claro.
  - Para cada método: emite la closure vía el nuevo helper `emitir_closure_de_funcion` (refactor de `compilar_funcion`, factor común) + `OP_METODO [name_idx]`.
  - `extiende Padre` (un solo padre): emite la expresión del padre + `OP_HEREDAR` antes de los métodos. Herencia múltiple rechazada en compilación.
- **Compilación de `EXPR_ATRIBUTO`** (lectura) y **`obj.attr = valor`** (escritura):
  - `obj.attr` lectura → `obj` + `OP_OBTENER_ATRIBUTO [idx]`.
  - `obj.attr = valor` → `obj` + `valor` + `OP_ASIGNAR_ATRIBUTO [idx]` + `OP_DESCARTAR`.
- **`yo` por convención** (decisión B5+B6): el primer parámetro de un método (idiomáticamente `yo`) recibe la instancia automáticamente al llamarlo via `instancia.metodo(args)`. No es palabra reservada.
- **Constructor `__iniciar__`** (dunder en castellano, decisión B5+B6) reemplaza el patrón `__init__` de Python.
- **Métodos encadenables**: `obj.m1().m2().m3()` retornando `yo`.
- **Herencia simple**: el hijo recibe los métodos del padre (por copia al ejecutar `OP_HEREDAR`); puede sobrescribir en su propio cuerpo. Hereda también `__iniciar__` si no lo redefine. Polimorfismo: cada subclase dispatcha a su propio método al ser invocado.
- **Limitaciones documentadas v0.7.0** (a cubrir en v0.7.x patches):
  - Sin `super` (la palabra clave existe en el lexer pero no se usa todavía).
  - Sin `__cadena__` (usar `imprimir(obj.atributo)` o métodos custom mientras tanto).
  - Sin operator overloading (otros dunders como `__sumar__`, `__igual__`, etc.).
  - Sin atributos de clase (solo de instancia).
  - Solo herencia simple (parser admite múltiples padres pero el compilador rechaza).
- **`tests/unit/test_bytecode_clases.c`** con 18 grupos cubriendo: definición y `tipo()`, instanciación, atributos (lectura/escritura/sobrescritura/mutación compartida), errores runtime (atributo inexistente / asignación a no-instancia / lectura de no-instancia / llamada con args), métodos (sin args, con args, mutación via `yo`, chaining, aridad incorrecta, sombrea con atributo, tipo correcto), constructor `__iniciar__` (con y sin args, retorno ignorado, aridad incorrecta, combinado con métodos), herencia (métodos heredados, override, constructor heredado, polimorfismo, mezcla override/heredado), errores compilación (herencia múltiple, heredar de no-clase), identidad por `es`.
- **Versión** bump a `0.7.0`.

## [0.6.3] — 2026-04-29 — excepciones en bytecode

El motor bytecode ahora maneja `intentar`/`atrapar` y `lanzar`. Cierra
el motor casi-completo módulo atributos (Fase 8) y módulos.

### Añadido (v0.6.3)
- **`VAL_EXCEPCION`** y **`struct Excepcion`** en `valor.{h,c}`: tipo runtime con clase + mensaje (cadenas heap-duplicadas) y refcount. Pretty-printer formato `<clase>: <mensaje>`.
- **Built-ins de construcción de excepciones**:
  - **`Excepcion(clase, mensaje)`**: constructor genérico.
  - **`ErrorAritmetico("...")`**, **`ErrorDeTipo("...")`**, **`ErrorDeValor("...")`**, **`ErrorDeIndice("...")`**, **`ErrorDeClave("...")`**, **`ErrorDeNombre("...")`**: atajos con clase prerellenada (1 argumento = mensaje).
- **3 opcodes nuevos** y `HandlerFrame` stack en VM:
  - **`OP_INTENTAR_INICIAR [u16 offset_handler]`**: empuja un `HandlerFrame` con el snapshot del estado (n_frames, tope, n_open_upvalues) y la dirección del handler. Hasta 64 handlers anidados (`VM_HANDLERS_MAX`).
  - **`OP_INTENTAR_FIN`**: pop el handler frame al salir limpio del bloque `intentar`.
  - **`OP_LANZAR`**: pop la excepción del tope. Si es cadena, se envuelve como `Excepcion("Excepcion", cadena)`. Busca el handler frame top: si no hay → error en VM con clase y mensaje. Si hay → cierra upvalues abiertos por encima del handler, descarta slots del stack hasta el `tope_offset`, hace pop de frames hasta `frame_idx`, y empuja la excepción para que el handler la consuma. Salta a `ip_handler`.
- **Compilación de `SENT_INTENTAR`**:
  - Estructura: `INTENTAR_INICIAR offset → cuerpo → INTENTAR_FIN → SALTAR fin → handler: → atrapador → fin:`.
  - Soporta `atrapar [Tipo] [como alias]:`. El alias se registra como local (mismo patrón que `x = 5` creando un local nuevo: el valor de la excepción ya está en su slot final tras el `OP_LANZAR`).
  - El tipo, si está presente, se evalúa pero **no se compara** todavía (limitación v0.6.3 — `atrapar Excepcion como e:` atrapa cualquier cosa). Atrapar discriminando por tipo llega en v0.6.4.
- **Compilación de `SENT_LANZAR`**: compila la expresión + `OP_LANZAR`. `lanzar` desnudo (re-raise) aún no soportado.
- **Limitaciones documentadas v0.6.3** (todas a cubrir en v0.6.4+):
  - Solo el primer atrapador de un `intentar` se compila (los demás se aceptan en parser pero se ignoran).
  - Tipo de excepción (`atrapar ErrorAritmetico:`) no discrimina; el handler atrapa todo.
  - Cláusula `sino` (rama "sin excepción") aún no compila — error explícito.
  - Cláusula `finalmente` aún no compila — error explícito.
  - `lanzar` sin valor (re-raise) aún no compila.
  - Tree-walking sigue sin implementar excepciones (decisión B2).
- **`tests/unit/test_bytecode_excepciones.c`** con 9 grupos: construcción de excepciones (genérica + atajos), atrapar simple con alias, mensaje del alias, cuerpo sin excepción no entra al atrapar, excepción dentro de función propaga al llamador, programa `dividir` robusto (con/sin cero), excepciones no atrapadas como error VM, anidamiento de `intentar` (interno atrapa / interno re-lanza al externo), `lanzar` cadena (azúcar a `Excepcion("Excepcion", cadena)`).
- **Versión** bump a `0.6.3`.
- **70 tests verde** (27 unit + 43 integración).

## [0.6.2] — 2026-04-29 — closures + lambdas + slicing en bytecode

El motor bytecode ahora ejecuta el lenguaje completo módulo
excepciones, atributos y módulos. **8 de 9 ejemplos jugables
v0.5 + el nuevo 19_closures corren con `--bytecode`**.

### Añadido (v0.6.2)
- **`OP_REBANADA`** y compilación de `EXPR_REBANADA`: slicing `lista[a:b:c]` con cualquier campo opcional. Operandos faltantes se emiten como `OP_NULO` (sentinela). VM despacha con la misma semántica que el evaluador tree-walking: defaults dependientes del signo del paso, índices negativos cuentan desde el final, fuera de rango se clampea silenciosamente, paso 0 → `ErrorDeValor`.
- **Closures con upvalues** (estilo clox cap. 25). Refactor mayor:
  - **Separación `FuncionBC` (plantilla, en pool) vs `Closure` (instancia, en stack)**:
    - `FuncionBC` mantiene chunk + nombre + aridad + metadata de upvalues (`info_upvalues[]`).
    - `Closure` envuelve un `FuncionBC` y añade `Upvalue **upvalues` runtime.
    - Nuevo `VAL_PLANTILLA_BC` para el constant pool; `VAL_FUNCION_BC` ahora apunta a `Closure`.
  - **`Upvalue` runtime**: linked-list ordenada por posición decreciente en stack (`vm->open_upvalues`). Refcount para compartir entre múltiples closures que capturan la misma variable.
  - **`OP_CLOSURE [byte fn_idx] [n_upvalues * (es_local, indice)]`**: lee la plantilla del pool, crea Closure nuevo, conecta cada upvalue a su slot del frame actual (vía `capturar_upvalue`) o a un upvalue existente.
  - **`OP_OBTENER_UPVALUE [slot]`** y **`OP_ASIGNAR_UPVALUE [slot]`**: lectura/escritura via `frame->closure->upvalues[slot]->posicion`.
  - **`OP_RETORNAR` cierra upvalues** del frame que termina con `cerrar_upvalues_hasta`. Al cerrar, el valor del slot se **transfiere** (no se duplica) al campo `cerrado` del upvalue, y el slot original se vacía a `nulo` para evitar double-free.
  - `CallFrame` ahora tiene un puntero `closure` (NULL en frame top-level) que la VM usa para resolver upvalues durante la ejecución.
- **Compilador con resolución de upvalues recursiva**: `EXPR_IDENT`, `compilar_asignar` y `compilar_asignar_aug` siguen el orden **local → upvalue → global**. La función `resolver_upvalue(scope)` busca la variable en el scope padre directo (como local) y, si no la encuentra, recurre subiendo la cadena de scopes (capturando como "upvalue de upvalue"). El compilador rellena la metadata de upvalues en la `FuncionBC` para que `OP_CLOSURE` la use en runtime.
- **Funciones anidadas dentro de función ahora se registran como locales** (no como globales como antes en S5). El compilador detecta `c->actual->es_funcion` y emite `OP_DEFINIR_GLOBAL` solo en top-level.
- **Lambdas (`EXPR_LAMBDA`)** compiladas como funciones anónimas con cuerpo expresión:
  - Crea `FuncionBC` con nombre `"lambda"`.
  - Abre scope hijo con parámetros como locales.
  - Compila el cuerpo como expresión y emite `OP_RETORNAR`.
  - Emite en el padre `OP_CLOSURE` con metadata de upvalues capturados.
  - Sin valores por defecto (mismo que `SENT_FUNCION` en bytecode); error explícito si se usan.
- **Nuevo ejemplo `examples/19_closures_jugable.cor`** que demuestra contadores con estado y lambdas factory. Bytecode-only (decisión B2: el tree-walking no implementa closures).
- **`tests/unit/test_bytecode_closures.c`** con 7 grupos:
  - Captura simple de local desde función anidada.
  - Contador clásico (3 invocaciones).
  - Closures independientes (factoría produce instancias con estado separado).
  - Captura de dos niveles arriba (upvalue de upvalue).
  - Lambda sin captura.
  - Lambda con captura (factory de multiplicadores).
  - Slicing varios casos (omisiones, paso negativo, fuera de rango).
- **`bc_run_*` ampliados**: `16_lista_busqueda` (slicing) y `19_closures_jugable` ahora se ejecutan con `--bytecode` y verifican salida.
- **Versión** bump a `0.6.2`.
- **69 tests verde** (26 unit + 43 integración).

### Aplazado a v0.6.3+
- **Excepciones** (`intentar`/`atrapar`/`finalmente`/`lanzar`).
- **Atributos** (`obj.attr`) — necesita primero el sistema de objetos (Fase 8).
- **Módulos** (`importar`).
- **`global`/`nolocal`** declaraciones explícitas.
- **Valores por defecto** en parámetros bytecode (sí en tree-walking).

## [0.6.1] — 2026-04-29 — bytecode con iteración

Bytecode amplía soporte: `SENT_PARA` con iteradores genéricos sobre
listas, tuplas, cadenas (UTF-8), rangos, diccionarios y conjuntos.
Asignación aumentada con destino índice (`dicc[k] += 1`). 7 de 9
ejemplos jugables corren ya por bytecode.

### Añadido (v0.6.1)
- **`VAL_ITERADOR`** y **`struct Iterador`** en `valor.{h,c}`: tipo VM-only (no expuesto al usuario) que mantiene el estado de iteración. Campos: copia con refcount del iterable + cursor int. La función `iter_siguiente` despacha por tipo:
  - **Lista/Tupla**: cursor = índice.
  - **Cadena**: cursor = byte position; avanza por code points UTF-8 con `utf8proc_iterate`.
  - **Diccionario/Conjunto**: cursor = slot interno; salta entradas vacías; emite claves (dict) o elementos (conjunto).
  - **Rango**: cursor = número de iteración; calcula `inicio + cursor*paso` cada vez.
- **`OP_ITER_INICIAR`**: pop iterable, push iterador (validado con `valor_es_iterable`).
- **`OP_ITER_SIGUIENTE [byte slot] [u16 offset]`**: lee el iterador del slot dado del frame actual. Si tiene siguiente, push valor; si no, salta `offset` bytes (los `OP_ASIGNAR_LOCAL` siguientes no se ejecutan, el slot iterador se libera con el frame).
- **`SENT_PARA` en compilador**:
  - Compila iterable + `OP_ITER_INICIAR`.
  - Reserva un local oculto `$iter` con el iterador en su slot.
  - Si el objetivo es local: pre-asigna con `OP_NULO` y emite `OP_ASIGNAR_LOCAL` en cada iteración (evita el bug "asignar al top sobre sí mismo").
  - Si es top-level: emite `OP_DEFINIR_GLOBAL` en cada iteración.
  - Soporta `romper`/`continuar` y cláusula `sino` (ejecutada solo al agotarse el iterador, no por break).
- **Locales en scope top-level**: el compilador permite registrar locales (vía `agregar_local`) incluso en `es_funcion=false`. Esto habilita el slot oculto `$iter` y evita interferencias con globales del usuario.
- **`OP_DUP_2`**: duplica los dos valores del tope (`a, b → a, b, a, b`). Necesario para implementar aug-assign en índice sin reevaluar `obj` y `key`.
- **`compilar_asignar_aug` con destino `EXPR_INDICE`**: compila como `obj key OP_DUP_2 OP_INDICE valor OP_op OP_ASIGNAR_INDICE OP_DESCARTAR`. Permite `dicc[k] += 1`, `lista[i] *= 2`, etc.
- **`OP_ES` y `OP_EN`** añadidos al enum `OpCode` y mapeados desde `TT_ES`/`TT_EN` en el compilador. La VM los despacha a `evaluador_aplicar_binario` igual que las comparaciones — la lógica completa (identidad, membership en cadena/lista/dicc/conjunto/tupla) se reusa del refactor de S2.
- **Versión** bump a `0.6.1`.
- **`tests/unit/test_bytecode_iter.c`** con 10 grupos: `para` sobre cadena (incluido UTF-8), rango (con paso negativo y `rango(n)`), lista/tupla, dicc/conjunto, `romper`, `continuar`, cláusula `sino`, `para` dentro de función (conteo de vocales en `"murcielago"`), aug-assign con índice (frecuencia de letras en `"abracadabra"`, mutación de lista), factorial(25) iterativo via bytecode (26 dígitos).
- **`bc_run_*` ampliados**: `02_fizzbuzz`, `14_contar_vocales`, `15_fizzbuzz_jugable`, `17_dicc_frecuencia`, `18_conj_y_tupla` ahora se ejecutan con `--bytecode` y verifican misma salida que tree-walking.
- **65 tests verde** (25 unit + 40 integración: 12 lex + 8 parse + 13 run + 7 bc_run).

### Aplazado a v0.6.2+
- **Closures con upvalues** (estilo clox cap. 25): funciones anidadas que capturan locales del scope enclosing. Requiere `OP_CLOSURE`, `OP_GET_UPVALUE`, `OP_SET_UPVALUE`, `OP_CLOSE_UPVALUE` y tracking runtime de upvalues abiertos.
- **Lambdas** (`lambda x: x*2`): mismo modelo que `SENT_FUNCION` pero como expresión.
- **Slicing** (`lista[a:b:c]`): `OP_REBANADA`.
- **Atributos** (`obj.attr`): hace falta primero el sistema de objetos (Fase 8).
- **Excepciones** (`intentar`/`atrapar`/`finalmente`): tabla de excepciones por chunk, manejo de stack unwinding.

## [0.6.0] — 2026-04-29 — motor bytecode (opt-in)

Cierre de Fase 6 según el plan: compilador AST → bytecode + VM
stack-based ejecutando expresiones, sentencias, control de flujo,
funciones con recursión y colecciones. Motor opt-in con flag
`--bytecode`; el tree-walking sigue siendo el por defecto en v0.6.0
para preservar la cobertura completa del lenguaje (incluida la
iteración `para`, que el bytecode aún no soporta).

### Añadido (Fase 6 sesión 6)
- **`FnNativa` refactorizado para `EvalError *`**: las funciones nativas (built-ins) ya no dependen del struct `Evaluador` — toman `EvalError *` directamente. Esto permite invocarlas tanto desde el evaluador tree-walking como desde la VM bytecode sin acoplarlas a uno de los dos motores. Cambio de firma propagado a todas las nativas (`imprimir`, `longitud`, `tipo`, `rango`, `agregar`, `quitar`, `insertar`, `invertir`, `ordenar`, `claves`, `valores`, `conjunto`).
- **`nativos_registrar_dicc(Diccionario *globales)`** en paralelo a `nativos_registrar(Entorno *)`: ambos iteran una **lista canónica única** de nativas (`NATIVAS[]` en `nativos.c`) garantizando que tree-walking y bytecode ofrezcan los mismos built-ins.
- **VM `vm_iniciar` ahora registra los built-ins** en `vm->globales` automáticamente — los programas bytecode ya tienen acceso a `imprimir`, `longitud`, `tipo`, `rango`, etc. desde el primer byte de ejecución.
- **`OP_LLAMAR` en VM extendido para `VAL_NATIVA`**: cuando el callee es una nativa, la VM la invoca pasando `&vm->error` y limpia los args/callee del stack al volver. Las funciones definidas por el usuario (`VAL_FUNCION_BC`) siguen creando un `CallFrame` nuevo como en S5.
- **Colecciones literales en bytecode** con cuatro nuevos opcodes:
  - `OP_BUILD_LISTA [n]`: pop n elementos, push lista.
  - `OP_BUILD_TUPLA [n]`: pop n elementos, push tupla.
  - `OP_BUILD_DICC [n_pares]`: pop 2n elementos (k,v intercalados), push diccionario.
  - `OP_BUILD_CONJUNTO [n]`: pop n elementos hashables, push conjunto.
- **Indexación en bytecode**:
  - `OP_INDICE`: pop key, pop obj, push obj[key]. Despacha por tipo (lista/tupla/diccionario) en runtime con mensajes de error específicos (`ErrorDeIndice`, `ErrorDeClave`).
  - `OP_ASIGNAR_INDICE`: pop value, pop key, pop obj — `obj[key] = value`. Soporta listas y diccionarios.
- **Compilador con `EXPR_LISTA`/`EXPR_TUPLA`/`EXPR_DICCIONARIO`/`EXPR_CONJUNTO`/`EXPR_INDICE`**: producen los nuevos opcodes. Los literales con más de 255 elementos producen error explícito (limitación del operando byte).
- **`SENT_ASIGNAR` con destino `EXPR_INDICE`**: el compilador emite el bytecode `obj key valor OP_ASIGNAR_INDICE OP_DESCARTAR`.
- **`VM_PILA_MAX`** ampliado a 8192, **`VM_FRAMES_MAX`** a 256 para acomodar recursión profunda como `factorial(100)` (64 dígitos vía bytecode).
- **`OP_ITER_INICIAR` y `OP_ITER_SIGUIENTE`** reservados en el enum pero no implementados todavía — `SENT_PARA` queda aplazado a v0.6.1 (necesita un VAL_ITERADOR ad hoc o desazucar a `mientras` con manejo de slots temporales). El motor tree-walking sigue siendo el camino para programas que usan `para`.
- **Integración con `cornamusa`** (motor opt-in):
  - **Flag `--bytecode`** en `main.c` activa el pipeline `lex → parse → compilar → vm_ejecutar`. Sin la flag, el motor sigue siendo tree-walking (default en v0.6.0).
  - Errores de compilación o de runtime se reportan con `imprimir_error_runtime` (mismo formato MENSAJES.md §2 con caret).
- **Tests `bc_run_*`**: ejemplos `01_hola_mundo` y `13_factorial_jugable` (recursión que deja `100!` con 158 dígitos vía bytecode) se ejecutan también con `--bytecode` y verifican misma salida que el tree-walking.
- **`tests/unit/test_bytecode_colecciones.c`** con 9 grupos: literales (lista, tupla, dicc, conjunto), indexación (lista/tupla/dicc + errores), asignación a índice, nativas sobre colecciones via OP_LLAMAR, programas mixtos (función que indexa, dicc acumulador).
- **`test_bytecode_funciones.c` extendido** con grupo de nativas vía OP_LLAMAR (longitud sobre cadena UTF-8, rango, tipo).
- **Limitaciones documentadas para v0.6.0**:
  - `SENT_PARA` (`para X en Y`) **no compila a bytecode**; se ejecuta solo en tree-walking.
  - **Closures con upvalues** no implementadas (decisión B2 ya las excluía del tree-walking; las añadiremos en v0.6.1+).
  - Atributos (`obj.attr`), lambda, slicing, f-string interpolada, intentar/atrapar, importar — todos siguen aplazados.
- **Versión** bump a `0.6.0` en `common.h`, `CMakeLists.txt`, smoke test.
- **59 tests verde** (24 unit + 35 integración: 12 lex + 8 parse + 13 run tree-walking + 2 bc_run bytecode).

### Añadido (Fase 6 sesión 5)
- ⏳ Sesión 6: colecciones + transición a tagged i63 + flag `--tree-walking` + tests diferenciales + tag v0.6.0.

### Añadido (Fase 6 sesión 1)
- **`src/chunk.{h,c}`**: estructura `Chunk` con bytecode, pool de constantes y array paralelo de números de línea fuente. Tres arrays paralelos siguiendo clox cap. 14 (renombrado al castellano):
  - `codigo[]` (uint8_t): instrucciones y operandos inline.
  - `constantes[]` (Valor): pool referenciado por `OP_CONST` y `OP_CONST_LARGO`. El chunk es DUEÑO de los Valores y los destruye al liberarse.
  - `lineas[]` (int): un número de línea por byte de código. Sin compresión (suficiente para v0.6; se podrá optimizar a run-length encoding más adelante).
- **`enum OpCode`** con 32 instrucciones reservadas para toda la fase: literales (`OP_CONST`, `OP_CONST_LARGO` para >256 constantes, `OP_NULO`, `OP_VERDADERO`, `OP_FALSO`), aritmética (`+`, `-`, `*`, `/`, `//`, `%`, `**`, negación), lógica/comparación (`no`, `==`, `!=`, `<`, `<=`, `>`, `>=`), stack (`DESCARTAR`), control de flujo (`SALTAR`, `SALTAR_SI_FALSO`, `BUCLE`), variables (locales y globales), llamadas y retorno. Reservadas ahora para que el orden quede estable; se implementan progresivamente en S2-S5.
- **API**:
  - `chunk_iniciar`/`chunk_destruir` (idempotente).
  - `chunk_emitir_byte` y `chunk_emitir_byte2` con crecimiento ×2 amortizado.
  - `chunk_agregar_constante` devuelve índice; `chunk_emitir_constante` elige entre `OP_CONST` (1 byte de índice) y `OP_CONST_LARGO` (3 bytes little-endian) automáticamente cuando el pool supera 255 entradas.
  - `opcode_nombre` para inspección/debug.
- **`src/debug.{h,c}`**: disassembler estilo clox. Formato:
  ```
  == nombre ==
  0000  123 OP_CONST            7 '42'
  0002    | OP_RETORNAR
  ```
  - Offset (4 dígitos), línea fuente (con `|` cuando coincide con la anterior), nombre del opcode alineado a 20 caracteres, operandos formateados según el tipo de instrucción (constante, byte, u16 con destino calculado para saltos).
  - `desensamblar_chunk(chunk, nombre, salida)` y `desensamblar_instruccion(chunk, offset, salida)` (devuelve siguiente offset).
  - Constantes se imprimen con `valor_a_repr` (cadenas con comillas).
- **Limitación documentada**: este es solo el armazón. Sin compilador ni VM funcional aún — eso llega en S2. La idea de S1 es congelar el formato del chunk antes de añadir muchos consumidores.
- **`tests/unit/test_chunk_disasm.c`** con 11 tests: chunk vacío + idempotencia destruir, crecimiento de capacidad (100 bytes), `emitir_byte2`, ownership de constantes (entero/decimal/cadena), `emitir_constante` con índice corto y largo (forzando el cambio a `OP_CONST_LARGO` con 256 constantes previas), disassembler simple/aritmético, marca `|` para línea repetida, `opcode_nombre`.
- **52 tests verde** (19 unit + 33 integración).

### Añadido (Fase 6 sesión 2)
- **Refactor del evaluador** para que la lógica de operadores sea reutilizable desde la VM bytecode sin duplicación:
  - Nueva función pública `evaluador_aplicar_binario(EvalError *err, int op_token, Valor a, Valor b, int linea, int columna)` que toma posesión de `a`/`b` y devuelve un Valor nuevo. Mismo modelo de error que la versión interna pero desacoplado del struct `Evaluador` (toma `EvalError *` directamente).
  - Análoga `evaluador_aplicar_unario(EvalError *err, ...)`.
  - Helpers internos (`entero_op_entero`, `decimal_op_decimal`, `cadena_concatenar`, `cadena_repetir`, `evaluar_comparacion`, `evaluar_en`) cambiados a tomar `EvalError *err` y `int linea, int columna` en lugar de `Evaluador *ev` y `const Expr *e`. El evaluador tree-walking sigue funcionando idénticamente — solo se ha movido el acoplamiento.
  - Wrapper interno `aplicar_binario(Evaluador *ev, ...)` y `aplicar_unario_pos` para que los call-sites del tree-walking (que tienen `Expr *e` a mano) no se vean obligados a desempaquetarlo.
- **`src/vm.{h,c}`**: máquina virtual stack-based estilo clox cap. 15.
  - Pila de 256 slots (capacidad fija por ahora; será dinámica con frames cuando lleguen llamadas en S5).
  - Dispatch loop con `for(;;) switch(*ip++)`. Tracking de línea fuente vía `chunk->lineas[ip - codigo - 1]` para mensajes de error.
  - Implementa: `OP_CONST`/`OP_CONST_LARGO` (con `valor_clonar` del pool), `OP_NULO`/`OP_VERDADERO`/`OP_FALSO`, los 7 operadores aritméticos, las 6 comparaciones, `OP_NEGAR`/`OP_NO`, `OP_DESCARTAR`, `OP_RETORNAR` (extrae el tope y lo devuelve al cliente). Opcodes reservados (saltos, locales, llamadas) emiten error explícito "no implementado en v0.6 sesión 2".
  - Reusa `evaluador_aplicar_binario`/`unario` con un mapeo `OpCode → TipoToken`. Cero duplicación de la aritmética bignum / comparaciones / cadenas.
- **`src/compilador.{h,c}`**: visita el AST y emite bytecode al chunk.
  - Compila `EXPR_LITERAL_*` (entero/decimal/cadena/booleano/nulo), `EXPR_BINARIO`, `EXPR_UNARIO`, `EXPR_GRUPO`.
  - Las constantes se almacenan en el pool del chunk; el chunk es DUEÑO y las destruye al liberarse (incluyendo `mp_int*` y cadenas con dueño).
  - Compilación de cadena literal procesa los escapes mínimos (`\n \t \r \\ \' \"`) igual que el evaluador tree-walking — ambos motores producen el mismo Valor cadena para la misma fuente.
  - Aplazadas con error explícito: `EXPR_IDENT`, `EXPR_LOGICA`, `EXPR_LLAMADA`, `EXPR_LAMBDA`, colecciones, indexación, slicing, f-strings, atributos.
- **`tests/unit/test_bytecode_expr.c`** con 8 grupos de tests end-to-end (`lex → parse → compilar → vm_ejecutar`): literales (cada tipo + escapes), aritmética bignum (precedencia, asociatividad, floor div Python, 2^100 = 31 dígitos), aritmética decimal y mixta (true div siempre decimal, promoción), comparaciones (todas + cross-tipo entero=decimal + lexicográfico de cadenas + tipos incomparables → error), unarios (`-`, `+`, `no`, doble negación), realistas (Pitágoras, promedio decimal, semántica izquierda-a-derecha de Cornamusa sin chained comparisons), errores de runtime (división por cero, tipo incompatible) y errores de compilación (identificadores/lambda/lógica explícitamente no implementados todavía).
- **53 tests verde** (20 unit + 33 integración).

### Añadido (Fase 6 sesión 3)
- **Variables globales en la VM**: la `VM` ahora contiene un `Diccionario *globales` (refcount) que persiste entre llamadas a `vm_ejecutar` (útil para REPL futuro). Inicializado en `vm_iniciar`, liberado en `vm_destruir`.
  - **`OP_DEFINIR_GLOBAL [idx]`**: lee el nombre de `chunk->constantes[idx]`, saca el valor del tope, define o sobrescribe en globales. Cornamusa no distingue declaración de asignación, así que esta es la operación habitual.
  - **`OP_OBTENER_GLOBAL [idx]`**: empuja al stack una copia del valor; si la clave no existe, `ErrorDeNombre: nombre 'X' no esta definido`.
  - **`OP_ASIGNAR_GLOBAL [idx]`**: variante estricta (la clave debe existir); reservada para futuras semánticas más rigurosas, no usada por el compilador en S3.
- **Built-in `imprimir(...)` en bytecode**: nuevo `OP_IMPRIMIR [n]` que saca `n` valores del stack, los imprime separados por espacio + newline, y empuja `nulo` (porque `imprimir(...)` es expresión y `SENT_EXPR` la envuelve con `OP_DESCARTAR`). Soporta hasta 255 argumentos.
- **`compilador_compilar_sent`** soporta:
  - **`SENT_PASAR`**: no-op explícito.
  - **`SENT_EXPR`**: compila la expresión y emite `OP_DESCARTAR`.
  - **`SENT_ASIGNAR`** con destino `EXPR_IDENT`: compila el valor y emite `OP_DEFINIR_GLOBAL` con el nombre como constante. Tuple destructuring, atributos e índices como destino quedan para S6+.
  - **`SENT_BLOQUE`**: compila secuencialmente cada sentencia.
  - Resto de sentencias (`if/while/for/funcion/clase/intentar/lanzar/importar`) producen error explícito con su sesión objetivo.
- **`compilador_compilar_programa(c, sents, n)`**: compila cada sentencia y emite `OP_NULO + OP_RETORNAR` al final, dejando el chunk listo para `vm_ejecutar`.
- **`EXPR_IDENT`** ahora se compila a `OP_OBTENER_GLOBAL` (lookup en globales).
- **`EXPR_LLAMADA` con callee `imprimir`** se detecta como caso especial en el compilador y emite `OP_IMPRIMIR [n]`. Otras llamadas siguen produciendo error "no implementado en bytecode v0.6 sesión 3" — el sistema completo de funciones definidas por el usuario llega en S5.
- **Limitación documentada**: nombres de globales con índice >255 en el pool del chunk dan error explícito ("demasiadas constantes para v0.6 (operando byte)"). Se resolverá con variantes `*_LARGO` cuando sea necesario.
- **`tests/unit/test_bytecode_programa.c`** con 5 grupos: asignación a global (incluye reasignación, varias variables, cambio de tipo libre), error de nombre no definido en runtime, captura de stdout para `imprimir(...)` (con redirección dup/dup2 portable Windows/POSIX), `pasar` y un programa combinado (Pitágoras 3-4-5 imprimiendo "hipotenusa: 5.0").
- **54 tests verde** (21 unit + 33 integración).

### Añadido (Fase 6 sesión 4)
- **`OP_SALTAR`, `OP_SALTAR_SI_FALSO`, `OP_BUCLE`** implementados en la VM con operandos `u16` big-endian. `OP_SALTAR_SI_FALSO` hace **PEEK** (no pop) — el compilador inserta `OP_DESCARTAR` donde toca. Estilo clox cap. 23.
- **Helpers de salto en el compilador**:
  - `emitir_salto(op, linea)`: emite el opcode con placeholder `0xffff`, devuelve el offset para parchear después.
  - `parchear_salto(offset)`: rellena el placeholder con la distancia hasta la posición actual del chunk. Reporta error si excede `UINT16_MAX`.
  - `emitir_bucle(inicio)`: emite `OP_BUCLE` con offset hacia atrás.
- **Stack de bucles abiertos** en el `Compilador` (`BucleAbierto bucles[16]`):
  - `inicio_continuar`: offset al que `continuar` debe saltar (la condición del `mientras`).
  - `parches_romper[]`: array dinámico de offsets de `OP_SALTAR` emitidos por `romper`, parcheados al cerrar el bucle.
  - `empujar_bucle` / `cerrar_bucle` mantienen la pila al entrar/salir.
- **`EXPR_LOGICA` con cortocircuito real**:
  - `a y b`: si `a` es falso, salta sobre `b` dejando `a` en stack; si verdad, descarta `a` y evalúa `b`.
  - `a o b`: si `a` es verdadero, salta sobre `b` dejando `a` en stack; si falso, descarta `a` y evalúa `b`.
  - Verificable porque `verdadero o (1 // 0)` no produce error de división por cero — la rama no se compila a saltar, sino a un OP_SALTAR_SI_FALSO + OP_SALTAR que evita ejecutar el lado derecho.
- **`SENT_SI`** con cadena arbitraria de `si` / `sino si` / `sino`:
  - Cada rama compila `cond → OP_SALTAR_SI_FALSO else → OP_DESCARTAR → cuerpo → OP_SALTAR fin`.
  - La rama final `sino` no tiene condición ni descart.
  - Hasta 64 ramas en una cadena (límite arbitrario, suficiente).
- **`SENT_MIENTRAS`** con cláusula `sino` y `romper`/`continuar`:
  - Loop estándar: `inicio: cond → OP_SALTAR_SI_FALSO salir → OP_DESCARTAR → cuerpo → OP_BUCLE inicio`.
  - `romper` emite `OP_SALTAR` patcheable al fin del bucle (DESPUÉS de la cláusula `sino` para que `romper` salte sobre ella, semántica Python).
  - `continuar` emite `OP_BUCLE` al inicio de la condición.
  - Cláusula `sino` ejecutada solo si terminamos por condición falsa (no por break) — gracias a que el OP_SALTAR_SI_FALSO `salir` apunta antes de `sino` y los `romper` saltan después.
- **`SENT_ASIGNAR_AUG`** (`x op= expr`) compila como `x = x op expr`: emite `OP_OBTENER_GLOBAL` + compilar expr + opcode binario + `OP_DEFINIR_GLOBAL`. Soporta `+= -= *= /= //= %= **=`.
- **Limitación documentada**: `SENT_PARA` queda para S6 (necesita iteración sobre cadena/rango/lista que se conectará con las colecciones en bytecode).
- **`tests/unit/test_bytecode_control.c`** con 8 grupos: lógica con cortocircuito demostrado, `si`/`sino si`/`sino`, `mientras` con romper/continuar/sino, asignación aumentada (todas las variantes), y programas realistas: factorial(25)=26 dígitos, Fibonacci(30)=832040, 2^64 (20 dígitos), anidamiento `mientras` en `mientras`.
- **55 tests verde** (22 unit + 33 integración).

### En desarrollo (Fase 6 — Compilador + VM bytecode)
- ✅ Sesión 1: infraestructura `Chunk` + enum `OpCode` + disassembler.
- ✅ Sesión 2: refactor del evaluador (helpers reutilizables) + compilador para expresiones + VM stack-based con dispatch loop.
- ✅ Sesión 3: variables globales (DEFINIR/OBTENER/ASIGNAR), `imprimir(...)` como built-in en bytecode, sentencias básicas (asignación, expresión, pasar, bloque).
- ✅ Sesión 4: control de flujo en bytecode (`si`/`mientras` con `romper`/`continuar`/`sino`, lógica con cortocircuito, asignación aumentada).
- ✅ Sesión 5: funciones top-level con recursión + variables locales + llamadas en bytecode.
- ✅ Sesión 6: nativas en VM via OP_LLAMAR + colecciones (lista/tupla/dicc/conjunto) en bytecode + indexación + flag `--bytecode` + tag v0.6.0.
- ⏳ Aplazado a v0.6.1: `SENT_PARA`, closures con upvalues, atributos, slicing, lambda, intentar/atrapar.

### Añadido (Fase 6 sesión 5)
- **`VAL_FUNCION_BC`** y **`struct FuncionBC`** en `chunk.{h,c}`: función compilada a bytecode con su propio `Chunk`, nombre (heap-duplicated), aridad y refcount. Comparte refcount con el resto de tipos colección. Diferente de `VAL_FUNCION` (que es para tree-walking) — coexisten para no romper el evaluador antiguo.
- **CallFrame stack en la VM**: refactor de la VM para soportar llamadas anidadas. Cada `CallFrame` contiene chunk activo, ip y `base_pila`. Stack de hasta 64 frames; pila de Valores ampliada a 1024 slots para acomodar varias llamadas. El frame[0] es el del chunk top-level.
- **`OP_LLAMAR [n_args]`** en la VM: lee el callee del slot `tope - n - 1`, valida que es `VAL_FUNCION_BC`, valida la aridad, crea un nuevo `CallFrame` con `base_pila = tope - n - 1`. Slot 0 del frame contiene el callee, slots 1..n los args, slots posteriores las locales.
- **`OP_RETORNAR` multi-frame**: pop el resultado, libera todos los slots del frame que termina (callee + args + locales) limpiamente, push el resultado en el frame anterior, decrementa `n_frames`. Si era el frame top-level, devuelve el resultado al cliente.
- **`OP_OBTENER_LOCAL [slot]`** y **`OP_ASIGNAR_LOCAL [slot]`**: acceso/escritura a `frame->base_pila[slot]`. Locales viven en el stack del frame; al retornar se liberan junto con el frame.
- **`ScopeCompilador`** en el compilador: representa una función en construcción. Mantiene el chunk de la función, la lista de locales (slot 0 = callee, slots 1..aridad = parámetros, posteriores = locales declaradas dinámicamente), y el stack de bucles abiertos para `romper`/`continuar` dentro de la función.
- **Lookup de identificadores con prioridad local → global**: `EXPR_IDENT` busca primero en `c->actual->locales`; si no encuentra, emite `OP_OBTENER_GLOBAL`.
- **Asignación con dispatch local/global**:
  - En el scope raíz (top-level): siempre globales.
  - Dentro de función: primera asignación a un nombre nuevo lo crea como **local** (sin emitir bytecode adicional — el valor ya quedó en el slot del stack); reasignaciones emiten `OP_ASIGNAR_LOCAL`. Mismo modelo para `+=`, `-=`, etc.
- **`SENT_FUNCION`** compilada con scope anidado:
  - `funcion_bc_nueva(nombre, aridad)` con chunk vacío.
  - Scope hijo con slot 0 = callee, slots 1..n = parámetros como locales.
  - Compila el cuerpo en el chunk hijo.
  - Emite `OP_NULO + OP_RETORNAR` implícitos al final (si el cuerpo no terminaba con `retornar`, esto cubre el caso `funcion f(): pasar fin funcion` → devuelve nulo).
  - Vuelve al scope padre y emite `OP_CONST <fn>` + `OP_DEFINIR_GLOBAL <nombre>`.
  - Limitación documentada: parámetros con valor por defecto NO soportados todavía en bytecode (sí en tree-walking) — error explícito.
- **`SENT_RETORNAR`**: compila el valor opcional + `OP_RETORNAR`. Error si está fuera de función.
- **`EXPR_LLAMADA` general**: callee + args + `OP_LLAMAR [n]`. El caso especial `imprimir(...)` sigue emitiendo `OP_IMPRIMIR` directamente (corto-circuitado solo cuando no hay un local llamado `imprimir` que sombrear).
- **Sin closures todavía**: una función definida dentro de otra NO captura las locales de la enclosing — solo accede a sus propias locales y a globales. Las closures con upvalues están planeadas para una sesión adicional o F6 S6.
- **`tests/unit/test_bytecode_funciones.c`** con 8 grupos: función básica con args, recursión (factorial 10/20, fib 10), variables locales (declaración + reasignación + sombrear global con local), aridad mal con mensaje específico, retornar con/sin valor + dentro de bucle + fuera de función, no invocable, locales aisladas (no filtran a globales), y factorial(50) bignum (64 dígitos) recursivo end-to-end por bytecode.
- **56 tests verde** (23 unit + 33 integración).

Cierre de Fase 5: tree-walking interpreter con todas las colecciones
básicas. **Último release con tree-walking activo** según decisión
[B2](decisiones/B2-tree-walking-vs-bytecode.md): desde v0.6 el motor
de producción será la VM bytecode y el tree-walking se congela como
referencia ejecutable de regresión.

### Añadido (Fase 5 sesión 5)
- **Versión `0.5.0`** en `common.h` y `CMakeLists.txt`. Smoke test ajustado.
- **Tres ejemplos jugables nuevos** que ejercitan las nuevas colecciones end-to-end:
  - [`16_lista_busqueda.cor`](examples/16_lista_busqueda.cor): construcción incremental, slicing, inversa con `[::-1]`, `ordenar()`, función auxiliar de búsqueda lineal.
  - [`17_dicc_frecuencia.cor`](examples/17_dicc_frecuencia.cor): conteo de letras en `"abracadabra"` con `dicc[c] += 1`, iteración `para letra en dicc`, `claves()`/`valores()`.
  - [`18_conj_y_tupla.cor`](examples/18_conj_y_tupla.cor): deduplicación con `conjunto(lista)`, mapa de coordenadas con tuplas como claves de diccionario.
- **Tres tests `run_X`** con `PASS_REGULAR_EXPRESSION` para verificar que cada ejemplo produce la salida esperada.
- **51 tests verde** (18 unit + 33 integración).

### Añadido (Fase 5 sesión 1)
- **`VAL_LISTA`** y **`struct Lista`** en `valor.{h,c}`: array dinámico de `Valor` con refcount manual (sin GC todavía — Fase 7). Operaciones: `lista_nueva`, `lista_retener` (++ref), `lista_liberar` (--ref + free si llega a 0), `lista_agregar` (toma posesión, crece ×2 amortizado), `lista_obtener_ref`, `lista_asignar`. Capacidad inicial 4.
- **Semántica de referencia compartida**: `valor_clonar` para `VAL_LISTA` hace `lista_retener` (no deep copy) → asignar `b = a` comparte el mismo objeto Python-style. Las cadenas dentro de la lista siguen su propia ownership (cadena con `dueno_cadena=true` se duplica al clonar, las referencias al fuente no).
- **Limitación documentada**: el refcount no detecta ciclos. Una lista que se contiene a sí misma filtrará memoria; aceptable hasta Fase 7 (mark-sweep real).
- **`valor_a_cadena` para lista**: produce `[a, b, c]` usando una nueva función `valor_a_repr` que envuelve cadenas en comillas (`[1, "hola"]` en lugar de `[1, hola]`). Recursivo para listas anidadas.
- **`valor_iguales` para lista**: comparación element-wise; mismo objeto (puntero) → `true` por short-circuit; longitudes distintas → `false`.
- **`valor_es_verdadero` para lista**: `cuenta > 0`. Lista vacía es falsa, no vacía es verdadera (Python).
- **`EXPR_LISTA` en evaluador**: evalúa cada elemento de izquierda a derecha; si alguno falla, libera lista parcial y propaga error. Trailing comma del parser ya soportada en sintaxis.
- **`EXPR_INDICE` en evaluador**: `lista[i]` con `i` entero o booleano. Soporta índice negativo (cuenta desde el final). Bounds check con `ErrorDeIndice` específico que reporta el índice y el tamaño. Cadenas, diccionarios y otros tipos quedan para sesiones siguientes.
- **`+` de listas**: `[1, 2] + [3, 4]` → `[1, 2, 3, 4]`. Lista NUEVA con refcount 1, deep-clona elementos (cadenas con dueño se duplican; bignum se copia; listas internas comparten refcount).
- **`*` de listas**: `[1, 2] * 3` y `3 * [1, 2]` → `[1, 2, 1, 2, 1, 2]`. Repetición negativa o por cero produce `[]`. Detecta overflow de tamaño.
- **`en` extendido** para listas: `valor en lista` con búsqueda lineal usando `valor_iguales`. Mantiene también `subcadena en cadena`.
- **`para x en lista`**: itera elementos en orden; cada iteración asigna un clon del elemento al objetivo. Soporta `romper`/`continuar` y cláusula `sino` con la misma semántica que sobre cadenas y rangos.
- **`longitud(lista)`** built-in: devuelve `cuenta` como entero. `tipo(lista)` devuelve `"lista"`.
- **`tests/unit/test_runtime_listas.c`** con 8 grupos: literal (vacío, mixto, anidado, trailing comma), indexación (positivo/negativo/fuera-de-rango, no-entero, anidado), operadores (`+`/`*`/`en`/`no en`/`==` con cross-tipo entero=decimal), iteración (suma, concat, romper, sino), built-ins, referencia compartida (asignar `b = a` no rompe), programa promedio decimal, construcción de cuadrados con `rango()` y concat.
- **42 tests verde** (15 unit + 27 integración).

### Añadido (Fase 5 sesión 2)
- **Mutación `lista[i] = valor`**: extendido `SENT_ASIGNAR` para aceptar `EXPR_INDICE` como destino. Soporta índices negativos. `ErrorDeIndice` específico cuando fuera de rango. La asignación destruye el valor previo y toma posesión del nuevo.
- **Mutación aumentada `lista[i] op= valor`**: `SENT_ASIGNAR_AUG` extendido con misma lógica. Lee, computa con `aplicar_binario`, escribe atómicamente. Funciona para todas las variantes (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`).
- **Semántica de referencia confirmada por tests**: `b = a; b[0] = 99` cambia también `a[0]` (Python-like). `agregar(b, 4); longitud(a)` reporta 4.
- **Slicing `lista[a:b:c]`** (`EXPR_REBANADA`) con semántica Python:
  - Cualquier campo opcional (`[:]`, `[a:]`, `[:b]`, `[::c]`, `[a:b:c]`).
  - Defaults dependientes del signo del paso: paso > 0 → `inicio=0`, `fin=cuenta`; paso < 0 → `inicio=cuenta-1`, `fin=-1`.
  - Índices negativos cuentan desde el final.
  - Índices fuera de rango se *clampean* silenciosamente (no error).
  - Paso negativo invierte: `[1,2,3,4,5][::-1] == [5,4,3,2,1]`.
  - Paso 0 produce `ErrorDeValor`.
- **Built-ins de mutación de listas** (estilo función-libre hasta que F8 traiga método-syntax `lista.agregar(x)`):
  - **`agregar(lista, x)`**: añade al final. Devuelve nulo.
  - **`quitar(lista, indice=-1)`**: elimina y devuelve el elemento. Sin índice quita el último. Negativos cuentan desde el final. Lista vacía → `ErrorDeIndice`.
  - **`insertar(lista, indice, valor)`**: inserta antes del índice. Indices fuera de rango se clampean a [0, cuenta] (Python `list.insert`).
  - **`invertir(lista)`**: invierte en sitio (O(n/2) swaps).
  - **`ordenar(lista)`**: ordena in-place con `qsort` libc + comparador propio. Numéricos (entero/decimal/booleano) por valor; cadenas lexicográfico. Tipos mixtos no comparables → error explícito.
- **`tests/unit/test_runtime_listas_mut.c`** con 11 grupos: mutación simple/aug, referencia compartida en mutación, slicing básico (omisiones, negativos, clamping), slicing con paso (positivo/negativo, paso 0 → error), `agregar`, `quitar` (con/sin índice, vacía), `insertar` (con clamping), `invertir`, `ordenar` (numérico/cadenas/mixto/incomparable), y un quicksort recursivo end-to-end como prueba de integración.
- **43 tests verde** (16 unit + 27 integración).

### Añadido (Fase 5 sesión 3)
- **`VAL_DICCIONARIO`** y **`struct Diccionario`** en `valor.{h,c}`: tabla hash con probing lineal, capacidad potencia de 2, factor de carga 0.75, refcount manual (mismo patrón que `Lista`). Funciones: `dicc_nuevo`, `dicc_retener`, `dicc_liberar`, `dicc_asignar` (toma posesión de clave/valor), `dicc_obtener` (devuelve clon), `dicc_contiene`, `dicc_quitar`.
- **Hash genérico de Valores** que cumple la invariante `a == b ⇒ hash(a) == hash(b)`:
  - Booleanos, enteros (que quepan en `int64`) y decimales con valor entero exacto comparten el camino rápido `hash_int64`. Por eso `dicc[1]`, `dicc[1.0]` y `dicc[verdadero]` acceden al mismo slot.
  - Bignums grandes hashean por dígitos + signo. Decimales no enteros por bit pattern del double. Cadenas con FNV-1a 64-bit. Funciones por puntero.
  - `valor_es_hashable` rechaza `lista`, `diccionario`, `rango` como claves.
- **`EXPR_DICCIONARIO` literal** `{clave: valor, ...}`. Diccionario vacío `{}` (resuelto por el parser distinguiéndolo del conjunto).
- **`dicc[clave]` (lectura)**: extiende `EXPR_INDICE`. Clave inexistente produce `ErrorDeClave: <repr>` con la representación del valor que faltaba.
- **`dicc[clave] = valor` (asignación e inserción)**: extiende `SENT_ASIGNAR` con destino `EXPR_INDICE` para diccionarios. Crea la entrada o sobrescribe el valor existente.
- **`dicc[clave] op= valor` (asignación aumentada)**: extiende `SENT_ASIGNAR_AUG` análogamente. La clave debe existir o se reporta `ErrorDeClave`.
- **`clave en dicc` y `clave no en dicc`**: extiende `evaluar_en` con búsqueda hash O(1) amortizado.
- **`para clave en dicc`**: itera las claves del diccionario en orden de slot (no inserción — limitación documentada). Soporta `romper`/`continuar`/cláusula `sino` igual que las otras iteraciones.
- **Igualdad estructural** dos diccionarios son iguales si tienen las mismas claves con valores iguales (orden irrelevante).
- **Built-ins nuevos**:
  - **`claves(dicc)`**: devuelve una lista con las claves.
  - **`valores(dicc)`**: devuelve una lista con los valores.
  - `longitud(dicc)` extendido para devolver `cuenta`.
  - `tipo(dicc)` devuelve `"diccionario"`.
- **Pretty-printer** produce `{"clave": valor, ...}` usando `valor_a_repr` en claves y valores.
- **Limitación documentada**: el orden de iteración (y de `claves`/`valores`) sigue el layout interno del hash table — NO el orden de inserción como Python 3.7+. Aceptable hasta v1.0; se puede cambiar a hash-table ordenada en una versión futura.
- **`tests/unit/test_runtime_diccionarios.c`** con 12 grupos: literal y acceso, asignación/aumentada, membership, iteración, `longitud`/`tipo`, `claves`/`valores`, igualdad estructural (orden distinto), hash unificado entero/decimal/booleano (`dicc[1]` y `dicc[1.0]` mismo slot), tipos no hashables como clave, referencia compartida (`b = a` y mutar `b` afecta `a`), programa de conteo de caracteres en `"abracadabra"` (resultado: `a` aparece 5 veces), diccionario anidado de personas.
- **44 tests verde** (17 unit + 27 integración).

### Añadido (Fase 5 sesión 4)
- **`VAL_CONJUNTO`** y **`struct Conjunto`** en `valor.{h,c}`: hash set construido con la misma estrategia de probing lineal y refcount que `Diccionario`. API: `conj_nuevo`, `conj_retener`, `conj_liberar`, `conj_agregar` (toma posesión, deduplica si ya existe), `conj_contiene`, `conj_quitar`. Sólo elementos hashables (no listas/diccionarios/conjuntos).
- **`VAL_TUPLA`** y **`struct Tupla`** en `valor.{h,c}`: secuencia inmutable con refcount. Sin operaciones de mutación (no hay agregar/asignar). API: `tupla_nueva` (aloca slots no inicializados), `tupla_retener`, `tupla_liberar`. Hashable si todos sus elementos lo son — combinable con `Diccionario` y `Conjunto` como clave.
- **`EXPR_CONJUNTO` literal `{a, b, c}`**: evalúa de izquierda a derecha y deduplica con la igualdad estructural. Vacío explícito requiere `conjunto()` porque `{}` es diccionario vacío.
- **`EXPR_TUPLA` literal**:
  - `()` tupla vacía.
  - `(x,)` tupla de un elemento (coma obligatoria).
  - `(a, b, ...)` tupla múltiple.
  - **Distinción** `(x)` (grupo) vs `(x,)` (tupla 1) ya manejada por el parser desde la sesión 3 sesión 5.
- **Pretty-printer** específico:
  - Conjunto vacío imprime `conjunto()` (no `{}`, que es diccionario).
  - Conjunto no vacío `{a, b, c}`.
  - Tupla `(a, b, c)`, vacía `()`, de uno `(x,)`.
- **`hash_valor` extendido** para tuplas: combina los hashes de cada elemento estilo Python `tuplehash`.
- **`valor_es_hashable` actualizado**: tupla es hashable si todos sus elementos lo son (recursivo); conjuntos NO son hashables.
- **`EXPR_INDICE` para tupla**: `t[i]` con índice positivo/negativo y `ErrorDeIndice` específico.
- **Operador `en`** extendido para conjunto (búsqueda hash O(1)) y tupla (búsqueda lineal).
- **Iteración `para x en conjunto`** y **`para x en tupla`** con la misma semántica que el resto: clon por iteración, soporte de `romper`/`continuar`/cláusula `sino`. El conjunto itera en orden de slot interno (no de inserción — limitación documentada).
- **`longitud(conjunto)`** y **`longitud(tupla)`** funcionan; `tipo()` devuelve `"conjunto"` o `"tupla"`.
- **`conjunto()` built-in** con dos formas:
  - `conjunto()` → conjunto vacío.
  - `conjunto(iterable)` con iterable lista o tupla → conjunto con sus elementos deduplicados.
- **`agregar(conjunto, x)` extendido**: ahora acepta listas Y conjuntos como primer argumento; sobre conjunto deduplica al añadir.
- **Igualdad estructural**: dos conjuntos iguales si tienen los mismos elementos (orden irrelevante); dos tuplas iguales si tienen los mismos elementos en el mismo orden.
- **`tests/unit/test_runtime_conj_tup.c`** con 12 grupos: literal y deduplicación, membership, iteración, igualdad (con hash unificado entero/decimal/booleano), tipos no hashables; tupla literal (vacía/uno/varios) con distinción grupo, indexación, iteración, igualdad cross-tipo (tupla != lista), membership, tupla como clave de dict (con error si contiene lista), `conjunto()` constructor, programa de palabras únicas.
- **45 tests verde** (18 unit + 27 integración).

## [0.4.0] — 2026-04-28 — primer release jugable

Cierre de Fase 4 según el plan: tree-walking interpreter completo y
jugable end-to-end. Decisión [B2](decisiones/B2-tree-walking-vs-bytecode.md):
este release sirve como referencia ejecutable y se congelará en v0.5
tras añadir colecciones; desde v0.6 el motor de producción será la VM
bytecode.

### Añadido (Fase 4 sesión 5)
- **`cornamusa <archivo.cor>`** ahora ejecuta el programa con el evaluador tree-walking en lugar de solo lexarlo. Errores de runtime se reportan con caret indicators reusando `error_imprimir` (formato MENSAJES.md §2). Exit codes: 0 OK, 64 uso, 65 error de parseo, 70 error de runtime, 74 error de E/S.
- **`cornamusa --tokens <archivo>`** (anteriormente el modo por defecto) sigue disponible para inspección del lexer.
- **`cornamusa --ast <archivo>`** sin cambios — vuelca el AST en S-expression.
- **REPL interactivo funcional**: `cornamusa` sin argumentos abre un prompt `>>> ` y `... ` para continuación. Variables y funciones definidas persisten entre líneas. Heurística de continuación: línea acabada en `:` abre bloque, `fin` lo cierra; al volver a profundidad 0 se ejecuta el buffer acumulado. Una línea vacía con buffer ejecuta y reinicia. `salir` o EOF terminan.
- **Persistencia REPL**: arena compartida durante toda la sesión + `strdup` de cada bloque ejecutado para que las claves del entorno y los nodos AST de funciones definidas previamente sigan vivos al ejecutar líneas posteriores. Las cadenas duplicadas se filtran deliberadamente — viven hasta el fin del proceso.
- **Tres ejemplos jugables nuevos**:
  - [`13_factorial_jugable.cor`](examples/13_factorial_jugable.cor): factorial recursivo con bignum (`100!` = 158 dígitos exactos).
  - [`14_contar_vocales.cor`](examples/14_contar_vocales.cor): `para letra en cadena` UTF-8 + acumulador + `o` encadenado + `longitud()`.
  - [`15_fizzbuzz_jugable.cor`](examples/15_fizzbuzz_jugable.cor): FizzBuzz clásico con `rango()`, `si`/`sino si`/`sino`, `%`.
- **Tests de ejecución end-to-end**: nuevos `run_X` con `PASS_REGULAR_EXPRESSION` para verificar que cada ejemplo jugable produce la salida esperada. Cubren los 4 casos representativos (hola_mundo, factorial, contar_vocales, fizzbuzz).
- **Tests `lex_X` ahora usan `--tokens`**: la verificación lexicográfica de los 12 ejemplos sigue intacta como paso de regresión, pero independiente del runtime — un ejemplo puede usar features futuras (closures, listas) y aún así pasar `lex_X`.
- **Versionado**: `CORNAMUSA_VERSION` actualizado a `"0.4.0"` en `common.h` y `CMakeLists.txt`. Smoke test ajustado.
- **41 tests verde** (14 unit + 27 integración: 12 lex + 8 parse + 4 run + 3 examples nuevos).

### Añadido (Fase 4 sesión 4)
- **Tipo `VAL_RANGO`**: nuevo variante en `Valor` con tres `mp_int *` (inicio, fin, paso). Iterable con bignum, ascendente o descendente. `valor_clonar` hace deep copy; `valor_destruir` libera los tres mp_int. `valor_es_verdadero` devuelve `true` si la iteración produciría al menos un elemento. `rango(a, b, paso)` se imprime como `"rango(a, b, paso)"`.
- **Refactor de `VAL_FUNCION` y `VAL_NATIVA`** a estructuras inline (sin allocations heap):
  - `VAL_FUNCION` referencia un `const Sent *def` del AST + un `Entorno *entorno_definicion`. Sin closures (decisión B2): el entorno_definicion siempre es global en S4 — campo reservado para closures futuros en Fase 6+.
  - `VAL_NATIVA` contiene nombre + puntero `FnNativa` (typedef en `valor.h`).
  - Ambos son trivialmente clonables (struct copy), no requieren ownership tracking, y `valor_iguales` compara por referencia subyacente.
- **`src/nativos.{h,c}`** con la API `nativos_registrar(globales)` que añade los built-ins al entorno:
  - **`imprimir(*args)`**: variádica. Imprime cada argumento separado por espacio + `\n`. Sin kwargs (`separador`, `final`) — se añadirán cuando lleguen kwargs al lenguaje. Devuelve nulo.
  - **`longitud(x)`**: cadena → número de **code points UTF-8** (no bytes); rango → número de elementos producidos. Otros tipos producen `ErrorDeTipo`.
  - **`tipo(x)`**: devuelve cadena con el nombre del tipo en castellano (`"entero"`, `"decimal"`, `"cadena"`, `"booleano"`, `"nulo"`, `"funcion"`, `"rango"`).
  - **`rango([inicio,] fin [, paso])`**: tres formas. Acepta solo enteros (booleano se promueve a 1/0). `paso == 0` produce `ErrorDeValor`. Bignum-friendly: `rango(0, 10**100, 1)` es válido (aunque su iteración tarde una eternidad).
- **`SENT_FUNCION` en evaluador**: crea un `VAL_FUNCION` y lo asigna en el entorno actual. La función puede llamarse a sí misma porque el nombre está definido antes de cualquier llamada.
- **`SENT_RETORNAR`**: evalúa la expresión opcional (`retornar` desnudo → nulo), guarda el valor en `ev->valor_retorno` y marca `ev->control = EJEC_RETORNAR`. El bucle envolvente o `llamar_usuario` la consume.
- **`EXPR_LLAMADA` en evaluador**: evalúa el callee, evalúa cada argumento, despacha:
  - `VAL_NATIVA`: invoca el puntero a función C con los args ya evaluados (ownership del cliente).
  - `VAL_FUNCION`: crea un nuevo `Entorno` hijo del entorno_definicion, liga parámetros (con valores por defecto si faltan), ejecuta el cuerpo, recoge `ev->valor_retorno` si apareció `EJEC_RETORNAR`, restaura entorno y control. Aridad validada con mensaje específico ("`f()` esperaba N argumentos, recibió M").
- **Recursión funcional**: factorial(50)=64 dígitos y factorial(100)=158 dígitos pasan tests recursivos sin stack-smashing (depth ≈ 100 frames).
- **`para` ahora itera también `VAL_RANGO`**: usa `mp_add` para avanzar y `mp_cmp` para terminar. Soporta paso ascendente y descendente. Combinable con `romper`/`continuar`/cláusula `sino` igual que con cadenas.
- **`Evaluador` ahora es `typedef struct Evaluador { ... }`** (con nombre explícito) para permitir forward declaration desde `valor.h` en la firma de `FnNativa`.
- **`tests/unit/test_runtime_funciones.c`** con 11 grupos de tests: definición y llamada simple, recursión (factorial 10/50, fib 15), parámetros con defaults, aridad mal con mensaje específico, no invocable, `retornar` con/sin valor y dentro de bucle, `tipo()` para cada tipo, `longitud()` UTF-8 + rangos, `rango()` 1/2/3 args con paso negativo y cero iteraciones, `imprimir()` no rompe, programa de pares con función auxiliar, factorial(100) recursivo (158 dígitos).
- **34 tests verde** (14 unit + 20 integración).

### Añadido (Fase 4 sesión 3)
- **Evaluador de sentencias** en `evaluador.{h,c}`: nueva API `evaluador_ejecutar_sent` y `evaluador_ejecutar_programa`. Modelo de control de flujo sin `setjmp`: nuevo enum `ControlFlujo` (`EJEC_NORMAL`, `EJEC_ROMPER`, `EJEC_CONTINUAR`, `EJEC_RETORNAR`). Las construcciones envolventes (bucles, llamadas) inspeccionan y resetean `ev->control`.
- **`SENT_ASIGNAR`**: solo destino `EXPR_IDENT` en v0.4 (tuple destructuring, atributos e índices como destino quedan para v0.3.1+/F5). La asignación crea o sobrescribe en el entorno actual con `entorno_definir`. Sin tipos: la misma variable puede pasar de entero a cadena a decimal.
- **`SENT_ASIGNAR_AUG`** (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`): obtiene el valor actual (clon) del entorno, evalúa el operando derecho, aplica el operador binario equivalente y reasigna. La variable debe estar previamente definida (semántica Python: `ErrorDeNombre` si no existe). `x /= 2` produce decimal aunque `x` sea entero.
- **Refactor de `eval_binario`**: extraída `aplicar_binario(ev, op, a, b, e)` que toma posesión de dos valores ya evaluados. Reutilizada por `SENT_ASIGNAR_AUG` para no duplicar la lógica.
- **`SENT_PASAR`**: no-op explícito.
- **`SENT_ROMPER` / `SENT_CONTINUAR`**: marcan `ev->control` y dejan que el bucle envolvente lo gestione. Si `evaluador_ejecutar_programa` detecta control de flujo no consumido al volver al top-level, produce error explícito ("control de flujo fuera de su contexto").
- **`SENT_SI`**: itera sobre la cadena de `RamaSi` (`si` + `sino si`* + `sino`?) y ejecuta la primera rama cuya condición sea verdadera; la rama final `sino` tiene `condicion=NULL` y siempre se toma si se llega.
- **`SENT_MIENTRAS`**: bucle clásico con `romper`/`continuar`. Cláusula `sino` con semántica Python: se ejecuta sólo si el bucle terminó por condición falsa, NO si se rompió.
- **`SENT_PARA`** sobre cadenas: itera **code points UTF-8** (no bytes), de modo que `"niño"` produce 4 iteraciones (`'n'`, `'i'`, `'ñ'`, `'o'`). Cada iteración crea un nuevo `Valor` cadena de 1 code point y lo asigna al objetivo. `romper`, `continuar` y cláusula `sino` con la misma semántica que `mientras`. Otros iterables (rango, lista, diccionario) llegarán en S4/F5. Iterable no soportado produce `ErrorDeTipo` específico.
- **`SENT_BLOQUE`**: secuencia de sentencias; para al primer error o cuando aparece control de flujo no normal (que el bloque envolvente recogerá).
- **Aplazadas con error explícito**: `SENT_FUNCION`/`SENT_RETORNAR` (S4), `SENT_CLASE`/`SENT_INTENTAR`/`SENT_LANZAR`/`SENT_IMPORTAR`/`SENT_DESDE_IMPORTAR`/`SENT_GLOBAL`/`SENT_NOLOCAL` (F5+).
- **`tests/unit/test_runtime_sentencias.c`** con 12 grupos de tests sobre programas completos parseados y ejecutados:
  - Asignación simple, múltiples variables, cambio de tipo libre.
  - Asignación aumentada (todas las variantes incluyendo concatenación de cadenas con `+=` y true-div con `/=`).
  - `si`/`sino si`/`sino` en cascada y one-liner.
  - `mientras` clásico (suma 1..10), `romper`, `continuar` (suma de pares), cláusula `sino` ejecutada y NO ejecutada.
  - `para` sobre cadena ASCII y UTF-8 (`"niño"` → 4 iteraciones), concatenación durante iteración, `romper`, cláusula `sino`, error con iterable entero.
  - Programas realistas: factorial(25) con bignum (26 dígitos), conteo de vocales en `"murcielago"`, Fibonacci(30) iterativo, 2^64 (20 dígitos).
  - Anidamiento: `si` en `mientras`, `mientras` en `para`.
- **33 tests verde** (13 unit + 20 integración).

### Añadido (Fase 4 sesión 2)
- **`src/evaluador.{h,c}`** — evaluador tree-walking de expresiones. Modelo de errores sin `setjmp`: cada función devuelve `Valor` y rellena `Evaluador.error` (con línea, columna y mensaje) en caso de fallo. El cliente comprueba `evaluador_tiene_error` tras cada evaluación.
- **Literales**: `EXPR_LITERAL_ENTERO` parsea decimal/hex/oct/bin con `_` separadores; `EXPR_LITERAL_DECIMAL` con notación científica; `EXPR_LITERAL_CADENA` quita comillas y procesa escapes mínimos (`\n \t \r \\ \' \"`); `EXPR_LITERAL_BOOLEANO`, `EXPR_LITERAL_NULO`. `EXPR_LITERAL_F_CADENA` produce error explícito (interpolación llega en F4 S5 + parser de sub-expresiones).
- **Identificadores**: lookup en el entorno actual con scope chain por punteros a padre. Si el nombre no existe, error `ErrorDeNombre: nombre 'X' no esta definido`.
- **Aritmética entero⊕entero** vía libtommath: `+`, `-`, `*`, `//` (floor division estilo Python para negativos), `%` (módulo matemático con resultado siempre del signo del divisor), `**` (potencia con exponente que cabe en `int`; exponente negativo promociona a decimal `pow()`). Sin overflow: `2 ** 100` da el bignum exacto de 31 dígitos, `10 ** 100` el gugol completo.
- **True division `/`**: siempre produce `VAL_DECIMAL` (estilo Python 3), incluso para enteros divisibles (`6 / 2` → `3.0`).
- **Promoción mixta entero/decimal**: cualquier operación con un decimal convierte el otro operando a doble. `1 + 2.5` → `3.5`. Para enteros muy grandes la conversión a doble pierde precisión, conducta documentada y consistente con Python.
- **Aritmética decimal⊕decimal** con `pow()`, `floor()` y módulo Python (`a - floor(a/b)*b` — resultado del signo del divisor: `-7.5 % 3.0 == 1.5`).
- **Bitwise**: `&`, `|`, `^` vía `mp_and`/`mp_or`/`mp_xor`. `<<` (`mp_mul_2d`) y `>>` (`mp_div_2d` con ajuste a floor para negativos). `~` (complemento a uno) vía `mp_complement`. Booleanos se promueven a entero (1/0). Errores específicos para desplazamiento negativo o demasiado grande.
- **Comparaciones**: `==`, `!=`, `<`, `<=`, `>`, `>=`. Función `comparar_valores` con `Orden` (LT/EQ/GT/INCOMP). `==` y `!=` permiten tipos distintos (devuelven `false`); `<` etc. dan `ErrorDeTipo` si los tipos no son comparables. Cross-tipo numérico: entero/decimal/booleano se comparan matemáticamente. Cadenas: lexicográfico byte a byte (UTF-8 preservado).
- **`valor_iguales` extendido**: ahora trata `verdadero == 1`, `falso == 0`, `verdadero == 1.0` como verdadero (Python: bool es subclase de int).
- **Lógica con cortocircuito**: `y` y `o` evalúan el operando derecho solo si el izquierdo no decide. Devuelven el **valor decisor original** (no booleano), igual que Python: `0 o 42` → `42`, `1 y "x"` → `"x"`. El test `verdadero o (1 // 0)` pasa porque la división por cero nunca se evalúa.
- **Unarios**: `-x` (negación numérica con `mp_neg`), `+x` (identidad), `no x` (negación lógica usando `valor_es_verdadero`), `~x` (complemento a uno).
- **Cadenas**: `+` concatena (nuevo buffer en heap, `dueno_cadena=true`), `*` con entero repite (con detección de overflow del tamaño total), comparaciones lexicográficas, `subcadena en cadena` mediante búsqueda lineal.
- **`es` (identidad)**: para funciones/nativas compara puntero. Para inmutables (entero, decimal, cadena, booleano, nulo) coincide con `valor_iguales` por ahora — se refinará cuando lleguen instancias y objetos heap.
- **`en` (membership)**: solo soportado para `subcadena en cadena` en esta sesión. Listas/diccionarios llegan en F5.
- **Aplazadas a sesiones siguientes** (devuelven error explícito): `EXPR_LLAMADA`, `EXPR_ATRIBUTO`, `EXPR_LAMBDA`, colecciones (`EXPR_LISTA`, `EXPR_DICCIONARIO`, `EXPR_CONJUNTO`, `EXPR_TUPLA`), `EXPR_INDICE`, `EXPR_REBANADA`, f-string con interpolación parseada.
- **`tests/unit/test_runtime_evaluador.c`** con ~70 verificaciones agrupadas en 14 grupos: literales (cada base, escapes), aritmética entera (precedencia, asociatividad, bignum 31 dígitos), división y mixto, decimales, comparaciones (mismo tipo y cross-tipo), bitwise (incluido `~`), unarios (incluida doble negación), lógica con cortocircuito demostrado, cadenas (concat/repetición/membership), identidad (`es`, `no es`, `es no`), identificadores con entorno definido, errores (división por cero, nombre, tipo), y combinaciones realistas (gugol, promedio, condiciones encadenadas).
- **32 tests verde** (12 unit + 20 integración).

### Añadido (Fase 4 sesión 1)
- **Vendoreado [libtommath 1.3.0](https://github.com/libtom/libtommath)** en `vendor/libtommath/` (~150 archivos `.c`, Public Domain). Bignum desde día 1 según decisión [B3](decisiones/B3-representacion-numerica.md). Compilado como librería estática separada en CMake.
- **`src/valor.{h,c}`** — tipo `Valor` con tagged union de 7 variantes:
  - `VAL_NULO`, `VAL_BOOLEANO`, `VAL_DECIMAL` (IEEE 754 double).
  - `VAL_ENTERO` con `mp_int *` boxed (precisión arbitraria; `factorial(100)` produce número de 158 dígitos sin overflow).
  - `VAL_CADENA` con bandera `dueno_cadena` (referencia al buffer fuente vs heap).
  - `VAL_FUNCION`, `VAL_NATIVA` (preparados para sesión 4).
- **Constructores**: `valor_nulo()`, `valor_booleano()`, `valor_decimal()`, `valor_decimal_de_lexema()`, `valor_entero_de_long()`, `valor_entero_de_lexema()` (acepta decimal, hex `0xff`, octal `0o755`, binario `0b1010`, con `_` separadores), `valor_cadena_referencia()`, `valor_cadena_duplicar()`.
- **Operaciones**: `valor_destruir`, `valor_clonar` (deep), `valor_imprimir`, `valor_a_cadena`, `valor_nombre_tipo`, `valor_es_verdadero` (truthiness ESPEC §6.2), `valor_iguales` (igualdad ESPEC §6.3 incluyendo `1 == 1.0`).
- **`src/entorno.{h,c}`** — `Entorno` (scope chain) con tabla hash de probing lineal:
  - Hash FNV-1a 32-bit, factor de carga 0.75, redimensionamiento dinámico.
  - API: `entorno_iniciar`, `entorno_destruir`, `entorno_definir`, `entorno_obtener` (devuelve clon), `entorno_asignar` (mutación), `entorno_existe`.
  - Scope chain por puntero a `padre`: una variable se busca aquí y, si no, en los entornos enclosing.
  - El entorno es **dueño** de los Valores; al destruirse libera todos sus mp_int y cadenas con dueño.
- **Sin GC** en Fase 4 (decisión B2 + B3): liberación eager. Cuando un entorno se destruye, todos los valores locales se liberan. En Fase 7 se añade GC mark-sweep.
- **`tests/unit/test_runtime_valor.c`** con ~25 tests: construcción de cada tipo, bignum (factorial 100 = 158 dígitos), verdadez, igualdad (incluyendo `1 == 1.0`), clonación, operaciones de entorno (definir, obtener, asignar, scope chain con padre, shadowing, redimensionamiento al añadir 100 variables).
- **31 tests verde** (11 unit + 20 integración).

### En desarrollo (Fase 3 — Parser y AST, objetivo v0.3.0)
- ✅ Sesión 1: AST + arena allocator + Pratt parser para expresiones.
- ✅ Sesión 2: sentencias simples + control de flujo + validación `fin <etiqueta>`.
- ✅ Sesión 3: funciones, clases, lambda.
- ✅ Sesión 4: excepciones, módulos, global/nolocal.
- ✅ Sesión 5: literales de colección, indexación, slicing, operadores de identidad/membership, `--ast` flag, tests de integración del parser, tag v0.3.0.

### Añadido (Fase 3 sesión 5)
- **Literales de colección** (`EXPR_LISTA`, `EXPR_DICCIONARIO`, `EXPR_CONJUNTO`, `EXPR_TUPLA`) con todas las variantes:
  - `[1, 2, 3]` lista; `[]` lista vacía; trailing comma permitida.
  - `{"k": "v"}` diccionario; `{}` diccionario vacío.
  - `{1, 2, 3}` conjunto.
  - `()` tupla vacía; `(x,)` tupla de 1; `(a, b)` tupla de 2+.
  - **Distinción tupla vs grupo**: `(x)` es grupo, `(x,)` es tupla.
- **Indexación** (`EXPR_INDICE`): `lista[0]`, `dicc[clave]`, `obj.attr[i]`, encadenamientos `matriz[i][j]`.
- **Slicing** (`EXPR_REBANADA`): `lista[a:b]`, `lista[a:b:c]`, con omisiones (`[:b]`, `[a:]`, `[:]`, `[::c]`).
- **Operadores de identidad y membership** (ESPEC §5 `op_comp`):
  - `a es b` → identidad.
  - `a es no b` → identidad negada (forma ESPEC).
  - `a no es b` → identidad negada (forma natural castellana).
  - `a en b` → pertenencia.
  - `a no en b` → pertenencia negada.
  - Las formas con `no` se desazucaran a `(uop "no" (op "es" / "en" izq der))`.
- **Flag `--ast`** en `cornamusa` que vuelca el AST del programa en formato S-expression. Ejemplo: `cornamusa --ast programa.cor`.
- **`tests/unit/test_parser_colecciones.c`** con ~30 tests cubriendo cada forma de literal, indexación, slicing, distinción tupla/grupo, anidamiento.
- **Tests de integración del parser**: 8 ejemplos parsean correctamente con `--ast`:
  - ✅ 01_hola_mundo, 02_fizzbuzz, 04_factorial, 07_clases_herencia, 08_excepciones, 09_closures, 11_iterador, 12_modulos.
- **30 tests verde** (10 unit + 12 integración del lexer + 8 integración del parser).

### Aplazado a v0.3.1 (parsean en sesiones futuras de Fase 3)
- **Multi-target assignment** (`a, b = b, a + b`) — usado en 03_fibonacci.
- **Iteración con tuple destructuring** (`para palabra, conteo en pares.elementos():`) — usado en 06_diccionarios.
- **List comprehensions** (`[x*x para x en y si cond]`) — usadas en 05_listas y 10_quicksort.
- **f-strings con interpolación parseada** (actualmente `EXPR_LITERAL_F_CADENA` almacena el lexema completo; las expresiones `{...}` no se parsean como sub-AST todavía).

### Añadido (Fase 3 sesión 4)
- **AST de excepciones, módulos y declaraciones**:
  - `SENT_INTENTAR`: cuerpo + lista de cláusulas `atrapar` + `sino` opcional + `finalmente` opcional.
  - `SENT_LANZAR`: expresión opcional (NULL = re-raise).
  - `SENT_IMPORTAR`: ruta dotted + alias opcional.
  - `SENT_DESDE_IMPORTAR`: ruta + items con aliases opcionales (o `*`).
  - `SENT_GLOBAL` / `SENT_NOLOCAL`: lista de nombres.
- **Tipos auxiliares**: `Nombre` (puntero+longitud al lexema), `ItemImportado` (nombre + alias opcional), `ClausulaAtrapar` (tipo + alias + cuerpo).
- **Parser de excepciones**:
  - `intentar:` con cero o más `atrapar [TipoExc [como alias]]:`, opcional `sino:` (rama sin excepción), opcional `finalmente:`, cerrado con `fin intentar`.
  - Validación: `intentar` requiere al menos un `atrapar` O `finalmente`. Error específico si ambos faltan.
  - `atrapar`/`finalmente` ahora son terminadores válidos de bloque (extendido `en_inicio_de_termino`).
- **Parser de `lanzar`**: `lanzar expr` con expresión, o `lanzar` desnudo en la misma línea de un atrapar como re-raise. Heurística para detectar bare lanzar: nuevo line o token de cierre tras el keyword.
- **Parser de imports**:
  - Helper `parsear_ruta_modulo` consume `IDENT ('.' IDENT)*`.
  - `importar X.Y.Z [como W]`.
  - `desde X.Y importar A [como A2], B, C` o `desde X importar *`.
- **Parser de `global`/`nolocal`**: lista de identificadores separados por coma.
- **`tests/unit/test_parser_excepciones_modulos.c`** con ~22 tests cubriendo: cada forma de `intentar`/`atrapar`/`finalmente`/`sino`, `lanzar` con valor y bare, imports simples/dotted/con-alias, `desde X importar Y` con uno/varios items/alias/`*`, `global` y `nolocal` con uno/varios nombres, anidamiento realista (función con `intentar` dentro como en `examples/08_excepciones.cor`, closure con `nolocal` como en `examples/09_closures.cor`), y errores específicos.
- **21 tests verde** (9 unit + 12 integración).

### Añadido (Fase 3 sesión 3)
- **`SENT_FUNCION`** en AST: nombre, parámetros, anotación de retorno opcional, cuerpo.
- **`SENT_CLASE`** en AST: nombre, lista de superclases (`extiende A, B, C`), cuerpo.
- **`EXPR_LAMBDA`** en AST: parámetros + cuerpo (una sola expresión, no bloque).
- **`Parametro`** struct: nombre + anotación de tipo opcional + valor por defecto opcional.
- **Parser de funciones**:
  - `funcion nombre(p1, p2, ...) [-> tipo]:`
  - Parámetros con anotación de tipo (`n: entero`) y valor por defecto (`idioma="es"`) en cualquier combinación.
  - Anotación de retorno con `-> tipo`.
  - Cuerpo: bloque multilínea cerrado con `fin funcion`, o one-liner.
- **Parser de clases**:
  - `clase Nombre [extiende A, B, ...]:`
  - Multi-herencia sintácticamente aceptada (semántica MRO en runtime).
  - Cuerpo cerrado con `fin clase`. Métodos son sentencias `funcion` dentro.
- **Parser de lambda**:
  - `lambda x, y, n=10: x + y + n`
  - Parámetros sin paréntesis. Defaults permitidos. **Anotaciones de tipo NO permitidas** en lambda (el `:` siempre es terminador).
  - Cuerpo es una sola expresión.
- **Pretty-printer extendido**: `(funcion nombre (param x) (param y (defecto ...)) (retorno ...) (bloque ...))`, `(clase Nombre (extiende ...) (bloque ...))`, `(lambda (param x) <expr-cuerpo>)`.
- **Validación de etiquetas extendida**: `fin funcion` y `fin clase` ahora se validan correctamente. `fin si` cerrando una función produce mensaje específico.
- **`tests/unit/test_parser_funciones.c`** con ~20 tests cubriendo:
  - Funciones con 0/1/varios parámetros, anotaciones de tipo, defaults, anotación de retorno, one-liner.
  - Clases vacías, con métodos, con herencia simple y múltiple, ejemplo realista del `examples/07_clases_herencia.cor`.
  - Lambdas vacías, con uno/varios parámetros, con defaults, anidadas en llamadas (`mapear(lambda x: x*2, lista)`).
  - Validación: `fin funcion` no cierra `si` (y viceversa).
  - Errores: función sin nombre, sin `(`, clase sin nombre, lambda sin cuerpo.
  - Anidamiento realista: función con `si` dentro (patrón fibonacci).
- **Limitación documentada**: las palabras `y`, `o`, `no`, `en`, `es` (operadores lógicos/comparativos como palabra) **son keywords y no se pueden usar como identificadores**. Tests usan nombres alternativos (`z`, `n`).
- **20 tests verde** (8 unit + 12 integración).

### Añadido (Fase 3 sesión 2)
- **AST de sentencias** en `ast.{h,c}`: 11 variantes (`SENT_EXPR`, `SENT_ASIGNAR`, `SENT_ASIGNAR_AUG`, `SENT_PASAR`, `SENT_ROMPER`, `SENT_CONTINUAR`, `SENT_RETORNAR`, `SENT_SI` con cadena de `RamaSi`, `SENT_MIENTRAS`, `SENT_PARA`, `SENT_BLOQUE`). Pretty-printer en S-expression.
- **Parser de sentencias**: `parser_parsear_sentencia` y `parser_parsear_programa`. Maneja:
  - Sentencias simples: `pasar`, `romper`, `continuar`, `retornar [expr]`.
  - **Asignación simple** (`x = expr`) y **aumentada** (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`).
  - **Sentencia-expresión** (cualquier expresión usada como sentencia: `imprimir(x)`).
  - **Bloques `si`/`sino si`/`sino`** con cadena completa de ramas, cerrado con `fin si`.
  - **`mientras`/`fin mientras`** con cláusula `sino` opcional.
  - **`para X en Y:`/`fin para`** con cláusula `sino` opcional.
- **Detección de one-liners**: si tras `:` el siguiente token está en la misma línea, se parsea una sola sentencia sin requerir `fin <X>`. Si va a línea siguiente, se exige bloque multilínea cerrado con `fin <etiqueta>`.
- **Validación de `fin <etiqueta>`** mediante stack de bloques abiertos en el parser (`pila_bloques[64]`):
  - `fin si` solo cierra `si`. `fin para` solo cierra `para`. Etc.
  - Mensaje específico cuando la etiqueta no coincide:
    *"se esperaba 'fin si' (bloque abierto en línea 9), encontrado 'fin para'"*.
  - Mensaje específico cuando falta el `fin`:
    *"se esperaba 'fin si' para cerrar el bloque abierto en línea 9"*.
- **Recuperación de errores** con panic mode: tras un error, el parser sale del modo pánico al inicio de cada sentencia para poder reportar varios errores en un programa.
- **Anidamiento arbitrario**: `si` dentro de `para` dentro de `mientras` funciona; cada bloque tiene su propia entrada en el stack.
- **`tests/unit/test_parser_sentencias.c`** con ~30 tests cubriendo: cada sentencia simple, asignaciones, todas las variantes de `si`/`mientras`/`para` (con/sin `sino`, one-liner vs multilínea), anidamiento, validación de etiquetas (`fin para` cerrando un `si` da error, etc.), errores de sintaxis (`fin` desnudo, falta `:`, falta `fin`), y un programa completo de varias sentencias.
- **19 tests verde** (7 unit + 12 integración del lexer).

### Añadido (Fase 3 sesión 1)
- **`src/arena.{h,c}`** — arena allocator con bloques crecientes (~80 líneas). Aloca alineado a 8 bytes, libera todo en una sola llamada con `arena_destruir`. Patrón estándar para ASTs (lo usan V8, GCC, LLVM).
- **`src/ast.{h,c}`** — AST tipado con tagged union. Esta sesión define **expresiones** con 13 variantes:
  - Literales: `EXPR_LITERAL_ENTERO`, `EXPR_LITERAL_DECIMAL`, `EXPR_LITERAL_CADENA`, `EXPR_LITERAL_F_CADENA`, `EXPR_LITERAL_BOOLEANO`, `EXPR_LITERAL_NULO`.
  - `EXPR_IDENT`, `EXPR_BINARIO`, `EXPR_UNARIO`, `EXPR_LOGICA` (`y`/`o`).
  - `EXPR_LLAMADA`, `EXPR_ATRIBUTO`, `EXPR_GRUPO`.
  - Pretty-printer en formato S-expression (`(op "+" (lit-int 1) (lit-int 2))`) para tests y depuración.
- **`src/parser.{h,c}`** — Parser estilo **Pratt** con tabla de reglas (prefijo, infijo, precedencia). Maneja:
  - **14 niveles de precedencia** desde `o` (más bajo) hasta llamada/atributo (más alto).
  - **Asociatividad correcta**: izquierda para `+ - * / // % == != < > <= >= y o & | ^ << >>`, derecha para `**`.
  - **Llamadas con argumentos** (0 o más, separados por coma).
  - **Acceso a atributo encadenado** (`a.b.c`).
  - **Operadores unarios**: `-x`, `+x`, `no x`, `~x`.
  - **Recuperación de errores** con panic mode + flag `tuvo_error`.
  - **Mensajes de error con caret** reusando `error_imprimir_token` de Fase 2.
- **`tests/unit/test_parser_expresiones.c`** — 35+ tests cubriendo: literales (cada tipo), identificadores, operadores con precedencia y asociatividad correctas (`1 + 2 * 3` → `1 + (2*3)`; `2 ** 3 ** 4` → `2 ** (3**4)`), unarios anidados, lógicas (`y`/`o` con precedencia entre ellos y vs `no`), agrupación, llamadas con varios args y anidadas, atributos encadenados, métodos (`obj.metodo(arg)`), combinaciones realistas extraídas de ejemplos (`tipo(yo).__nombre__`, `n * factorial(n - 1)`, `x > 0 y x < 100`), y errores (paréntesis sin cerrar, atributo sin nombre, operador sin operando).
- Build verde con flags estrictos. **18 tests verde** (6 unit + 12 integración del lexer).

### En desarrollo (Fase 2 — Lexer, objetivo v0.2.0)
- ✅ Sesión 1: esqueleto del lexer + tokens simples (símbolos, operadores, comentarios).
- ✅ Sesión 2: literales numéricos y cadenas básicas.
- ✅ Sesión 3: identificadores Unicode + NFC + tabla de keywords.
- ✅ Sesión 4: f-strings y triple-quoted strings.
- ✅ Sesión 5: mensajes de error pulidos siguiendo MENSAJES.md + tests exhaustivos.

### Añadido (Fase 2 sesión 5)
- **Refactor `Token`**: nuevo campo `mensaje` (NULL para tokens normales, contiene el mensaje de error para `TT_ERROR`). El campo `inicio`/`longitud` ahora describe siempre el span en la fuente — para errores, el fragmento problemático que producirá el caret indicator. Esto permite mensajes de error con calidad de Rust/Python 3.10.
- **`struct Token` con nombre** (en lugar de typedef anónimo) para permitir forward declarations entre módulos.
- **`error_imprimir_token`** en `errores.{h,c}`: formatea un token de error siguiendo MENSAJES.md §2 con anatomía completa:
  ```
  ErrorDeSintaxis en archivo.cor:3:18
          retornar 1__2
                   ^^
  no se permiten guiones bajos consecutivos en literales numéricos
  ```
  Carets dibujados a partir de `columna` y `longitud` del token. La línea de fuente se localiza en el buffer original sin copiar.
- **`error_imprimir`** extendida para aceptar `fuente` y `longitud_span` opcionales. Si se proporcionan, dibuja el contexto de línea + carets.
- **`main.c` reescrita**: pipeline completo `archivo → fuente_cargar_archivo (NFC) → Lexer → tokens`. Reporta errores léxicos con `error_imprimir_token`. Nuevo flag `--tokens` que vuelca todos los tokens en formato debug `LINEA:COL TIPO "lexema"`.
- **Tests de integración**: 12 tests CTest (uno por ejemplo en `examples/`) que invocan `cornamusa <archivo.cor>` y verifican exit code 0 (sin errores léxicos). Etiquetados con label `integracion` en CTest.
- Tests unitarios actualizados: `t.inicio` → `t.mensaje` en las verificaciones de mensajes de error (4 archivos, ~15 ocurrencias).
- Verificado manualmente: los 12 ejemplos en `examples/` lexán sin error. El error de muestra (`1__2` en código) produce el caret indicator correcto bajo el span ofensivo.

**Total tests al cerrar Fase 2:** 17 (5 unit + 12 integración), 100% verde con build Release y -O3.

### Añadido (Fase 2 sesión 4)
- Lexer reconoce **f-strings** (`TT_F_CADENA`):
  - Prefijo `f` o `F` inmediatamente seguido de comilla simple o doble.
  - Interpolación `{expresión}` con tracking de profundidad de llaves balanceadas.
  - `{{` y `}}` son llaves literales (no abren ni cierran interpolación).
  - El lexema completo (incluyendo `f` y comillas) se almacena en el token; el parser/AST hará el mini-parse de cada interpolación cuando llegue Fase 3.
- Lexer reconoce **cadenas triple-quoted** (`"""..."""` y `'''...'''`):
  - Multilínea: el contador de líneas avanza correctamente al ver `\n` interno.
  - Comillas dobles o simples sueltas dentro no cierran la triple (solo tres consecutivas idénticas a la apertura).
  - Compatible con prefijo `f`: `f"""..."""` y `f'''...'''` se reconocen como `TT_F_CADENA`.
- Refactor interno: `escanear_cadena` es ahora dispatcher entre `escanear_cadena_simple` y `escanear_cadena_triple`. Helpers `procesar_escape` y `saltar_interpolacion` factorizan la lógica de escapes y brace tracking. Firma `bool` para señalar errores limpiamente.
- Errores nuevos:
  - `f"hola {sin cerrar` → "interpolación de f-cadena sin cerrar antes del fin de archivo".
  - `f"hola {x\ny}"` (newline dentro de interp en f-string simple) → mensaje específico.
  - `f"hola }"` → "'}' inesperado en f-cadena (usa '}}' para llave literal)".
  - `"""sin cerrar` → "cadena triple sin cerrar antes del fin de archivo".
- `tests/unit/test_lexer_f_cadenas.c` añadido con 36 tests cubriendo: f-strings sin/con interpolación, mayúsculas (`F`), comillas simples, llaves literales, triple-quoted con conteo de líneas correcto, combinación f+triple, escapes, errores específicos, distinción `f"..."` vs `f` + `"..."` (ident + cadena), lexemas y secuencias realistas inspiradas en `examples/03_fibonacci.cor` y `06_diccionarios.cor`.
- `tests/unit/test_lexer_literales.c` renombrado a `tests/unit/test_lexer_numeros_cadenas.c` por consistencia (el nombre describe mejor el contenido).
- 5/5 tests verde con build Release optimizado: smoke + simbolos + numeros_cadenas + identificadores + f_cadenas.

### Añadido (Fase 2 sesión 3)
- Vendoreado [utf8proc 2.10.0](https://github.com/JuliaStrings/utf8proc) en `vendor/utf8proc/` (~700 KB) para soporte Unicode y NFC. Compilado como librería estática que se enlaza al binario y los tests.
- Lexer reconoce **identificadores ASCII** (`TT_IDENT`): letras, dígitos (no al inicio), `_`, `$`. Camino rápido sin decodificación UTF-8.
- Lexer reconoce **identificadores Unicode**: cualquier letra Unicode (categorías Lu, Ll, Lt, Lm, Lo, Nl) puede iniciar un identificador; continuación admite además dígitos (Nd), marks (Mn, Mc) y connector punctuation (Pc).
- Ejemplos válidos: `niño`, `año_actual`, `función_principal`, `días_vividos`, `contar_niños`.
- **Tabla de keywords castellanas** (~33 entradas) implementada como switch sobre el primer carácter:
  - Control de flujo: `si`, `sino`, `mientras`, `para`, `en`, `romper`, `continuar`, `retornar`, `pasar`, `fin`.
  - Funciones, clases, módulos: `funcion`, `lambda`, `clase`, `extiende`, `super`, `importar`, `desde`, `como`, `global`, `nolocal`.
  - Excepciones: `intentar`, `atrapar`, `finalmente`, `lanzar`.
  - Lógicas: `y`, `o`, `no`, `es`.
  - Literales: `verdadero`, `falso`, `nulo`.
  - Reservadas para futuro: `producir`, `asincrono`, `esperar`, `con`, `borrar`, `coincidir`.
- Las keywords son **case-sensitive y solo en minúscula** (decisión B4): `Si`, `FUNCION` son identificadores. `función` (con tilde) es identificador. `silencio` no es `si`.
- Multi-token keywords (`fin si`, `sino si`, `es no`) se emiten como tokens separados por decisión B1; la combinación se hace en el parser.
- Bytes UTF-8 inválidos producen `TT_ERROR` con mensaje "byte UTF-8 inválido".
- `src/fuente.{h,c}` añadidos: utility de carga (`fuente_cargar_archivo`, `fuente_normalizar`) que lee un archivo del disco, salta BOM UTF-8 si lo hay, valida UTF-8 y normaliza a NFC con `utf8proc_NFC`. Usa estructura `FuenteCargada` con código de error explícito y mensaje. Aún no conectado a `main.c` (sesión 4 o 5).
- `tests/unit/test_lexer_identificadores.c` añadido con 35+ tests cubriendo identificadores ASCII, Unicode (con `ñ` y tildes), las 33 keywords, casos delicados (palabra que empieza con keyword, case-sensitivity, keyword con tilde), errores UTF-8, y secuencias realistas (`funcion saludar(nombre):`, clase con método, etc.).
- `tests/unit/test_lexer_simbolos.c`: actualizado `test_secuencia_realista` que ahora reconoce `a` y `b` como `TT_IDENT`.
- Build verde con CMake; ctest 4/4 tests pasan (test_smoke, test_lexer_simbolos, test_lexer_literales, test_lexer_identificadores).

### Añadido (Fase 2 sesión 2)
- Lexer reconoce literales numéricos `TT_ENTERO`:
  - Decimales con guiones bajos opcionales (`42`, `1_000_000`, `1_00_00`).
  - Hexadecimal (`0xff`, `0xCAFE`, `0xCa_fE`, `0x_ff`).
  - Octal (`0o755`).
  - Binario (`0b1010`, `0b1010_1010`).
- Lexer reconoce literales decimales `TT_DECIMAL`:
  - Punto decimal (`3.14`, `0.5`).
  - Notación científica (`1e10`, `1.5E-3`, `2.5e+10`, `3E5`).
- Reglas de guiones bajos en numéricos: prohibidos al inicio del literal, al final, y consecutivos. `0x_ff` permitido (tras prefijo de base) por ergonomía visual.
- `1.` (sin dígito tras el punto) tokeniza como `TT_ENTERO 1` + `TT_PUNTO .`. Evita ambigüedad con acceso a atributo `obj.metodo`.
- Lexer reconoce literales de cadena `TT_CADENA` con comilla doble `"..."` o simple `'...'`. El lexema incluye las comillas (parser hará el unescape al construir el AST).
- Escape sequences aceptadas: `\n \t \r \\ \' \" \0 \x \u`. Validación profunda de los argumentos de `\xHH` y `\uHHHH` se aplaza a sesión 5.
- Errores específicos:
  - `1__2` → "no se permiten guiones bajos consecutivos".
  - `12_` → "literal numérico no puede terminar en '_'".
  - `0x` / `0o` / `0b` sin dígitos → mensaje específico por base.
  - `1e` / `1e+` → "exponente vacío en literal decimal".
  - `\z` → "secuencia de escape no reconocida".
  - Cadena con `\n` interno → "cadena sin cerrar antes del fin de línea".
  - Cadena que llega a EOF → "cadena sin cerrar antes del fin de archivo".
- `tests/unit/test_lexer_literales.c` añadido con 38 tests cubriendo enteros decimales, las tres bases especiales, decimales con punto y científica, cadenas con ambos delimitadores, escape sequences, errores y secuencias mixtas realistas.
- `tests/unit/test_lexer_simbolos.c` actualizado: `test_secuencia_realista` reconoce ahora `10` como `TT_ENTERO`.
- Build verde con 3/3 tests pasando (test_smoke, test_lexer_simbolos, test_lexer_literales).

### Añadido (Fase 2 sesión 1)
- `src/lexer.{h,c}` — esqueleto del lexer con enum `TipoToken` (~70 tipos), struct `Token`, struct `Lexer` y funciones `lexer_iniciar()` / `lexer_siguiente()` / `tipo_token_nombre()`.
- En esta sesión se reconocen: símbolos individuales (`(`, `)`, `[`, `]`, `{`, `}`, `,`, `.`, `:`, `;`, `@`, `~`), operadores aritméticos y sus formas compuestas (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`), comparaciones (`==`, `!=`, `<`, `<=`, `>`, `>=`), bitwise (`&`, `|`, `^`, `<<`, `>>`), y la flecha `->`.
- Whitespace y comentarios `# ...` se ignoran. Saltos de línea avanzan correctamente el contador de línea y reinician el cómputo de columna.
- Caracteres no reconocidos producen `TT_ERROR` con mensaje. `!` aislado sugiere `!=`.
- `src/errores.{h,c}` — infraestructura mínima de errores (struct `Error`, `error_iniciar()`, `error_destruir()`, `error_set_mensaje()`, `error_set_sugerencia()`, `error_imprimir()`). Formato siguiendo MENSAJES.md §2 sin caret indicators todavía (sesión 5).
- `tests/unit/test_lexer_simbolos.c` — 18 tests cubriendo: fuente vacía, whitespace, saltos de línea, todos los símbolos individuales, operadores compuestos, comentarios en distintas posiciones, tracking de línea/columna, errores léxicos, EOF idempotente, lexema apunta a fuente original.
- Build verde con CMake; `ctest` 2/2 tests pasan.

### Decisiones de diseño
- **[B1](decisiones/B1-modelo-de-bloques.md):** Modelo de delimitación de bloques resuelto. Cornamusa usa apertura con `:` y cierre explícito con `fin <etiqueta>` (`fin si`, `fin funcion`, `fin clase`, etc.), inspirado en la tradición castellana de PSeInt y Latino. La indentación es estilística, no semántica. Se descartó la indentación significativa por coste de implementación y peor calidad de errores.
- **[B4](decisiones/B4-tildes-y-unicode.md):** Reglas de tildes y Unicode resueltas. Las palabras clave del lenguaje son **ASCII puro sin tildes** (`funcion`, no `función`); los identificadores definidos por el usuario admiten cualquier letra Unicode (`niño`, `año_actual` válidos). El lexer normaliza a NFC obligatoriamente. Identificadores case-sensitive.
- **[B7](decisiones/B7-formato-numerico.md):** Formato numérico resuelto. El separador decimal en código es siempre `.` (universal); el separador de miles es `_` opcional. La convención castellana de coma decimal se gestiona en la biblioteca estándar (`formato.formatear` y `formato.leer_numero` con parámetro `locale`), no en la sintaxis.
- **[B5+B6](decisiones/B5-B6-yo-y-dunders.md):** Convención del primer parámetro y nomenclatura de dunders resueltos en un único ADR. El primer parámetro de métodos de instancia es **`yo` por convención** (no keyword: el nombre es libre, la stdlib y ejemplos oficiales usan `yo`). Los **dunders se nombran en castellano** según lista canónica de ~32 nombres (`__iniciar__`, `__cadena__`, `__longitud__`, `__sumar__`, etc.). Excepción razonada: `__repr__` mantiene su forma inglesa por brevedad y uso técnico universal.
- **[B2](decisiones/B2-tree-walking-vs-bytecode.md):** Arquitectura del pipeline de ejecución resuelta. **AST compartido** entre dos backends: tree-walking (Fase 4-5) y bytecode (Fase 6+). El tree-walking es minimalista (sin closures/clases/excepciones), sirve como primer release jugable y queda **congelado en v0.5** como referencia ejecutable de regresión. La VM bytecode es el motor de producción y destino de todas las optimizaciones. Esta arquitectura habilita tiered execution futura (Fase 12 JIT) sin reestructuración. Se descartó la opción A (ambos motores activos) tras analizar que es redundancia, no potencia — la potencia real a largo plazo viene de tiered execution sobre bytecode.
- **[B3](decisiones/B3-representacion-numerica.md):** Representación numérica de enteros resuelta. **Polimórfico fasado**: bignum boxed con [libtommath](https://www.libtom.net/LibTomMath/) (Public Domain, vendoreada) desde v0.4 con semántica matemáticamente correcta sin overflow; transición a tagged i63 + bignum en Fase 6 (fast path 1-3 ciclos, promoción transparente); especialización en Fase 10 con inline caching. **Sin breaking changes entre versiones** — `factorial(100)` funciona idéntico en v0.4 y v1.0, solo cambia velocidad. Descartadas: i64 puro (rompe pedagogía), bignum siempre (~50x más lento incluso en hot loops), tagged desde día 1 (complejidad innecesaria en tree-walking).
- **[I2](MENSAJES.md):** Estándar de calidad de mensajes de error definido. Documento normativo `MENSAJES.md` con anatomía formal de un error (categoría + ubicación + caret + mensaje + sugerencia), reglas de tono (tutear, no culpar, sugerir cuando aplica), 12 plantillas canónicas para los errores más comunes (variable no definida con "did you mean", tipo incompatible, bloque mal cerrado, división por cero, índice fuera de rango, etc.), anti-patterns explícitos, plan de implementación por fases (lexer en v0.2 con plantillas 5.5-5.6, parser en v0.3, runtime en v0.4) y estructura técnica (`Error` en C + tabla de mensajes preparada para futuro i18n).
- **[I5]** UTF-8 en consola Windows configurado en `src/main.c`. Función `configurar_consola_utf8()` llama `SetConsoleOutputCP(CP_UTF8)` y `SetConsoleCP(CP_UTF8)` al inicio del programa cuando se compila para Windows. Sin Windows-specific en otras plataformas (Linux/macOS ya son UTF-8 por defecto). Arregla mojibake al imprimir `ñ`, `á`, `¡` en cmd.exe / PowerShell.

### Cambios derivados de B1
- `ESPEC.md`: actualizada la sección 1 (filosofía), 2.7 (renombrada de "Indentación" a "Bloques"), tabla de keywords (añadido `fin`), gramática PEG sección 5, y programa de ejemplo sección 7.
- `examples/`: los 12 ejemplos `.cor` reescritos con `fin <etiqueta>`.
- `examples/11_iterador.cor`: campo `fin` renombrado a `limite` (colisión con keyword reservada).

### Cambios derivados de B4
- `ESPEC.md`: sección 1 (filosofía) reformulada — eliminada regla "tildes opcionales", añadidas reglas de keywords ASCII e identificadores Unicode con NFC.
- `ESPEC.md`: sección 2.2 (identificadores) — añadida normalización NFC y aclaración de case-sensitivity.
- `ESPEC.md`: sección 2.3 (keywords) — eliminada la columna "Forma sin tilde" de todas las tablas; `función` → `funcion` como única forma; `asíncrono` → `asincrono` en reservadas para futuro.

### Cambios derivados de B7
- `ESPEC.md`: sección 2.5 (literales numéricos) — añadida nota explicando el uso universal de `.` decimal y `_` separador de miles, con referencia al módulo `formato` para E/S localizada.
- Plan: módulo `formato` añadido a la stdlib mínima de Fase 9 con funciones `formatear()` y `leer_numero()` con parámetro `locale`.

### Cambios derivados de B5+B6
- `ESPEC.md` §2.2: añadida convención del primer parámetro `yo` con referencia a §6.6.
- `ESPEC.md` §2.3: `yo` eliminado de la tabla de keywords (es convención, no keyword reservada).
- `ESPEC.md` §4 (Métodos especiales): tabla reescrita con la lista canónica de dunders castellanos, organizada por categorías (construcción, comparaciones, colecciones, aritméticos, llamada, atributos dinámicos).
- `ESPEC.md` §6.6 (Modelo de objetos): expandida con sección sobre métodos de instancia y la convención `yo`, y mapeo de operadores → dunders.
- `examples/`: verificados — los dunders ya usados (`__iniciar__`, `__iterar__`, `__siguiente__`, `__cadena__`, `__nombre__`) coinciden con la lista canónica. Sin cambios necesarios.

### Cambios derivados de B2
- `ESPEC.md` §9: "Cuestiones abiertas" reescrita como índice de ADRs y pendientes menores.
- `README.md`: hoja de ruta con tabla de features explícitas por release y nota arquitectónica sobre AST compartido.

### Cambios derivados de B3
- ESPEC.md §3 (tipos primitivos): `entero` actualizado con descripción de precisión arbitraria desde v0.4 + transición tagged en Fase 6.
- ESPEC.md §2.5 (literales numéricos): añadida nota explícita sobre precisión arbitraria con ejemplo `gugol = 10 ** 100`.
- Plan: módulo `formato` añadido a la stdlib mínima de Fase 9 con funciones `formatear()` y `leer_numero()` con parámetro `locale`.

### Cambios derivados de I2
- Nuevo documento normativo `MENSAJES.md` (~600 líneas) en raíz del repo.
- Estándar aplicable a errores producidos por lexer (Fase 2), parser (Fase 3), tree-walking (Fase 4) y bytecode VM (Fase 6+).
- Plan de implementación detallado por fase con plantillas concretas listas para usar.

### Cambios derivados de I5
- `src/main.c`: añadido `#include <windows.h>` con guard `#ifdef _WIN32`.
- `src/main.c`: nueva función `configurar_consola_utf8()` llamada al inicio de `main()`.
- Verificado: `cornamusa.exe --version` y `--ayuda` ahora producen UTF-8 correcto en Windows. Sin impacto en Linux/macOS.

## [0.1.0] — 2026-04-27

### Añadido
- Estructura del repositorio: `src/`, `tests/`, `examples/`, `stdlib/`, `docs/`, `benchmarks/`.
- Build system con CMake (multiplataforma) y Makefile de conveniencia.
- Configuración de CI con GitHub Actions para Linux, Windows y macOS.
- `ESPEC.md`: especificación formal del lenguaje (gramática PEG, keywords, semántica).
- 12 programas de ejemplo en `examples/` que validan el diseño sintáctico.
- `README.md` en castellano con hoja de ruta y badges.
- Licencia MIT.
- `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `.editorconfig`, `.gitignore`.
- REPL trivial (eco) en `src/main.c` como esqueleto inicial.

<!-- TODO al publicar el repo: añadir enlaces de comparación de versiones -->
<!-- [No publicado]: https://github.com/USUARIO/cornamusa/compare/v0.1.0...HEAD -->
<!-- [0.1.0]: https://github.com/USUARIO/cornamusa/releases/tag/v0.1.0 -->

