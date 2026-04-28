# B2 — Destino del intérprete tree-walking en Fase 4

**Estado:** ✅ **Decidido**.
**Fecha de propuesta:** 2026-04-28
**Fecha de decisión:** 2026-04-28
**Decisor:** David Castilla
**Bloqueador identificado en:** [REPASO_CRITICO.md](../REPASO_CRITICO.md), problema B2.

**Decisión:** Opción **D** — AST compartido entre tree-walking y bytecode. Tree-walking minimalista en Fase 4 (sin closures/clases/excepciones), congelado en v0.5 como referencia ejecutable de regresión. Bytecode VM es el motor de producción a partir de Fase 6. Esta arquitectura habilita tiered execution futura (Fase 12 JIT) sin churn.

## Contexto

El plan actual tiene dos motores de ejecución:

- **Fase 4 (v0.4.0)** — intérprete *tree-walking*: evalúa el AST recursivamente. Es la primera versión ejecutable y el primer release "jugable" en GitHub.
- **Fase 6 (v0.6.0)** — VM bytecode: compila el AST a bytecode y lo ejecuta en una máquina virtual stack-based.

El problema: **¿qué pasa con el tree-walking cuando llegue Fase 6 y posteriores?** Tres efectos no resueltos:

1. **Doble esfuerzo** — cada feature nueva (clases en Fase 8, excepciones en Fase 9, etc.) debe implementarse dos veces si mantenemos ambos motores activos.
2. **Tree-walking como trabajo perdido** — si lo descartamos en v0.6, las 4-5 semanas de Fase 4-5 son inversión cuya única salida es "primer release jugable".
3. **Decisión de arquitectura del parser** — clox emite bytecode directamente desde el Pratt parser sin AST intermedio. Si copiamos ese modelo, no hay AST que el tree-walking pueda visitar.

Esta decisión define **cómo se construye realmente el código de Cornamusa**, no su sintaxis.

## Modelo de referencia: clox

clox de *Crafting Interpreters* parte III no usa tree-walking. Su pipeline es:

```
fuente → Lexer → Compiler (Pratt parser que emite bytecode directamente) → VM
```

No hay AST. El compilador one-pass mantiene estado (locals, scope depth) en el `Compiler` struct y emite opcodes a medida que parsea.

**Ventajas:** simplicidad extrema (~600 líneas de C para todo el pipeline), velocidad de compilación.

**Desventajas:**
- Sin AST no hay análisis estático (linting, type checking, refactorizaciones).
- Optimizaciones complejas (constant folding cross-expression, dead code elimination) son difíciles.
- Imposible servir un tree-walking encima — no hay árbol que recorrer.

Para Cornamusa con visión a v1.0 (Fase 10 inline caching, Fase 12 JIT), tener AST en algún momento es valioso.

## Opciones

### Opción A — Mantener ambos motores activos hasta v1.0

Tree-walking y bytecode coexisten. Cada feature se implementa en los dos. Diferenciales tests verifican equivalencia.

**Coste:** alto — 1.5-2x el esfuerzo en Fases 6-11.
**Ventaja:** dos implementaciones independientes detectan bugs por divergencia.
**Recomendación:** descartar. El coste es enorme y los beneficios marginales (los tests por golden output son más fiables que por divergencia).

### Opción B — Saltar tree-walking, ir directo a bytecode (estilo clox)

No hay Fase 4 separada. El parser de Fase 3 emite bytecode directamente (sin AST). Primer release "jugable" se retrasa de v0.4 a v0.6.

**Coste:** medio (-3 semanas porque no hay tree-walking, +n por modelo más rígido en futuro).
**Ventaja:** simplicidad de clox, código compacto.
**Desventaja:** **5-6 meses sin release público jugable** (de mes 4 a mes ~10). Para un proyecto largo, esto mata motivación y feedback temprano.

### Opción C — Tree-walking minimalista en Fase 4, descartado en Fase 6

Tree-walking **limitado** (sin closures, sin clases, sin excepciones) sirve como primer release jugable. Cuando Fase 6 implementa el bytecode con todas las features, el tree-walking se elimina.

**Coste:** 2-3 semanas de Fase 4 + tiempo de eliminación en Fase 6.
**Trabajo perdido:** sí, la Fase 4 entera.
**Ventaja:** primer release jugable a tiempo (mes 4-5).

### Opción D — Tree-walking minimalista compartiendo AST con bytecode *(recomendada)*

Combinación de C con un cambio arquitectónico: **el parser produce AST en Fase 3** (no emite bytecode directo). Ese AST lo consumen dos backends:

- Fase 4: evaluador tree-walking que visita el AST.
- Fase 6: compilador que visita el AST y emite bytecode.

```
                          ┌→ Tree-walking (Fase 4-5, congelado en v0.5)
fuente → Lexer → Parser → AST
                          └→ Compilador → Bytecode → VM (Fase 6+, motor principal)
```

**Tree-walking se congela en v0.5** (con listas/dicts incluidos). De v0.6 en adelante todas las features nuevas (clases, excepciones, módulos) **solo en bytecode**. El tree-walking queda como **referencia ejecutable para tests de regresión** sobre el subconjunto de features de v0.5.

**Coste real estimado:**
- Fase 4 (tree-walking minimalista): 2-3 semanas.
- Fase 6 (bytecode compiler que reusa AST): mismo tiempo que con clox-style, ~5 semanas.
- Diferencia con clox-style: el AST añade ~300-500 líneas de código y un nivel de indirección. Negligible.

**Ventajas:**
- Primer release jugable a tiempo (mes 4-5).
- Tree-walking no es trabajo perdido: se mantiene como referencia para regresiones.
- AST disponible para Fase 10 (inline caching) y Fase 12 (JIT).
- Modelo más realista para producción (todos los lenguajes serios tienen AST).

**Desventajas:**
- ~10-15% más complejidad inicial vs clox-style.
- Tree-walking tiene "feature drift" desde v0.6: cualquier feature nueva no está soportada en tree-walking. Documentar el corte explícitamente.

## Comparativa resumida

| Criterio | A: ambos | B: solo bytecode | C: tree-walking sacrificable | D: AST compartido |
|---|---|---|---|---|
| Primer release jugable | mes 4 | mes 10 | mes 4 | mes 4 |
| Esfuerzo total v1.0 | +30% | -10% | +5% | +5% |
| Tree-walking sirve para algo en v1.0 | Sí (motor) | N/A | No | Sí (regresiones) |
| AST disponible para optimizaciones | Sí | No | No (en clox-style) | Sí |
| Complejidad arquitectónica | Alta | Mínima | Baja | Media |
| Encaja con visión Fase 10-12 | Sí | Limitado | Limitado | Sí |

## Recomendación

**Opción D — tree-walking minimalista compartiendo AST con bytecode.**

**Razones técnicas:**
1. **Primer release jugable a tiempo** sin sacrificar arquitectura limpia.
2. **AST como infraestructura compartida**: se construye una vez, se aprovecha en dos backends y en futuras herramientas (linter, formatter, LSP).
3. **Tree-walking de v0.5 se mantiene como referencia ejecutable** — útil para regresiones y para validar que el lexer/parser producen AST coherente, **sin coste de mantenimiento ulterior** (se congela).
4. **Encaja con la visión Fase 10-12**: inline caching y JIT especulativo aprovechan el AST. Si vamos por B (clox-style sin AST) tendríamos que reintroducir AST en Fase 10 con churn masivo.

**Razones de proyecto:**
- David ha pedido **calidad sobre velocidad**. Un AST limpio es inversión a largo plazo.
- El primer release jugable mes 4-5 mantiene motivación y feedback.
- Trabajo "perdido" en tree-walking es mínimo: solo el evaluador (visit pattern), ~500 líneas, no el AST ni el parser.

**Trade-offs aceptados:**
- Más complejidad inicial vs clox puro (manejable, ~10-15%).
- Tree-walking nunca tendrá clases/excepciones/módulos. Documentar como **limitación intencional** desde v0.4.
- Los **tests diferenciales solo son posibles para programas que no usan features posteriores a v0.5**. Aceptable.

## Detalles operativos (si se decide D)

### Reorganización del plan modular

**Fase 3 (v0.3.0) — Parser y AST.** Cambia: el parser produce **AST**, no bytecode. AST tipado con `enum` + `union` en C, `ast.{h,c}` con visitor pattern.

**Fase 4 (v0.4.0) — Intérprete tree-walking minimalista.** Limitado a:
- Tipos: entero, decimal, booleano, nulo, cadena.
- Aritmética, comparaciones, operadores lógicos.
- Variables locales y globales.
- Control de flujo: `si`/`sino`, `mientras`, `para`.
- Funciones top-level (sin closures).
- Built-ins mínimos: `imprimir`, `longitud`, `tipo`, `rango`.
- REPL funcional.
- **Sin closures, sin clases, sin excepciones, sin módulos.**

**Fase 5 (v0.5.0) — Estructuras de datos sobre tree-walking.**
- Listas, diccionarios, conjuntos (en el evaluador tree-walking).
- String interning (compartido por ambos motores cuando llegue Fase 6).

**Fase 6 (v0.6.0) — Compilador a bytecode + VM.** El compilador toma el mismo AST que produce el parser y emite bytecode. La VM stack-based ejecuta el bytecode. **Implementa todo lo de v0.5 + closures + (preparación para clases en Fase 8).**
- Tree-walking se **congela** en su estado v0.5: ningún cambio nuevo, solo bug fixes.
- Activable con flag `--tree-walking` para depuración o para correr el subconjunto v0.5 contra el evaluador antiguo.

**Fases 7-11:** features nuevas (GC, clases, excepciones, módulos, inline caching, GC generacional) **solo en el motor bytecode**.

### Estructura de archivos resultante

```
src/
├── ast.{h,c}          # AST tipado, compartido (Fase 3)
├── parser.{h,c}       # produce AST (Fase 3)
├── interprete.{h,c}   # tree-walking visitor sobre AST (Fase 4-5, congelado en v0.5)
├── compilador.{h,c}   # AST → bytecode (Fase 6)
├── chunk.{h,c}        # contenedor de bytecode
└── vm.{h,c}           # ejecutor stack-based (Fase 6+)
```

### Política de tests diferenciales

Los tests en `tests/integracion/` van etiquetados:
- `[tree-walking ok]` — programas v0.5 que se corren contra ambos motores.
- `[bytecode-only]` — programas con features de v0.6+ (clases, excepciones, etc.).

CI ejecuta:
- Tests `[tree-walking ok]`: tree-walking + bytecode, salidas deben coincidir byte a byte.
- Tests `[bytecode-only]`: bytecode únicamente, salida vs golden output.

### Eliminación futura del tree-walking

**No está planeada.** El tree-walking se queda como referencia ejecutable para siempre. Coste de mantenerlo en estado v0.5: cercano a cero (no se le añaden features). Si se rompe, se considera test regression, no feature regression.

## Decisión

**Opción D adoptada el 2026-04-28.**

Tras analizar si la opción A (ambos motores activos) es "más potente a largo plazo", concluimos que **no lo es** — es más redundante, y el verdadero patrón de "potencia a largo plazo" es **tiered execution** (intérprete + JIT en niveles), no dos motores independientes. La arquitectura D habilita tiered execution futura (Fase 12) sin reestructuración, lo que A no permitiría limpiamente.

**Resumen de la arquitectura adoptada:**

```
                          ┌→ Tree-walking (Fase 4-5, congelado en v0.5 como referencia)
fuente → Lexer → Parser → AST
                          └→ Compilador → Bytecode → VM (Fase 6+, motor de producción)
                                                         │
                                                         └─→ JIT futuro (Fase 12)
```

Tree-walking se mantiene **vivo pero estático** desde v0.6: sirve como reference implementation para tests de regresión sobre el subconjunto de features de v0.5.

## Consecuencias (si se decide D)

- Plan maestro: ajustar descripción de Fase 3 (produce AST), Fase 4 (tree-walking minimalista, lista de features explícita), Fase 5 (sobre tree-walking), Fase 6 (compila desde AST).
- ESPEC.md §9: actualizar "cuestiones abiertas" eliminando bignum como abierto y aclarando la pista del tree-walking minimalista.
- README.md: actualizar la tabla de hoja de ruta con las features explícitas de cada release.
- Riesgos: documentar "tree-walking feature drift" como riesgo conocido y aceptado.
