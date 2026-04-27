# Registro de cambios

Todos los cambios notables a este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto adhiere a [Versionado Semántico](https://semver.org/lang/es/).

## [No publicado]

### En desarrollo
- Lexer UTF-8 con keywords castellanas (objetivo: v0.2.0).

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

