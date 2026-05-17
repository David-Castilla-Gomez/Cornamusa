# Preguntas frecuentes

> Lo que un usuario nuevo o un programador curioso se preguntaría sobre Cornamusa. Si tu duda no está aquí, [abre un issue](https://github.com/David-Castilla-Gomez/Cornamusa/issues).

---

## Sobre el lenguaje

### ¿Por qué un lenguaje en castellano?

La mayoría de lenguajes de programación están en inglés y eso pone una barrera invisible a quien aprende a programar sin dominar el inglés. Cornamusa demuestra que un lenguaje moderno y razonablemente rápido puede tener su sintaxis, built-ins y mensajes de error completamente en castellano sin sacrificar nada importante.

No es una traducción literal de Python: las palabras clave se eligieron para que **suenen idiomáticas a un hispanohablante** (`y`/`o`/`no`, `es`, `en`, `extiende`, `intentar`/`atrapar`, etc.) — no `and`/`or`/`not`/`is` con tilde.

### ¿Es un toy language o se puede usar en serio?

Es funcional para programas reales: OOP completo con dunders, closures con `nolocal`, pattern matching, generadores, comprehensions, GC, excepciones con traceback, módulos y una stdlib de dieciséis módulos (`archivos`, `json`, `csv`, `base64`, `hashing`, `jwt`, `regex`, `fechas`, `azar`, `proceso`, `red`...). Sirve bien para **enseñar a programar en castellano** y para scripting pequeño/mediano.

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
- `shadow` — local que sombrea una variable del scope exterior (v1.55). `nolocal`/`global` no warnean (son intencionales). `_` se omite.
- `unused-loop-var` — `para X en ...:` donde X no se referencia en el cuerpo ni después (v1.55). Convención: usar `_` para descartes.
- `mutable-default` — defaults `=[]`, `={}`, `={1,2}` (v1.55). Estos literales se evalúan una sola vez al definir la función y se comparten entre llamadas — bug clásico Python.
- `concat-in-loop` — `x = x + ...` o `x += ...` dentro de `mientras`/`para` cuando RHS es claramente cadena (literal o f-cadena) (v1.63). Detecta el patrón O(n²) que motivó las optimizaciones de v1.61-v1.62. Sugiere usar lista + `cadena_unir`. Heurística conservadora: skip si RHS no es claramente string-like, para evitar falsos positivos en acumuladores numéricos.
- `same-comparison` — `x == x`, `x != x`, `x < x`, etc. con el mismo identificador en ambos lados (v1.68). Siempre verdadero o siempre falso; generalmente un typo del programador queriendo otra variable. Skip rule: llamadas a funciones (`g() == g()`) no warnean por posibles side-effects. Si lo quieres deliberadamente (demo de un dunder, NaN check), suprime con `# noqa: same-comparison`.
- `empty-except` — cláusula `atrapar` con cuerpo vacío o solo `pasar` (v1.69). Anti-patrón clásico: silencia el error sin tratarlo, dificultando debugging. Si genuinamente quieres ignorar la excepción (por ejemplo, cleanup best-effort), documenta y suprime con `# noqa: empty-except`.

**Supresión selectiva (v1.64)**: cualquier warning puede silenciarse en su línea con `# noqa: <categoria>`:

```cornamusa
importar fechas              # noqa: unused-import     ← silencia ese warning
funcion f(x=[]):             # noqa: mutable-default    ← caso didáctico
    retornar x
fin funcion

# Multiples categorias separadas por coma:
funcion g(a, b):             # noqa: unused-param, shadow
    retornar 1
fin funcion

# Bare noqa silencia TODAS las categorías:
codigo_legado()              # noqa
```

Útil para casos didácticos (como `examples/42_defaults.cor` que demuestra el footgun de `mutable-default` intencionalmente), código generado, o trade-offs deliberados.

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

**Capacidades anunciadas** (v1.54):
- `textDocumentSync: 1` (sincronización full-document, no incremental).
- `hoverProvider`: cursor sobre función/clase top-level → popup con firma + doc + métodos.
- `definitionProvider`: Ctrl-click sobre una referencia a función/clase salta a su declaración.
- `documentFormattingProvider`: "Format Document" del editor reformatea reusando el `cornamusa fmt` interno.
- `publishDiagnostics` con parse errors detallados + warnings del linter.

**Limitaciones actuales**:
- Hover y goto-def solo funcionan para símbolos top-level (funciones y clases). Variables, parámetros, atributos y métodos `obj.metodo` aún no.
- Sin completion (`textDocument/completion`).
- Document sync solo en modo completo.

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

Desde **v1.57**, declarando `global X` al inicio del cuerpo:

```cornamusa
contador = 0

funcion incrementar():
    global contador
    contador += 1
fin funcion

incrementar()
incrementar()
imprimir(contador)   # 2
```

`global` también funciona si la variable **no existía antes** — la primera asignación la crea a nivel módulo:

```cornamusa
funcion configurar():
    global config
    config = {"modo": "produccion"}
fin funcion

configurar()
imprimir(config)
```

Validaciones del compilador:
- `global` fuera de función → `ErrorDeSintaxis`.
- `global X` cuando X ya es local del scope actual → `ErrorDeSintaxis` (contradictorio).
- `global X` cuando X ya es `nolocal` → `ErrorDeSintaxis` (solo uno o el otro).

Antes de v1.57 había que usar un contenedor mutable a nivel módulo (`estado = {"contador": 0}; estado["contador"] += 1`) — sigue funcionando, pero `global` es más limpio para estado primitivo.

### ¿Cómo trabajo con archivos CSV?

Desde **v1.58**, el módulo `csv` cubre lectura/escritura RFC 4180-like:

```cornamusa
importar csv

# Parsear texto a lista de filas:
filas = csv.parsear("a,b,c\n1,2,3")
# → [["a", "b", "c"], ["1", "2", "3"]]

# Separador alternativo:
filas = csv.parsear("a;b;c", ";")

# Lectura/escritura de archivos:
filas = csv.leer("entrada.csv")
csv.escribir("salida.csv", [["nombre", "edad"], ["Ana", 30]])

# Round-trip con escape automatico:
datos = [["queso, manchego", "lo dice \"el experto\""]]
csv.parsear(csv.serializar(datos))   # devuelve `datos` igual
```

Soporta:
- Campos quoted con comas/comillas/saltos de línea internos.
- Escape de `"` dentro de quoted como `""`.
- Separadores configurables (`,`, `;`, `\t`, ...).
- Tanto `\n` como `\r\n` como separador de líneas en parse; escribe siempre con `\n`.

**No** infiere tipos: todos los campos vienen como cadena. Conviértelos tú (`entero(campo)`, `decimal(campo)`).

### ¿Cómo codifico/decodifico Base64?

Desde **v1.59** con el módulo `base64`:

```cornamusa
importar base64

# Codificar (RFC 4648 estándar):
encoded = base64.codificar("Cornamusa")
# → "Q29ybmFtdXNh"

# Decodificar:
texto = base64.decodificar("SG9sYSBtdW5kbw==")
# → "Hola mundo"

# HTTP Basic Auth típico:
creds = "usuario:contraseña"
header = "Authorization: Basic " + base64.codificar(creds)
```

Cobertura:
- Alfabeto estándar `A-Z a-z 0-9 + /` con padding `=`.
- Round-trip verificado con los 7 test vectors de RFC 4648 §10.
- Decodificador tolerante a whitespace (espacios, `\n`, `\r`, `\t`) — útil para input MIME-style con line-wrap.
- Errores atrapables: `ErrorDeValor` si la entrada tiene caracteres fuera del alfabeto, padding inválido, o longitud no múltiplo de 4.

**Variante URL-safe** (desde v1.66, RFC 4648 §5):

```cornamusa
# `-_` en vez de `+/`, sin padding `=` — listo para JWTs, OAuth:
url_token = base64.codificar_url("usuario:42")    # "dXNlYXJpbzo0Mg"

# El decoder es tolerante a ambas variantes:
base64.decodificar("dXNlYXJpbzo0Mg")    # también funciona sin padding
```

Implementación nativa en C — rápida incluso para inputs grandes.

### ¿Cómo calculo un hash SHA-256 o MD5?

Desde **v1.60** con el módulo `hashing`:

```cornamusa
importar hashing

hashing.sha256("hola mundo")
# → "0b894166d3336435c800bea36ff21b29eaa801a52f584c006c49289a0dcf6e2f"

hashing.md5("hola mundo")
# → "0ad066a5d29f3f2a2a1c7c17dd082a79"

# Checksum de un archivo:
importar archivos
checksum = hashing.sha256(archivos.leer("documento.pdf"))
```

Ambos algoritmos están implementados nativamente en C (rápidos) y validados contra los test vectors canónicos:
- **SHA-256**: FIPS 180-4 / RFC 6234, incluyendo el vector de 1 millón de "a".
- **MD5**: RFC 1321 §A.5.

**HMAC** (desde v1.65, RFC 2104 / RFC 4231): autenticación de mensajes con clave secreta. Útil para JWT signing, webhooks, sesiones.

```cornamusa
firma = hashing.hmac_sha256("clave-secreta", "user_id=42")
# → "410434e1769746896abe17c47551c15911c0cf2a3a9e1397cfd3c3bf69dc24c2"

# Verificación: re-computar con misma clave da el mismo digest.
firma_verif = hashing.hmac_sha256("clave-secreta", "user_id=42")
verifica = (firma == firma_verif)   # verdadero

# Con clave equivocada → digest distinto (autenticidad):
falso = hashing.hmac_sha256("clave-mala", "user_id=42")
diferente = (falso != firma)        # verdadero
```

Disponible también `hashing.hmac_md5(clave, mensaje)`. HMAC-MD5 sigue siendo **seguro como MAC** (no como hash plano), por la naturaleza del esquema HMAC.

**Notas de seguridad**:
- **MD5 está criptográficamente roto desde 2004** para hashing simple. Pero **HMAC-MD5 sigue siendo seguro** como MAC.
- **SHA-256** sigue considerado seguro para integridad y como parte de protocolos (HMAC, TLS, Bitcoin). **Para hashes de passwords** usa scrypt/argon2 — no provistos por Cornamusa.

Lo que no incluye: SHA-1 (obsoleto), SHA-384/512, SHA-3, hashing incremental.

### ¿Hay soporte de JWT (JSON Web Tokens)?

Sí desde v1.67 con el módulo `jwt`:

```cornamusa
importar jwt

# Firmar:
token = jwt.codificar({"sub": "42", "exp": 1735689600}, "mi-secreto")
# → "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiI0MiIs...firma"

# Verificar y leer:
intentar:
    payload = jwt.decodificar(token, "mi-secreto")
    imprimir(payload["sub"])
atrapar ErrorDeValor como e:
    imprimir("Token inválido:", e)
fin intentar

# Atajo booleano sin try/except:
si jwt.verificar(token, "mi-secreto"):
    # ...continuar autenticado
fin si
```

**Pure-Cornamusa**: el módulo entero son ~80 líneas que combinan `json` + `base64.codificar_url` + `hashing.hmac_sha256_bytes`. Demuestra que las stdlib previas forman una suite coherente.

**Algoritmo soportado**: solo `HS256` (HMAC-SHA-256). RS256/ES256 requieren criptografía de clave pública (no provista por Cornamusa).

**Validación de claims**: `decodificar()` valida solo la firma, NO `exp`/`nbf`/`iat`. La política de expiración es responsabilidad del código cliente (compara `payload["exp"]` con tu reloj).

**`alg=none` (RFC 7519 §6.1)** NO se acepta — mitigación estándar contra ataques de algorithm confusion.

### ¿Funciona `borrar d[k]` y `obj.x += 1`?

Sí desde v1.56:

```cornamusa
# `borrar` para diccionarios, listas, conjuntos e instancias:
d = {"a": 1, "b": 2}
borrar d["a"]

lst = [10, 20, 30]
borrar lst[1]               # quita el indice 1, desplaza el resto

conj = {1, 2, 3}
borrar conj[2]

obj.cache = "data"
borrar obj.cache             # quita el atributo de la instancia

# Aug-assign sobre atributos (todos los operadores: +=, -=, *=, /=, //=, %=, **=):
funcion incrementar(yo, n=1):
    yo.contador += n          # antes habia que escribir: yo.contador = yo.contador + 1
fin funcion
```

Errores atrapables: `ErrorDeClave` (clave inexistente en dict/conjunto), `ErrorDeIndice` (índice fuera de rango en lista), `ErrorDeAtributo` (atributo no presente en instancia), `ErrorDeTipo` (objeto no soporta `borrar`).

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
