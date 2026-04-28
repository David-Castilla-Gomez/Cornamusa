# B4 — Tildes, `ñ` y normalización Unicode

**Estado:** ✅ Decidido.
**Fecha de propuesta:** 2026-04-27 ([REPASO_CRITICO.md](../REPASO_CRITICO.md))
**Fecha de decisión:** 2026-04-28
**Decisor:** David Castilla

## Contexto

Cornamusa es UTF-8 nativo y vive en castellano. Hay que fijar reglas precisas para:
1. Tildes y `ñ` en keywords del lenguaje.
2. Tildes y `ñ` en identificadores del usuario.
3. Normalización Unicode (NFC vs NFD).
4. Sensibilidad a mayúsculas/minúsculas.

El ESPEC borrador decía "tildes opcionales: `función` ≡ `funcion`". Esto duplicaba la tabla de keywords y abría discusiones de estilo.

## Decisión

### 1. Keywords: ASCII puro, sin tildes ni `ñ`

| Forma canónica (única) | Antes (descartada) |
|---|---|
| `funcion` | ~~`función`~~ |
| `mientras`, `para`, `si`, `sino` | (igual) |
| `clase`, `extiende` | (igual) |
| `intentar`, `atrapar`, `finalmente`, `lanzar` | (igual) |
| `verdadero`, `falso`, `nulo` | (igual) |
| `y`, `o`, `no`, `es`, `en` | (igual) |
| `fin`, `fin si`, `fin funcion`, ... | (igual) |
| `asincrono` (futuro) | ~~`asíncrono`~~ |

**Justificación:**
- Portabilidad de teclado en aulas, sistemas con configuraciones legacy o usuarios principiantes.
- Filosofía "una sola forma evidente": ofrecer `función` y `funcion` invita a discusiones de estilo en revisiones de código.
- Convención universal: ningún lenguaje del mundo permite tildes en keywords.
- Trade-off aceptado: pequeña pérdida de pureza ortográfica castellana en el código.

### 2. Identificadores: UTF-8 completo, libres

```cornamusa
funcion contar_niños(años):
    días_vividos = años * 365
    año_actual = 2026
    retornar días_vividos
fin funcion
```

**Permitido en identificadores:**
- Letras Unicode (categoría `L`): incluye `ñ Ñ á é í ó ú ü Á É Í Ó Ú Ü` y otras de cualquier idioma.
- Dígitos (excepto al inicio).
- `_` y `$`.

**No permitido:**
- Caracteres de la categoría `M` (combining marks aislados, salvo dentro de NFC).
- Espacios, signos de puntuación, símbolos.

### 3. Normalización Unicode: NFC obligatorio

El lexer normaliza todo el código fuente a **NFC** (Normalization Form Canonical Composed) antes de tokenizar.

**Por qué:** el carácter `ó` puede codificarse de dos maneras:
- `U+00F3` (precompuesto, NFC) — un solo code point.
- `U+006F U+0301` (descompuesto, NFD) — dos code points: `o` + acento combinante.

Visualmente idénticos, byte-distintos. Sin normalización, `función` escrito en macOS (NFD por defecto) sería un identificador distinto de `función` escrito en Windows (NFC). Bug invisible.

**Coste:** una pasada de normalización al cargar el archivo. Trivial con tabla precomputada o llamada a librería.

### 4. Sensibilidad a mayúsculas: case-sensitive

- `nombre` ≠ `Nombre` ≠ `NOMBRE` (identificadores distintos).
- `niño` ≠ `Niño` ≠ `NIÑO`.
- Convenciones recomendadas:
  - Variables y funciones: `serpiente_minuscula`.
  - Clases: `MayusculaCamello`.
  - Constantes: `MAYUSCULAS_CON_GUION`.

Las keywords están todas en minúscula. Escribir `SI` o `Funcion` es identificador, no keyword.

## Consecuencias

- ESPEC sección 1 (filosofía): regla de "tildes opcionales" eliminada.
- ESPEC sección 2.2 (identificadores): UTF-8 + NFC explícito.
- ESPEC sección 2.3 (keywords): tabla simplificada, eliminada columna "forma sin tilde".
- Lexer (Fase 2): añadir paso de normalización NFC y validación de identificadores Unicode.
- Coste estimado del lexer: +50-100 líneas (tabla NFC reducida) o +1 dependencia ligera (`utf8proc` ~200KB).

## Alternativas descartadas

| Opción | Razón de descarte |
|---|---|
| Aceptar ambas formas (`función` y `funcion`) | Duplica tabla de keywords, fomenta inconsistencia, sin beneficio real |
| Solo con tildes (`función`) | Barrera mecánica de teclado, fricciona con principiantes |
| Sin normalización Unicode | Bugs invisibles cross-OS, casi imposibles de depurar |
