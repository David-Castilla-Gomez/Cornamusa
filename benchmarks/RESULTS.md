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

## Baseline histórica (v0.9.2)

Primer punto de referencia, motor bytecode recién estrenado:

| Benchmark              | tiempo  |
|------------------------|---------|
| `fibonacci_recursivo`  | ~1.4 s  |
| `dicc_intensivo`       | ~120 ms |
| `oo_intensivo`         | ~45 ms  |
| `bignum_factorial`     | ~25 ms  |
