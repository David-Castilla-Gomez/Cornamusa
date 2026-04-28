# Registro de cambios

Todos los cambios notables a este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto adhiere a [Versionado Semántico](https://semver.org/lang/es/).

## [No publicado]

### En desarrollo (Fase 2 — Lexer, objetivo v0.2.0)
- ✅ Sesión 1: esqueleto del lexer + tokens simples (símbolos, operadores, comentarios).
- ✅ Sesión 2: literales numéricos y cadenas básicas.
- ✅ Sesión 3: identificadores Unicode + NFC + tabla de keywords.
- ⏳ Sesión 4: f-strings y triple-quoted strings.
- ⏳ Sesión 5: mensajes de error pulidos siguiendo MENSAJES.md + tests exhaustivos.

### Añadido (Fase 2 sesión 3)
- Vendoreado [utf8proc 2.10.0](https://github.com/JuliaStrings/utf8proc) en `vendor/utf8proc/` (~700 KB) para soporte Unicode y NFC. Compilado como librería estática que se enlaza al binario y los tests.
- Lexer reconoce **identificadores ASCII** (`TT_IDENT`): letras, dígitos (no al inicio), `_`, `$`. Camino rápido sin decodificación UTF-8.
- Lexer reconoce **identificadores Unicode**: cualquier letra Unicode (categorías Lu, Ll, Lt, Lm, Lo, Nl) puede iniciar un identificador; continuación admite además dígitos (Nd), marks (Mn, Mc) y connector punctuation (Pc).
- Ejemplos válidos: `niño`, `año_actual`, `función_principal`, `días_vividos`, `contar_niños`.
- **Tabla de keywords castellanas** (~33 entradas) implementada como switch sobre el primer carácter:
  - Control de flujo: `si`, `sino`, `mientras`, `para`, `en`, `romper`, `continuar`, `retornar`, `pasar`, `fin`.
  - Funciones, clases, módulos: `funcion`, `lambda`, `clase`, `extiende`, `super`, `importar`, `desde`, `como`, `global`, `nolocal`.
  - Excepciones: `intentar`, `atrapar`, `finalmente`, `lanzar`.
  - Lógicas: `y`, `o`, `no`, `es`.
  - Literales: `verdadero`, `falso`, `nulo`.
  - Reservadas para futuro: `producir`, `asincrono`, `esperar`, `con`, `borrar`, `coincidir`.
- Las keywords son **case-sensitive y solo en minúscula** (decisión B4): `Si`, `FUNCION` son identificadores. `función` (con tilde) es identificador. `silencio` no es `si`.
- Multi-token keywords (`fin si`, `sino si`, `es no`) se emiten como tokens separados por decisión B1; la combinación se hace en el parser.
- Bytes UTF-8 inválidos producen `TT_ERROR` con mensaje "byte UTF-8 inválido".
- `src/fuente.{h,c}` añadidos: utility de carga (`fuente_cargar_archivo`, `fuente_normalizar`) que lee un archivo del disco, salta BOM UTF-8 si lo hay, valida UTF-8 y normaliza a NFC con `utf8proc_NFC`. Usa estructura `FuenteCargada` con código de error explícito y mensaje. Aún no conectado a `main.c` (sesión 4 o 5).
- `tests/unit/test_lexer_identificadores.c` añadido con 35+ tests cubriendo identificadores ASCII, Unicode (con `ñ` y tildes), las 33 keywords, casos delicados (palabra que empieza con keyword, case-sensitivity, keyword con tilde), errores UTF-8, y secuencias realistas (`funcion saludar(nombre):`, clase con método, etc.).
- `tests/unit/test_lexer_simbolos.c`: actualizado `test_secuencia_realista` que ahora reconoce `a` y `b` como `TT_IDENT`.
- Build verde con CMake; ctest 4/4 tests pasan (test_smoke, test_lexer_simbolos, test_lexer_literales, test_lexer_identificadores).

### Añadido (Fase 2 sesión 2)
- Lexer reconoce literales numéricos `TT_ENTERO`:
  - Decimales con guiones bajos opcionales (`42`, `1_000_000`, `1_00_00`).
  - Hexadecimal (`0xff`, `0xCAFE`, `0xCa_fE`, `0x_ff`).
  - Octal (`0o755`).
  - Binario (`0b1010`, `0b1010_1010`).
- Lexer reconoce literales decimales `TT_DECIMAL`:
  - Punto decimal (`3.14`, `0.5`).
  - Notación científica (`1e10`, `1.5E-3`, `2.5e+10`, `3E5`).
- Reglas de guiones bajos en numéricos: prohibidos al inicio del literal, al final, y consecutivos. `0x_ff` permitido (tras prefijo de base) por ergonomía visual.
- `1.` (sin dígito tras el punto) tokeniza como `TT_ENTERO 1` + `TT_PUNTO .`. Evita ambigüedad con acceso a atributo `obj.metodo`.
- Lexer reconoce literales de cadena `TT_CADENA` con comilla doble `"..."` o simple `'...'`. El lexema incluye las comillas (parser hará el unescape al construir el AST).
- Escape sequences aceptadas: `\n \t \r \\ \' \" \0 \x \u`. Validación profunda de los argumentos de `\xHH` y `\uHHHH` se aplaza a sesión 5.
- Errores específicos:
  - `1__2` → "no se permiten guiones bajos consecutivos".
  - `12_` → "literal numérico no puede terminar en '_'".
  - `0x` / `0o` / `0b` sin dígitos → mensaje específico por base.
  - `1e` / `1e+` → "exponente vacío en literal decimal".
  - `\z` → "secuencia de escape no reconocida".
  - Cadena con `\n` interno → "cadena sin cerrar antes del fin de línea".
  - Cadena que llega a EOF → "cadena sin cerrar antes del fin de archivo".
- `tests/unit/test_lexer_literales.c` añadido con 38 tests cubriendo enteros decimales, las tres bases especiales, decimales con punto y científica, cadenas con ambos delimitadores, escape sequences, errores y secuencias mixtas realistas.
- `tests/unit/test_lexer_simbolos.c` actualizado: `test_secuencia_realista` reconoce ahora `10` como `TT_ENTERO`.
- Build verde con 3/3 tests pasando (test_smoke, test_lexer_simbolos, test_lexer_literales).

### Añadido (Fase 2 sesión 1)
- `src/lexer.{h,c}` — esqueleto del lexer con enum `TipoToken` (~70 tipos), struct `Token`, struct `Lexer` y funciones `lexer_iniciar()` / `lexer_siguiente()` / `tipo_token_nombre()`.
- En esta sesión se reconocen: símbolos individuales (`(`, `)`, `[`, `]`, `{`, `}`, `,`, `.`, `:`, `;`, `@`, `~`), operadores aritméticos y sus formas compuestas (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`), comparaciones (`==`, `!=`, `<`, `<=`, `>`, `>=`), bitwise (`&`, `|`, `^`, `<<`, `>>`), y la flecha `->`.
- Whitespace y comentarios `# ...` se ignoran. Saltos de línea avanzan correctamente el contador de línea y reinician el cómputo de columna.
- Caracteres no reconocidos producen `TT_ERROR` con mensaje. `!` aislado sugiere `!=`.
- `src/errores.{h,c}` — infraestructura mínima de errores (struct `Error`, `error_iniciar()`, `error_destruir()`, `error_set_mensaje()`, `error_set_sugerencia()`, `error_imprimir()`). Formato siguiendo MENSAJES.md §2 sin caret indicators todavía (sesión 5).
- `tests/unit/test_lexer_simbolos.c` — 18 tests cubriendo: fuente vacía, whitespace, saltos de línea, todos los símbolos individuales, operadores compuestos, comentarios en distintas posiciones, tracking de línea/columna, errores léxicos, EOF idempotente, lexema apunta a fuente original.
- Build verde con CMake; `ctest` 2/2 tests pasan.

### Decisiones de diseño
- **[B1](decisiones/B1-modelo-de-bloques.md):** Modelo de delimitación de bloques resuelto. Cornamusa usa apertura con `:` y cierre explícito con `fin <etiqueta>` (`fin si`, `fin funcion`, `fin clase`, etc.), inspirado en la tradición castellana de PSeInt y Latino. La indentación es estilística, no semántica. Se descartó la indentación significativa por coste de implementación y peor calidad de errores.
- **[B4](decisiones/B4-tildes-y-unicode.md):** Reglas de tildes y Unicode resueltas. Las palabras clave del lenguaje son **ASCII puro sin tildes** (`funcion`, no `función`); los identificadores definidos por el usuario admiten cualquier letra Unicode (`niño`, `año_actual` válidos). El lexer normaliza a NFC obligatoriamente. Identificadores case-sensitive.
- **[B7](decisiones/B7-formato-numerico.md):** Formato numérico resuelto. El separador decimal en código es siempre `.` (universal); el separador de miles es `_` opcional. La convención castellana de coma decimal se gestiona en la biblioteca estándar (`formato.formatear` y `formato.leer_numero` con parámetro `locale`), no en la sintaxis.
- **[B5+B6](decisiones/B5-B6-yo-y-dunders.md):** Convención del primer parámetro y nomenclatura de dunders resueltos en un único ADR. El primer parámetro de métodos de instancia es **`yo` por convención** (no keyword: el nombre es libre, la stdlib y ejemplos oficiales usan `yo`). Los **dunders se nombran en castellano** según lista canónica de ~32 nombres (`__iniciar__`, `__cadena__`, `__longitud__`, `__sumar__`, etc.). Excepción razonada: `__repr__` mantiene su forma inglesa por brevedad y uso técnico universal.
- **[B2](decisiones/B2-tree-walking-vs-bytecode.md):** Arquitectura del pipeline de ejecución resuelta. **AST compartido** entre dos backends: tree-walking (Fase 4-5) y bytecode (Fase 6+). El tree-walking es minimalista (sin closures/clases/excepciones), sirve como primer release jugable y queda **congelado en v0.5** como referencia ejecutable de regresión. La VM bytecode es el motor de producción y destino de todas las optimizaciones. Esta arquitectura habilita tiered execution futura (Fase 12 JIT) sin reestructuración. Se descartó la opción A (ambos motores activos) tras analizar que es redundancia, no potencia — la potencia real a largo plazo viene de tiered execution sobre bytecode.
- **[B3](decisiones/B3-representacion-numerica.md):** Representación numérica de enteros resuelta. **Polimórfico fasado**: bignum boxed con [libtommath](https://www.libtom.net/LibTomMath/) (Public Domain, vendoreada) desde v0.4 con semántica matemáticamente correcta sin overflow; transición a tagged i63 + bignum en Fase 6 (fast path 1-3 ciclos, promoción transparente); especialización en Fase 10 con inline caching. **Sin breaking changes entre versiones** — `factorial(100)` funciona idéntico en v0.4 y v1.0, solo cambia velocidad. Descartadas: i64 puro (rompe pedagogía), bignum siempre (~50x más lento incluso en hot loops), tagged desde día 1 (complejidad innecesaria en tree-walking).
- **[I2](MENSAJES.md):** Estándar de calidad de mensajes de error definido. Documento normativo `MENSAJES.md` con anatomía formal de un error (categoría + ubicación + caret + mensaje + sugerencia), reglas de tono (tutear, no culpar, sugerir cuando aplica), 12 plantillas canónicas para los errores más comunes (variable no definida con "did you mean", tipo incompatible, bloque mal cerrado, división por cero, índice fuera de rango, etc.), anti-patterns explícitos, plan de implementación por fases (lexer en v0.2 con plantillas 5.5-5.6, parser en v0.3, runtime en v0.4) y estructura técnica (`Error` en C + tabla de mensajes preparada para futuro i18n).
- **[I5]** UTF-8 en consola Windows configurado en `src/main.c`. Función `configurar_consola_utf8()` llama `SetConsoleOutputCP(CP_UTF8)` y `SetConsoleCP(CP_UTF8)` al inicio del programa cuando se compila para Windows. Sin Windows-specific en otras plataformas (Linux/macOS ya son UTF-8 por defecto). Arregla mojibake al imprimir `ñ`, `á`, `¡` en cmd.exe / PowerShell.

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

### Cambios derivados de I5
- `src/main.c`: añadido `#include <windows.h>` con guard `#ifdef _WIN32`.
- `src/main.c`: nueva función `configurar_consola_utf8()` llamada al inicio de `main()`.
- Verificado: `cornamusa.exe --version` y `--ayuda` ahora producen UTF-8 correcto en Windows. Sin impacto en Linux/macOS.

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

