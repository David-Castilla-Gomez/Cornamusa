# Preguntas frecuentes

> Lo que un usuario nuevo o un programador curioso se preguntaría sobre Cornamusa. Si tu duda no está aquí, [abre un issue](https://github.com/David-Castilla-Gomez/Cornamusa/issues).

---

## Sobre el lenguaje

### ¿Por qué un lenguaje en castellano?

La mayoría de lenguajes de programación están en inglés y eso pone una barrera invisible a quien aprende a programar sin dominar el inglés. Cornamusa demuestra que un lenguaje moderno y razonablemente rápido puede tener su sintaxis, built-ins y mensajes de error completamente en castellano sin sacrificar nada importante.

No es una traducción literal de Python: las palabras clave se eligieron para que **suenen idiomáticas a un hispanohablante** (`y`/`o`/`no`, `es`, `en`, `extiende`, `intentar`/`atrapar`, etc.) — no `and`/`or`/`not`/`is` con tilde.

### ¿Es un toy language o se puede usar en serio?

Es funcional para programas reales: OOP completo con dunders, closures con `nolocal`, pattern matching, generadores, comprehensions, GC, excepciones con traceback, módulos y una stdlib de doce módulos (`archivos`, `json`, `regex`, `fechas`, `azar`, `proceso`, `red`...). Sirve bien para **enseñar a programar en castellano** y para scripting pequeño/mediano.

Lo que todavía le falta para producción seria:

- Threads / async (planeado para v2.x).
- HTTPS/TLS en el cliente de red (solo HTTP/1.1 plano por ahora).
- Ecosistema de bibliotecas de terceros y gestor de paquetes.
- Tooling: depurador, formateador, language server.

### ¿Por qué se llama Cornamusa?

*Cornamusa* es el nombre del castellano antiguo para **gaita**, instrumento de viento de origen incierto. La idea: un lenguaje en castellano debería tener nombre castellano. (Y ningún lenguaje serio se llamaba así todavía.)

### ¿Es Cornamusa "Python en castellano"?

Comparte modelo dinámico, objetos, listas/dicc, indentación irrelevante (no como Python — Cornamusa usa `fin <etiqueta>` explícito), y filosofía pragmática. Pero hay diferencias:

- Bloques con `:` y `fin <etiqueta>`, indentación estilística no semántica.
- Enteros bignum desde día uno (sin `int` 32-bit ni `long` separado).
- Tipos numéricos: solo `entero` (precisión arbitraria) y `decimal` (IEEE 754 64-bit). No hay `Fraccion` ni `Decimal` exacto en core.
- No hay GIL ni hilos: Cornamusa es single-threaded por diseño.

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

Desde v1.47 el REPL tiene **edición de línea** (cursores ←/→, Home/End, Backspace, Delete) y **navegación de historial** con ↑/↓. El historial se guarda en `~/.cornamusa_historial` (`%USERPROFILE%\.cornamusa_historial` en Windows) y persiste entre sesiones.

### ¿Hay un formateador integrado?

Sí desde v1.48:

```bash
./build/cornamusa fmt programa.cor             # reescribe in-place
./build/cornamusa fmt --check programa.cor     # exit 1 si no esta formateado (CI)
./build/cornamusa fmt --stdout programa.cor    # imprime resultado, no toca archivo
cat programa.cor | ./build/cornamusa fmt -     # stdin → stdout
```

Reglas conservadoras: reindenta a 4 espacios, normaliza líneas en blanco y trailing whitespace, preserva comentarios. No toca espaciado de operadores ni rompe líneas largas — eso queda para releases posteriores.

### ¿Hay linter integrado?

Sí desde v1.49:

```bash
./build/cornamusa lint programa.cor
# programa.cor:3:1: warning [unused-import]: modulo importado pero no usado: 'fechas'
# programa.cor:7:10: warning [eq-nulo]: comparacion con nulo via '==' — prefiere 'es nulo'
# 2 avisos.
```

Categorías chequeadas:

- `unreachable` — código tras `retornar`, `romper`, `continuar` o `lanzar` en el mismo bloque (v1.49).
- `redundant-pasar` — `pasar` dentro de un bloque que tiene otras sentencias (v1.49).
- `eq-nulo` — comparación con `nulo` usando `==`/`!=` (sugiere `es nulo` / `no es nulo`) (v1.49).
- `unused-import` — módulo importado pero nunca referenciado en el programa (v1.49).
- `unused-local` — variable local asignada en cuerpo de función pero nunca leída (v1.50).
- `unused-param` — parámetro de función nunca usado, exceptuando `yo`, nombres con `_` inicial, `*args` y `**kwargs` (v1.50).

El análisis de scope (v1.50) respeta closures: si una función anidada captura una variable del cuerpo enclosing con `nolocal`, la outer se marca como usada. Lo mismo aplica al destructuring (`a, b = par`): cada nombre se analiza por separado.

Exit 0 sin avisos, 1 si los hay — apto para `pre-commit` y CI.

### ¿Hay generador de documentación?

Sí desde v1.51:

```bash
./build/cornamusa docs stdlib/matematicas.cor                  # imprime Markdown a stdout
./build/cornamusa docs stdlib/matematicas.cor -o mat.md        # escribe a archivo
```

El generador:

- Usa el basename del archivo como título H1 del módulo.
- El bloque de comentarios `#` al inicio del archivo se trata como **doc del módulo**.
- Cada `funcion` top-level genera una sección H2 con su firma (incluye `*args`, `**kwargs` y defaults como `=...`).
- Cada `clase` top-level genera una sección H2; sus métodos aparecen como subsecciones H3.
- El bloque de comentarios `#` **inmediatamente** anterior a un item (sin línea en blanco intermedia) se asocia como su doc.

Convención Go-style: los comentarios de doc preceden a la declaración, sin línea en blanco. Una línea en blanco corta la asociación — útil para distinguir comentarios "de grupo" de docstrings per-item.

### ¿Hay integración con editores (VS Code, Neovim, etc.)?

Sí desde v1.52 vía LSP:

```bash
./build/cornamusa lsp     # arranca el Language Server Protocol por stdio
```

El servidor implementa el subset mínimo del [LSP](https://microsoft.github.io/language-server-protocol/) necesario para diagnostics en tiempo real:

- `initialize` / `initialized` / `shutdown` / `exit`
- `textDocument/didOpen` / `didChange` / `didClose`
- `textDocument/publishDiagnostics` — emite los avisos del linter como diagnostics LSP (severities, ranges, codes con la categoría: `unreachable`, `unused-local`, etc.).

Para conectar desde VS Code necesitas un cliente LSP (extensión genérica como [generic-lsp-client](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.lldb-dap)) configurado para ejecutar `cornamusa lsp` con `*.cor` como tipo de archivo. Documentación detallada en [docs/editor-setup.md](docs/editor-setup.md) (próxima release).

**Capacidades desde v1.53**:
- `hoverProvider`: pasar el cursor sobre una función o clase top-level muestra firma + comentarios doc + lista de métodos.
- Parse errors detallados (línea/col/mensaje del parser tal cual).

**Limitaciones actuales**:
- Hover solo funciona para símbolos top-level (funciones y clases). Variables, parámetros, atributos y métodos en uso aún no.
- Sin completion, goto-definition, ni formatting via LSP (queda para v1.54+).
- Document sync solo en modo completo (no incremental).

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

### ¿Funcionan los closures con escritura?

Sí. Una función anidada puede **leer** las variables del scope que la envuelve directamente, y para **escribirlas** las declara con `nolocal`:

```cornamusa
funcion crear_contador():
    n = 0
    funcion siguiente():
        nolocal n
        n = n + 1
        retornar n
    fin funcion
    retornar siguiente
fin funcion

contar = crear_contador()
imprimir(contar(), contar(), contar())   # 1 2 3
```

Sin `nolocal`, una asignación dentro de la función anidada crea un local nuevo en lugar de modificar la variable capturada.

### ¿Cómo modifico una variable global desde dentro de una función?

La keyword `global` está reservada pero **aún no está implementada** en la VM bytecode. Mientras tanto, si necesitas estado mutable compartido, usa un contenedor mutable (lista o diccionario) a nivel de módulo:

```cornamusa
estado = {"contador": 0}

funcion incrementar():
    estado["contador"] = estado["contador"] + 1   # muta el dict, no reasigna
fin funcion
```

O reestructura para que la función **devuelva** el nuevo valor y el llamador lo reasigne.

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

David Castilla Gómez como autor único. Contribuciones externas bienvenidas siguiendo [CONTRIBUTING.md](CONTRIBUTING.md).

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
| v0.4-v0.5 — Sintaxis + estructuras de datos | ✅ |
| v0.6-v0.9 — VM bytecode, clases, GC, módulos | ✅ |
| v0.10-v0.11 — Inline caching + small-int tagging | ✅ |
| v1.0 — Documentación + sitio web + estabilidad | ✅ |
| v1.2-v1.16 — Dunders, `nolocal`, context managers, pattern matching | ✅ |
| v1.21-v1.34 — Destructuring, `*args`/`**kwargs`, comprehensions, generadores | ✅ |
| v1.35-v1.40 — Sugerencias de error, traceback, `--check`, `-O3`+LTO | ✅ |
| Próximo | Tooling (formateador, depurador, LSP) |
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
