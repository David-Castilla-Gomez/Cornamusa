# Registro de cambios

Todos los cambios notables a este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto adhiere a [Versionado Semántico](https://semver.org/lang/es/).

## [No publicado]

### En desarrollo
- Lexer UTF-8 con keywords castellanas (objetivo: v0.2.0).

### Decisiones de diseño
- **[B1](decisiones/B1-modelo-de-bloques.md):** Modelo de delimitación de bloques resuelto. Cornamusa usa apertura con `:` y cierre explícito con `fin <etiqueta>` (`fin si`, `fin funcion`, `fin clase`, etc.), inspirado en la tradición castellana de PSeInt y Latino. La indentación es estilística, no semántica. Se descartó la indentación significativa por coste de implementación y peor calidad de errores.
- **[B4](decisiones/B4-tildes-y-unicode.md):** Reglas de tildes y Unicode resueltas. Las palabras clave del lenguaje son **ASCII puro sin tildes** (`funcion`, no `función`); los identificadores definidos por el usuario admiten cualquier letra Unicode (`niño`, `año_actual` válidos). El lexer normaliza a NFC obligatoriamente. Identificadores case-sensitive.
- **[B7](decisiones/B7-formato-numerico.md):** Formato numérico resuelto. El separador decimal en código es siempre `.` (universal); el separador de miles es `_` opcional. La convención castellana de coma decimal se gestiona en la biblioteca estándar (`formato.formatear` y `formato.leer_numero` con parámetro `locale`), no en la sintaxis.
- **[B5+B6](decisiones/B5-B6-yo-y-dunders.md):** Convención del primer parámetro y nomenclatura de dunders resueltos en un único ADR. El primer parámetro de métodos de instancia es **`yo` por convención** (no keyword: el nombre es libre, la stdlib y ejemplos oficiales usan `yo`). Los **dunders se nombran en castellano** según lista canónica de ~32 nombres (`__iniciar__`, `__cadena__`, `__longitud__`, `__sumar__`, etc.). Excepción razonada: `__repr__` mantiene su forma inglesa por brevedad y uso técnico universal.
- **[B2](decisiones/B2-tree-walking-vs-bytecode.md):** Arquitectura del pipeline de ejecución resuelta. **AST compartido** entre dos backends: tree-walking (Fase 4-5) y bytecode (Fase 6+). El tree-walking es minimalista (sin closures/clases/excepciones), sirve como primer release jugable y queda **congelado en v0.5** como referencia ejecutable de regresión. La VM bytecode es el motor de producción y destino de todas las optimizaciones. Esta arquitectura habilita tiered execution futura (Fase 12 JIT) sin reestructuración. Se descartó la opción A (ambos motores activos) tras analizar que es redundancia, no potencia — la potencia real a largo plazo viene de tiered execution sobre bytecode.
- **[B3](decisiones/B3-representacion-numerica.md):** Representación numérica de enteros resuelta. **Polimórfico fasado**: bignum boxed con [libtommath](https://www.libtom.net/LibTomMath/) (Public Domain, vendoreada) desde v0.4 con semántica matemáticamente correcta sin overflow; transición a tagged i63 + bignum en Fase 6 (fast path 1-3 ciclos, promoción transparente); especialización en Fase 10 con inline caching. **Sin breaking changes entre versiones** — `factorial(100)` funciona idéntico en v0.4 y v1.0, solo cambia velocidad. Descartadas: i64 puro (rompe pedagogía), bignum siempre (~50x más lento incluso en hot loops), tagged desde día 1 (complejidad innecesaria en tree-walking).
- **[I2](MENSAJES.md):** Estándar de calidad de mensajes de error definido. Documento normativo `MENSAJES.md` con anatomía formal de un error (categoría + ubicación + caret + mensaje + sugerencia), reglas de tono (tutear, no culpar, sugerir cuando aplica), 12 plantillas canónicas para los errores más comunes (variable no definida con "did you mean", tipo incompatible, bloque mal cerrado, división por cero, índice fuera de rango, etc.), anti-patterns explícitos, plan de implementación por fases (lexer en v0.2 con plantillas 5.5-5.6, parser en v0.3, runtime en v0.4) y estructura técnica (`Error` en C + tabla de mensajes preparada para futuro i18n).

### Cambios derivados de B1
- `ESPEC.md`: actualizada la sección 1 (filosofía), 2.7 (renombrada de "Indentación" a "Bloques"), tabla de keywords (añadido `fin`), gramática PEG sección 5, y programa de ejemplo sección 7.
- `examples/`: los 12 ejemplos `.cor` reescritos con `fin <etiqueta>`.
- `examples/11_iterador.cor`: campo `fin` renombrado a `limite` (colisión con keyword reservada).

### Cambios derivados de B4
- `ESPEC.md`: sección 1 (filosofía) reformulada — eliminada regla "tildes opcionales", añadidas reglas de keywords ASCII e identificadores Unicode con NFC.
- `ESPEC.md`: sección 2.2 (identificadores) — añadida normalización NFC y aclaración de case-sensitivity.
- `ESPEC.md`: sección 2.3 (keywords) — eliminada la columna "Forma sin tilde" de todas las tablas; `función` → `funcion` como única forma; `asíncrono` → `asincrono` en reservadas para futuro.

### Cambios derivados de B7
- `ESPEC.md`: sección 2.5 (literales numéricos) — añadida nota explicando el uso universal de `.` decimal y `_` separador de miles, con referencia al módulo `formato` para E/S localizada.
- Plan: módulo `formato` añadido a la stdlib mínima de Fase 9 con funciones `formatear()` y `leer_numero()` con parámetro `locale`.

### Cambios derivados de B5+B6
- `ESPEC.md` §2.2: añadida convención del primer parámetro `yo` con referencia a §6.6.
- `ESPEC.md` §2.3: `yo` eliminado de la tabla de keywords (es convención, no keyword reservada).
- `ESPEC.md` §4 (Métodos especiales): tabla reescrita con la lista canónica de dunders castellanos, organizada por categorías (construcción, comparaciones, colecciones, aritméticos, llamada, atributos dinámicos).
- `ESPEC.md` §6.6 (Modelo de objetos): expandida con sección sobre métodos de instancia y la convención `yo`, y mapeo de operadores → dunders.
- `examples/`: verificados — los dunders ya usados (`__iniciar__`, `__iterar__`, `__siguiente__`, `__cadena__`, `__nombre__`) coinciden con la lista canónica. Sin cambios necesarios.

### Cambios derivados de B2
- `ESPEC.md` §9: "Cuestiones abiertas" reescrita como índice de ADRs y pendientes menores.
- `README.md`: hoja de ruta con tabla de features explícitas por release y nota arquitectónica sobre AST compartido.

### Cambios derivados de B3
- ESPEC.md §3 (tipos primitivos): `entero` actualizado con descripción de precisión arbitraria desde v0.4 + transición tagged en Fase 6.
- ESPEC.md §2.5 (literales numéricos): añadida nota explícita sobre precisión arbitraria con ejemplo `gugol = 10 ** 100`.
- Plan: módulo `formato` añadido a la stdlib mínima de Fase 9 con funciones `formatear()` y `leer_numero()` con parámetro `locale`.

### Cambios derivados de I2
- Nuevo documento normativo `MENSAJES.md` (~600 líneas) en raíz del repo.
- Estándar aplicable a errores producidos por lexer (Fase 2), parser (Fase 3), tree-walking (Fase 4) y bytecode VM (Fase 6+).
- Plan de implementación detallado por fase con plantillas concretas listas para usar.

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

