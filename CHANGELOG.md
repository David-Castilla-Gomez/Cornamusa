# Registro de cambios

Todos los cambios notables a este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto adhiere a [Versionado Semántico](https://semver.org/lang/es/).

## [No publicado]

### En desarrollo
- Lexer UTF-8 con keywords castellanas (objetivo: v0.2.0).

### Decisiones de diseño
- **[B1](decisiones/B1-modelo-de-bloques.md):** Modelo de delimitación de bloques resuelto. Cornamusa usa apertura con `:` y cierre explícito con `fin <etiqueta>` (`fin si`, `fin funcion`, `fin clase`, etc.), inspirado en la tradición castellana de PSeInt y Latino. La indentación es estilística, no semántica. Se descartó la indentación significativa por coste de implementación y peor calidad de errores.
- **[B4](decisiones/B4-tildes-y-unicode.md):** Reglas de tildes y Unicode resueltas. Las palabras clave del lenguaje son **ASCII puro sin tildes** (`funcion`, no `función`); los identificadores definidos por el usuario admiten cualquier letra Unicode (`niño`, `año_actual` válidos). El lexer normaliza a NFC obligatoriamente. Identificadores case-sensitive.

### Cambios derivados de B1
- `ESPEC.md`: actualizada la sección 1 (filosofía), 2.7 (renombrada de "Indentación" a "Bloques"), tabla de keywords (añadido `fin`), gramática PEG sección 5, y programa de ejemplo sección 7.
- `examples/`: los 12 ejemplos `.cor` reescritos con `fin <etiqueta>`.
- `examples/11_iterador.cor`: campo `fin` renombrado a `limite` (colisión con keyword reservada).

### Cambios derivados de B4
- `ESPEC.md`: sección 1 (filosofía) reformulada — eliminada regla "tildes opcionales", añadidas reglas de keywords ASCII e identificadores Unicode con NFC.
- `ESPEC.md`: sección 2.2 (identificadores) — añadida normalización NFC y aclaración de case-sensitivity.
- `ESPEC.md`: sección 2.3 (keywords) — eliminada la columna "Forma sin tilde" de todas las tablas; `función` → `funcion` como única forma; `asíncrono` → `asincrono` en reservadas para futuro.

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

