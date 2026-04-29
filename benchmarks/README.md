# Cornamusa — benchmarks

Mini-suite para medir el rendimiento del motor bytecode antes de F10
(inline caching tipo PEP 659). Los números aquí son **baseline para
v0.9.2** — no comparamos contra otros lenguajes ni automatizamos en CI;
solo queremos un punto de referencia para evaluar mejoras futuras.

## Programas

- **fibonacci_recursivo.cor** — `fib(30)` recursivo. Estresa el coste de
  llamadas a función y bifurcación.
- **dicc_intensivo.cor** — 50 000 inserciones + lecturas en un dicc.
  Mide hashing y crecimiento de tabla.
- **oo_intensivo.cor** — 5 000 instancias, llamadas a método y acceso a
  atributos. Mide vtable lookup en ausencia de inline caching.
- **bignum_factorial.cor** — `factorial(1000)`. Aritmética bignum
  (libtommath).

## Cómo correrlos

```bash
make build
./benchmarks/run.sh
```

O en PowerShell:

```powershell
.\benchmarks\run.ps1
```

El script imprime el tiempo de cada uno. No suma ni promedia — la
variabilidad entre corridas es alta para benchmarks tan cortos.

## Baseline v0.9.2

Medido en Windows 11 / WSL bash, máquina de desarrollo de David, build
release Ninja, motor bytecode (`--bytecode`):

| Benchmark             | Tiempo |
|-----------------------|--------|
| bignum_factorial      | ~25 ms |
| dicc_intensivo        | ~120 ms |
| fibonacci_recursivo   | ~1.4 s |
| oo_intensivo          | ~45 ms |

> Estos números varían bastante entre máquinas — sirven para detectar
> regresiones grandes (>2x) o validar que F10 mejora `fibonacci_recursivo`
> y `oo_intensivo` (los más sensibles a llamadas e indirecciones).

## Cuándo añadir más

- Cuando aparezca un caso de uso real que sea lento — añadir benchmark
  que lo reproduzca antes de optimizar.
- Cuando lleguemos a F10, añadir un benchmark de "código tipo-monomórfico"
  (mismo tipo siempre en un sitio caliente) y "polimórfico" para medir
  inline caching directamente.
