# Repaso crítico de la planificación — v0.1.0

**Fecha:** 2026-04-27
**Autor del repaso:** revisión interna previa al primer release público.
**Documentos revisados:** `ESPEC.md`, plan de desarrollo, `CMakeLists.txt`, `src/main.c`, ejemplos `.cor`, infraestructura de CI.

> Tono deliberadamente crítico. El propósito es atrapar fallos de diseño ahora, no validar trabajo hecho.

---

## Resumen ejecutivo

He identificado **18 problemas potenciales** de los cuales:
- **6 son bloqueadores** que deberían resolverse antes de tocar el lexer (Fase 2).
- **7 son importantes** y conviene decidir antes de v0.4.
- **5 son menores** o aplazables.

La mayor parte se concentra en **decisiones de diseño del lenguaje** (no en implementación), porque el código actual es esqueleto. Esto es lo esperado: en sistemas, los errores de requisitos cuestan x100 más al final que al principio.

---

## 🔴 Bloqueadores (decidir antes de Fase 2)

### B1. Indentación significativa con parser one-pass es trampa

**Problema:** ESPEC dice "indentación significativa estilo Python". Pero clox (nuestro modelo de referencia) usa llaves precisamente porque es un compilador one-pass simple. Implementar INDENT/DEDENT correctamente requiere:
- Pila de niveles de indentación en el lexer.
- Manejo de continuación de líneas dentro de paréntesis (`(`, `[`, `{` desactivan indentación).
- Reglas para líneas en blanco, comentarios, strings multilínea.
- Tokens virtuales que no aparecen en el código fuente.

CPython tarda 30+ años puliendo esto. Si lo hacemos mal, el parser de Fase 3 hereda los bugs.

**Opciones:**
- **(a) Llaves estilo C** (`{ ... }`): triviales, copiamos clox 1-a-1. **Pierde sabor castellano/Python.**
- **(b) Bloques con `fin`** (estilo Lua/Ruby): `si x: ... fin` o `funcion f(): ... fin`. Natural en castellano, parseable trivialmente. **Verboso, fea anidación profunda.**
- **(c) Indentación significativa**: la opción del ESPEC. **Más trabajo, más bugs, mejor UX si sale bien.**
- **(d) Indentación + llaves opcionales**: aceptar las dos. **Confunde a usuarios.**

**Recomendación:** Opción **(b) bloques con `fin`** para v0.1–v0.4 (ergonomía castellana sin complicar el lexer), evaluar migración a indentación significativa en v0.5 si la experiencia lo justifica. O alternativamente **(c)** asumiendo el coste y usando como referencia el lexer de CPython 3.13 (`Parser/tokenizer.c`).

**Por qué bloqueante:** define la arquitectura del lexer y el parser. Cambiar después de v0.3 implica reescribir ambos.

---

### B2. Tree-walking interpreter (Fase 4) es trabajo doble

**Problema:** El plan tiene Fase 4 (tree-walking) y Fase 6 (VM bytecode). Mantener ambos significa:
- Dos motores de evaluación que evolucionan en paralelo.
- Doble esfuerzo en cada feature de v0.5–v0.10 (clases, excepciones, etc.).
- Tests diferenciales suenan elegantes pero son frágiles: cuando difieren, ¿cuál es la verdad?

clox de Crafting Interpreters salta directo de Pratt parser → bytecode sin AST intermedio (es de hecho su característica didáctica clave en Part III).

**Opciones:**
- **(a) Mantener tree-walking solo hasta v0.4**, descartarlo al llegar Fase 6. **Pero Fase 4 es justo el "primer release jugable"; descartarlo crea regresión en releases.**
- **(b) Saltar tree-walking, ir directo a bytecode.** Reordenar fases: lexer → parser que emite bytecode (estilo clox) → VM. Primer "jugable" llega en lo que ahora sería v0.6. **Pierde el hito intermedio.**
- **(c) Tree-walking minimalista solo en Fase 4** (sin closures ni clases), reemplazado por bytecode en Fase 6. Acepta limitaciones del primer "jugable". **Más realista.**

**Recomendación:** **(c)**. Tree-walking de Fase 4 cubre solo: aritmética, variables, condicionales, bucles, funciones top-level. Closures/clases/módulos esperan a Fase 6+. Esto reduce el alcance de Fase 4 a ~3 semanas en lugar de 3.

**Por qué bloqueante:** afecta la dependencia entre fases y el contenido de cada release.

---

### B3. Bignum vs i64 desde día 1

**Problema:** ESPEC actualmente promete "i64 hasta v1.0, bignum en v1.0". Esto significa que:
- Programas que funcionan en v0.4 con `factorial(20)` se rompen silenciosamente con `factorial(21)` (overflow de i64).
- En v1.0 cambiamos la semántica: el mismo programa ahora produce resultado correcto. **Esto es un cambio breaking.**
- Toda la stdlib y los ejemplos se diseñan asumiendo entero "natural".

Python tiene bignum desde 1991. Es una decisión arquitectónica.

**Opciones:**
- **(a) Bignum desde v0.4**, usando `libtommath` (BSD, ~80KB) o implementación casera básica.
- **(b) i64 hasta siempre**, documentado como diseño consciente (estilo Lua, Go, Rust).
- **(c) Tipo `entero` polimórfico**: i64 mientras quepa, promueve a bignum en overflow (CPython lo hizo así hasta 3.0).

**Recomendación:** **(c)**. Implementable en Fase 6 con tagged pointers (i63 + 1 bit de tag). Antes de Fase 6 documentamos overflow como warning conocido.

**Por qué bloqueante:** afecta la representación de `Value` (Fase 6) y la API de built-ins matemáticos (Fase 9).

---

### B4. Tildes opcionales en keywords requiere normalización Unicode

**Problema:** ESPEC dice "`función` ≡ `funcion`". Implicaciones que no he resuelto:

1. **¿Las tildes opcionales solo aplican a keywords o también a identificadores?** Si `función` y `funcion` son la misma keyword, ¿`función_de_prueba` y `funcion_de_prueba` son la misma variable? Probablemente **no** — pero la regla debe estar explícita.
2. **Normalización Unicode**: el carácter `ó` puede ser U+00F3 (precompuesto, NFC) o U+006F + U+0301 (descompuesto, NFD). Sin normalización al lexar, dos archivos visualmente idénticos producen tokens distintos.
3. **¿`Ñ` y `ñ` son keywords distintas?** ESPEC no lo dice. Suponemos case-sensitive como Python, pero entonces `SI` ≠ `si`. ¿Y `NIÑO` vs `niño`?
4. **Tabla de keywords se duplica**: cada keyword acentuada necesita 2 entradas en la tabla hash del lexer.

**Recomendación:**
- **Identificadores**: NFC obligatorio, case-sensitive, tildes y `ñ` significativas (es decir, `niño` ≠ `nino`).
- **Keywords**: aceptar ambas formas (con/sin tilde) hasta v0.4 para ergonomía. Reevaluar en v0.5 si alguien las pide o si la dualidad confunde.
- **Lexer**: normalizar a NFC al lexar para evitar bugs sutiles.

**Por qué bloqueante:** tabla de keywords y normalización son del lexer (Fase 2).

---

### B5. `yo` vs `self`: validación necesaria antes de v0.4

**Problema:** Hemos elegido `yo` como nombre del primer parámetro de métodos. Es la decisión más visible del lenguaje (aparece en cada método de cada clase). Riesgos:

- A muchos hispanohablantes les sonará **infantil** o forzado. `self` es ya un préstamo aceptado en programación.
- `yo` es una palabra vacía en castellano (pronombre, una sílaba). En Python `self` también es genérico. Pero culturalmente, "yo" se asocia con narrativa personal, no técnica.
- Alternativas: `self` (anglicismo familiar), `propio`, `mismo`, `este` (estilo Java/C++ con `this`), o **dejarlo libre como en Python** (es solo convención, no keyword).

**Recomendación:**
- **Hacerlo convención, no keyword**, igual que Python: el primer parámetro se llama como quieras, pero la convención del proyecto es `yo`. Así, programadores que prefieran `self` o `este` pueden usarlo sin error.
- **Validar con 3-5 hispanohablantes reales** antes de v0.4 (`yo` aparece en cada ejemplo del README; primera impresión).

**Por qué bloqueante:** aparece en absolutamente todo el código v0.8+. Cambiarlo después es churn masivo.

---

### B6. Métodos especiales (dunders) en castellano rompen puentes con Python

**Problema:** ESPEC propone `__iniciar__`, `__cadena__`, `__longitud__`, `__obtener__`, etc.

**Trade-off:**
- **Pro castellano puro:** coherencia con la filosofía del lenguaje.
- **Contra:** un programador que pasa de Cornamusa a Python (cosa probable: aprenden Cornamusa de niños, luego trabajan en Python) tiene que **reaprender 30+ nombres de dunders**. Lo mismo si lee código Python para inspirarse.

Los dunders son **convenciones internacionales** del lenguaje OO dinámico (`__init__`, `__str__`, `__len__`). Casi todos los lenguajes tipo Python los preservan.

**Opciones:**
- **(a) Dunders en castellano** (estado actual del ESPEC).
- **(b) Dunders en inglés**, igual que Python: `__init__`, `__str__`, `__len__`, etc. Pragmatismo > pureza.
- **(c) Aceptar ambos**: el runtime busca primero el nombre castellano, luego el inglés. **Ambigüedad y duplicación.**

**Recomendación:** **(b)**. Mantener dunders en inglés. Justificación: son **mecanismo del runtime**, no API de usuario diaria. La keyword `clase`, las funciones `imprimir`/`longitud`, los nombres de excepciones → en castellano. Los dunders → estándar internacional. Esto preserva el sabor castellano donde se nota (cabecera del programa) sin amputar la transferencia a Python.

**Por qué bloqueante:** define la API del runtime y aparece en clases user-defined desde Fase 8.

---

## 🟡 Importantes (decidir antes de v0.4)

### I1. Estimación de tiempo realista

**Problema:** El plan estima "~10 meses" para v1.0 con GC generacional, inline caching y biblioteca estándar. Crafting Interpreters Part III por sí sola requiere a la mayoría de los lectores 3-6 meses (para alguien con experiencia previa en C). Las fases 10-11 son cada una un proyecto de meses.

**Estimación realista** para una persona desarrollando en tiempo parcial:
- Fases 0-6 (hasta VM bytecode con clases): **6-9 meses**.
- Fases 7-9 (GC, excepciones, módulos, stdlib mínima): **+4-6 meses**.
- Fases 10-11 (inline caching, GC generacional): **+6-12 meses**.
- **Total realista: 18-30 meses para v1.0.**

**Recomendación:** ajustar el CHANGELOG y la hoja de ruta con plazos calendario relativos ("3-6 meses tras v0.6") en lugar de absolutos. Y mover el **anuncio público v1.0** a "el día que esté listo, no antes". El primer release serio (v0.4) sí debería tener fecha objetivo (~mes 4-5).

---

### I2. Mensajes de error en castellano son producto, no decoración

**Problema:** ESPEC menciona "mensajes en castellano" pero no define el **estándar de calidad** de esos mensajes. Para un lenguaje pedagógico, los errores son la principal interfaz con el principiante. Un error bueno puede salvar una hora de frustración.

CPython 3.10+ invirtió grandes esfuerzos en mensajes accionables (PEP 657). Lenguajes como Rust son referencia mundial.

**Recomendación:** crear `MENSAJES.md` en Fase 0 con:
- Plantillas de los 30 errores más comunes (sintaxis, NameError, TypeError, etc.).
- Ejemplo bueno + ejemplo malo de cada uno.
- Convención de tono: tutear, sugerir corrección concreta, mostrar la línea con caret.

Ejemplo de error bueno:
```
Error de nombre en programa.cor:5:8
    imprimir(saludaar(nombre))
             ^^^^^^^^
"saludaar" no está definido. ¿Quisiste decir "saludar"?
```

---

### I3. Concurrencia / GIL: filosofía sin definir

**Problema:** Python tiene GIL desde siempre y CPython 3.13 introduce free-threading opt-in. Lua es single-threaded. Decisión que afecta **GC**, **refcounting si lo usamos**, **representación de objetos compartidos**.

**Opciones:**
- **(a) Single-threaded** (Lua, primer Python): simple, suficiente para didáctica, no escalable.
- **(b) GIL desde el principio**: permite I/O concurrente con threads, no paralelismo CPU.
- **(c) Free-threaded por diseño**: ambicioso, complica todo el runtime.

**Recomendación:** **(a) single-threaded** hasta v1.0. Async/await (cooperativo, single-threaded) en v1.1. Threads + GIL en v2.0. Free-threading nunca o como rama experimental.

---

### I4. Sanitizers, fuzzing, análisis estático

**Problema:** Estamos escribiendo C. La probabilidad de bugs de memoria es alta, y los lexers/parsers son **el target #1 de fuzzing** en lenguajes de programación.

**Estado actual:** ninguno configurado.

**Recomendación:**
- **Fase 1 (ahora):** añadir job de CI con `-fsanitize=address,undefined` en builds Debug.
- **Fase 2 (lexer):** integración mínima de libFuzzer para fuzzear el lexer.
- **Fase 3 (parser):** extender fuzzing al parser.
- **Fase 7 (GC):** Valgrind o ASan + LeakSanitizer en CI.

---

### I5. UTF-8 en Windows console afecta el REPL desde día 1

**Problema:** Windows cmd.exe y PowerShell por defecto usan code pages legacy (CP-850, CP-1252). Imprimir UTF-8 sin configuración produce mojibake. Probado: el REPL actual escribe `ó` en `--ayuda`, y en Windows clásico se ve `Â³` o similar.

**Solución técnica:** llamar `SetConsoleOutputCP(CP_UTF8)` y `SetConsoleCP(CP_UTF8)` al inicio en Windows.

**Recomendación:** añadir a `main.c` ahora. Es trivial (5 líneas con `#ifdef _WIN32`).

---

### I6. Performance baseline / metas claras

**Problema:** Fase 10 ("inline caching y especialización") no tiene criterio de éxito objetivo. ¿Cuánto más rápido que qué?

**Recomendación:** definir antes de Fase 6:
- **Suite de microbenchmarks** (fibonacci recursivo, dict-heavy, OO-heavy, parsing JSON).
- **Baseline:** "Cornamusa v0.6 puro vs CPython 3.12 vs Lua 5.4 en los mismos benchmarks".
- **Meta v1.0:** "≥0.5x CPython, ≥0.3x Lua" (no necesitamos ser más rápidos, pero sí estar en el mismo orden de magnitud).

Sin esto, las optimizaciones de Fase 10 se convierten en piscina sin fondo.

---

### I7. Versionado del bytecode

**Problema:** A partir de Fase 6 generamos bytecode (`.coc`?). Cuando Fase 8 añada opcodes para clases, los `.coc` viejos se invalidan silenciosamente.

**Recomendación:** definir desde Fase 6:
- **Magic number** + versión de bytecode en el header (4 bytes).
- VM rechaza bytecode incompatible con error claro.
- Convención: subir el número con cada fase que cambia opcodes.

---

## 🟢 Menores / aplazables

### M1. Estructura `docs/` mezcla referencias académicas con docs futuras de usuario

**Sugerencia:** mover los 20 PDFs/MDs académicos a `docs/referencia/` o `recursos/`. Reservar `docs/` raíz para documentación de usuario (tutorial, referencia del lenguaje, referencia de stdlib).

### M2. Falta `AUTHORS.md` o `MAINTAINERS.md`

Nominal, pero ayuda a la cultura open source y al SEO del repo.

### M3. Logo / branding visual

El README es texto puro. Un logo SVG sencillo (incluso un emoji de gaita) refuerza identidad.

### M4. Setup script para nuevos contributors

Un `bootstrap.sh` que: detecta SO, instala dependencias (`cmake`, compilador), corre `make build && make test`. Reduce fricción para colaboradores.

### M5. Shebang `#!/usr/bin/env cornamusa`

ESPEC dice `#` es comentario. Pero el lexer en Unix encontrará programas que empiezan con `#!`. Decisión trivial: si línea 1 empieza con `#!`, ignorarla completa. Documentar.

---

## Recomendaciones de acción inmediata

**Antes de hacer público el repo (próximas horas):**

1. ✅ **Decidir B1** (indentación). Mi recomendación: bloques con `fin` para v0.4, reevaluar luego.
2. ✅ **Decidir B6** (dunders en inglés vs castellano). Mi recomendación: inglés.
3. ✅ **Aplicar I5** (UTF-8 console en Windows). Trivial, fixea el REPL ya.
4. ✅ **Actualizar ESPEC.md** con las decisiones de B1, B3, B4, B5, B6.
5. ✅ **Crear `MENSAJES.md`** estándar mínimo de calidad de errores (I2).
6. ✅ **Reescribir los 12 ejemplos `.cor`** con la sintaxis decidida (probablemente `fin` en lugar de indentación).
7. ✅ **Ajustar plan**: alcance realista de Fase 4 (B2), tiempos calendario (I1).

**Antes de Fase 2 (próximas semanas):**

8. Añadir job de CI con sanitizers (I4).
9. Definir suite de benchmarks y baseline (I6).
10. Decidir filosofía de concurrencia (I3) — ya documentar la decisión.

**Aplazable a Fase 6:**

11. Versionado de bytecode (I7).

---

## Conclusión

El esqueleto v0.1.0 es sólido. La especificación tiene **6 decisiones bloqueantes pendientes** que conviene cerrar antes del primer release público — son cuestiones de **diseño de producto**, no de implementación, y todas son del tipo "elegir entre 2-3 alternativas razonadas".

La hoja de ruta es **demasiado optimista en plazos** y **mezcla niveles de granularidad** (un proyecto de 6 meses como Fase 11 al lado de uno de 1 semana como Fase 7). Esto no es un problema técnico pero sí de gestión: si el primer "release jugable" se retrasa 3 meses sobre lo planeado, corre el riesgo de matar la motivación.

**Mi recomendación final:** trata este documento como input para un **reset de v0.1.1** que cierre las decisiones bloqueantes, y solo después abre el repo público. El coste es 1-2 días de trabajo; el beneficio es no tener que hacer "v0.2 con keywords renombradas porque la v0.1 estaba mal pensada" frente a contributors externos.
