# Cornamusa

> Un lenguaje de programación dinámico, interpretado y **en castellano**.

[![Licencia: MIT](https://img.shields.io/badge/licencia-MIT-blue.svg)](LICENSE)
[![Versión](https://img.shields.io/badge/versión-0.1.0-orange.svg)](CHANGELOG.md)
[![Estado](https://img.shields.io/badge/estado-en%20desarrollo-yellow.svg)](CHANGELOG.md)

<!-- TODO al publicar el repo: añadir badge de CI con la URL real de GitHub Actions -->


Cornamusa es un lenguaje de programación tipo Python con **palabras clave, built-ins y mensajes de error íntegramente en castellano**. Está diseñado para que aprender a programar no requiera dominar el inglés primero.

```cornamusa
funcion saludar(nombre):
    si nombre == "":
        retornar "¡Hola, desconocido!"
    retornar f"¡Hola, {nombre}!"

para persona en ["Ana", "Luis", "María"]:
    imprimir(saludar(persona))
```

## Características (objetivo v1.0)

- **Sintaxis castellana natural**: `si`/`sino`, `mientras`, `para X en Y`, `función`, `clase`, `intentar`/`atrapar`, `verdadero`/`falso`/`nulo`, etc.
- **Tipado dinámico** con tipos enriquecidos: enteros, decimales, cadenas Unicode, listas, diccionarios, conjuntos, tuplas.
- **Indentación significativa**, estilo Python.
- **Soporte UTF-8 completo** en código fuente e identificadores: `función contar_niños(años)` es un identificador válido.
- **Clases y herencia simple** con `yo` como referencia a la instancia (equivalente a `self`).
- **Manejo de excepciones** estructurado.
- **Sistema de módulos** y biblioteca estándar mínima.
- **VM bytecode** stack-based con GC generacional e *inline caching* especializado al estilo PEP 659 (a partir de v0.6).

## Estado del proyecto

> **v0.1.0 — Andamiaje.** Esta es la primera fase del desarrollo; el lenguaje aún no ejecuta programas. Se está construyendo el lexer (v0.2). Consulta [CHANGELOG.md](CHANGELOG.md) para ver el progreso.

Hoja de ruta resumida (12 fases hasta v1.0):

| Versión | Hito |
|---|---|
| v0.1 | ✅ Andamiaje + CI |
| v0.2 | Lexer UTF-8 |
| v0.3 | Parser + AST |
| v0.4 | Intérprete tree-walking — **primer release jugable** |
| v0.5 | Listas y diccionarios |
| v0.6 | Compilador + VM bytecode |
| v0.7 | GC mark-sweep |
| v0.8 | Clases y herencia |
| v0.9 | Excepciones, módulos, stdlib |
| v0.10 | *Inline caching* especializado |
| v1.0 | GC generacional + sitio web |

## Compilación

Requisitos: compilador C11 (GCC, Clang o MSVC) y CMake ≥ 3.16.

```bash
make build          # equivale a: cmake -B build && cmake --build build
make repl           # abre el REPL
make test           # corre tests
```

O directamente con CMake:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/cornamusa programa.cor
```

## Ejemplos

Mira el directorio [`examples/`](examples/) para programas de muestra:

- [`01_hola_mundo.cor`](examples/01_hola_mundo.cor)
- [`02_fizzbuzz.cor`](examples/02_fizzbuzz.cor)
- [`03_fibonacci.cor`](examples/03_fibonacci.cor)
- [`07_clases_herencia.cor`](examples/07_clases_herencia.cor)
- [`10_quicksort.cor`](examples/10_quicksort.cor)
- ...

## Documentación

- **[ESPEC.md](ESPEC.md)** — Especificación formal del lenguaje (gramática PEG, keywords, semántica).
- **[CHANGELOG.md](CHANGELOG.md)** — Historial de cambios.
- **[CONTRIBUTING.md](CONTRIBUTING.md)** — Cómo contribuir.

## Inspiración

Cornamusa se construye sobre la literatura clásica y moderna de implementación de lenguajes dinámicos. Los recursos que guían el diseño están en [`recursos.md`](recursos.md): *Crafting Interpreters* (Nystrom), *CPython Internals* (Shaw), *The Implementation of Lua 5.0*, los papers de SELF/SmallTalk-80, *PyPy meta-tracing*, *Truffle/Graal*, entre otros.

## Licencia

[MIT](LICENSE) — libre para uso personal, educativo y comercial.

---

*Cornamusa* — del castellano antiguo, **gaita** o instrumento de viento de origen incierto. Que este lenguaje suene tan bien en tu código como una buena cornamusa en una fiesta popular.
