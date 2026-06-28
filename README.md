# Cornamusa

> Un lenguaje de programación dinámico, interpretado y **en castellano**.

[![Licencia: MIT](https://img.shields.io/badge/licencia-MIT-blue.svg)](LICENSE)
[![Versión](https://img.shields.io/badge/versión-1.209.0-blue.svg)](CHANGELOG.md)
[![Estado](https://img.shields.io/badge/estado-funcional-green.svg)](CHANGELOG.md)

Cornamusa es un lenguaje de programación tipo Python con **palabras clave, built-ins y mensajes de error íntegramente en castellano**. Está diseñado para que aprender a programar no requiera dominar el inglés primero.

```cornamusa
clase Persona:
    funcion __iniciar__(yo, nombre, edad):
        yo.nombre = nombre
        yo.edad = edad
    fin funcion

    funcion saludar(yo):
        retornar "Hola, soy " + yo.nombre
    fin funcion
fin clase

importar matematicas como mat
desde cadenas importar repetir

para persona en [Persona("Ana", 30), Persona("Luis", 25)]:
    imprimir(persona.saludar())
fin para

imprimir(repetir("=", 20))
imprimir("PI =", mat.PI)
imprimir("100! =", mat.factorial(100))
```

## Características

**Lenguaje:**

- ✅ **Sintaxis castellana natural**: `si`/`sino`, `mientras`, `para X en Y`, `funcion`, `clase`, `intentar`/`atrapar`/`finalmente`, `verdadero`/`falso`/`nulo`.
- ✅ **Tipado dinámico** con tipos completos: enteros bignum, decimales f64, cadenas UTF-8, listas, tuplas, diccionarios (con orden de inserción), conjuntos.
- ✅ **Bloques explícitos** con `:` al abrir y `fin <etiqueta>` al cerrar. Indentación recomendada pero no obligatoria.
- ✅ **UTF-8 completo** en código e identificadores con normalización NFC obligatoria.
- ✅ **Clases con herencia** y `super` multinivel, y **dunders** que se invocan automáticamente: `__sumar__`, `__cadena__`, `__indice__`, `__iterar__`, `__llamar__`, `__entrar__`/`__salir__`, comparación, operadores reflejados...
- ✅ **Funciones flexibles**: argumentos por defecto, `*args`, `**kwargs`, keyword arguments, spread `*`/`**` en llamadas, lambdas.
- ✅ **Closures con `nolocal`** para escribir en el scope envolvente.
- ✅ **Destructuring**: `a, b = par`, swap sin temporal, anidación.
- ✅ **Pattern matching**: `coincidir`/`cuando` con literales, estructuras, OR-patterns, star-patterns, type-match y guardas.
- ✅ **Generadores** con `producir`, `producir desde` y generator expressions; **comprehensions** de lista, dict y conjunto.
- ✅ **Context managers**: `con expr como x:` con `__entrar__`/`__salir__`.
- ✅ **Excepciones** con `atrapar Tipo como e`, `finalmente`, `lanzar`, **traceback multi-frame** y **mensajes con sugerencias** ("¿quisiste decir...?").
- ✅ **F-cadenas con interpolación real**: `f"hola {nombre}, {edad + 10}"` evalúa cada `{expr}`.
- ✅ **Módulos**: `importar X.Y como Z`, `desde X importar A, B como C`.

**Biblioteca estándar** — veintitrés módulos: `matematicas`, `cadenas`, `funcionales`, `formato`, `archivos`, `json`, `csv`, `fechas`, `tiempo`, `azar`, `proceso`, `regex`, `red`, `sistema`, `base64`, `hashing`, `jwt`, `coleccion`, `inspeccion`, `validacion`, `argumentos`, `ruta`, `pruebas`.

**Implementación:**

- ✅ **VM bytecode** con **inline caching** estilo PEP 659 (quickening por reescritura de opcode) y **small-int tagging** (enteros i63 inline en `Valor`, bignum transparente vía libtommath).
- ✅ **GC mark-sweep** automático con `recolectar()` para forzarlo manualmente.
- ✅ Build **`-O3` + LTO**; intérprete de referencia tree-walking congelado como oráculo de regresión.
- ✅ **Tests diferenciales** tree-walking vs bytecode + **benchmarks** en [`benchmarks/`](benchmarks/).
- ✅ Flag **`--check`** para validar sintaxis y compilación sin ejecutar (CI/editores).

## Cómo probar (5 minutos)

```bash
git clone https://github.com/David-Castilla-Gomez/Cornamusa.git
cd Cornamusa
cmake -B build && cmake --build build
./build/cornamusa --bytecode examples/22_modulos_avanzado.cor
```

Otros ejemplos jugables:

```bash
./build/cornamusa --bytecode examples/13_factorial_jugable.cor   # bignum
./build/cornamusa --bytecode examples/15_fizzbuzz_jugable.cor    # control de flujo
./build/cornamusa --bytecode examples/19_closures_jugable.cor    # closures
./build/cornamusa --bytecode examples/20_clases_jugable.cor      # OOP + herencia
./build/cornamusa --bytecode examples/21_modulos_jugable.cor     # módulos
./build/cornamusa --bytecode examples/23_sistema_jugable.cor a b  # sistema.argv
```

## Estado del proyecto

**v1.92.0 publicada.** Cornamusa es estable y maduro: paridad sintáctica cercana a Python 3.10+ y una stdlib de **veinte** módulos. **Nueva stdlib `validacion`**: helpers para validar formatos comunes (email, URL, fecha ISO, teléfono), rangos numéricos, longitud de cadenas, pertenencia a conjuntos. Incluye la clase `Validador` para acumular múltiples errores sobre un formulario y reportarlos juntos. Todos los validadores son silenciosos (devuelven booleano, no lanzan) y aceptan tipos no-cadena como falso por diseño. Útil para frameworks de formularios, APIs, procesamiento de entradas de usuario. 252 tests verde con 20 asserts nuevos. [Tutorial paso a paso](docs/tutorial.md), [Cookbook](docs/cookbook.md), [referencia rápida](docs/referencia.md), [FAQ](FAQ.md) y [sitio web](https://david-castilla-gomez.github.io/Cornamusa/) disponibles. Compromisos de estabilidad post-v1.0 documentados en [B10](decisiones/B10-scope-de-v1.md).

Hoja de ruta resumida:

| Versión | Hito | Estado |
|---|---|---|
| v0.1 | Andamiaje + CI | ✅ |
| v0.2 | Lexer UTF-8 con NFC | ✅ |
| v0.3 | Parser + AST compartido | ✅ |
| v0.4 | Tree-walking (primer release jugable) | ✅ |
| v0.5 | Estructuras de datos (tree-walking congelado aquí) | ✅ |
| v0.6 | Compilador + VM bytecode + closures + excepciones | ✅ |
| **v0.7** | **Clases y herencia** | ✅ |
| **v0.8** | **GC mark-sweep + super multinivel + excepciones polish** | ✅ |
| **v0.9** | **Módulos + stdlib** | ✅ |
| **v0.10** | **Inline caching especializado tipo PEP 659** | ✅ |
| **v0.11** | **Small-int tagging (i63 inline en `Valor`)** | ✅ |
| **v1.0** | **Documentación, sitio web, ejemplos avanzados, estabilidad** | ✅ |
| **v1.1** | **Conversores, `leer()`, f-cadenas con interpolación real** | ✅ |
| **v1.2** | **Dunders aritméticos, comparación, `__cadena__`, indexación** | ✅ |
| **v1.3** | **Dunders reflejados, `__llamar__`, `__longitud__`** | ✅ |
| **v1.4** | **`nolocal` + cobertura de tests OOP** | ✅ |
| **v1.5** | **Inline path para dunders triviales (perf 1.17x)** | ✅ |
| **v1.6** | **Inline path unario (`__cadena__`, `__longitud__`)** | ✅ |
| **v1.7** | **Inline path con constructor (cierre del experimento OOP-perf)** | ✅ |
| **v1.8** | **Stdlib `archivos` (I/O persistente)** | ✅ |
| **v1.9** | **Stdlib `json` (intercambio universal)** | ✅ |
| **v1.10** | **Errores atrapables en built-ins** | ✅ |
| **v1.11** | **Funcionales (`mapear`/`filtrar`/`reducir`/`enumerar`/`suma`/`minimo`/`maximo`) + reflexión (`absoluto`/`redondear`/`instancia_de`/`subclase_de`/`id`/`repr`)** | ✅ |
| **v1.12** | **Dunder `__iterar__` — clases iterables con `para x en obj`** | ✅ |
| **v1.13** | **Context managers — `con expr [como x]:` con dunders `__entrar__`/`__salir__`** | ✅ |
| **v1.14** | **Pulido: re-raise sin alias, slicing de cadenas (UTF-8), f-cadenas triples, fix handler-leak en `retornar` dentro de `intentar`** | ✅ |
| **v1.15** | **Pattern matching — `coincidir/cuando` con literales, bind y guardas** | ✅ |
| **v1.16** | **Patrones estructurales — `cuando (x, y):` y `cuando [a, b]:` con anidación arbitraria** | ✅ |
| **v1.16.1** | **JSON pretty-print + `quitar` extendido a dict/conjunto** | ✅ |
| **v1.16.2** | **OR-patterns (`cuando 1 \| 2 \| 3:`) + star-pattern (`cuando [a, *resto, b]:`)** | ✅ |
| **v1.16.3** | **Type-match (`cuando Foo():`) + `como nombre` para bind del sujeto** | ✅ |
| **v1.17** | **Argumentos por defecto en bytecode (`funcion f(a, b=1):`) + errores de aridad atrapables** | ✅ |
| **v1.18** | **Stdlib `formato` (padding, números, hex/bin, tablas) + `cadenas` extendida (separar, reemplazar, recortar, ASCII case, indice_de, contiene)** | ✅ |
| **v1.18.1** | **Fix bug re-import de módulos** | ✅ |
| **v1.19** | **Stdlib `fechas` (timestamps, descomponer/componer/formato, aritmética, calendario)** | ✅ |
| **v1.20** | **Diccionarios preservan orden de inserción (Python 3.7+ semantics)** | ✅ |
| **v1.21** | **Destructuring assignment (`a, b = par`, swap, anidación, lista LHS)** | ✅ |
| **v1.22** | **`*args` en definiciones y llamadas (recoger + expandir iterables)** | ✅ |
| **v1.23** | **Keyword arguments en llamadas (`f(x=1, y=2)`, orden libre, defaults selectivos)** | ✅ |
| **v1.24** | **`**kwargs` en definición (`f(a, *args, **kw)`, recoger sobrantes en dict)** | ✅ |
| **v1.25** | **Spread `**dict` en llamadas (cierra trilogía `*/**`, merge de configs)** | ✅ |
| **v1.26** | **Stdlib `azar` (PRNG xoshiro256**, elegir/barajar/muestra) + fix OP_LANZAR globals** | ✅ |
| **v1.27** | **Stdlib `proceso` (lanzar procesos externos, capturar stdout/stderr/exit, cross-platform)** | ✅ |
| **v1.28** | **Stdlib `regex` (motor backtracking propio, validación/búsqueda/reemplazo) + fix destructuring en función** | ✅ |
| **v1.29** | **Stdlib `red` (cliente HTTP/1.1 plano, cross-platform WinSock2/POSIX)** | ✅ |
| **v1.30** | **Comprehensions (list/dict/set, `[x*2 para x en xs si cond]`)** | ✅ |
| **v1.31** | **Generadores con `producir` (CallFrames suspendibles, fib infinito, pipelines lazy)** | ✅ |
| **v1.32** | **Fix comprehensions: dentro de bucles + top-level (bug de slots en `compilar_para`)** | ✅ |
| **v1.33** | **`producir desde` — delegación de generadores (yield from)** | ✅ |
| **v1.34** | **Generator expressions inline `(x*2 para x en xs)` — lazy, captura upvalues** | ✅ |
| **v1.35** | **Mensajes de error con sugerencias (`ErrorDeNombre` → "¿quisiste decir...?")** | ✅ |
| **v1.36** | **Sugerencias en atributos (`obj.atriuto`, `mod.simbol`, métodos)** | ✅ |
| **v1.37** | **Errores de runtime con línea de código fuente y caret `^`** | ✅ |
| **v1.38** | **Traceback multi-frame en errores fatales (cadena de llamadas)** | ✅ |
| **v1.39** | **Flag `--check` — valida sintaxis y compilación sin ejecutar (CI/editores)** | ✅ |
| **v1.40** | **Performance: `-O3` + LTO (−16/−36% en benchmarks) + fix de UB latente en `nolocal`** | ✅ |
| **v1.41** | **Dunders de coerción `__repr__` y `__booleano__` — instancias controlan su representación inspeccionable y su verdadez** | ✅ |
| **v1.42** | **`__hash__` + `__igual__` — instancias hashables por valor (dict/conjunto), sub-VM síncrono para despacho desde `valor.c`** | ✅ |
| **v1.43** | **`__siguiente__` — iteradores lazy stateful; `ErrorDeIteracion` como señal de fin** | ✅ |
| **v1.44** | **Expresión ternaria `A si C sino B` + slicing assignment `xs[i:j] = ...`** | ✅ |
| **v1.45** | **F-string format specifiers `{x:>10.2f}` — ancho, alineación, relleno, precisión, tipo** | ✅ |
| **v1.46** | **`con a, b:` multi-recurso (anidado) + combinar `*args` y `**kwargs` en la misma llamada → cierra la Fase 4** | ✅ |
| **v1.47** | **REPL con edición de línea, history navegable y persistencia entre sesiones — abre la Fase 5 (tooling)** | ✅ |
| **v1.48** | **Formateador integrado `cornamusa fmt` — reindenta, normaliza blancas, modo `--check` para CI** | ✅ |
| **v1.49** | **Linter integrado `cornamusa lint` — codigo inalcanzable, `pasar` redundante, `== nulo`, imports no usados** | ✅ |
| **v1.50** | **Scope analysis en el linter — `unused-local` + `unused-param` con respeto a closures, `nolocal`/`global`, destructuring y skip de `_`/`yo`** | ✅ |
| **v1.51** | **Generador de docs `cornamusa docs` — Markdown con firmas + comentarios precedentes, clases con metodos como subsecciones** | ✅ |
| **v1.52** | **LSP server `cornamusa lsp` (MVP) — JSON-RPC por stdio, `publishDiagnostics` desde el linter en `didOpen`/`didChange`** | ✅ |
| **v1.53** | **LSP polish — parse errors estructurados + `textDocument/hover` para funciones y clases top-level (firma + doc + metodos)** | ✅ |
| **v1.54** | **LSP completion basico — `textDocument/definition` (goto-def) + `textDocument/formatting` (delega al formateador integrado)** | ✅ |
| **v1.55** | **Linter: 3 checks nuevos — `shadow` (variables que sombrean outer), `unused-loop-var`, `mutable-default` (defaults `=[]`/`={}`)** | ✅ |
| **v1.56** | **Lenguaje core: `borrar d[k]` / `borrar obj.attr` (keyword desde v0.5, ahora implementado) + aug-assign sobre atributos (`obj.x += 1`)** | ✅ |
| **v1.57** | **`global X` implementado en bytecode VM (keyword desde v0.5, parseada desde v1.4, ejecutable desde hoy)** | ✅ |
| **v1.58** | **Stdlib `csv` — parser/writer RFC 4180-like (quoted, escapes, separadores configurables, round-trip)** | ✅ |
| **v1.59** | **Stdlib `base64` — codec RFC 4648 nativo (HTTP Basic Auth, Data URIs, JWT)** | ✅ |
| **v1.60** | **Stdlib `hashing` — SHA-256 (FIPS 180-4) + MD5 (RFC 1321) nativos** | ✅ |
| **v1.61** | **Perf round 2 — `cadena_unir` nativo + iter-en-texto: csv_parse_1000 33× speedup** | ✅ |
| **v1.62** | **Perf round 2 cont. — 5 nativas más en `cadenas` (indice_de, empieza_con, ...) tras audit de stdlib** | ✅ |
| **v1.63** | **Linter `concat-in-loop` — detecta automáticamente el patrón cazado en v1.61-62 (10ª categoría)** | ✅ |
| **v1.64** | **Linter `# noqa: <categoria>` — supresión selectiva por línea (bare o por categoría)** | ✅ |
| **v1.65** | **HMAC-SHA-256 + HMAC-MD5 (RFC 2104/4231) — autenticación de mensajes con clave secreta** | ✅ |
| **v1.66** | **base64 URL-safe (RFC 4648 §5) — `-_` sin padding, prerequisito para JWT** | ✅ |
| **v1.67** | **stdlib `jwt` (RFC 7519 HS256) — codificar/decodificar/verificar, pure-Cornamusa sobre json+base64+hashing** | ✅ |
| **v1.68** | **Linter `same-comparison` — detecta `x == x` / `x < x` (typos clasicos), 11ª categoría** | ✅ |
| **v1.69** | **Linter `empty-except` — `atrapar X: pasar` (error swallowing), 12ª categoría** | ✅ |
| **v1.70** | **`jwt.expirado()` + `jwt.decodificar_y_validar()` — validación de claims `exp`/`nbf` (RFC 7519 §4.1.4/4.1.5)** | ✅ |
| **v1.71** | **Profiler determinista `cornamusa prof` — tabla por función con `llamadas`/`total`/`self`/`per-call`** | ✅ |
| **v1.72** | **Decoradores `@nombre` — desugar puro en el compilador, con stacking y factories `@retry(3)`** | ✅ |
| **v1.73** | **Stdlib `tiempo` — epoch_segundos/ms, monotonic, dormir, Cronometro (17º módulo)** | ✅ |
| **v1.74** | **Auditoría: tests seguridad JWT, tests directos csv + nativas cadenas, fix `ErrorDeClave` atrapable** | ✅ |
| **v1.75** | **Coverage tracker `cornamusa cov` — reporta % de líneas top-level cubiertas, complementa profiler** | ✅ |
| **v1.76** | **Debugger interactivo `cornamusa depurar` — breakpoints, step, inspect, backtrace; cierra el tooling de Fase 5** | ✅ |
| **v1.77** | **Decoradores `@x` sobre métodos de clase (con stacking + factories); opcode `OP_INTERCAMBIAR`** | ✅ |
| **v1.78** | **`@propiedad` — getters automáticos invocados al acceder al atributo (sin paréntesis)** | ✅ |
| **v1.79** | **Tutorial expandido a curso con ejercicios — ~35 ejercicios + anexo de soluciones validadas** | ✅ |
| **v1.80** | **Limpieza stdlib: dead code eliminado, params renombrados, ~80 líneas menos** | ✅ |
| **v1.81** | **Linter `redundant-bool-compare` + `useless-return` — 14 categorías totales** | ✅ |
| **v1.82** | **Cookbook con 10 recetas validadas (CSV, JWT, hashing, backoff, memoize, ...)** | ✅ |
| **v1.83** | **ESPEC.md alineado a v1.82 — stdlib 17 módulos, tooling, decoradores, `@propiedad`** | ✅ |
| **v1.84** | **`@estaticometodo` — métodos sin `yo` implícito, soporte `Clase.metodo`** | ✅ |
| **v1.85** | **`@clasemetodo` — método recibe `cls`, constructores alternativos polimórficos** | ✅ |
| **v1.86** | **`tiene_atributo` / `obtener_atributo` / `asignar_atributo` — atributos dinámicos** | ✅ |
| **v1.87** | **`funcionales` extendido: `agrupar_por`, `tomar`, `saltar`, `combinar`, `aplanar`, `unicos`** | ✅ |
| **v1.88** | **Stdlib `coleccion` — Pila, Cola, ColaDoble (18º módulo)** | ✅ |
| **v1.89** | **Linter `bool-coerce-conditional` + `for-rango-longitud` — 16 categorías totales** | ✅ |
| **v1.90** | **Cookbook ampliado: 5 recetas más (email, config, logger, csv-dicts, ordenar) — 15 totales** | ✅ |
| **v1.91** | **Stdlib `inspeccion` — introspección de clases/instancias (19º módulo)** | ✅ |
| **v1.92** | **Stdlib `validacion` — email/URL/fecha/rango + clase `Validador` (20º módulo)** | ✅ |
| **v1.93** | **Stdlib `argumentos` — parser CLI estilo argparse (21º módulo)** | ✅ |
| **v1.94** | **Stdlib `ruta` — `Ruta` lexicográfica estilo `pathlib.PurePath` (22º módulo)** | ✅ |
| **v1.95** | **Fix compilador: pre-declarar locales nuevos antes de `si` (bug v1.94)** | ✅ |
| **v1.96** | **Stdlib `pruebas` — framework de testing minimalista (23º módulo)** | ✅ |
| **v1.97** | **Filesystem: `archivo_es_directorio`, `directorio_listar`, `obtener_cwd`, `directorio_crear` + `ruta.cwd()` y métodos `Ruta.es_archivo/es_directorio/listar`** | ✅ |
| **v1.98** | **`cornamusa nuevo <nombre>` — scaffold de proyecto con main + tests usando stdlib `pruebas`** | ✅ |
| **v1.99** | **FS completo: `archivo_borrar`, `directorio_borrar`, `archivo_info` (tamano + mtime) + métodos en `Ruta`** | ✅ |
| **v1.100** | **Glob recursivo: `recorrer(dir)`, `encontrar(dir, patron)` + métodos `Ruta.coincide/recorrer/encontrar` (`*` y `?`)** | ✅ |
| **v1.101** | **`funcionales.ordenar_por(xs, clave)` + `ordenar_por_inverso` — mergesort estable con key function** | ✅ |
| **v1.102** | **`archivos.eliminar_arbol` (rm -rf) + `crear_arbol` (mkdir -p) + métodos `Ruta` equivalentes** | ✅ |
| **v1.103** | **Matemáticas: raíz, log, exp, trig, redondeo + `azar.normal(mu, sigma)` Box-Muller** | ✅ |
| **v1.104** | **Entorno: `sistema.obtener_variable`, `establecer_variable`, `variables()`, `inicio()` (home dir)** | ✅ |
| **v1.105** | **FS copy: `archivo_copiar` + `archivos.copiar_arbol` (recursivo con mkdir -p implícito) + métodos `Ruta`** | ✅ |
| **v1.106** | **Cookbook ampliado a 20 recetas (suite de tests, cleanup por mtime, backup, config, estadística)** | ✅ |
| **v1.107** | **Typo suggestions: filtro de idéntico + case-insensitive ASCII (`IMPRIMIR` sugiere `imprimir`)** | ✅ |
| **v1.108** | **Sistema completo: `usuario_actual`, `hostname`, `directorio_temporal` + wrappers en `sistema`** | ✅ |
| **v1.109** | **`@propiedad` con setter via `@escritor` — cierra OOP (deuda de v1.78) + `ErrorDeAtributo` atrapable** | ✅ |
| **v1.110** | **`azar.exponencial/binomial/poisson` + constantes `TAU/INFINITO/NO_NUMERO` + predicados `es_infinito/es_no_numero/es_finito`** | ✅ |
| **v1.111** | **FS modificadores: `archivo_mover` (rename atómico) + `archivo_set_mtime`/`tocar` + métodos `Ruta`** | ✅ |
| **v1.112** | **F-string debug format `f"{x=}"` — Python 3.8 style, sin ambigüedad con `==`/`!=`/`<=`/`>=`** | ✅ |
| **v1.113** | **Walrus operator `:=` — asignación en expresión (PEP 572) para `si (n := f()) > 0:` y similares** | ✅ |
| **v1.114** | **Tipos opcionales sintácticos (PEP 484): `funcion f(x: tipo) -> tipo:` + `nombre: tipo = valor`** | ✅ |
| **v1.115** | **Cookbook a 25 recetas (walrus, f-string debug, sandbox temporal, anotaciones, propiedad solo lectura)** | ✅ |
| **v1.116** | **Stdlib `coleccion` extendida: `Heap` (min-heap binario, O(log n)) + `Contador` (multiset estilo Counter)** | ✅ |
| **v1.117** | **Stdlib `estadisticas`: media, mediana, moda, varianza, percentiles, correlación de Pearson, regresión lineal** | ✅ |
| **v1.118** | **Stdlib `iteradores`: combinatoria (producto, permutaciones, combinaciones) + ventana deslizante + run-length** | ✅ |
| **v1.119** | **Stdlib `grafos`: clase Grafo + BFS + DFS + Dijkstra + camino más corto + orden topológico + ciclos + componentes** | ✅ |
| **v1.120** | **`Heap(clave=lambda)`: heap con función clave + Dijkstra refactor a O((V+E) log V) — cierra deuda técnica v1.119** | ✅ |
| **v1.121** | **Kwargs en constructores de clase: `Persona(nombre="Ana", edad=30)` — cierra deuda técnica v1.120** | ✅ |
| **v1.122** | **+10 métodos nativos sobre cadena/lista/dict: `.separar`, `.reemplazar`, `.recortar`, `.contiene`, `.unir`, `.contar`, `.copiar`, `.items`, `.obtener` + fases corpus** | ✅ |
| **v1.123** | **Fix del caso patológico residual de destructuring (`a, b = ...` con NUEVAS variables dentro de un bucle ya no acumula stack)** | ✅ |
| **v1.124** | **Mensajes de error en kwargs de constructor: `Persona() no acepta keyword 'profesion'` en vez de `__iniciar__()`** | ✅ |
| **v1.125** | **Typo suggestions en `ErrorDeClave`: `d["nomre"]` → `(¿quisiste decir 'nombre'?)`** | ✅ |
| **v1.126** | **REPL: autocompletado con TAB sobre nombres globales y keywords** | ✅ |
| **v1.127** | **LSP `textDocument/completion`: nativas + keywords + funciones/clases del documento abierto** | ✅ |
| **v1.128** | **Métodos nativos sobre `conjunto` (union/interseccion/diferencia/...) y `tupla` (contar/contiene/indice_de) + `lista.indice_de`** | ✅ |
| **v1.129** | **Star binding en destructuring: `a, *resto, c = lista`. OP_REBANADA acepta tuplas (devuelve lista)** | ✅ |
| **v1.130** | **Cierra bug `OP_ITER_SIGUIENTE sin iterador en slot N` con `para` en `mientras` en `para`. Limpia workaround en `stdlib/grafos.cor`** | ✅ |
| **v1.131** | **`OP_INDICE` y `OP_REBANADA` sobre `rango`: `rango(0, 10)[3]`, `r[2:5]`, `a, *m, b = rango(0, 5)`** | ✅ |
| **v1.132** | **Comprehensions con múltiples `para`: `[(x, y) para x en xs para y en ys si x != y]`** | ✅ |
| **v1.133** | **Star binding en primera posición del destructuring: `*previos, ultimo = it`** | ✅ |
| **v1.134** | **Destructuring en `para`: `para a, b en pares:` y `para *previos, ultimo en xs:`** | ✅ |
| **v1.135** | **Destructuring en comprehensions: `[a + b para a, b en pares]`, también con `*`** | ✅ |
| **v1.136** | **Generator expressions con multi-`para` y destructuring: `(a+b para a,b en xs para c en ys)`** | ✅ |
| **v1.137** | **Patrones anidados en `para`: `para (a, (b, c)) en triples:`, también con `*`** | ✅ |
| **v1.138** | **Patrones anidados en comprehensions y genex: `[expr para (a, (b, c)) en triples]`** | ✅ |
| **v1.139** | **F-string specs: octal `:o`, porcentaje `:%`, signo `:+d`, zero-pad+signo `:+05d`** | ✅ |
| **v1.140** | **F-string specs: separadores `:,d` `:_d` y code-point `:c` (`{65:c}` → `"A"`)** | ✅ |
| **v1.141** | **`con` invoca `__salir__(yo, tipo, valor, traza)` — firma Python con 3 args** | ✅ |
| **v1.142** | **`*args` y `**kw` en métodos de clase: `funcion __salir__(yo, *_):` funciona** | ✅ |
| **v1.143** | **Kwargs reales en métodos: `obj.m(x=1, y=2)`, defaults via kw, `**kw` absorbe** | ✅ |
| **v1.144** | **`minimo`/`maximo` con `clave=`: `minimo(personas, clave=lambda p: p.edad)`** | ✅ |
| **v1.145** | **Tuplas/listas ordenables lex, `ordenado(xs, clave=, invertido=)` funcional estable** | ✅ |
| **v1.146** | **`juntar(*its)` (zip) y `juntar_mas_largo(its, relleno=)` (zip_longest) en stdlib** | ✅ |
| **v1.147** | **`tomar_mientras(p, xs)`, `descartar_mientras(p, xs)`, `particionar(p, xs)`** | ✅ |
| **v1.148** | **Nativas `escribir(*args)` (sin `\n` final) y `imprimir_error(*args)` (a stderr)** | ✅ |
| **v1.149** | **`contar_si(p, xs)` y `unicos_por(xs, clave)` en stdlib funcionales** | ✅ |
| **v1.150** | **`d.actualizar(otro)` nativa + `fusionar(*dicts)` para combinar diccionarios** | ✅ |
| **v1.151** | **`d.sacar(k[, default])` (Python pop) y `d.vaciar()` (Python clear)** | ✅ |
| **v1.152** | **`s.dividir_lineas()` + `s.centrar`/`alinear_izquierda`/`alinear_derecha` (code-points)** | ✅ |
| **v1.153** | **`s.mayusculas()` / `s.minusculas()` Unicode: `"ñoño".mayusculas()` → `"ÑOÑO"`** | ✅ |
| **v1.154** | **Predicados `s.es_alfa/es_digito/es_alfanum/es_espacios` + `s.titulo()` Unicode** | ✅ |
| **v1.155** | **`lst.vaciar()` (Python clear) y `lst.extender(iterable)` (Python extend)** | ✅ |
| **v1.156** | **`conj.vaciar`/`actualizar(it)`/`descartar(e)` cierra simetría list/dict/set** | ✅ |
| **v1.157** | **`s.dividir_palabras()` (Python split sin args) y `s.rellenar_ceros(n)` (zfill con signo)** | ✅ |
| **v1.158** | **`divmod(a, b)` con floor div + `potencia_modular(b, e, m)` bignum + `mcm(a, b)`** | ✅ |
| **v1.159** | **`entero(s, base)` (Python int(s, base)) + `binario/hexadecimal/octal(n)` bignum** | ✅ |
| **v1.160** | **`inverso(iterable)` no-mutante (Python `list(reversed(it))`)** | ✅ |
| **v1.161** | **`aplanar_profundo(xs)` recursivo y `aplanar_hasta(xs, n)` con profundidad** | ✅ |
| **v1.162** | **`s.sin_acentos()` Unicode (slugs, búsqueda tolerante): `"CAFÉ"` → `"CAFE"`** | ✅ |
| **v1.163** | **`hash(x)` expone el hash interno (mismo que usan dicc/conjunto): claves estables, no criptográfico** | ✅ |
| **v1.164** | **`congelar(s)` → conjunto inmutable y hashable (Python `frozenset`): usable como clave de dicc** | ✅ |
| **v1.165** | **`copia(x)` shallow + `copia_profunda(x)` deep (con detección de ciclos) — paridad Python `copy`** | ✅ |
| **v1.166** | **`producto(xs)` + `acumular(xs, op, inicial)` (sumas parciales / running totals) en `funcionales`** | ✅ |
| **v1.167** | **`~x` con variables en bytecode + dunders unarios `__negar__` y `__tilde__` en instancias** | ✅ |
| **v1.168** | **`__contiene__(yo, x)` dunder: `x en obj` ahora despacha en instancias (rangos lazy, conjuntos predicados)** | ✅ |
| **v1.169** | **`__positivo__` dunder + `OP_POSITIVO`: cierra trilogía unaria (`+/-/~`) sobre instancias** | ✅ |
| **v1.170** | **Bitwise binarios `& \| ^ << >>` en bytecode (con variables) + dunders en instancias** | ✅ |
| **v1.171** | **Spread `*xs` en literales de lista (`[a, *xs, b]`) — funciona con lista/tupla/cadena/conjunto/dicc/rango** | ✅ |
| **v1.172** | **Spread `*xs` en literales de tupla y conjunto: `(a, *xs, b)` y `{a, *xs, b}`** | ✅ |
| **v1.173** | **`**d` dict spread en literales: `{**defecto, "k": v, **override}` para fusionar configs** | ✅ |
| **v1.174** | **Primer elemento spread en tupla `(*xs,)` y conjunto `{*xs}` — cierra limitación de v1.172** | ✅ |
| **v1.175** | **Spread con generadores: `[*gen()]`, `(*gen(),)`, `{*gen()}` — cierra limitación de v1.171** | ✅ |
| **v1.176** | **Spread con instancias `__siguiente__`/`__iterar__` — iteradores custom desempacables** | ✅ |
| **v1.177** | **Comprehensions/genex inline en spread: `[*[x para x en xs], 99]` (cierra última limitación de v1.171)** | ✅ |
| **v1.178** | **`coincidir Punto(a, b)` con args: bind/literal/wildcard por atributo (cierra limitación de v1.16.3)** | ✅ |
| **v1.179** | **Dict patterns `cuando {"k": v, "tipo": "x"}`: matchea por claves, semántica super-set** | ✅ |
| **v1.180** | **Sub-patrones anidados en `Foo(a=Bar(x))` y `{"k": Foo(a)}`: recursión arbitraria (refactor PathSegmento)** | ✅ |
| **v1.181** | **`**resto` en dict patterns: captura claves no mencionadas en un dict nuevo (paridad Python)** | ✅ |
| **v1.182** | **Parámetros keyword-only tras `*args`: `funcion f(*args, sep=", "):` (paridad Python `def f(*args, sep=", ")`)** | ✅ |
| **v1.183** | **Lambdas con keyword-only: `lambda *args, kw=default: ...`** | ✅ |
| **v1.184** | **Keyword-only obligatorios sin default: `funcion enviar(*args, destino, copia=falso):`** | ✅ |
| **v1.185** | **Parámetros posicional-only con `/`: `funcion f(a, b, /, c, d):` (paridad Python 3.8)** | ✅ |
| **v1.186** | **Conversores `!r`, `!s`, `!a` en f-strings: `f"{x!r:>10}"` aplica repr antes del spec** | ✅ |
| **v1.187** | **F-string tipos `g`/`G` (formato general, paridad printf `%g`)** | ✅ |
| **v1.188** | **F-string `#` alternate form: prefijo `0x`/`0X`/`0b`/`0o` en hex/bin/oct** | ✅ |
| **v1.189** | **F-string spec con interpolaciones: `f"{x:{ancho}}"`, `f"{n:.{prec}f}"`** | ✅ |
| **v1.190** | **`finalmente` se ejecuta antes de `retornar` (cierra limitación documentada de v0.8.3)** | ✅ |
| **v1.191** | **`finalmente` se ejecuta antes de `romper`/`continuar` que salen del intentar** | ✅ |
| **v1.192** | **`enumerar(it, inicio=0)` como builtin global — sin `importar funcionales`** | ✅ |
| **v1.193** | **`juntar(*its)` (zip) como builtin global — `para a, b en juntar(xs, ys):`** | ✅ |
| **v1.194** | **`suma`, `minimo`, `maximo`, `cualquiera`, `todos` como builtins globales (paridad sum/min/max/any/all)** | ✅ |
| **v1.195** | **`mapear`/`filtrar` builtins + infraestructura: las nativas C ya pueden invocar callables Cornamusa** | ✅ |
| **v1.196** | **`ordenado(it, clave, invertido)` builtin con sort estable (merge sort, paridad Python `sorted`)** | ✅ |
| **v1.197** | **`reducir(f, it, inicial?)` builtin — completa la tríada map/filter/reduce (paridad `functools.reduce`)** | ✅ |
| **v1.198** | **Bugfix estructural: comprehensions en cualquier posición de expresión (tracking `prof_expr` en compilador)** | ✅ |
| **v1.199** | **Bugfix: excepciones desde generadores hacia handlers del caller (mata el flaky histórico, SegFault ~23%)** | ✅ |
| **v1.200** | **Bugfix: los builtins (`lista`, `suma`, `mapear`, `ordenado`...) consumen generadores en vez de tratarlos como vacíos en silencio** | ✅ |
| **v1.201** | **Bugfix: el GC marca `@propiedad`/`@estaticometodo`/`@clasemetodo` (use-after-free al re-acceder tras una recolección)** | ✅ |
| **v1.202** | **`atrapar (TipoA, TipoB)` — varios tipos de excepción en un solo manejador (semántica OR)** | ✅ |
| **v1.203** | **Bugfix: el alias `como e` se corrompía si el cuerpo del `intentar` declaraba locals (off-by-N en el slot)** | ✅ |
| **v1.204** | **`conjunto()` e `inverso()` aceptan cualquier iterable (cadena, dict, rango, generador), no solo lista/tupla** | ✅ |
| **v1.205** | **Los builtins (`lista`, `conjunto`, `suma`, `mapear`...) aceptan instancias iterables (`__iterar__`/`__siguiente__`), igual que `para`** | ✅ |
| **v1.206** | **Excepciones definidas por el usuario: `lanzar`/`atrapar` instancias de clase, con herencia** | ✅ |
| **v1.207** | **`booleano(obj)` despacha `__booleano__` (consistente con `si obj:` / `no obj`)** | ✅ |
| **v1.208** | **`cualquiera()`/`todos()` despachan `__booleano__` (antes daban resultados silenciosamente incorrectos)** | ✅ |
| **v1.209** | **Los errores de importación (`ErrorDeImportacion`) son atrapables con `intentar`/`atrapar`** | ✅ |
| v2.0 (lejano) | concurrencia, async/await, NaN-boxing | ⏳ |

> Nota: el orden real de las fases divergió del plan original (v0.7 fueron clases, v0.8 fue GC) por dependencias técnicas — clases generan ciclos refcount que motivaron el GC.

**Arquitectura:** AST compartido entre dos motores. El intérprete tree-walking sirve como primer release jugable (v0.4-v0.5) y como referencia ejecutable congelada para regresión desde v0.6. La VM bytecode es el motor de producción y el destino de todas las optimizaciones. Detalle en [decisiones/B2-tree-walking-vs-bytecode.md](decisiones/B2-tree-walking-vs-bytecode.md).

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

**72 programas** en [`examples/`](examples/), uno por feature y numerados por orden de aparición. Una selección:

**Fundamentos** — [`01_hola_mundo`](examples/01_hola_mundo.cor) · [`02_fizzbuzz`](examples/02_fizzbuzz.cor) · [`05_listas`](examples/05_listas.cor) · [`06_diccionarios`](examples/06_diccionarios.cor) · [`07_clases_herencia`](examples/07_clases_herencia.cor) · [`08_excepciones`](examples/08_excepciones.cor) · [`10_quicksort`](examples/10_quicksort.cor)

**OOP y closures** — [`28_dunders_jugable`](examples/28_dunders_jugable.cor) · [`29_oop_avanzado`](examples/29_oop_avanzado.cor) · [`30_closures_nolocal`](examples/30_closures_nolocal.cor) · [`36_con_recursos`](examples/36_con_recursos.cor) (context managers)

**Pattern matching** — [`38_coincidir`](examples/38_coincidir.cor) · [`39_coincidir_estructural`](examples/39_coincidir_estructural.cor) · [`40_or_y_star`](examples/40_or_y_star.cor) · [`41_type_match`](examples/41_type_match.cor)

**Funciones modernas** — [`46_destructuring`](examples/46_destructuring.cor) · [`47_varargs`](examples/47_varargs.cor) · [`48_kwargs`](examples/48_kwargs.cor) · [`50_dspread`](examples/50_dspread.cor)

**Generadores y comprehensions** — [`55_comprehensions`](examples/55_comprehensions.cor) · [`56_generadores`](examples/56_generadores.cor)

**Stdlib** — [`31_archivos`](examples/31_archivos.cor) · [`32_json_archivos`](examples/32_json_archivos.cor) · [`44_fechas`](examples/44_fechas.cor) · [`51_azar`](examples/51_azar.cor) · [`52_proceso`](examples/52_proceso.cor) · [`53_regex`](examples/53_regex.cor) · [`54_red`](examples/54_red.cor)

**Robustez** — [`33_atrapar_robusto`](examples/33_atrapar_robusto.cor) · [`57_sugerencias`](examples/57_sugerencias.cor) (errores con sugerencias)

## Documentación

- **[Sitio web](https://david-castilla-gomez.github.io/Cornamusa/)** — tutorial paso a paso + referencia rápida (mdBook desplegado a GitHub Pages).
- **[Tutorial](docs/tutorial.md)** — aprende Cornamusa desde cero, con código ejecutable.
- **[Referencia rápida](docs/referencia.md)** — cheatsheet de sintaxis, built-ins, stdlib, errores.
- **[Cookbook](docs/cookbook.md)** — recetas listas para copy-paste cubriendo casos reales.
- **[FAQ](FAQ.md)** — preguntas frecuentes para nuevos usuarios.
- **[ESPEC.md](ESPEC.md)** — especificación formal del lenguaje (gramática EBNF, semántica).
- **[Decisiones (ADRs)](decisiones/)** — `B1` a `B10`, razonamiento detrás de las decisiones de diseño.
- **[CHANGELOG.md](CHANGELOG.md)** — historial de cambios.
- **[CONTRIBUTING.md](CONTRIBUTING.md)** — cómo contribuir.
- **[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)** — código de conducta.

## Inspiración

Cornamusa se construye sobre la literatura clásica y moderna de implementación de lenguajes dinámicos. Los recursos que guían el diseño están en [`recursos.md`](recursos.md): *Crafting Interpreters* (Nystrom), *CPython Internals* (Shaw), *The Implementation of Lua 5.0*, los papers de SELF/SmallTalk-80, *PyPy meta-tracing*, *Truffle/Graal*, entre otros.

## Licencia

[MIT](LICENSE) — libre para uso personal, educativo y comercial.

---

*Cornamusa* — del castellano antiguo, **gaita** o instrumento de viento de origen incierto. Que este lenguaje suene tan bien en tu código como una buena cornamusa en una fiesta popular.
