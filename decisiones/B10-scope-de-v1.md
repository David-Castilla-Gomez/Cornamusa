# B10 — Scope de v1.0: documentación y madurez sobre rendimiento

**Estado:** ✅ Decidido.
**Fecha de propuesta:** 2026-04-30
**Fecha de decisión:** 2026-04-30
**Decisor:** David Castilla

**Decisión:** El alcance de v1.0 se reorienta de **"GC generacional + docs + sitio web"** (plan original B2) a **"documentación completa + ejemplos avanzados + sitio web + cierre de scope"**. El **GC generacional queda explícitamente postergado a post-v1.0** (potencial v1.1 o v1.2 si los benchmarks de programas reales lo justifican).

## Contexto

El plan original (referenciado en [B2-tree-walking-vs-bytecode.md](B2-tree-walking-vs-bytecode.md) y en mensajes de plan a través del proyecto) listaba para v1.0:

> v1.0 — GC generacional + docs completos + sitio web

Tras completar v0.7 (clases) → v0.11.4 (small-int tagging + 4 patches), el rendimiento del intérprete está en buen lugar:

- ~3x geomedia sobre v0.10.0 tras B9.
- `fibonacci_recursivo` 5.9x más rápido (1.33s → 222ms).
- `globales_lookup` con loop de 1M iteraciones en ~220ms.

El GC mark-sweep tri-color actual (introducido en F7/v0.8.0, deferred-to-opcode-boundary) **no aparece como cuello en ningún benchmark**. Sus pausas son imperceptibles en programas pequeños, y los programas grandes que existen para Cornamusa son... ninguno todavía, porque **el lenguaje no está documentado para el usuario final**.

### El cuello real de v1.0 es la documentación

- `ESPEC.md` está marcado como **"Versión 0.1.0-borrador, Estado: En diseño activo (Fase 0)"** desde el inicio del proyecto. No refleja v0.7 (clases), v0.8 (GC), v0.9 (módulos), v0.10 (IC), ni v0.11 (small-int).
- **No hay tutorial paso-a-paso para nuevos usuarios**.
- **No hay referencia consolidada** del lenguaje (sintaxis + built-ins + stdlib en un sitio).
- **No hay sitio web**. Solo existe el README en GitHub.
- **No hay ejemplos avanzados** que demuestren el lenguaje resolviendo un problema real (los 23 ejemplos actuales son micro-demos de features).

Sin esos artefactos, **un v1.0 marca "estable" un lenguaje que nadie sabe cómo usar**. Es el peor outcome.

### Por qué postergar GC generacional

GC generacional asume que **objetos jóvenes mueren jóvenes** ("hipótesis generacional"). La técnica es valiosa cuando:
1. La aplicación crea muchos objetos efímeros (allocation rate alto).
2. Las pausas del GC tradicional son perceptibles (ms+).
3. El GC consume parte significativa del tiempo de ejecución (>5%).

En Cornamusa hoy:
1. Tras B9 (v0.11), los enteros pequeños no allocan en heap. La allocation rate cayó significativamente.
2. Los benchmarks no muestran pausas perceptibles.
3. El GC no aparece como cuello — los hot paths son aritmética y dispatch, no allocaciones.

Implementar GC generacional implicaría:
- Nursery + tenured spaces, write barriers en cada `Diccionario`/`Lista`/`Instancia` mut.
- Re-arquitectura del allocator y del mark phase.
- ~6-9 sesiones de trabajo muy técnico, alto riesgo de bugs sutiles.
- Beneficio observable solo en programas que aún no existen.

**Hacer GC generacional ahora es ingeniería especulativa**. Hacerlo cuando los benchmarks de programas reales (post-v1.0) lo justifiquen es ingeniería guiada por datos.

## Plan revisado de v1.0

### Sesiones (~6-8)

1. **B10 + plan revisado** (esta sesión, ya hecha).

2. **`ESPEC.md` actualizado a v0.11.4**: sintaxis al día (clases, herencia, `super`, módulos, excepciones con `atrapar Tipo`, lambdas, slicing, comprehensiones si las hay), tipos completos, semántica de truthy/falsy, igualdad, ordenamiento, MRO, manejo de errores. Borrar el placeholder "Fase 0".

3. **`docs/tutorial.md`**: paso-a-paso ("hola mundo" → variables → control de flujo → funciones → listas/dicc/conj → clases → módulos → manejo de errores). Cada sección con código ejecutable. Probado contra el intérprete real.

4. **`docs/referencia.md`**: referencia rápida del lenguaje (cheatsheet de sintaxis), tabla de built-ins, contenido de stdlib (`matematicas`, `cadenas`, `sistema`).

5. **2 ejemplos avanzados** en `examples/`: 
   - Un programa no trivial (parser CSV simple, conversor de unidades, juego del ahorcado).
   - Un programa pedagógico que use OOP de forma real (e.g. simulación de una biblioteca con préstamos).

6. **Sitio web**: `docs/index.md` + `docs/SUMMARY.md` y CI con [mdBook](https://rust-lang.github.io/mdBook/) o similar para generar HTML estático y desplegarlo a GitHub Pages. Header con badge de versión, link al GitHub, ejemplo destacado en la portada.

7. **Pulido final**: `CONTRIBUTING.md` actualizado, `FAQ.md`, `CODE_OF_CONDUCT.md` (estándar Contributor Covenant), revisión de README como puerta de entrada.

8. **Bench final + bump v1.0.0 + tag + nota de release destacada en GitHub**.

### Lo que v1.0 NO incluye

- **GC generacional** — postergado a post-v1.0 si datos lo justifican.
- **Threaded code dispatch** — descartado en revisión post-v0.11.4 (refactor de 200 cambios por 10-15% ganancia que no se nota).
- **JIT, tracing, AOT** — Fase 12+ del plan original; aplazadas indefinidamente.
- **Concurrencia / hilos** — decisión I3 ya aplazada al post-v1.0.
- **F-strings** — pendiente del lexer (v0.4 dejó f-strings parser stub); no bloquea v1.0 si lo documentamos como "v1.x".
- **Async/await** — fuera del alcance original.
- **stdlib amplia** — `matematicas`, `cadenas`, `sistema` son suficientes para v1.0; `archivos`, `red`, `regex`, `json` se añaden a partir de v1.1 según demanda.
- **Tipos opcionales / type hints** — ergonómico pero no esencial para un lenguaje pedagógico.

### Compromiso de estabilidad de v1.0

A partir de v1.0:

- **La sintaxis del lenguaje queda congelada**. Cambios incompatibles requieren `v2.0`.
- **El AST de `--ast` y el formato de chunks de bytecode pueden cambiar** entre versiones menores; son detalles de implementación.
- **Built-ins y stdlib pueden añadir miembros** entre minor versions; **no pueden cambiar comportamiento de los existentes** sin major version.
- **Errores de runtime** (categoría + mensaje) pueden mejorar redacción pero no cambiar significado entre minor versions.
- **GC, IC, small-int, etc.** son detalles internos. Pueden cambiar sin afectar programas .cor.

## Verificación de v1.0

Antes de tagear v1.0.0:

- [ ] `ESPEC.md` actualizado, sin "Fase 0" o "borrador".
- [ ] `docs/tutorial.md` completo y ejecutado contra `cornamusa --bytecode` sin errores.
- [ ] `docs/referencia.md` cubre sintaxis + built-ins + stdlib.
- [ ] Sitio web desplegado a GitHub Pages, accesible públicamente.
- [ ] 2+ ejemplos avanzados en `examples/` corriendo.
- [ ] CONTRIBUTING.md, FAQ.md, CODE_OF_CONDUCT.md presentes.
- [ ] README portada actualizada con link al sitio.
- [ ] 92+ tests verde en CI (Linux + Windows + macOS) con sanitizers.
- [ ] `cornamusa --version` reporta `1.0.0`.
- [ ] Bench v1.0.0 documentado en CHANGELOG; sin regresiones contra v0.11.4.

## Riesgo principal

**Que la documentación sea aburrida o incorrecta**. Mitigación:
- El tutorial se prueba ejecutando los snippets contra el intérprete real (el CI puede correr un script que extraiga bloques `cornamusa` y los pase al intérprete).
- La referencia se genera de forma semi-automática donde sea posible (lista de built-ins viene de `nativos.c`).
- ESPEC se revisa contra implementación: para cada feature del lenguaje, hay que poder señalar el archivo `src/*.c` que la implementa.

## Consecuencias

- v1.0.0 saldrá antes y será más útil para usuarios reales que un v1.0 con GC generacional sin docs.
- GC generacional, threaded dispatch, JIT y otras optimizaciones quedan como roadmap post-v1.0 guiada por datos.
- El proyecto se vuelve "presentable" a alguien externo (estudiante de programación, profesor de informática) por primera vez en su historia.
