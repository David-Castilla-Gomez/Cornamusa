# Cornamusa — Resultados de benchmarks

Suite de microbenchmarks del motor bytecode. Los `.cor` de este
directorio se ejecutan con `benchmarks/run.sh` (o `run.ps1` en
Windows nativo). No es un benchmark riguroso — los tiempos son
sub-segundo y tienen varianza notable entre corridas; sirven como
**referencia de tendencia** entre versiones, no como medida absoluta.

Equipo de referencia: Windows 11, GCC 13.2 (Strawberry), build Release.
Cada medición es el mejor tiempo de 3 corridas.

## v1.40.0 — `-O3` + LTO

Cambios de build: `-O2` → `-O3` explícito y **LTO** activado
(`CMAKE_INTERPROCEDURAL_OPTIMIZATION`). LTO inlinea los helpers hot
del intérprete (`empujar`/`sacar`/`valor_clonar`/`dicc_obtener`,
definidos en `valor.c`) dentro del dispatch loop de `vm.c`.

| Benchmark              | v1.39 (`-O2`) | v1.40 (`-O3`+LTO) | Δ      |
|------------------------|---------------|-------------------|--------|
| `bignum_factorial`     | 0.027 s       | 0.028 s           | ~0%    |
| `dicc_intensivo`       | 0.069 s       | 0.053 s           | −23%   |
| `fibonacci_recursivo`  | 0.263 s       | 0.222 s           | −16%   |
| `globales_lookup`      | 0.221 s       | 0.212 s           | −4%    |
| `oo_dunder_aritmetico` | 0.112 s       | 0.091 s           | −19%   |
| `oo_dunder_indice`     | 0.084 s       | 0.054 s           | −36%   |
| `oo_intensivo`         | 0.038 s       | 0.032 s           | −16%   |

Observaciones:

- **`bignum_factorial`** no mejora: está dominado por libtommath
  (operaciones de bignum), no por el dispatch del intérprete.
- **OO y dicc** son los que más se benefician — hacen muchas llamadas
  a helpers pequeños cross-file (`dicc_obtener`, `valor_clonar`,
  ligado de métodos) que LTO ahora inlinea.
- **`fibonacci_recursivo`** (el más dispatch/llamada-pesado) baja −16%.

### Bug latente destapado por LTO

Activar LTO+O3 destapó un UB que llevaba latente desde v1.4: el campo
`n_nolocales` de `ScopeCompilador` no se inicializaba en
`scope_iniciar` (`compilador.c`). Con optimización menos agresiva
solía caer en 0 por suerte; con LTO+O3 arrancaba en basura de stack y
segfaultaba en closures con `nolocal`. Arreglado en v1.40 — ver
CHANGELOG.

## Computed-goto — evaluado y descartado (post-v1.40)

Tras v1.40 se implementó **dispatch computed-goto** (extensión GCC/Clang
"labels as values": cada opcode salta directamente al inicio del
siguiente con `goto *tabla[ip++]`, en lugar de volver a la cabeza de un
`switch` compartido). La implementación quedó completa y correcta —
`vm.c` con macros `TARGET`/`DESPACHAR`/`ABRIR_DESPACHO` que expanden a
labels en GCC y a `switch` clásico en MSVC, **185/185 tests verde en
ambos modos**.

Pero los benchmarks (mejor de 6-8 corridas, ambos builds con LTO+O3)
mostraron una **regresión consistente**, no una mejora:

| Benchmark              | switch+LTO | cgoto+LTO | Δ      |
|------------------------|------------|-----------|--------|
| `bignum_factorial`     | 0.0200 s   | 0.0223 s  | +11.6% |
| `dicc_intensivo`       | 0.0455 s   | 0.0508 s  | +11.6% |
| `fibonacci_recursivo`  | 0.2230 s   | 0.2427 s  | +8.8%  |
| `globales_lookup`      | 0.2123 s   | 0.2233 s  | +5.2%  |
| `oo_dunder_aritmetico` | 0.0881 s   | 0.0965 s  | +9.6%  |
| `oo_dunder_indice`     | 0.0498 s   | 0.0497 s  | −0.1%  |
| `oo_intensivo`         | 0.0298 s   | 0.0272 s  | −8.7%  |

Regresión en 5 de 7, neutro en 1, mejora solo en 1. Causa probable:

- **Bloat de código**: el chequeo de GC diferido (`CHEQUEAR_GC`), que en
  el bucle `switch` aparecía **una vez**, con computed-goto se inlinea
  en los ~130 sitios de despacho. Esto infla el código máquina de la
  función más caliente del intérprete y presiona la i-cache.
- **Predictor de saltos moderno**: en CPUs recientes (el equipo de
  referencia) el predictor de saltos indirectos ya maneja bien el único
  branch del `switch`; el supuesto beneficio del computed-goto
  (predicción por-opcode) no se materializa.
- **Tabla rematerializada por llamada**: `&&label` no son constantes de
  enlace, así que `tabla_dispatch[256]` no puede ser `static` — se
  rellena en cada invocación del dispatch.

**Decisión**: descartado. Igual que PGO, es un *pivot* honesto — no se
publica un release que ralentiza el intérprete. `vm.c` se mantiene con
el `switch` clásico de v1.40. El computed-goto podría reevaluarse si en
el futuro se mueve el chequeo de GC fuera del camino caliente (p.ej.
solo en saltos hacia atrás y llamadas) o en hardware/compiladores
distintos.

## Baseline histórica (v0.9.2)

Primer punto de referencia, motor bytecode recién estrenado:

| Benchmark              | tiempo  |
|------------------------|---------|
| `fibonacci_recursivo`  | ~1.4 s  |
| `dicc_intensivo`       | ~120 ms |
| `oo_intensivo`         | ~45 ms  |
| `bignum_factorial`     | ~25 ms  |
