# Cornamusa

> Un lenguaje de programación dinámico, interpretado y **en castellano**.

[![Licencia: MIT](https://img.shields.io/badge/licencia-MIT-blue.svg)](LICENSE)
[![Versión](https://img.shields.io/badge/versión-1.40.0-blue.svg)](CHANGELOG.md)
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

**Biblioteca estándar** — doce módulos: `matematicas`, `cadenas`, `funcionales`, `formato`, `archivos`, `json`, `fechas`, `azar`, `proceso`, `regex`, `red`, `sistema`.

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

**v1.40.0 publicada.** Cornamusa es estable y maduro: paridad sintáctica cercana a Python 3.10+ (pattern matching, generadores, comprehensions, destructuring, `*args`/`**kwargs`, context managers) y una stdlib de doce módulos útil para scripting real. 185 tests en verde, toda la documentación validada contra el intérprete. [Tutorial paso a paso](docs/tutorial.md), [referencia rápida](docs/referencia.md), [FAQ](FAQ.md) y [sitio web](https://david-castilla-gomez.github.io/Cornamusa/) disponibles. Compromisos de estabilidad post-v1.0 documentados en [B10](decisiones/B10-scope-de-v1.md).

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

**57 programas** en [`examples/`](examples/), uno por feature y numerados por orden de aparición. Una selección:

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
