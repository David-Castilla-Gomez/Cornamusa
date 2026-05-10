# Cornamusa

> Un lenguaje de programación dinámico, interpretado y **en castellano**.

[![Licencia: MIT](https://img.shields.io/badge/licencia-MIT-blue.svg)](LICENSE)
[![Versión](https://img.shields.io/badge/versión-1.12.0-blue.svg)](CHANGELOG.md)
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

## Características (en v1.0.0)

- ✅ **Sintaxis castellana natural**: `si`/`sino`, `mientras`, `para X en Y`, `funcion`, `clase`, `intentar`/`atrapar`/`finalmente`, `verdadero`/`falso`/`nulo`.
- ✅ **Tipado dinámico** con tipos completos: enteros bignum, decimales f64, cadenas UTF-8, listas, diccionarios, conjuntos, tuplas.
- ✅ **Bloques explícitos** con `:` al abrir y `fin <etiqueta>` al cerrar. Indentación recomendada pero no obligatoria.
- ✅ **UTF-8 completo** en código e identificadores con normalización NFC obligatoria.
- ✅ **Clases con herencia** y `super` multinivel; `yo` como referencia a la instancia.
- ✅ **Excepciones** completas: `atrapar Tipo como e`, `sino`, `finalmente`, `lanzar` con re-raise.
- ✅ **Closures con upvalues**, lambdas, slicing de listas.
- ✅ **GC mark-sweep** automático con `recolectar()` para forzarlo manualmente.
- ✅ **Módulos**: `importar X.Y como Z`, `desde X importar A, B como C`.
- ✅ **Stdlib mínima**: `matematicas` (PI, E, factorial, mcd, ...), `cadenas` (repetir, contar, empieza_con, ...), `sistema` (`argv`, `salir`), `archivos` (`leer`, `escribir`, `lineas`, `existe`, `agregar`) desde v1.8 y `json` (`parsear`, `serializar`) desde v1.9.
- ✅ **Conversores explícitos** (v1.1): `cadena()`, `entero()`, `decimal()`, `booleano()`, `lista()`, `tupla()`, `diccionario()`.
- ✅ **F-cadenas con interpolación real** (v1.1): `f"hola {nombre}, tienes {edad+10} en 10 anos"` evalúa cada `{expr}`. Soporta llaves dobles `{{` `}}`, expresiones complejas, anidación.
- ✅ **Entrada interactiva** (v1.1): `leer([prompt])` lee de stdin para programas interactivos.
- ✅ **Dunders aritméticos y de coerción** (v1.2): `Vector + Vector` invoca `__sumar__`, `imprimir(obj)` usa `__cadena__`, `obj[k]` usa `__indice__`. 14 dunders soportados (aritméticos, comparación, indexación, representación).
- ✅ **Dunders avanzados** (v1.3): operadores reflejados (`5 + V(...)` invoca `__sumar_derecho__`), `__llamar__` (instancias callable), `__longitud__` (`longitud(obj)` invoca el dunder), `cadena(obj)` también invoca `__cadena__`.
- ✅ **Closures completas con `nolocal`** (v1.4): declaración explícita de variables de scope envolvente con validación temprana.
- ✅ **Tests diferenciales** tree-walking vs bytecode + **benchmarks** en [`benchmarks/`](benchmarks/).
- ✅ **Inline caching** estilo PEP 659 con quickening por reescritura de opcode (decisión [B8](decisiones/B8-inline-caching.md)). Especializaciones: `OP_OBTENER_GLOBAL_CACHE`, `OP_LLAMAR_{NATIVA,BC,CLASE,METODO_LIGADO}`, `OP_*_INT_INT` (suma/resta/mult + comparaciones), `OP_OBTENER_ATRIBUTO_INSTANCIA` (shape cache).
- ✅ **Small-int tagging** (decisión [B9](decisiones/B9-small-int-tagging.md)): enteros que caben en 63 bits viven inline en `Valor` sin alocar `mp_int`. Operaciones SMALL+SMALL inline con detección de overflow vía `__builtin_*_overflow`. Aritmética bignum (libtommath) sigue disponible transparentemente para enteros grandes. ~2.7x geomedia sobre v0.10 (6x en `fibonacci_recursivo`).
- ⏳ **GC generacional + sitio web + docs completos**: planeados para v1.0.

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

**v1.0.0 publicada.** Cornamusa es estable, documentado y usable. Lenguaje funcional para programas reales con rendimiento ~3x sobre v0.10 (5.8x en `fibonacci_recursivo`, 4.8x en `globales_lookup`, 3x en `dicc_intensivo`). [Tutorial paso a paso](docs/tutorial.md), [referencia rápida](docs/referencia.md), [FAQ](FAQ.md) y [sitio web](https://david-castilla-gomez.github.io/Cornamusa/) disponibles. Compromisos de estabilidad post-v1.0 documentados en [B10](decisiones/B10-scope-de-v1.md).

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
| v1.13+ | context managers (`con`), `formato`, `fechas`, `__siguiente__` (iteración lazy) | ⏳ |
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

23 programas en [`examples/`](examples/), agrupados por feature:

**Básicos** (todos corren en tree-walking y bytecode):
- [`01_hola_mundo.cor`](examples/01_hola_mundo.cor) · [`02_fizzbuzz.cor`](examples/02_fizzbuzz.cor) · [`13_factorial_jugable.cor`](examples/13_factorial_jugable.cor) · [`15_fizzbuzz_jugable.cor`](examples/15_fizzbuzz_jugable.cor)

**Estructuras de datos**:
- [`05_listas.cor`](examples/05_listas.cor) · [`06_diccionarios.cor`](examples/06_diccionarios.cor) · [`16_lista_busqueda.cor`](examples/16_lista_busqueda.cor) · [`17_dicc_frecuencia.cor`](examples/17_dicc_frecuencia.cor) · [`18_conj_y_tupla.cor`](examples/18_conj_y_tupla.cor)

**Bytecode-only** (closures, OOP, módulos, sistema):
- [`19_closures_jugable.cor`](examples/19_closures_jugable.cor) · [`20_clases_jugable.cor`](examples/20_clases_jugable.cor) · [`21_modulos_jugable.cor`](examples/21_modulos_jugable.cor) · [`22_modulos_avanzado.cor`](examples/22_modulos_avanzado.cor) · [`23_sistema_jugable.cor`](examples/23_sistema_jugable.cor)

**Avanzados** (programas no triviales):
- [`24_notas_clase.cor`](examples/24_notas_clase.cor) — análisis de notas con dicc, listas, ordenamiento, mediana
- [`25_biblioteca_oop.cor`](examples/25_biblioteca_oop.cor) — simulación de biblioteca con OOP, herencia, polimorfismo

**v1.1**:
- [`26_leer_jugable.cor`](examples/26_leer_jugable.cor) — calculadora de IMC con `leer()` interactivo y conversores
- [`27_fstrings_jugable.cor`](examples/27_fstrings_jugable.cor) — interpolación real con f-cadenas (anidación, bignum, lista)

**v1.2**:
- [`28_dunders_jugable.cor`](examples/28_dunders_jugable.cor) — Vector2D, TablaInversa y Persona usando `__sumar__`, `__cadena__`, `__indice__`, `__menor__`

**v1.3**:
- [`29_oop_avanzado.cor`](examples/29_oop_avanzado.cor) — operadores reflejados, instancias callable (`Multiplicador`), `__longitud__` (`Pila`), `Contador` con estado interno

**v1.4**:
- [`30_closures_nolocal.cor`](examples/30_closures_nolocal.cor) — contador, sumador, memoización con cache, toggle compartido entre dos closures

**v1.8**:
- [`31_archivos.cor`](examples/31_archivos.cor) — I/O persistente con `archivos.escribir`/`leer`/`lineas`/`agregar`/`existe`

**v1.9**:
- [`32_json_archivos.cor`](examples/32_json_archivos.cor) — `json.parsear`/`serializar` combinado con `archivos`, mostrando auto-traducción `verdadero/falso/nulo ↔ true/false/null`

**v1.10**:
- [`33_atrapar_robusto.cor`](examples/33_atrapar_robusto.cor) — programas robustos con errores atrapables: cargar config con fallback ante archivo inexistente o JSON corrupto

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
