# B7 — Formato numérico: separador decimal y de miles

**Estado:** ✅ Decidido.
**Fecha de propuesta:** 2026-04-28 (pregunta de David sobre coma decimal castellana)
**Fecha de decisión:** 2026-04-28
**Decisor:** David Castilla

## Contexto

En castellano, la convención ortográfica para números es:
- Decimal: coma (`3,14`).
- Miles: punto (`1.000.000`) o espacio fino (`1 000 000`).

En todo lenguaje de programación mainstream, la convención sintáctica es:
- Decimal: punto (`3.14`).
- Miles: ninguno o `_` como separador opcional (Python 3.6+).

Pregunta planteada: ¿podemos usar la convención castellana en el código fuente de Cornamusa?

## Decisión

### En el código fuente: punto decimal universal

```cornamusa
pi = 3.14159
precio = 99.99
porcentaje = 0.21
millon = 1_000_000           # separador opcional con `_`
```

**El código fuente usa `.` como decimal y `_` como separador de miles**, igual que Python, Java, Rust, Swift, Go, JavaScript moderno, etc.

### En la presentación al usuario: locale en biblioteca estándar

```cornamusa
desde formato importar formatear, leer_numero

# Salida con formato castellano
imprimir(formatear(3.14, locale="es"))         # → "3,14"
imprimir(formatear(1000000, locale="es"))      # → "1.000.000"
imprimir(formatear(3.14, locale="en"))         # → "3.14"

# Entrada respetando convención castellana
n = leer_numero("3,14", locale="es")           # → 3.14
n = leer_numero("1.000.000", locale="es")      # → 1000000
```

El formateo localizado vive en el **módulo `formato` de la stdlib** (planificado para Fase 9), no en la sintaxis del lenguaje.

## Justificación

### Por qué NO coma decimal en código fuente

La coma como decimal **destruye la sintaxis** porque colisiona con la coma como separador de elementos:

```cornamusa
# Si la coma fuese decimal, esto sería ambiguo:
lista = [1,5, 2,7, 3,9]
# ¿3 decimales [1.5, 2.7, 3.9] o 6 enteros [1, 5, 2, 7, 3, 9]?

llamar(99,99, 0,21, 10)
# ¿3 args (99.99, 0.21, 10) o 5 args?
```

Sin paréntesis envolviendo cada número, **no hay forma de desambiguar**. Las opciones para resolverlo (paréntesis obligatorios, `;` como separador, lexing contextual) son todas peores que aceptar `.`.

**Hecho histórico:** ningún lenguaje mainstream del mundo (Pascal, Fortran, Cobol, C, Lua, Python, Ada, Algol, etc.) usa coma decimal. No es por desprecio al castellano, es por imposibilidad técnica de coexistir con la coma como separador.

### Por qué locale en stdlib

La coma decimal castellana es un tema de **presentación de datos**, no de sintaxis del lenguaje. La distinción es la misma que:

- En castellano se dice "veintiuno" pero el código JSON guarda `21`.
- En España se escribe `27/04/2026` pero ISO 8601 (estándar) es `2026-04-27`.

El **código fuente es notación formal**; la **interfaz de usuario es localización**. Cornamusa puede ofrecer una interfaz de usuario perfectamente castellana sin renunciar a sintaxis universal.

### Separador de miles: `_`

- Universal (Python 3.6+, Rust, Java 7+, Swift, etc.).
- No colisiona con decimales (que usan `.`).
- Legible sin pisar otros símbolos del lenguaje.
- Posición libre: `1_000_000` o `1_00_00_00` (estilo indio) — el lexer lo ignora.

```cornamusa
poblacion_españa = 47_500_000
distancia_luna_metros = 384_400_000
```

## Decisiones secundarias

1. **`'` (apóstrofo) como separador estilo europeo**: descartado para v1.0. Crea ambigüedad con literales de carácter en lenguajes que los tienen, y `_` ya cubre el caso. Reevaluable en v1.1.

2. **Sufijo de tipo en literales** (`3.14f`, `100u`): descartado. Tipos numéricos son inferidos por contexto, no necesitamos sufijos.

3. **Notación científica**: estándar `1e10`, `2.5e-3`, igual que todo el mundo.

4. **Bases**: `0b` binario, `0o` octal, `0x` hexadecimal — universal.

## Consecuencias

- ESPEC sección 2.5 (literales numéricos): añadir nota explicando que el código usa `.` y la stdlib ofrece `formatear`/`leer_numero` para locale.
- Plan: módulo `formato` añadido a la stdlib mínima de Fase 9.
- Tutorial pedagógico (futuro): un capítulo dedicado a "números en Cornamusa" que explique punto vs coma, con ejemplos de I/O localizado. **Esto es importante:** sin esa explicación, el usuario hispanohablante puede sentirse engañado al ver `3.14` en código que pretende ser "en castellano".

## Alternativas descartadas

| Opción | Razón |
|---|---|
| Coma decimal universal | Ambigüedad fatal con separador de listas/argumentos |
| Coma decimal con `;` como separador | Rompe convención universal de coma como separador |
| Coma decimal contextual (lexer adivina) | Frágil, produce bugs sutiles, código difícil de copiar |
| Aceptar ambos `.` y `,` | "Una sola forma evidente" violada, parser complejo |
| Punto y `,` ambos en literales con paréntesis obligatorios | Feo: `lista = [(1,5), (2,7)]` |
