# Preguntas frecuentes

> Lo que un usuario nuevo o un programador curioso se preguntaría sobre Cornamusa. Si tu duda no está aquí, [abre un issue](https://github.com/David-Castilla-Gomez/Cornamusa/issues).

---

## Sobre el lenguaje

### ¿Por qué un lenguaje en castellano?

La mayoría de lenguajes de programación están en inglés y eso pone una barrera invisible a quien aprende a programar sin dominar el inglés. Cornamusa demuestra que un lenguaje moderno y razonablemente rápido puede tener su sintaxis, built-ins y mensajes de error completamente en castellano sin sacrificar nada importante.

No es una traducción literal de Python: las palabras clave se eligieron para que **suenen idiomáticas a un hispanohablante** (`y`/`o`/`no`, `es`, `en`, `extiende`, `intentar`/`atrapar`, etc.) — no `and`/`or`/`not`/`is` con tilde.

### ¿Es un toy language o se puede usar en serio?

Es funcional para programas reales (OOP completo, GC, excepciones, módulos, stdlib mínima, ~3x más rápido que CPython equivalente en algunos benchmarks tras small-int tagging). Pero todavía le faltan cosas para producción seria:

- Built-ins de I/O (`abrir`, `leer`).
- Stdlib amplia (regex, json, fechas, red, ...).
- Threads / async.
- Ecosistema de bibliotecas de terceros.

Ahora mismo sirve bien para **enseñar a programar en castellano** y para programas pequeños/medianos donde la stdlib mínima es suficiente.

### ¿Por qué se llama Cornamusa?

*Cornamusa* es el nombre del castellano antiguo para **gaita**, instrumento de viento de origen incierto. La idea: un lenguaje en castellano debería tener nombre castellano. (Y ningún lenguaje serio se llamaba así todavía.)

### ¿Es Cornamusa "Python en castellano"?

Comparte modelo dinámico, objetos, listas/dicc, indentación irrelevante (no como Python — Cornamusa usa `fin <etiqueta>` explícito), y filosofía pragmática. Pero hay diferencias:

- Bloques con `:` y `fin <etiqueta>`, indentación estilística no semántica.
- Enteros bignum desde día uno (sin `int` 32-bit ni `long` separado).
- Tipos numéricos: solo `entero` (precisión arbitraria) y `decimal` (IEEE 754 64-bit). No hay `Fraccion` ni `Decimal` exacto en core.
- Solo `__iniciar__` se invoca automáticamente en v0.11.x; los demás dunders aritméticos quedan para v1.x.
- No hay GIL ni hilos en v1.0.

Más detalles en §8 de [ESPEC.md](ESPEC.md).

---

## Instalación y uso

### ¿Cómo instalo Cornamusa?

Compilando desde fuente:

```bash
git clone https://github.com/David-Castilla-Gomez/Cornamusa.git
cd Cornamusa
cmake -B build && cmake --build build
./build/cornamusa --version
```

Requiere C11 (GCC/Clang/MSVC) y CMake ≥ 3.16. **No hay paquetes binarios todavía** — llegarán a partir de v1.x.

### ¿Funciona en Windows?

Sí. Compilación con MSVC y MinGW probada. Sin embargo, algunas optimizaciones (`__builtin_*_overflow` para detección de overflow en aritmética small-int) son más conservadoras en MSVC porque no soporta esos builtins.

### ¿Cómo ejecuto un archivo `.cor`?

```bash
./build/cornamusa --bytecode programa.cor       # motor bytecode (recomendado, 3x más rápido)
./build/cornamusa programa.cor                  # motor tree-walking (compatibilidad)
./build/cornamusa --bytecode prog.cor a b c     # con argumentos a sistema.argv
```

### ¿Cómo lo ejecuto interactivamente (REPL)?

```bash
./build/cornamusa
```

Pulsa Ctrl-D (Ctrl-Z en Windows) o escribe `salir` para terminar. El REPL acumula líneas hasta que cierras un bloque (con `fin <etiqueta>`); una línea vacía ejecuta el bloque.

---

## Sobre la sintaxis

### ¿Por qué `fin <etiqueta>` en vez de indentación significativa como Python?

Decisión [B1](decisiones/B1-modelo-de-bloques.md). Razones:

- **Robustez**: copiar y pegar código no rompe la indentación.
- **Pedagogía**: para alguien que aprende, el cierre explícito hace los bloques visibles. Errores de "olvidé desindentar" no existen.
- **Identidad castellana**: leer `mientras x: ... fin mientras` se siente más cercano al castellano hablado que indentación silenciosa.

La indentación de 4 espacios sigue siendo la **convención de estilo**, simplemente no es semántica.

### ¿Por qué `y`/`o`/`no` y no `&&`/`||`/`!`?

Decisión [B4](decisiones/B4-tildes-y-unicode.md): **keywords ASCII castellanas, identificadores Unicode**. Los operadores lógicos como palabra son más legibles para quien lee castellano.

> ⚠️ **`y` es keyword AND**: NO puedes usar `y` como nombre de variable. `funcion f(x, y)` da error de sintaxis. Usa `funcion f(x, b)` o renombra.

### ¿Por qué `__iniciar__` y no `__init__`?

Decisión [B5+B6](decisiones/B5-B6-yo-y-dunders.md). Por consistencia: si las keywords están en castellano, los dunders también. `__iniciar__` (constructor), `__cadena__` (str), `__longitud__` (len), etc.

### ¿Cómo declaro una variable global desde dentro de una función?

```cornamusa
contador = 0

funcion incrementar():
    global contador
    contador = contador + 1
fin funcion
```

Sin `global`, `contador = ...` dentro de la función crea un local nuevo.

### ¿Funcionan los closures con escritura?

En v0.11.x sí para **lectura**, no para escritura. La keyword `nolocal` está reservada pero no implementada — viene en v1.x. Workaround: usar una lista o dicc como contenedor mutable:

```cornamusa
funcion crear_contador():
    estado = [0]                  # lista de un elemento, mutable
    funcion siguiente():
        estado[0] = estado[0] + 1
        retornar estado[0]
    fin funcion
    retornar siguiente
fin funcion
```

---

## Sobre el rendimiento

### ¿Cómo de rápido es Cornamusa?

Tras v0.11 (small-int tagging), comparado con v0.10.0:

| Benchmark | Mejora |
|---|---|
| `fibonacci_recursivo(30)` | 5.9x más rápido |
| `globales_lookup` (1M iter) | 4.5x más rápido |
| `dicc_intensivo` (50k inserts) | 2.4x más rápido |

En absoluto, `fibonacci_recursivo(30)` corre en ~220ms — comparable o mejor que CPython en programas dominados por aritmética entera.

### ¿Por qué hay dos motores (tree-walking y bytecode)?

Decisión [B2](decisiones/B2-tree-walking-vs-bytecode.md). El intérprete tree-walking se introdujo en v0.4 como primer release jugable. La VM bytecode llegó en v0.6 como motor de producción. Ambos comparten el AST y producen los mismos resultados (validado por 8 tests diferenciales).

El tree-walking se mantiene **congelado** desde v0.5 como red de seguridad: si alguna optimización del bytecode rompe semántica, la divergencia con tree-walking lo revela. Para programas reales, usa siempre `--bytecode`.

### ¿Tiene JIT?

No. Solo IC (inline caching estilo PEP 659, decisión [B8](decisiones/B8-inline-caching.md)). JIT/tracing está en el roadmap para post-v1.0 si los datos de programas reales lo justifican.

### ¿Tiene threads?

No. Cornamusa es single-threaded por diseño en v1.0. Concurrencia es trabajo de v2.0+ (decisión I3 aplazada).

---

## Sobre el desarrollo

### ¿Quién mantiene Cornamusa?

David Castilla Gómez como autor único hasta v1.0. A partir de v1.0, contribuciones externas bienvenidas siguiendo [CONTRIBUTING.md](CONTRIBUTING.md).

### ¿Es estable la API?

A partir de v1.0:

- **Sintaxis del lenguaje**: congelada hasta v2.0. Cualquier ruptura requiere major version.
- **Built-ins y stdlib**: pueden añadirse, no romperse, entre minor versions.
- **AST, formato de chunks de bytecode, GC, IC, small-int**: detalles internos que pueden cambiar entre minor versions.
- **CLI flags públicas (`--bytecode`, `--version`, etc.)**: estables.
- **Mensajes de error de runtime**: la categoría (`ErrorDeTipo`, etc.) es estable. La redacción puede mejorar.

Detalle completo en §"Compromiso de estabilidad" de [B10](decisiones/B10-scope-de-v1.md).

### ¿Cómo reporto un bug?

[Abre un issue](https://github.com/David-Castilla-Gomez/Cornamusa/issues) incluyendo:

- Versión: `cornamusa --version`.
- Sistema operativo y compilador.
- Programa `.cor` mínimo que reproduce el problema.
- Salida observada vs esperada.

Si quieres ir un paso más: comprueba primero si el bug aparece solo en `--bytecode` o también en tree-walking. Eso ayuda enormemente a localizar la causa.

### ¿Hay roadmap?

Sí, en hitos:

| Hito | Estado |
|---|---|
| v0.4-v0.5 — Sintaxis + estructuras | ✅ |
| v0.6 — VM bytecode | ✅ |
| v0.7 — Clases | ✅ |
| v0.8 — GC | ✅ |
| v0.9 — Módulos | ✅ |
| v0.10 — Inline caching | ✅ |
| v0.11 — Small-int tagging | ✅ |
| v1.0 — Documentación + sitio web | en curso |
| v1.1+ | f-strings reales, dunders aritméticos, `nolocal`, más stdlib |
| v2.0 (lejano) | concurrencia, async/await, NaN-boxing |

Detalle de qué entra en cada release en [CHANGELOG.md](CHANGELOG.md).

---

## Curiosidades

### ¿Es válido `ñoño = 42`?

Sí. Los identificadores admiten Unicode (decisión [B4](decisiones/B4-tildes-y-unicode.md)):

```cornamusa
niño = "Pablito"
año_actual = 2026
función_principal = lambda: imprimir("Hola")
ñ = 1
```

Las keywords son ASCII puro: `funcion` (no `función`), pero los nombres que tú creas pueden tener cualquier letra Unicode incluyendo tildes y `ñ`.

### ¿Y emojis en identificadores?

```cornamusa
🦄 = "magia"   # ✗ ErrorDeSintaxis: 🦄 no es letra Unicode (cat. L)
```

No. Los identificadores admiten letras Unicode (categoría `L`), no símbolos ni emojis.

### ¿Puedo escribir Cornamusa en NFC y NFD intercambiablemente?

Sí. El lexer normaliza a NFC antes de tokenizar (decisión [B4](decisiones/B4-tildes-y-unicode.md)). `función` escrito en macOS (NFD) y en Windows (NFC) son el mismo identificador.
