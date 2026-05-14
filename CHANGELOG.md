# Registro de cambios

Todos los cambios notables a este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto adhiere a [Versionado Semántico](https://semver.org/lang/es/).

## [No publicado]

## [1.32.0] — 2026-05-14 — Fix: comprehensions en bucles y top-level

Release de pulido. Levanta las dos limitaciones documentadas de v1.30:
las comprehensions ahora funcionan **dentro de bucles** y **a
top-level**, sin workarounds.

### Antes (v1.30/v1.31)

```cornamusa
# ❌ Fallaba: "OP_ITER_SIGUIENTE sin iterador en slot N"
funcion f():
    para n en [1, 2, 3]:
        sub = [x + n para x en [10, 20]]   # comprehension en bucle
    fin para
fin funcion

# ❌ Rechazado en compilación
x = [n * 2 para n en [1, 2, 3]]            # comprehension top-level
```

### Ahora (v1.32)

```cornamusa
# ✅ Funciona
funcion primos(lim):
    ps = []
    para n en rango(2, lim):
        divs = [d para d en rango(2, n) si n % d == 0]
        si longitud(divs) == 0:
            agregar(ps, n)
        fin si
    fin para
    retornar ps
fin funcion
primos(30)  # [2, 3, 5, 7, 11, 13, 17, 19, 23, 29]

# ✅ Funciona
x = [n * 2 para n en [1, 2, 3]]   # [2, 4, 6]
```

### Causa raíz

El bug v0.11.5b "pre-reservar locales del cuerpo del `para`" colocaba
los `OP_NULO` de pre-reserva **dentro** del cuerpo del loop (entre
`inicio_loop` y `emitir_bucle`). Cada iteración los re-ejecutaba,
empujando un valor que nunca se compensaba → **el stack crecía +1 por
vuelta**.

Para variables simples era inocuo (el slot fijo se sigue leyendo bien
aunque haya basura encima). Pero rompía cualquier cosa que dependa de
`tope == n_locales`: una comprehension anidada hacía `OP_ITER_INICIAR`
sobre un stack desplazado, y su `$comp_iter` quedaba en un slot
distinto del que `OP_ITER_SIGUIENTE` leía → `"OP_ITER_SIGUIENTE sin
iterador en slot N"`.

### Fix

1. **`compilar_para`**: mover `pre_reservar_locales` ANTES de capturar
   `inicio_loop`. Los `OP_NULO` se ejecutan una sola vez, fuera del
   loop. (`compilar_mientras` ya lo hacía bien — solo `compilar_para`
   tenía el orden invertido.)
2. **`EXPR_COMPREHENSION`**: unificar el path de compilación para usar
   `agregar_local` SIEMPRE (función y top-level), igual que
   `compilar_para` hace con `$iter`. Esto elimina la rama especial de
   top-level que rechazaba la comprehension.

### Tests añadidos

- `test_bytecode_comprehensions.c`: `test_toplevel_funciona` (antes
  `test_toplevel_rechazado`, invertido), `test_comprehension_en_bucle`,
  `test_comprehension_en_bucle_con_guarda` (primos).
- `examples/55_comprehensions.cor`: el caso 6 ahora demuestra
  comprehension dentro de un bucle directamente (antes usaba el
  workaround de función auxiliar).
- **178/178 tests verde**.

## [1.31.0] — 2026-05-14 — Generadores con `producir`

Cierra **Fase 3 — generadores y comprehensions**. Una función que
contiene `producir` se convierte en **generador**: llamarla NO ejecuta
el cuerpo, sino que devuelve un objeto suspendible. Iterar con
`para...en...` reanuda hasta el próximo `producir` o `retornar`. El
estado (locales, IP, expresiones a medias) se preserva entre yields.

El reto técnico mayor del roadmap completado en una sola release.

### Sintaxis

```cornamusa
funcion contar(ini, tope):
    i = ini
    mientras i <= tope:
        producir i           # suspende, retorna i, IP guardado
        i = i + 1
    fin mientras
fin funcion

para v en contar(1, 5):
    imprimir(v)   # 1, 2, 3, 4, 5
fin para
```

### Generador infinito

Sin problema — `romper` desde el `para` lo agota correctamente:

```cornamusa
funcion fib():
    a = 0
    b = 1
    mientras verdadero:
        producir a
        c = a + b
        a = b
        b = c
    fin mientras
fin funcion

i = 0
para n en fib():
    si i >= 10: romper fin si
    imprimir("fib", i, "=", n)
    i = i + 1
fin para
```

### Pipelines lazy

Los generadores **se componen**: pasar el resultado de uno a otro
construye un pipeline lazy completo. El valor producido en cada paso
fluye uno a uno sin materializar listas intermedias:

```cornamusa
funcion pares_naturales():
    n = 0
    mientras verdadero:
        producir n * 2
        n = n + 1
    fin mientras
fin funcion

funcion filtrar(gen, pred):
    para v en gen:
        si pred(v):
            producir v
        fin si
    fin para
fin funcion

funcion tomar(gen, n):
    r = []
    i = 0
    para v en gen:
        si i >= n: romper fin si
        agregar(r, v)
        i = i + 1
    fin para
    retornar r
fin funcion

es_par = lambda x: x % 2 == 0
imprimir(tomar(filtrar(fib(), es_par), 5))
# [0, 2, 8, 34, 144]  — tres generadores encadenados
```

### Implementación técnica

**AST**: `SENT_PRODUCIR { Expr *valor }` parseado por el parser.
`FuncionBC.es_generador` se marca al ver `OP_PRODUCIR` en el cuerpo.

**Opcode nuevo**: `OP_PRODUCIR` pop el valor del TOS y, si la VM está
en `modo_yield`, suspende el dispatch retornando `VM_OK_YIELD`.

**Tipo nuevo**: `VAL_GENERADOR` con `struct Generador`:

```c
struct Generador {
    GCObject obj;
    Closure *closure;
    Valor *stack_buf;       /* heap snapshot del frame stack */
    int stack_n, stack_cap;
    int ip_offset;          /* offset desde inicio del chunk */
    bool agotado, ejecutando;
    int refcount;
};
```

**Sub-VM** ([src/vm.c](src/vm.c)):

1. `vm_ejecutar_dispatch_impl(vm, chunk, out, inicial)` — el loop
   refactorizado para aceptar entrada `inicial=false` (sub-dispatch
   que no resetea estado VM).
2. `vm_generador_paso(vm, gen, out)`:
   - Restaura `gen->stack_buf` al stack VM en el tope.
   - Push CallFrame con `ip = gen->ip_offset`.
   - Set `frame_techo = n_frames`, `modo_yield = true`.
   - Llama al sub-dispatch.
   - `VM_OK_YIELD`: snapshot stack al gen, save ip, pop frame, retorna
     valor producido.
   - `VM_OK_GEN_AGOTADO`: marca agotado, retorna EOF.
3. `OP_RETORNAR` chequea `n_frames < frame_techo` y retorna
   `VM_OK_GEN_AGOTADO` si aplica.
4. `OP_ITER_INICIAR` detecta `VAL_GENERADOR` y lo deja en slot directo
   (no construye `Iterador` envoltorio).
5. `OP_ITER_SIGUIENTE` despacha por tipo del slot: si es generador,
   llama `vm_generador_paso`; sino, `iter_siguiente` clásico.

**GC**: `GC_TIPO_GENERADOR` con marcado que recurre sobre
`gen->closure` y cada valor en `gen->stack_buf`. Garantiza que el GC no
recolecta objetos referenciados por un generador suspendido.

### Estado preservado

El frame stack se SERIALIZA en `gen->stack_buf` al producir, y se
RESTAURA al reanudar. Esto incluye:
- Args de la llamada original (en slots fijos).
- Locales (variables del cuerpo).
- Expresiones a medias (si el yield ocurre durante una expresión
  compuesta — el compilador garantiza que `producir EXPR` solo
  cambia tope en +0 al final).

### Limitaciones (v1.31)

- **Solo generadores planos**. Sin `producir desde` para
  sub-generadores (v1.31.x si surge demanda).
- **Sin `enviar()`** — generadores bidireccionales con `gen.enviar(x)`
  vendrán solo si los pide alguien.
- **Sin generator expressions** `(x * 2 para x en xs)` inline — el
  parser solo reconoce `(...)` como tupla/grupo. Workaround: definir
  la función explícitamente. Sintaxis inline → v1.32.

### Tests añadidos

- `tests/unit/test_bytecode_generadores.c` — 8 tests cubriendo
  llamada devuelve VAL_GENERADOR, iteración simple, generador con
  estado, generador con args, fib infinito con `romper`, agotado,
  vacío, `producir` top-level rechazado.
- `bc_run_56_generadores` — ejecuta `examples/56_generadores.cor`
  (contar perezoso, Fibonacci, pipeline tomar/filtrar, agotado).
- **178/178 tests verde**.

### Estado final del roadmap v1.21 → v1.31

11 releases consecutivas. **Cornamusa ahora tiene**:
- **Fase 1** (sintaxis idiomática moderna): destructuring, `*args`,
  keyword arguments, `**kwargs`, spread `**dict`.
- **Fase 2** (stdlib esencial): azar (PRNG), proceso (procesos
  externos), regex (motor propio), red (HTTP/1.1).
- **Fase 3** (comprehensions + generadores): list/dict/set
  comprehensions, generadores con `producir` y pipelines lazy.

**Paridad sintáctica y semántica con Python 3.10+** para la mayoría
de scripts de propósito general.

## [1.30.0] — 2026-05-14 — Comprehensions (list, dict, set)

Arranca **Fase 3 — generadores y comprehensions**. Sintaxis
Python-paritaria para construir colecciones idiomáticamente:

```cornamusa
funcion ejemplos():
    # List comprehension
    dobles = [n * 2 para n en rango(10)]
    pares = [n para n en rango(20) si n % 2 == 0]

    # Dict comprehension
    cuadrados = {n: n * n para n en rango(1, 6)}
    iniciales = {w: w[0] para w en ["alfa", "beta", "gamma"]}

    # Set comprehension (deduplica)
    primeras = {w[0] para w en ["alfa", "abeja", "barba", "casa"]}

    # Con expresiones complejas
    digitos = [regex.extraer("\\d+", l) para l en lineas]
fin funcion
```

### Sintaxis

```
[ EXPR para VAR en ITERABLE [si GUARDA] ]      # lista
{ EXPR para VAR en ITERABLE [si GUARDA] }      # conjunto (deduplica)
{ CLAVE: VAL para VAR en ITERABLE [si GUARDA] } # diccionario
```

- `VAR` es un identificador simple (sin destructuring en v1.30).
- `GUARDA` es opcional; si está, solo se incluye el elemento cuando es
  verdadera.
- Solo **un `para...en...`** (sin nested loops en v1.30.x).

### Implementación

AST extendido en [src/ast.h](src/ast.h):

```c
EXPR_COMPREHENSION { tipo_destino, expr_elem, expr_valor,
                      nombre_var, longitud_var, iterable, guarda }
```

`tipo_destino`: 0=lista, 1=dict, 2=conjunto.

El parser detecta `para` tras la primera expresión (o par `k: v` en
dict) dentro de `[` o `{` y construye un `EXPR_COMPREHENSION` en lugar
del literal correspondiente.

El compilador desugar a bytecode equivalente a un bucle `para`:

1. `OP_BUILD_LISTA/DICC/CONJUNTO 0` — acumulador vacío.
2. Eval iterable + `OP_ITER_INICIAR`.
3. Loop: `OP_ITER_SIGUIENTE` → asignar a var.
4. Si guarda: eval + `OP_SALTAR_SI_FALSO`.
5. Eval expr_elem (+ expr_valor para dict) → `OP_LISTA_AGREGAR` /
   `OP_CONJUNTO_AGREGAR` / `OP_DICC_AGREGAR_PAR`.
6. Salta al inicio.
7. **Limpieza crítica** (clave para evitar slots muertos en bucles):
   `OP_OBTENER_LOCAL slot_acc` + `OP_ASIGNAR_LOCAL slot_acc` +
   2× `OP_DESCARTAR` para dejar el acumulador como TOS y descartar
   slot_iter / slot_var. `n_locales -= 3`.

### Opcode nuevo

- `OP_CONJUNTO_AGREGAR` — TOS=valor, debajo=conjunto. Pop valor,
  agregar. Verifica hashable; `RAISE_OR_DIE` si no.

### Limitaciones (v1.30)

- **Solo dentro de funciones**. En top-level se rechaza con
  `ErrorDeCompilacion: comprehensions solo soportadas dentro de
  funciones en v1.30` (problema de slots: top-level no tiene scope
  formal para reservar el acumulador). Workaround: envolver en
  función.
- **Comprehensions dentro de bucles**: las pasada multipasada
  comparte slots de iteración pero la lógica de "agotar iterador"
  puede confundirse si la comprehension está dentro de un bucle
  externo en la misma función. Workaround: extraer a función
  auxiliar.
- **Sin nested loops**: `[x para x en xs para y en ys]` no soportado.
- **Sin destructuring**: `para k, v en pares.items()` no soportado.
- **Sin generator expressions** `(...)`: requiere generadores
  (v1.31).

### Tests añadidos

- `tests/unit/test_bytecode_comprehensions.c` — 13 tests cubriendo
  list/dict/set con y sin guarda, iterar cadenas, dedup en sets,
  rechazo top-level.
- `bc_run_55_comprehensions` — ejecuta `examples/55_comprehensions.cor`
  (cuadrados, filtros, iniciales por palabra, extracción regex,
  cálculo de primos vía función auxiliar).
- **175/175 tests verde**.

## [1.29.0] — 2026-05-14 — Stdlib `red` (cliente HTTP/1.1)

Cierra **Fase 2 — stdlib esencial**. Cliente HTTP/1.1 plano (sin TLS)
cross-platform: WinSock2 en Windows, BSD sockets en POSIX. Cubre
GET sobre URLs `http://host[:puerto]/path`. Cubre el 80% de casos:
APIs internas, health-checks, scraping simple en LAN.

### API del módulo `red`

```cornamusa
importar red

# GET básico
r = red.obtener("http://httpbin.org/get")
imprimir(r["codigo"])      # 200
imprimir(r["cabeceras"])   # "Content-Type: application/json\r\n..."
imprimir(r["cuerpo"])      # cuerpo de la respuesta

# Con cabeceras extra y timeout
r = red.obtener("http://api.local/datos",
                "X-Token: secreto\r\nAccept: text/csv\r\n",
                timeout=5)

# Atajo: solo cuerpo, lanza ErrorDeSistema si codigo != 2xx
texto = red.descargar_cuerpo("http://httpbin.org/uuid")

# Parsear cabeceras a dict
cabs = red.parsear_cabeceras(r["cabeceras"])
imprimir(cabs["Content-Type"])
```

### Sintaxis de URL

```
http://host[:puerto][/path]
```

- **Scheme**: solo `http://`. `https://` rechazado con
  `ErrorDeSistema: URL debe empezar con 'http://'`.
- **Host**: nombre o IPv4 literal. Resolución DNS via `getaddrinfo`.
- **Puerto**: opcional, default 80.
- **Path**: opcional, default `/`.

### Built-in primitivo

`nativos.c` añade:

```c
red_http_obtener(url, cabeceras_extra=nulo, timeout=10) →
    {"codigo": int, "cabeceras": str, "cuerpo": str}
```

- `cabeceras_extra`: cadena con CRLFs (`"Nombre: valor\r\n..."`),
  añadidas al request además de las obligatorias.
- `timeout` en segundos, default 10.

### Implementación cross-platform

[src/red.c](src/red.c) — ~320 líneas C:

**Windows (WinSock2)**:
- `WSAStartup` lazy.
- `socket` + `connect` + `setsockopt(SO_RCVTIMEO/SO_SNDTIMEO)`.
- `send`/`recv` en bucle.
- Linker: `target_link_libraries(cornamusa PRIVATE ws2_32)`.

**POSIX (BSD sockets)**:
- `socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)`.
- `getaddrinfo` para DNS.
- `setsockopt(SO_RCVTIMEO/SO_SNDTIMEO)` con `struct timeval`.

### Parseo de respuesta

- Status line: `HTTP/1.x <CODE> <reason>\r\n` → extrae codigo.
- Cabeceras: hasta `\r\n\r\n` (o `\n\n` LF only) → cadena cruda.
- Body: lo restante. Tope 16 MB.

`parsear_cabeceras(raw)` en `stdlib/red.cor` convierte la cadena
cruda en dict `{nombre: valor}` con trim de espacios.

### Errores atrapables

- `ErrorDeSistema: URL debe empezar con 'http://'` — scheme no soportado.
- `ErrorDeSistema: getaddrinfo('host') fallo` — DNS o host inexistente.
- `ErrorDeSistema: connect(...) fallo` — conexión rechazada/timeout.
- `ErrorDeSistema: respuesta no es HTTP` — servidor habla HTTP/0.9 u
  otro protocolo.
- `ErrorDeSistema: HTTP 500 al GET <url>` — `descargar_cuerpo()` con
  status != 2xx.

### Limitaciones (v1.29)

- **Solo GET** — POST/PUT/DELETE en v1.29.x.
- **Solo HTTP plano** — HTTPS aplazado (requiere libtls/OpenSSL).
- **Sin redirecciones** automáticas — 3xx llega al caller.
- **Sin keep-alive** — cada request nueva conexión + DNS.
- **Sin Transfer-Encoding: chunked** — asume Content-Length o EOF.
- **Sin compresión** (gzip).
- **Respuesta máxima 16 MB**.

Para HTTPS, scraping moderno, sesiones autenticadas o anti-bots,
usar fetch-toolkit externo (separate dependency).

### Tests añadidos

- `tests/unit/test_bytecode_red.c` — 6 tests de validación de
  argumentos y manejo de errores **sin red** (URLs malformadas,
  HTTPS rechazado, DNS fallo, timeout inválido).
- `bc_run_54_red` — ejemplo `examples/54_red.cor` haciendo requests
  reales a `httpbin.org`. **NO se ejecuta por defecto** (requiere
  internet); activar con `cmake -DCORNAMUSA_TEST_RED=ON ..`.
- **172/172 tests verde** (los 6 unitarios cuentan; el bc_run_54 es
  opt-in).

### Estado de Fase 2

Con v1.29 se cierra la **Fase 2 — stdlib esencial**:

| Versión | Módulo | Estado |
|---|---|---|
| v1.26 | `azar` (PRNG xoshiro256\*\*) | ✅ |
| v1.27 | `proceso` (procesos externos cross-platform) | ✅ |
| v1.28 | `regex` (motor backtracking propio) | ✅ |
| v1.29 | `red` (HTTP/1.1 plano cross-platform) | ✅ |

**Cornamusa tiene una stdlib útil para scripting real**. Siguiente:
**Fase 3 — generadores y comprehensions** (v1.30+).

## [1.28.0] — 2026-05-13 — Stdlib `regex` + fix destructuring en función

Tercer módulo de **Fase 2 — stdlib esencial**. Motor de expresiones
regulares con backtracking propio (~500 líneas C, sin dependencias).
Subset acotado pero suficiente para validación de formato, búsqueda y
reemplazo.

### API del módulo `regex`

```cornamusa
importar regex

regex.coincide("\\d+", "123")              # fullmatch → verdadero
regex.coincide("\\d+", "abc 123 def")      # fullmatch → falso

regex.buscar("\\d+", "abc 123 def")        # primera ocurrencia → (4, 7)
regex.contiene("\\d", "abc")               # → falso
regex.extraer("[A-Z]\\w+", "hola Cornamusa")  # primer match → "Cornamusa"

regex.todos("\\w+", "hola mundo amigo")    # → ["hola", "mundo", "amigo"]

regex.reemplazar("\\d+", "ano 2026", "N")  # → "ano N"
```

### Sintaxis soportada

| Construcción | Significado |
|---|---|
| `abc` | literales |
| `.` | cualquier carácter (excepto `\n`) |
| `\.`, `\\`, `\n`, `\t`, `\r` | escapes |
| `*`, `+`, `?` | quantifiers greedy (0+, 1+, 0/1) |
| `^`, `$` | anclas de inicio/fin |
| `[abc]`, `[^abc]`, `[a-z]` | clases de carácter |
| `\d \D \w \W \s \S` | clases predefinidas (ASCII) |
| `a\|b\|c` | alternancia |
| `(?:...)` o `(...)` | grupos no-captura (estructurales) |

**NO soportado en v1.28**: backreferences `\1`, lookahead, `\b`,
quantifiers explícitos `{n,m}`, lazy `*?`, captura de grupos.

### Implementación

[src/regex.c](src/regex.c) — parser-to-AST + matcher con
*continuation-passing style*:

```c
match_nodo(nodo, resto, texto, pos)
  → -1 si falla, o posición tras matchear nodo+resto si OK
```

El `resto` (continuation) permite que los quantifiers ambiciosos
hagan backtracking correctamente. Las clases usan bitmasks de 256
bits para test O(1).

### Excepciones atrapables

Patrón sintácticamente inválido lanza `ErrorDeValor` con la posición:

```cornamusa
intentar:
    regex.coincide("[abc", "abc")
atrapar ErrorDeValor como e:
    imprimir(e)
    # ErrorDeValor: patron regex invalido: clase '[...]' sin cerrar en pos 4
fin intentar
```

### Bug fix incluido: destructuring dentro de función

Pre-v1.28: `ini, fin = par` dentro de una función definía mal los
locales. El slot anónimo del valor a destructurar quedaba **bajo** los
slots de los destinos en el stack, y el `OP_DESCARTAR` final
descartaba el último destino en vez del slot anónimo → corrupción de
los locales (`ErrorDeNombre` al usarlos).

```cornamusa
# Pre-v1.28 (bug):
funcion partir(par):
    a, b = par
    retornar a + b   # ErrorDeNombre: 'b' no esta definido
fin funcion
```

Fix en `emitir_destructuring`: cuando estamos dentro de una función,
**pre-reservar slots** para cada destino IDENT con `OP_NULO` ANTES de
evaluar el RHS, y luego usar `OP_ASIGNAR_LOCAL` (pop) para rellenarlos.
Los destinos anidados (`(a, (b, c)) = ...`) siguen el path recursivo.
El slot anónimo del iterador queda como local "muerto" hasta el final
del frame — coste de un slot extra, aceptable.

El destructuring a top-level seguía funcionando porque
`OP_DEFINIR_GLOBAL` ya hacía pop, por eso este bug no se detectó hasta
que la stdlib `regex` (con destructuring en `extraer()` dentro de una
función) lo evidenció.

### Tests añadidos

- `tests/unit/test_bytecode_regex.c` — 21 tests cubriendo literales,
  fullmatch, `.`, quantifiers, clases simples/negadas/rango, clases
  predefinidas, anchors, alternancia, grupos no-captura,
  `regex_buscar`, `regex_todos`, `regex_reemplazar`, errores de
  sintaxis.
- `bc_run_53_regex` — ejecuta `examples/53_regex.cor` (validación de
  hora `HH:MM`, extracción de palabras capitalizadas, todos los
  números en una cadena, redactar IPs con reemplazo).
- **170/170 tests verde**.

## [1.27.0] — 2026-05-13 — Stdlib `proceso` (lanzar procesos externos)

Segundo módulo de **Fase 2 — stdlib esencial**. `proceso` permite
ejecutar comandos externos y capturar su stdout, stderr y exit code.
Implementación cross-platform: Windows (`CreateProcess` + pipes
anónimos) y POSIX (`fork`/`execvp` + pipes + `select` para evitar
deadlock).

### API del módulo `proceso`

```cornamusa
importar proceso

# Ejecutar y leer todo el resultado
r = proceso.ejecutar("git", "status", "--short")
imprimir(r["stdout"])
imprimir("código:", r["codigo"])

# Atajo: solo stdout (si exit != 0 lanza ErrorDeSistema)
texto = proceso.capturar("cmd", "/c", "echo", "hola")

# Atajo: solo exit code (descarta output)
c = proceso.codigo("git", "rev-parse", "--is-inside-work-tree")
```

Cada función acepta `programa` + args via `*args` (recordar: trilogía
`*/**` cerrada en v1.25).

### Forma del resultado

```cornamusa
{
    "stdout": "salida estandar del proceso\n",
    "stderr": "salida de error\n",
    "codigo": 0     # exit code; en POSIX, 128+N para terminación por señal N
}
```

### Built-in primitivo

`nativos.c` añade:
- `proceso_ejecutar(programa, argv_lista)` → dict con 3 claves.

argv sigue convención `execvp`: `argv[0]` es el nombre del programa
(repetido). El módulo lo construye automáticamente.

### Excepciones primitivas nuevas

- `ErrorDeSistema(msg)` — para errores de proceso/red/IO.
- `ErrorDeIO(msg)` — alias compañero (ya se atrapaba por nombre via
  `archivos`, ahora también lanzable explícitamente).

Antes solo se podía atrapar `ErrorDeIO` (el mensaje del error lo
construía la VM con el prefijo). Ahora también se puede lanzar
directamente: `lanzar ErrorDeSistema("conexion cerrada")`.

### Implementación cross-platform

[src/proceso.c](src/proceso.c) — ~320 líneas C:

**Windows**:
- Pipes anónimos via `CreatePipe` + `SetHandleInformation` para
  hacer no-heredables los extremos de lectura del padre.
- Línea de comando construida con escapado mínimo (solo entrecomilla
  args con espacios/comillas), porque `cmd.exe` se confunde con
  comillas redundantes (`cmd /c "echo" "hola"` ≠ `cmd /c echo hola`).
- `WaitForSingleObject(INFINITE)` + `GetExitCodeProcess`.
- Lectura secuencial stdout → stderr (aceptable para tamaños < 10 MB).

**POSIX**:
- `pipe()` + `fork()` + `dup2()` en child antes de `execvp()`.
- Lectura concurrente con `select()` para evitar deadlock cuando el
  child satura uno de los pipes mientras el padre lee del otro.
- `waitpid()` + `WIFEXITED`/`WIFSIGNALED` para decodificar exit
  status (signal terminación → exit code `128 + N`).

### Limitaciones (v1.27)

- **Sin stdin interactivo** — el child hereda stdin del padre.
- **Sin timeout** — bloquea hasta que el proceso termine.
- **Sin env vars o cwd personalizados** — hereda del padre.
- **Output máximo: 10 MB por stream** — más allá lanza
  `ErrorDeSistema: salida excede 10485760 bytes`.

Estas limitaciones se pueden levantar en v1.27.x si surge demanda.

### Tests añadidos

- `tests/unit/test_bytecode_proceso.c` — 8 tests portables:
  - Forma del dict (3 claves stdout/stderr/codigo).
  - Exit code 0 y no-0.
  - Tipo de cada campo (cadena/cadena/entero).
  - Programa inexistente → `ErrorDeSistema` atrapable.
  - Tipos incorrectos de argumentos → `ErrorDeTipo`.
- `bc_run_52_proceso` — ejecuta `examples/52_proceso.cor` (solo en
  Windows; el ejemplo usa `cmd /c` y `dir`). Lista archivos reales
  de `C:\Windows\System32\drivers\etc`.
- **167/167 tests verde**.

## [1.26.0] — 2026-05-13 — Stdlib `azar` + fix `OP_LANZAR` globals

Arranca **Fase 2 — stdlib esencial**. Primer módulo: `azar`, PRNG de
calidad estadística (xoshiro256**, passes BigCrush) con las
primitivas comunes para juegos, simulación, muestreo y testing.

### API del módulo `azar`

```cornamusa
importar azar

azar.semilla(42)                  # reproducibilidad para tests
azar.decimal()                    # uniforme en [0, 1) — 53 bits mantisa
azar.entero(1, 100)               # uniforme en [1, 100] inclusive
azar.uniforme(0.0, 10.0)          # uniforme [a, b) decimal
azar.booleano()                   # moneda 50/50
azar.booleano(p=0.7)              # sesgado: 70% de verdaderos
azar.elegir(["pan", "vino"])      # un elemento al azar
azar.barajar(lista)               # Fisher-Yates in-place, retorna lista
azar.muestra(seq, k)              # k sin reemplazo, nueva lista
```

### Motor xoshiro256**

Implementación en [src/azar.c](src/azar.c) — ~80 líneas C:

- **256 bits de estado**, período `2^256 - 1`.
- Sembrado por `SplitMix64` desde una semilla `uint64`.
- Semilla inicial automática del reloj (`time(NULL) ^ tv_nsec`)
  si no se llama `azar.semilla()` explícitamente.
- `azar_entero_en(a, b)` usa **rechazo** para eliminar el bias del
  módulo: `lim = (UINT64_MAX / rango) * rango`.

### Built-ins primitivos

`nativos.c` registra:
- `azar_decimal()` → VAL_DECIMAL en [0, 1).
- `azar_entero(a, b)` → VAL_ENTERO en [a, b].
- `azar_semilla(n)` → re-siembra; retorna `nulo`.

El módulo `stdlib/azar.cor` envuelve estos primitivos con
`elegir`/`barajar`/`muestra`/`booleano`/`uniforme` escritos en
Cornamusa puro.

### Ejemplo de uso: Monte Carlo de π

```cornamusa
importar azar
azar.semilla(2026)

dentro = 0
n = 100000
i = 0
mientras i < n:
    xc = azar.decimal()
    yc = azar.decimal()
    si xc * xc + yc * yc <= 1.0:
        dentro = dentro + 1
    fin si
    i = i + 1
fin mientras

imprimir("π ≈", 4.0 * dentro / n)
# π ≈ 3.13984 (real: 3.14159265...)
```

### Bug fix incluido: `OP_LANZAR` restauraba mal `vm->globales`

Pre-v1.26: si una función en un módulo importado hacía `lanzar` y la
excepción se atrapaba en el caller, `vm->globales` quedaba apuntando
al diccionario del módulo. Los siguientes accesos a globales del
caller fallaban con `ErrorDeNombre: nombre 'X' no esta definido`.

```cornamusa
# Pre-v1.26 (bug):
importar azar
intentar:
    azar.muestra([1, 2], 5)   # lanza ErrorDeValor adentro
atrapar ErrorDeValor como e:
    imprimir("ok")
fin intentar
azar.decimal()   # ❌ ErrorDeNombre: 'azar' no esta definido
```

Causa: el bucle de unwind en `OP_LANZAR` no restauraba
`globales_pre_llamada` (a diferencia de `vm_lanzar_excepcion` de
nativas que sí lo hacía). Fix: aplicar la misma lógica en ambos paths.

### Limitaciones documentadas

- **`azar.normal(mu, sigma)` no incluida** — Box-Muller requiere
  `log_natural`, `coseno` y `raiz` que Cornamusa aún no expone como
  built-ins. Comentado con TODO en el módulo; se completará cuando
  se añada un mini-pack matemático (probablemente v1.27 o v1.28).
- **Estado global del proceso, no por-VM**. Adecuado para single-
  threaded; cuando llegue concurrencia habrá que migrar a estado
  thread-local o por-VM.

### Tests añadidos

- `tests/unit/test_bytecode_azar.c` — 8 tests: reproducibilidad con
  semilla, rango de decimal sobre 1000 muestras, rango de entero,
  distribución uniforme de d6 sobre 6000 tiradas (chi-square ligero),
  errores atrapables.
- `bc_run_51_azar` — ejecuta `examples/51_azar.cor` (Monte Carlo de π
  con 100k muestras, simulación de dados, lotería, moneda sesgada).
- **164/164 tests verde**.

## [1.25.0] — 2026-05-13 — Spread `**dict` en llamadas

Cierra la trilogía `*/**`: `f(**dict)` expande un diccionario como
keyword arguments en la llamada. Combinable libremente con kwargs
explícitos y posicionales, en cualquier orden.

### Sintaxis

```cornamusa
config = {"puerto": 443, "tls": verdadero}
api("api.dev", **config)                      # equivale a api("api.dev", puerto=443, tls=verdadero)
api("api.dev", **{"puerto": 8080}, tls=verdadero)   # mezcla con kw explícito
api("api.dev", tls=verdadero, **{"puerto": 443})    # orden libre
```

### Merge de configuraciones

Cuando se combinan varios `**dict`, las claves de los posteriores
sobrescriben las anteriores (semántica como Python ≥ 3.5):

```cornamusa
base     = {"region": "eu", "version": 1}
override = {"version": 2, "debug": verdadero}

config(**base, **override)
# {"region": "eu", "version": 2, "debug": verdadero}
```

Ideal para componer configuraciones, opciones de CLI, parámetros de
constructor, etc.

### Forwarding genérico

`**dict` alimenta directamente un receptor `**kw`:

```cornamusa
funcion wrapper(host, **opciones):
    imprimir("opciones:", opciones)
    retornar api(host, **opciones)
fin funcion

wrapper("api.dev", puerto=443, tls=verdadero)
# opciones: {"puerto": 443, "tls": verdadero}
# → api("api.dev", puerto=443, tls=verdadero)
```

### Errores atrapables

- **Kwarg desconocido** vía spread → `ErrorDeTipo: f() no acepta keyword 'X'`.
- **Duplicado** (posicional ya consumido o kwarg explícito) → `ErrorDeTipo: recibio multiple valor para 'X'`.
- **`**X` con X no-dict** → `ErrorDeTipo: 'tipo' no es diccionario para spread (**)`.
- **Clave no-cadena** en el dict spread → `ErrorDeTipo: clave de **dict debe ser cadena`.

### Implementación

Nuevos opcodes en [src/chunk.h](src/chunk.h):

- `OP_DICC_AGREGAR_PAR` — TOS=valor, debajo=clave, debajo=dict. Asigna.
- `OP_DICC_EXTENDER` — TOS=otro_dict, debajo=dict. Merge claves.
- `OP_LLAMAR_KW_DICT [n_pos]` — TOS=dict_kw, debajo n_pos, debajo callee. Despacha.

El compilador detecta `**expr` en argumentos y emite la secuencia:

1. Empuja callee + posicionales.
2. `OP_BUILD_DICC 0` para arrancar dict vacío.
3. Por cada kwarg/spread en orden:
   - Si kwarg explícito: empuja clave (cadena) + valor + `OP_DICC_AGREGAR_PAR`.
   - Si `**spread`: empuja la expr + `OP_DICC_EXTENDER`.
4. `OP_LLAMAR_KW_DICT n_pos`.

Refactor importante en [src/vm.c](src/vm.c): la lógica de matching de
keyword args (~150 líneas) se extrajo a `ejecutar_llamar_kw(vm, frame,
base, n_pos, n_kw)` y la usan **tanto** `OP_LLAMAR_KW` como
`OP_LLAMAR_KW_DICT`. `OP_LLAMAR_KW_DICT` ilumina el dict como pares
(key, val) sobre el stack y delega al helper.

### Limitaciones (v1.25)

- **No combinable con `*lista` spread** en la misma llamada. Para
  forwardear ambos hay que separar:

  ```cornamusa
  funcion wrap(*args, **kw):
      retornar f(*args)          # OK
      # retornar f(*args, **kw)  # ERROR (v1.25)
  fin funcion
  ```

  Esta restricción se levantará cuando se unifique el path de llamadas
  con un opcode `OP_LLAMAR_GENERICO` (Fase 1 cerrada, lo dejo para
  Fase 4 si surge demanda).

### Tests añadidos

- `tests/unit/test_bytecode_dspread.c` — 10 tests cubriendo spread
  solo/mezcla, alimentación a `**kw`, merge override, errores.
- `bc_run_50_dspread` — ejecuta `examples/50_dspread.cor`.
- **161/161 tests verde**.

### Estado de Fase 1

Con v1.25 se cierra la **Fase 1 — Sintaxis idiomática moderna**:

| Versión | Feature | Estado |
|---|---|---|
| v1.21 | Destructuring (`a, b = par`) | ✅ |
| v1.22 | `*args` (def + spread llamada) | ✅ |
| v1.23 | Keyword args (`f(x=1)`) | ✅ |
| v1.24 | `**kwargs` en def | ✅ |
| v1.25 | Spread `**dict` en llamadas | ✅ |

**Paridad sintáctica con Python 3.10+** para argumentos, destructuring
y desempaquetado. Siguiente fase: **stdlib esencial** (`azar`,
`proceso`, `regex`, `red`).

## [1.24.0] — 2026-05-13 — `**kwargs` en definición

Parámetros `**kw` recogen los keyword arguments sobrantes en un
diccionario. Combinable con `*args` para funciones totalmente
flexibles — ideal para wrappers y decoradores.

### Sintaxis

```cornamusa
funcion api(host, **opciones):
    # `opciones` es un dict con los kwargs no-fijos
    si "tls" en opciones y opciones["tls"]:
        ...
    fin si
fin funcion

api("api.dev")                                  # opciones = {}
api("api.dev", puerto=443, tls=verdadero)       # opciones = {"puerto": 443, "tls": verdadero}
```

### Combinable con `*args`

```cornamusa
funcion debug_call(f, *args, **kw):
    imprimir("args:", args, "  kw:", kw)
    retornar f(*args)
fin funcion

debug_call(sumar, 5, 8, etiqueta="suma", trace=verdadero)
# args = (5, 8)
# kw   = {"etiqueta": "suma", "trace": verdadero}
```

### Orden de parámetros

`fijos → *args → **kw`. El compilador rechaza otros órdenes:

```cornamusa
funcion mal(**kw, a):  # ErrorDeCompilacion: no puede haber parámetros tras '**kw'
funcion mal(*args, b, **kw):  # ErrorDeCompilacion: *resto debe ir antes de **kw
```

### Errores atrapables (regresión)

Una función SIN `**kw` sigue rechazando keywords desconocidos:

```cornamusa
funcion f(a):
    retornar a
fin funcion

intentar:
    f(a=1, zorro=5)
atrapar ErrorDeTipo como e:
    imprimir(e)  # ErrorDeTipo: f() no acepta keyword 'zorro'
fin intentar
```

### Lambdas

```cornamusa
con_opciones = lambda **op: cadena(longitud(op)) + " opciones"
con_opciones(a=1, b=2)   # "2 opciones"
```

### Implementación

`FuncionBC` añade `bool tiene_doble_estrella` y `Parametro` añade
`bool es_doble_estrella`. El parser reconoce `**ident` en defs de
`funcion` y `lambda` (token `TT_DOBLE_ASTERISCO` ya existía para `**`).

`ejecutar_llamar_bc` y `OP_LLAMAR_KW` se generalizaron:

- `aridad_fija = aridad - (tiene_estrella ? 1 : 0) - (tiene_doble_estrella ? 1 : 0)`.
- Posicionales excedentes → tupla `*args` (si existe).
- Kwargs no-matched contra params fijos → `**kw` (si existe).
- Si no hay kwargs explícitos pero la función tiene `**kw`, su slot
  recibe dict vacío.

### Limitaciones (v1.24)

- **Spread `**dict`** en llamadas viene en v1.25.
- **No combinable con defaults** todavía (`f(a, b=1, **kw)` rechazado
  por compilador). Se levantará en v1.25 al añadir el spread.

### Tests añadidos

- `tests/unit/test_bytecode_kwkw.c` — 9 tests cubriendo recogida,
  combinación con *args, dict vacío, regresión sin **kw, lambda.
- `bc_run_49_kwkw` — ejecuta `examples/49_kwkw.cor`.
- **158/158 tests verde**.

## [1.23.0] — 2026-05-13 — Keyword arguments

Pasar argumentos por **nombre** en llamadas: `f(x=1, y=2)`. Permite
saltar defaults selectivamente, reordenar libremente y mejora la
legibilidad de funciones con muchos parámetros opcionales.

### Sintaxis

```cornamusa
funcion conectar(host, puerto=80, usar_tls=falso, reintentos=3):
    ...
fin funcion

conectar("api.dev")
conectar("api.dev", usar_tls=verdadero)
conectar("api.dev", reintentos=10, usar_tls=verdadero)  # orden libre
conectar("api.dev", 443, usar_tls=verdadero)            # mezcla
```

### Reglas

- **Posicionales antes de keyword args** — `f(x=1, 2)` es error de
  sintaxis.
- **Orden libre entre keyword args** — `f(b=2, a=1)` y `f(a=1, b=2)`
  son equivalentes.
- **Cada parámetro se asigna una sola vez** — `f(1, a=99)` falla con
  `ErrorDeTipo` si `a` ya tomó el `1` posicional.
- **Keywords desconocidos** → `ErrorDeTipo` atrapable.
- **Falta argumento obligatorio** → `ErrorDeTipo` con el nombre del
  parámetro.

### Combinación con `*args`

Funciones `f(a, b, *resto)` aceptan kwargs para sus parámetros fijos:

```cornamusa
funcion f(a, b, *resto):
    retornar [a, b, resto]
fin funcion

f(b=2, a=1)   # [1, 2, ()]
```

No se puede combinar `f(*lista, key=val)` en la misma llamada (la
sintaxis se mantiene simple en v1.23; viene en v1.24 con `**kwargs`).

### Implementación

AST extendido en [src/ast.h](src/ast.h):

```c
struct { ...
    const char **kwarg_keys;   /* v1.23: NULL si solo posicional */
    int *kwarg_lens;
} llamada;
```

Nuevo opcode en [src/chunk.h](src/chunk.h):

- `OP_LLAMAR_KW [n_pos] [n_kw]` — layout en stack:
  `[callee, pos0..posN-1, key0, val0, key1, val1, ...]`.

`FuncionBC` añade `char **nombres_params` (duplicados en heap) para
matching nombre → slot. El compilador los popula tanto para
`funcion` como para `lambda`.

La VM en `OP_LLAMAR_KW`:

1. Lee `n_pos` y `n_kw` del bytecode.
2. Inicializa array `params_finales[aridad]` y `params_asignados[aridad]`.
3. Copia posicionales a los primeros `n_pos` slots.
4. Para cada (key, val), busca slot por nombre — error si desconocido
   o si ya está asignado.
5. Rellena defaults para slots no-asignados — error si no hay default.
6. Si `tiene_estrella`, slot final recibe tupla vacía.
7. Empuja `params_finales` y pushea un `CallFrame` nuevo.

### Limitaciones (v1.23)

- **Solo funciones bytecode** soportan kwargs (no nativas como
  `imprimir`, `longitud`). Las nativas siguen siendo posicionales.
- **No combinable con `*lista` spread** en la misma llamada.
- **No hay `**kwargs`** (recoger keywords sobrantes en dict) ni
  spread `**dict`. Viene en v1.24.

### Tests añadidos

- `tests/unit/test_bytecode_kwargs.c` — 11 tests cubriendo matching,
  defaults, mezcla con `*args`, errores atrapables.
- `bc_run_48_kwargs` — ejecuta `examples/48_kwargs.cor`.
- **155/155 tests verde**.

## [1.22.0] — 2026-05-13 — `*args` (variádicos y spread)

Parámetros variádicos en definiciones y *spread* de iterables en
llamadas. Equivalente al `*args` de Python.

### Definición: recoger args sobrantes

```cornamusa
funcion suma(*nums):
    total = 0
    para n en nums:
        total = total + n
    fin para
    retornar total
fin funcion

suma()              # 0
suma(1, 2, 3)       # 6
```

El parámetro estrellado recibe una **tupla** con los args sobrantes.
Combinable con parámetros fijos previos:

```cornamusa
funcion saludar(nombre, *titulos):
    pre = ""
    para t en titulos:
        pre = pre + t + " "
    fin para
    retornar pre + nombre
fin funcion

saludar("Ana")                       # "Ana"
saludar("Castilla", "Sr.", "Prof.")  # "Sr. Prof. Castilla"
```

### Llamada: expandir iterable como args

```cornamusa
nums = [10, 20, 30]
suma(*nums)              # 60
suma(1, *nums)           # 61
suma(1, *nums, 100)      # 161    # mezcla libre
```

### Forwarding genérico (decorador-like)

```cornamusa
funcion log_llamada(f, *args):
    imprimir(f"-> llamando con {longitud(args)} arg(s)")
    retornar f(*args)
fin funcion

funcion area(w, h):
    retornar w * h
fin funcion

log_llamada(area, 5, 8)  # imprime trace y devuelve 40
```

### Lambdas variádicas

```cornamusa
contar = lambda *xs: longitud(xs)
contar(1, 2, 3, 4)  # 4
```

### Errores atrapables

- Aridad insuficiente → `ErrorDeTipo: f() esperaba al menos N argumentos`.
- `*expr` sobre no-iterable → `ErrorDeTipo: 'X' no es iterable para spread (*)`.

```cornamusa
intentar:
    f(*"abc")
atrapar ErrorDeTipo como e:
    imprimir(e)
fin intentar
```

### Limitaciones (v1.22)

- **`*resto` debe ser el último parámetro**. No se permite parámetros
  fijos tras él (eso requiere kwargs — viene en v1.23).
- **No combinable con defaults** en la misma función. La restricción
  desaparece en v1.23 al añadir kwargs (que permitirán pasar valores
  nombrados después del `*resto`).
- **Spread acepta lista o tupla**, no cadenas/rangos/iteradores
  genéricos (consistente con la semántica conservadora de v1.22).

### Implementación

AST extendido en [src/ast.h](src/ast.h):

```c
struct Parametro {
    ...
    bool es_estrella;       /* v1.22: `*resto` */
};
struct { ... Expr **args; int n_args;
         bool *args_spread; /* v1.22: por-arg */
       } llamada;
```

Nuevos opcodes en [src/chunk.h](src/chunk.h):

- `OP_LISTA_AGREGAR` — TOS=valor; debajo lista. Pop, append.
- `OP_LISTA_EXTENDER` — TOS=iterable; debajo lista. Pop, extiende.
- `OP_LLAMAR_SPREAD` — TOS=lista args; bajo callee. Llama expandido.

`FuncionBC` añade `bool tiene_estrella`. En `ejecutar_llamar_bc`, si
está activo, los args [n_fijos..n_args-1] se recolectan en una tupla
que ocupa el slot del param estrellado.

El parser ([src/parser.c](src/parser.c)) detecta `*ident` en listas de
parámetros (función y lambda) y `*expr` en argumentos de llamada. La
heurística F10 de inline cache no se ve afectada — solo las llamadas
con spread usan el path nuevo; el resto sigue por `OP_LLAMAR` y se
promueve normalmente.

### Tests añadidos

- `tests/unit/test_bytecode_varargs.c` — 13 tests cubriendo recoger,
  fijos+estrella, spread, forwarding, lambda, errores atrapables.
- `bc_run_47_varargs` — ejecuta `examples/47_varargs.cor`.
- **152/152 tests verde**.

## [1.21.0] — 2026-05-13 — Destructuring assignment

Asignación múltiple en una sola línea: `a, b = par`, `[x, y, z] = lista`,
swap idiomático `a, b = b, a` y anidación arbitraria
`(a, (b, c)) = (1, (2, 3))`. Equivalente al *tuple unpacking* de Python.

### Antes (v1.20 y previos)

```cornamusa
par = (1, 2)
a = par[0]
b = par[1]

# Swap:
tmp = a
a = b
b = tmp
```

### Ahora (v1.21)

```cornamusa
par = (1, 2)
a, b = par                       # 1, 2

# Swap sin variable temporal:
a, b = b, a                      # 2, 1

# Lista al lado izquierdo:
[x, yv, z] = [10, 20, 30]

# Anidado:
(nombre, (op, valor)) = ("set", ("+", 42))

# Iteración con destructuring:
para par_dato en [("ana", 30), ("luis", 25)]:
    nombre, edad = par_dato
    imprimir(f"{nombre} tiene {edad} años")
fin para
```

### Errores atrapables

- **Aridad incorrecta** → `ErrorDeValor` con mensaje claro.
- **Tipo no iterable** → `ErrorDeTipo` (consistente con `longitud()`).

```cornamusa
intentar:
    a, b, c = (1, 2)
atrapar ErrorDeValor como e:
    imprimir(e)  # ErrorDeValor: aridad incorrecta en destructuring
fin intentar
```

### Iterables soportados

Tupla, lista y cadena (esta última desempaqueta por *code point*).
Cualquier valor con longitud conocida e indexación entera.

### Implementación

Reusa la infraestructura de [src/chunk.h](src/chunk.h) existente:
`OP_LONGITUD`, `OP_INDICE`, `OP_IGUAL`, `OP_SALTAR_SI_FALSO`. Sin
opcode nuevo. El compilador detecta `EXPR_TUPLA`/`EXPR_LISTA` como
destino en [src/compilador.c](src/compilador.c) y emite:

1. Eval RHS → slot anónimo.
2. Verifica `longitud(slot) == n_destinos`. Si no → `lanzar ErrorDeValor`.
3. Para cada destino `i`: `slot[i]` → asignación recursiva.

El parser de [src/parser.c](src/parser.c) recoge `a, b, c` como tupla
LHS sin paréntesis en `parsear_asignar_o_expr`, y el RHS también
permite tupla sin paréntesis (`a, b = b, a`).

Heurística añadida: al final de una expresión Pratt, si el siguiente
token (`[` o `(`) está en otra línea, no continuar como infijo. Sin
esto, `lista = [1, 2]` seguido por `[x, y] = lista` se parsearía como
`lista = [1, 2][x, y]`.

Paridad tree-walking en [src/evaluador.c](src/evaluador.c) vía
`asignar_destructuring` recursivo.

### Tests añadidos

- `tests/unit/test_bytecode_destructuring.c` — 15 tests cubriendo
  tupla, lista, swap, anidación, cadena, errores atrapables, RHS
  computado e iteración.
- `bc_run_46_destructuring` — ejecuta `examples/46_destructuring.cor`.
- **149/149 tests verde**.

## [1.20.0] — 2026-05-13 — Diccionarios preservan orden de inserción

Los diccionarios ahora iteran y serializan en el **orden en que se
insertaron las claves**. Coincide con la semántica de Python 3.7+ y
elimina sorpresas con hash order.

### Antes (v1.19 y previos)

```cornamusa
d = {"version": 2, "nombre": "app", "activo": verdadero}
para k en d:
    imprimir(k)
# Orden DEPENDÍA del hash → impredecible.
imprimir(d)
# {"activo": ..., "version": ..., "nombre": ...}  (orden arbitrario)
```

### Ahora (v1.20)

```cornamusa
d = {"version": 2, "nombre": "app", "activo": verdadero}
para k en d:
    imprimir(k)
# version
# nombre
# activo

imprimir(d)
# {"version": 2, "nombre": "app", "activo": true}
```

### Reglas

- **Inserción nueva** → al final del orden.
- **Sobreescribir** una clave existente → mantiene posición.
- **Quitar** → elimina del orden.
- **Quitar + re-insertar** → la clave va al final.
- **Literal** `{a: 1, b: 2, c: 3}` → en orden de aparición.

### Implementación

Extensión de `Diccionario` ([src/valor.h](src/valor.h)):

```c
struct Diccionario {
    ...
    int *orden_insercion;   /* array de slot indices en orden */
    int orden_capacidad;
};
```

- `dicc_asignar`: inserción nueva → append slot al array.
- `dicc_quitar`: localiza el slot en el array, shift para cerrar el
  hueco; re-inserciones tras borrado actualizan slot indices in-place
  con un helper.
- `dicc_redimensionar`: re-mapea slot indices viejos a nuevos
  manteniendo el orden viejo.
- `iter_siguiente` para `VAL_DICCIONARIO`: itera `orden_insercion[]`
  en lugar de `entradas[]`.
- `valor_a_cadena` para dict, `claves()`, `valores()`,
  `js_serializar`, `nativa_diccionario` (constructor copia),
  iteración `para…en` del evaluador: todos usan el orden.

### Costos

- **Memoria**: `+sizeof(int) * capacidad` por dict.
- **Inserción**: O(1) amortizado (mismo que antes).
- **Borrado**: O(N) por la búsqueda lineal en el array + shift. Para
  dicts pequeños es despreciable; para dicts grandes con muchos
  borrados existe margen de mejora futuro (lista doblemente enlazada
  intrusiva).
- **Iteración**: O(N) — ANTES era O(capacidad) que con factor de
  carga puede ser hasta 2N. Después de v1.20 es estrictamente N.

### Tests

15 tests unit en
[tests/unit/test_bytecode_dict_orden.c](tests/unit/test_bytecode_dict_orden.c)
cubriendo inserción incremental/inversa, sobreescribir, quitar (cada
posición), iteración `para…en`, JSON, `cadena()`, literales, rehash
con 20 claves, copia. Total: **144 verde**.

Ejemplo:
[examples/45_dict_ordenado.cor](examples/45_dict_ordenado.cor) con
config legible, tabla con columnas en orden, rehash invisible.

## [1.19.0] — 2026-05-13 — Stdlib `fechas`

Operaciones con fechas y horas: timestamps Unix, descomposición y
composición, formateo, aritmética, validaciones del calendario
Gregoriano. Nuevo módulo `fechas` sobre 4 built-ins C.

### Built-ins nuevos en C

```cornamusa
ts = tiempo_actual()                          # entero ts Unix (segundos)
c = tiempo_descomponer(ts)                    # tupla (año, mes, dia, hora,
                                              #         min, seg, dia_sem, dia_año)
ts = tiempo_componer(año, mes, dia, h, m, s)  # entero ts desde componentes
s = tiempo_formato(ts, "%Y-%m-%d")            # cadena con strftime spec
```

`tiempo_descomponer` y `tiempo_formato` usan **zona horaria local del
sistema**. `tiempo_componer` interpreta los componentes como local time
y devuelve el ts UTC equivalente.

Convenciones:
- `mes`: 1-12 (no 0-11 como C `struct tm`).
- `dia`: 1-31.
- `hora`/`min`/`seg`: 0-23/0-59/0-60.
- `dia_semana`: 0=lunes, 1=martes, ..., 6=domingo (ISO 8601, no
  como C que usa 0=domingo).
- `dia_año`: 1-366.

### Módulo `stdlib/fechas.cor`

API amigable sobre los built-ins:

```cornamusa
importar fechas

ahora = fechas.ahora()
imprimir(fechas.legible(ahora))       # "2026-05-13 16:25:57"
imprimir(fechas.iso8601(ahora))       # "2026-05-13T16:25:57"
imprimir(fechas.solo_fecha(ahora))    # "2026-05-13"

# Componentes como dict.
c = fechas.componentes(ahora)
imprimir(f"{fechas.nombre_dia(c['dia_semana'])} {c['dia']} de {fechas.nombre_mes(c['mes'])}")
# → "miércoles 13 de mayo"

# Aritmética.
mañana    = fechas.sumar_dias(ahora, 1)
hace_sem  = fechas.sumar_dias(ahora, -7)
en_2_hrs  = fechas.sumar_horas(ahora, 2)
delta     = fechas.diferencia_dias(b, a)

# Validación.
fechas.es_bisiesto(2024)              # verdadero
fechas.dias_en_mes(2024, 2)           # 29
fechas.dias_en_mes(2026, 4)           # 30

# Constantes para aritmética.
fechas.SEGUNDO, fechas.MINUTO, fechas.HORA, fechas.DIA, fechas.SEMANA
```

`fechas.construir(año, mes, dia, hora=0, minuto=0, segundo=0)` usa
defaults v1.17 — solo año/mes/día son obligatorios.

`nombre_dia` y `nombre_mes` devuelven texto en **español** (no
dependen del locale del sistema, a diferencia de `%A`/`%B` de
strftime).

### Implementación

`src/nativos.c`: 4 funciones nuevas usando `time.h` (POSIX/C99). En
Windows/MinGW usan `localtime_s`; en POSIX `localtime_r`. `mktime`
con `tm_isdst = -1` deja que el sistema decida DST. Pre-existing
helper `tupla_de_tm` convierte `struct tm` a tupla Cornamusa con
ajustes de convenciones (mes +1, día_semana ISO, día_año +1).

### Tests

12 tests unit en
[tests/unit/test_bytecode_tiempo.c](tests/unit/test_bytecode_tiempo.c)
para los 4 built-ins: tipos, aridad, roundtrip
componer↔descomponer, formato ISO/fecha/hora/literal `%%`, errores
atrapables. Total: **142 verde**.

Ejemplo:
[examples/44_fechas.cor](examples/44_fechas.cor) con fecha actual,
componentes legibles, construir timestamps, aritmética, calendario
del mes y validación de años bisiestos.

### Limitaciones

- Sin **milisegundos**: `tiempo_actual()` retorna segundos. Para
  cronometrar operaciones rápidas el granularidad es 1s. Versión
  futura con `tiempo_ns()` basado en `clock_gettime`/`QueryPerformanceCounter`.
- Sin **UTC explícito**: todo usa local time. Para UTC el usuario
  debe gestionar offset manualmente. Versión futura: parámetro
  `utc=verdadero`.
- Sin **parsear fechas**: la inversa de `tiempo_formato` (parsear
  cadena → ts) no está. Usar `fechas.construir` con componentes
  numéricos.

## [1.18.1] — 2026-05-13 — Fix: re-import de módulos

Arregla un bug preexistente del runtime descubierto durante el
desarrollo de v1.18: si dos módulos importaban el mismo sub-módulo
(p.ej. el programa principal Y un módulo importado ambos hacían
`importar cadenas`), la VM fallaba con
`Pila vacia (bug del compilador)`.

### Causa

El compilador emite `OP_DESCARTAR` tras `OP_IMPORTAR` asumiendo que
el frame del módulo (cache miss) deja `nulo` en stack al retornar
(vía `OP_RETORNAR`). En el camino de **cache hit** no hay frame
nuevo — `OP_IMPORTAR` solo asigna la global desde el cache y termina
— pero NO empujaba `nulo`. El `OP_DESCARTAR` siguiente popeaba el
valor que estuviera en la cima, corrompiendo el stack.

### Fix

`OP_IMPORTAR` en cache hit ahora:
1. Retiene el módulo antes de asignarlo a globales (fix secundario
   de doble-liberación: `dicc_obtener` retorna por value sin retain,
   pero `dicc_asignar` toma ownership; sin retain, el módulo se
   liberaba dos veces al limpiar cache + globales).
2. **Empuja `nulo` al stack** para que el `OP_DESCARTAR` siguiente
   tenga algo válido que descartar — convención consistente con el
   camino de cache miss.

Un patch quirúrgico en [src/vm.c](src/vm.c) de ~5 líneas.

### Refactor de stdlib

Con el bug arreglado, `formato.cor` vuelve a delegar en `cadenas`
sin duplicación. Los helpers locales `_repetir`, `_unir`,
`_indice_de` ahora son aliases triviales.

### Test diferencial

Añadido `bc_run_43_formato` en
[tests/CMakeLists.txt](tests/CMakeLists.txt). El ejemplo
[examples/43_formato.cor](examples/43_formato.cor) ya ejercitaba el
patrón (`main` importa `formato`+`cadenas`; `formato` importa
`cadenas`); ahora pasa como test de regresión. Total: **140 verde**.

## [1.18.0] — 2026-05-12 — Stdlib `formato` y `cadenas` extendida

Stdlib más amplia para reportes legibles y manipulación de cadenas.
Sin cambios al runtime — todo escrito en Cornamusa puro.

### Nuevo módulo `formato`

```cornamusa
importar formato

formato.rellenar("izq", 10)            # "izq       "
formato.alinear_derecha("der", 10)     # "       der"
formato.centrar("centro", 10, "·")     # "··centro··"
formato.con_decimales(3.14159, 2)      # "3.14"
formato.numero_con_separador(1234567)  # "1_234_567"
formato.porcentaje(0.857)              # "85.70%"
formato.como_hex(255)                  # "0xff"
formato.como_binario(10)               # "0b1010"
formato.linea("-", 30)                 # "------------------------------"
formato.fila(["a", 1, 2.5], [5, 3, 5]) # "a     | 1   | 2.5  "
```

Todas las funciones de padding aceptan `caracter` opcional (default
espacio). `con_decimales`, `numero_con_separador`, `porcentaje`,
`como_hex`, `como_binario` aceptan defaults para parámetros opcionales
(usando v1.17). `numero_con_separador` ahora maneja decimales:
`56251.5` → `"56_251.5"`.

### `cadenas` extendida

Funciones nuevas en [stdlib/cadenas.cor](stdlib/cadenas.cor):

```cornamusa
importar cadenas

cadenas.indice_de("hola mundo", "mun")    # 5
cadenas.contiene("hola mundo", "mun")     # verdadero
cadenas.separar("a,b,c", ",")             # ["a", "b", "c"]
cadenas.separar("hola", "")               # ["h", "o", "l", "a"]
cadenas.reemplazar("ho la ho", "ho", "X") # "X la X"
cadenas.recortar("  espacios  ")          # "espacios"
cadenas.recortar_izquierda("  left")      # "left"
cadenas.recortar_derecha("right  ")       # "right"
cadenas.minusculas_ascii("Hola Mundo")    # "hola mundo"
cadenas.mayusculas_ascii("Hola Mundo")    # "HOLA MUNDO"
```

Las funciones de case son **ASCII-only** (`A`–`Z` ↔ `a`–`z`).
Caracteres no-ASCII se preservan sin cambios. La versión Unicode-aware
requiere `utf8proc` exposed como built-in, planeado para versiones
futuras.

### Bug conocido y workaround

Durante el desarrollo de v1.18 detectamos un bug preexistente del
runtime: si dos módulos importan el mismo sub-módulo (e.g.
`formato.cor` y el programa principal ambos importan `cadenas`),
ocurre un error `Pila vacia (bug del compilador)`. Para evitarlo, el
módulo `formato` es **self-contained**: incluye helpers locales
prefijados `_` que duplican el código de `_repetir`, `_unir`,
`_indice_de`. Cuando el bug del runtime se arregle, `formato`
delegará en `cadenas` sin duplicación.

### Tests

Sin tests unit dedicados (los tests unit corren sin acceso a stdlib).
El ejemplo
[examples/43_formato.cor](examples/43_formato.cor) ejercita TODO el
nuevo código de `formato` y `cadenas`. Total: **138 verde**.

## [1.17.0] — 2026-05-12 — Argumentos por defecto en bytecode

Cierra una limitación heredada desde v0.7: el motor bytecode rechazaba
`funcion f(a, b=valor):` con un error "aun no estan en bytecode". El
evaluador tree-walking ya los soportaba — esta asimetría obligaba a
duplicar funciones en stdlib (`enumerar`/`enumerar_desde`,
`suma`/`suma_desde`). v1.17 unifica ambos motores.

### Sintaxis

```cornamusa
funcion saludar(nombre, saludo="Hola", signo="!"):
    retornar f"{saludo}, {nombre}{signo}"
fin funcion

saludar("Ana")                # "Hola, Ana!"
saludar("Bob", "Buenas")      # "Buenas, Bob!"
saludar("Carla", "Hey", ".")  # "Hey, Carla."

# Lambda también
escalar = lambda n, factor=2: n * factor
escalar(5)      # 10
escalar(5, 10)  # 50
```

### Semántica: capturados al `def`, no al call

Los defaults se evalúan **una sola vez** cuando se crea la función
(Python-like). Mutables compartidos entre llamadas:

```cornamusa
N = 10
funcion f(z, base=N):
    retornar z + base
fin funcion
N = 999
f(5)  # 15, no 1004 — el default ya está capturado
```

```cornamusa
funcion log(item, acc=[]):
    agregar(acc, item)
    retornar acc
fin funcion
log("a")  # ["a"]
log("b")  # ["a", "b"] — mismo objeto reusado (igual que Python)
```

### Implementación

- **`FuncionBC`** ([src/chunk.h](src/chunk.h)): nuevo campo
  `n_defaults` que el compilador setea al construir la plantilla.
- **`Closure`** ([src/chunk.h](src/chunk.h)): nuevo array
  `Valor *defaults` con `n_defaults` valores, evaluados al crear el
  closure.
- **Compilador** ([src/compilador.c](src/compilador.c)): tras
  `funcion_bc_nueva`, emite las expresiones de default en orden ANTES
  de `OP_CLOSURE`. Sin nuevos opcodes.
- **VM `OP_CLOSURE`** ([src/vm.c](src/vm.c)): pop `n_defaults` valores
  y los guarda en `cl->defaults`.
- **VM `ejecutar_llamar_bc` / `ejecutar_llamar_clase` /
  `ejecutar_llamar_metodo_ligado`**: si `n_args < aridad` y la
  función tiene defaults suficientes, completa con `valor_clonar`
  desde el array.
- **Validación**: el parser ya rechazaba parámetros con default antes
  del primero sin default; el compilador refuerza esto en v1.17.

### Cambio colateral: errores de aridad atrapables

Antes de v1.17, `f(1)` cuando `f` espera 2 args producía un error de
runtime no atrapable. Ahora las llamadas con aridad incorrecta
(faltan args y no hay defaults, o sobran args) usan `RAISE_OR_DIE()`,
consistente con el patrón de v1.10/v1.13.

```cornamusa
intentar:
    saludar()  # falta `nombre`
atrapar ErrorDeTipo como e:
    imprimir(f"capturado: {e}")
fin intentar
```

### Stdlib actualizada

`enumerar(xs)` y `suma(xs)` ahora usan defaults:

```cornamusa
importar funcionales
funcionales.enumerar([10, 20, 30])         # [(0, 10), (1, 20), (2, 30)]
funcionales.enumerar([10, 20], 100)         # [(100, 10), (101, 20)]
funcionales.suma([1, 2, 3])                # 6
funcionales.suma(["a", "b"], "")           # "ab"
funcionales.suma([[1], [2]], [])           # [1, 2]
```

`enumerar_desde` y `suma_desde` se mantienen como aliases deprecated
por compatibilidad con código v1.11-v1.16.

### Tests

12 tests nuevos en
[tests/unit/test_bytecode_defaults.c](tests/unit/test_bytecode_defaults.c)
cubriendo simple, múltiples, captura de scope al def, expresiones
complejas, defaults mutables compartidos, lambdas, validación,
aridad atrapable, métodos con default. Total: **137 verde**.

Ejemplo: [examples/42_defaults.cor](examples/42_defaults.cor) con
saludador, captura al def, defaults mutables, factory con `Pedido`,
lambdas y errores atrapables.

## [1.16.3] — 2026-05-11 — Type-match y `como nombre` en `coincidir`

Cierra la story de pattern matching para v1.x. Ahora `coincidir`
cubre TODOS los patrones idiomáticos: literal, bind, wildcard, OR,
tupla, lista (con star), guarda **y type-match**. Cornamusa equivale
funcionalmente a `match/case` de Python 3.10+.

### Sintaxis nueva

**Type-match**: `cuando NombreClase():` matchea cualquier instancia
de la clase (vía cadena de superclases). Sin destructuring posicional
(Cornamusa no tiene atributos posicionales como dataclasses).

```cornamusa
clase Animal:
    funcion __iniciar__(yo, n): yo.nombre = n fin funcion
fin clase

clase Perro extiende Animal: ... fin clase
clase Gato extiende Animal:  ... fin clase

funcion describir(a):
    coincidir a:
        cuando Perro() como p:    retornar f"Perro {p.nombre}"
        cuando Gato() como g:     retornar f"Gato {g.nombre}"
        cuando Animal() como x:   retornar f"Animal {x.nombre}"
        cuando _:                 retornar "no es un animal"
    fin coincidir
fin funcion
```

**`como <nombre>`** tras el patrón bindea el sujeto entero a un local.
Compatible con cualquier patrón:

```cornamusa
cuando 5 como n:                  imprimir(f"cinco con n={n}")
cuando Foo() como obj:            obj.metodo()
cuando (x, y) como par:           imprimir(f"par={par}")
cuando Perro() como p si ...:     ...  # se evalúa con `p` ya bindeado
```

### Implementación

**PATRON_TIPO** ([src/compilador.c](src/compilador.c) `emitir_verify`):
emite una llamada al built-in `instancia_de(sujeto, NombreClase)` y
salta a no_match si el resultado es falso. Reusa
`OP_OBTENER_GLOBAL` + `OP_LLAMAR` — cero opcodes nuevos. Si el
usuario sombrea `instancia_de`, el patrón usa esa versión (consistente
con el resto del lenguaje).

**`como` en ClausulaCuando**: tras `emitir_binds`, si hay
`bind_completo_texto`, emit `OP_OBTENER_LOCAL slot_sujeto +
agregar_local(nombre)`. Funciona como un bind extra sobre el sujeto
entero.

### Bug fix: aterrizajes de cláusula con guarda + bind_completo

Bug introducido durante v1.16.3 y arreglado en el mismo release: cuando
una cláusula tenía verify + guarda + binds (incluido bind_completo),
el aterrizaje no_match caía erróneamente en el aterrizaje
guarda_falso (que descartaba binds inexistentes en runtime → stack
underflow → crash silencioso).

Fix: si ambos aterrizajes están presentes, emitir un `OP_SALTAR`
entre ellos a un punto post-aterrizajes común. Si solo uno está
presente, el caso degenera al simple.

### Limitaciones

- **Sin destructuring posicional** (`cuando Foo(x, y):`): Cornamusa
  no tiene atributos posicionales. Para extraer atributos usar
  `como v` y luego `v.campo` en el cuerpo o guarda.
- **`__match_args__`** (Python 3.10) no soportado.

### Tests

10 tests nuevos en
[tests/unit/test_bytecode_v163.c](tests/unit/test_bytecode_v163.c)
cubriendo type-match básico, herencia, orden de cláusulas, no-instancia,
bind con `como`, combinaciones con guarda, integración en bucles,
rechazo de `Foo(args)`. Total: **136 verde**.

Ejemplo:
[examples/41_type_match.cor](examples/41_type_match.cor) con jerarquía
animal, refinamiento por guarda, y despachador de eventos UI.

## [1.16.2] — 2026-05-11 — OR-patterns y star-pattern en `coincidir`

Cierra la mayoría de los patterns idiomáticos de matching. Ahora
`coincidir` cubre prácticamente lo mismo que `match/case` de Python
3.10+.

### OR-patterns: `cuando 1 | 2 | 3:`

```cornamusa
funcion describir_codigo(codigo):
    coincidir codigo:
        cuando 200 | 201 | 204:           retornar "éxito"
        cuando 301 | 302 | 304:           retornar "redirect"
        cuando 400 | 401 | 403 | 404:     retornar "error de cliente"
        cuando 500 | 502 | 503:           retornar "error de servidor"
        cuando _:                         retornar f"otro: {codigo}"
    fin coincidir
fin funcion
```

**Restricción intencional**: las alternativas solo pueden ser
literales o `_`. No se permite `cuando a | b:` (bindings entre
alternativas requieren bookkeeping para garantizar consistencia, lo
dejamos para una versión futura si surge demanda).

### Star-pattern: `cuando [a, *resto, b]:`

```cornamusa
funcion partir(xs):
    coincidir xs:
        cuando [primero, *medio, ultimo]:
            retornar f"primero={primero}, medio={medio}, ultimo={ultimo}"
        cuando [unico]:
            retornar f"un solo elem: {unico}"
        cuando _:
            retornar "vacía"
    fin coincidir
fin funcion

partir([1, 2, 3, 4, 5])  # → "primero=1, medio=[2, 3, 4], ultimo=5"
partir([1, 2])           # → "primero=1, medio=[], ultimo=2"  (star captura []!)
partir([42])             # → "un solo elem: 42"
```

Combinable con OR y patrones estructurales anidados:

```cornamusa
funcion ejecutar(args):
    coincidir args:
        cuando []:                  retornar "uso: ..."
        cuando ["ayuda"]:           retornar "muestra ayuda"
        cuando ["sumar", *nums]:    retornar f"suma = {funcionales.suma(nums)}"
        cuando [cmd, *resto]:       retornar f"'{cmd}' no soportado"
    fin coincidir
fin funcion
```

**Restricción intencional**:
- `*nombre` solo permitido dentro de `[...]` (no en tuplas).
  Workaround: usar lista en su lugar, o bindear y manipular en el cuerpo.
- Solo **un** `*nombre` por lista (el parser rechaza con mensaje claro).
- `*_` también funciona (descartar el resto).

### Implementación

**OR-patterns** ([src/compilador.c](src/compilador.c) `emitir_verify`
caso `PATRON_OR`): cadena de tests donde cada test no-final salta a
`L_match_ok` si verdadero, descartando el bool. El último test sale al
no_match común si falla. Sin nuevos opcodes.

**Star-pattern** ([src/compilador.c](src/compilador.c) `emitir_verify`
y `emitir_binds` casos `PATRON_LISTA` extendidos):
- Verify: `OP_LONGITUD >= n - 1` (en lugar de `==`).
- Sub-elementos antes del star: índice positivo `i`.
- Sub-elementos después del star: índice negativo `-(n - i)` —
  `OP_INDICE` ya acepta negativos (Python-style).
- Star bind: emit `OP_REBANADA` con `[s : len - cola]` para capturar
  el slice. Resultado bindeado al nombre con `agregar_local`.

Reusa `OP_MAYOR_IGUAL`, `OP_RESTAR`, `OP_REBANADA`, `OP_LONGITUD` —
todos existentes. Cero opcodes nuevos.

### Tests

14 tests nuevos en
[tests/unit/test_bytecode_v162.c](tests/unit/test_bytecode_v162.c):
OR con literales/cadenas/negativos/no-match/rechazo de bind; star
head/tail/medio/captura-vacío/lista-corta/rechazo-tupla/rechazo-dos;
integración en bucle. Total: **133 verde**.

Ejemplo:
[examples/40_or_y_star.cor](examples/40_or_y_star.cor) con 6 casos:
clasificador de caracteres, códigos HTTP, head/tail, primero/medio/
último, parser de comandos, y combinación de OR + star + estructural.

## [1.16.1] — 2026-05-10 — Patches: JSON pretty-print + `quitar` extendido

Patches menores que cierran limitaciones documentadas.

### JSON pretty-print

`json.serializar(obj, indent)` ahora acepta un segundo argumento
opcional con el número de espacios por nivel de anidación. Útil para
configs y logs legibles:

```cornamusa
importar json

cfg = {
    "version": "1.16.1",
    "tags": ["lenguaje", "castellano"],
    "config": {"debug": falso, "puerto": 8080}
}

# Compacto (default v1.9):
imprimir(json.serializar(cfg))
# {"version":"1.16.1","tags":["lenguaje","castellano"],"config":{"debug":false,"puerto":8080}}

# Indentado:
imprimir(json.serializar_indentado(cfg, 2))
# {
#   "version": "1.16.1",
#   "tags": [
#     "lenguaje",
#     "castellano"
#   ],
#   "config": {
#     "debug": false,
#     "puerto": 8080
#   }
# }
```

`indent=0` mantiene compatibilidad exacta con v1.9. `indent < 0` o no
entero produce `ErrorDeTipo` atrapable. Clamp a 32 espacios por nivel
para evitar abuso.

### `quitar` extendido a diccionarios y conjuntos

ESPEC §4.1 documentaba `quitar(lista_o_dicc_o_conj, clave_o_indice)`,
pero la implementación de v0.5 solo cubría listas. v1.16.1 cierra
esto:

```cornamusa
d = {"a": 1, "b": 2}
v = quitar(d, "a")          # v = 1, d = {"b": 2}

s = conjunto([1, 2, 3])
quitar(s, 2)                # s ahora tiene {1, 3}

# Errores atrapables:
intentar:
    quitar(d, "no_existe")
atrapar ErrorDeClave como e:
    imprimir(f"capturado: {e}")
fin intentar
```

Para diccionario retorna el valor extraído (igual que `dict.pop` de
Python). Para conjunto retorna `nulo` (no hay valor asociado). Si la
clave/elemento no está, `ErrorDeClave` atrapable.

### Tests

10 tests nuevos en
[tests/unit/test_bytecode_v161.c](tests/unit/test_bytecode_v161.c)
para JSON indent y `quitar` extendido. Total: **132 verde**.

## [1.16.0] — 2026-05-10 — Patrones estructurales en `coincidir`

Extiende v1.15 con patrones de tupla y lista, soportando anidación
arbitraria. Con esto `coincidir` se vuelve una herramienta de
**desestructuración + dispatch** que era difícil de escribir con
`si/sino` y `[i]`.

### Sintaxis

```cornamusa
coincidir punto:
    cuando (0, 0):              imprimir("origen")
    cuando (a, 0):              imprimir(f"eje X en {a}")
    cuando (0, b):              imprimir(f"eje Y en {b}")
    cuando (a, b) si a == b:    imprimir(f"diagonal en {a}")
    cuando (a, b):              imprimir(f"({a}, {b})")
    cuando _:                   imprimir("?")
fin coincidir
```

### Lo que hay de nuevo

| Patrón | Sintaxis | Cuándo matchea |
|---|---|---|
| Tupla | `(p1, p2, ..., pn)` | TOS es `VAL_TUPLA` con cuenta `n` y cada elemento matchea su sub-patrón. |
| Lista | `[p1, p2, ..., pn]` | TOS es `VAL_LISTA` con cuenta `n` y cada elemento matchea. |

Tupla NO matchea lista y viceversa: el tipo se chequea estricto.
Sub-patrones pueden ser literal/bind/wildcard/tupla/lista anidados —
hasta 16 niveles de profundidad.

### Ejemplos prácticos

**Análisis de comandos como listas:**
```cornamusa
coincidir comando:
    cuando []:                       retornar "vacío"
    cuando ["ayuda"]:                retornar "muestra ayuda"
    cuando ["sumar", a, b]:          retornar f"sumar = {a + b}"
    cuando ["mult", a, b, c]:        retornar f"mult = {a * b * c}"
    cuando _:                        retornar "desconocido"
fin coincidir
```

**Mensajes etiquetados con anidación:**
```cornamusa
coincidir msg:
    cuando ("login", (usuario, password)):  ...
    cuando ("data", [a, b, c]):             ...
    cuando ("ping", _):                     retornar "pong"
fin coincidir
```

**Tree walker:**
```cornamusa
funcion evaluar(expr, env):
    coincidir expr:
        cuando ("num", n):          retornar n
        cuando ("var", nombre):     retornar env[nombre]
        cuando ("suma", a, b):      retornar evaluar(a, env) + evaluar(b, env)
        cuando ("mult", a, b):      retornar evaluar(a, env) * evaluar(b, env)
    fin coincidir
fin funcion
```

### Implementación: pasada doble + cleanup determinista

El compilador genera bytecode en dos fases:

1. **Verify** ([src/compilador.c](src/compilador.c) `emitir_verify`):
   verifica recursivamente tipo, longitud y literales. Sin tocar
   locales — cada test fallido emite un salto a `L_no_match` con UN
   booleano FALSE en stack y nada más.
2. **Bind** (`emitir_binds`): solo se ejecuta si la verify pasó.
   Recorre el patrón y emite `OP_OBTENER_LOCAL slot_sujeto + cadena
   de OP_INDICE` por cada `bind`, asignando el resultado a un local.

Esta separación garantiza un stack invariante simple: en cualquier
salto a `L_no_match`, el stack es `pre-cláusula + 1 bool false`. Un
único `OP_DESCARTAR` aterriza todos los saltos.

Tras el cuerpo (o tras un fallo de guarda), se emiten N
`OP_DESCARTAR` (uno por bind creado) y se restaura
`c->n_locales = n_locales_pre` para que la cláusula siguiente vea el
estado limpio.

### Nuevos opcodes

- `OP_ES_TUPLA` y `OP_ES_LISTA`: peek/consume el TOS, push booleano
  según el tipo. Necesarios para verify del patrón estructural.

### Limitaciones

- **Sin OR-patterns** (`cuando 1 | 2 | 3:`). Workaround: cláusulas
  duplicadas o guarda `cuando v si v == 1 o v == 2 o v == 3:`.
- **Sin patrón star** (`cuando [primero, *resto]:`). Workaround: bind
  + slicing manual.
- **Sin type-match** (`cuando Foo(x, y):`). Workaround: guarda con
  `instancia_de`.
- **Profundidad de anidación**: 16 niveles. Suficiente para casos
  prácticos.

### Tests

15 tests nuevos en
[tests/unit/test_bytecode_coincidir_estructural.c](tests/unit/test_bytecode_coincidir_estructural.c)
cubriendo tuplas/listas básicas, aridad, tipos, anidación heterogénea,
guardas estructurales, secuencias en bucles. Todos los 16 tests de
v1.15 siguen verde. Total: **130 verde**.

Ejemplo:
[examples/39_coincidir_estructural.cor](examples/39_coincidir_estructural.cor)
con coordenadas, comandos, mensajes etiquetados y un evaluador AST
recursivo.

## [1.15.0] — 2026-05-10 — Pattern matching (`coincidir`/`cuando`)

Llega la sentencia `coincidir/cuando` (`match/case` de Python). Más
clara que cadenas largas de `si/sino si` para despachar por valor.
Esta versión cubre los patrones comunes: literales, bind y wildcard,
con guardas booleanas opcionales.

### Sintaxis

```cornamusa
coincidir <expr>:
    cuando <patron> [si <guarda>]:
        <cuerpo>
    cuando ...
fin coincidir
```

### Patrones soportados en v1.15

| Patrón | Sintaxis | Comportamiento |
|---|---|---|
| Wildcard | `_` | Matchea cualquier valor sin bindear. |
| Literal | `0`, `-1`, `3.14`, `"hola"`, `verdadero`, `falso`, `nulo` | Compara con `==`. Acepta `-`/`+` unario antes de números. |
| Bind | `nombre` | Crea local `nombre` con el valor del sujeto. Siempre matchea. |
| Guarda | `cuando <patron> si <expr>:` | Refina el match: solo entra al cuerpo si `expr` es verdadera. |

Sin fall-through entre cláusulas: el primer `cuando` que matchea
ejecuta su cuerpo y sale. Si ninguno matchea, el `coincidir` no hace
nada (sin error).

### Ejemplos

```cornamusa
# Despachador de comandos.
funcion ejecutar(cmd):
    coincidir cmd:
        cuando "ayuda":  mostrar_ayuda()
        cuando "salir":  terminar()
        cuando _:        imprimir(f"desconocido: {cmd}")
    fin coincidir
fin funcion

# Clasificador con guardas.
funcion clasificar(n):
    coincidir n:
        cuando 0:                retornar "cero"
        cuando v si v < 0:       retornar f"negativo: {v}"
        cuando v si v > 100:     retornar "grande"
        cuando _:                retornar "normal"
    fin coincidir
fin funcion
```

### Implementación

- **Lexer** ([src/lexer.c](src/lexer.c)): `cuando` reservada como
  `TT_CUANDO`. `coincidir` ya estaba en el lexer desde Fase 0.
- **AST** ([src/ast.h](src/ast.h)): nuevo `SENT_COINCIDIR` y tipos
  `Patron`/`ClausulaCuando`. `TipoPatron` con tres variantes
  (WILDCARD, LITERAL, BIND).
- **Parser** ([src/parser.c](src/parser.c)): `parsear_coincidir`
  recolecta cláusulas; `parsear_patron` distingue `_` / identificador
  / literal con `-` opcional.
- **Compilador** ([src/compilador.c](src/compilador.c)):
  `compilar_coincidir` desugar a if/else chain. Eval sujeto a slot
  anónimo, por cada cláusula emite check de patrón + guarda + cuerpo
  + salto al fin. Sin nuevos opcodes — reusa `OP_OBTENER_LOCAL`,
  `OP_IGUAL`, `OP_SALTAR_SI_FALSO`, `OP_SALTAR`.
- **Evaluador tree-walking**: rechaza con el mensaje habitual ("no
  implementado en v0.4"). Coherente con `clase`/`intentar`/etc.

### Limitaciones intencionales

- **Sin patrones estructurales** en v1.15: `cuando (x, y):` o
  `cuando [a, b]:` no soportados. Se aplazan a v1.15.x cuando haya
  demanda real (requieren chequeo de longitud + tipo + recursión).
- **Sin OR-patterns**: `cuando 1 | 2 | 3:` no disponible. Repetir
  cláusulas o usar guardas.
- **Sin type-match**: `cuando Foo(...)` no disponible. Usar
  `instancia_de(obj, Foo)` con guarda: `cuando obj si instancia_de(obj, Foo):`.
- **Bind no rollback**: si la guarda falla tras un bind, el local
  queda definido (con el valor del sujeto). Sombra entre cláusulas
  funciona normalmente.

### Tests

16 tests en
[tests/unit/test_bytecode_coincidir.c](tests/unit/test_bytecode_coincidir.c)
cubriendo todos los patrones, guardas, anidación, sintaxis errónea.
Total: **128 verde**.

Ejemplo:
[examples/38_coincidir.cor](examples/38_coincidir.cor) con cuatro
casos de uso: despachador de comandos, clasificador numérico, estado
de jobs, procesador con anidación.

## [1.14.0] — 2026-05-10 — Pulido

Tres mejoras menores que cierran limitaciones documentadas, más una
corrección de bug preexistente expuesto durante el trabajo.

### Re-raise sin alias

`lanzar` sin valor (re-raise) ahora funciona en cualquier `atrapar`,
no solo los que tienen alias `como e`. El compilador asigna la
excepción a un local con nombre vacío cuando el atrapar no nombra el
alias, manteniendo el slot accesible para `lanzar`.

```cornamusa
intentar:
    procesar(arg)
atrapar ErrorDeIO:
    imprimir("[log] fallo de I/O")
    lanzar       # re-propaga al caller, sin necesidad de `como e`
fin intentar
```

### Slicing de cadenas (UTF-8)

`s[i:j]`, `s[i:j:k]` con índices y paso negativos. Los índices son
**code points**, no bytes — funciona correctamente con caracteres
multibyte:

```cornamusa
"café"[3:]        # "é"        (un solo code point)
"Año"[::-1]            # "oñA"
"Hola mundo"[5:]       # "mundo"
"abc"[10:20]           # ""         (clamp Python-style)
```

Implementación: en `OP_REBANADA` para `VAL_CADENA`, una pasada por
la cadena con `utf8proc_iterate` construye una tabla `offsets[i]`
con la posición en bytes del code point `i`. La aritmética de
`inicio`/`fin`/`paso`/`clamp` reusa la del slicing de listas. El
resultado es una cadena nueva con los bytes seleccionados.

### F-cadenas triples y cadenas triples

`f"""..."""` y `f'''...'''` funcionan ahora (antes daban
`ErrorDeSintaxis: aún no soportadas`). Las cadenas triples sin `f`
también respetan los delimitadores (antes el compilador se confundía
y dejaba comillas residuales).

```cornamusa
reporte = f"""
Usuario: {usuario}
Saldo:   {redondear(saldo, 2)} EUR
"""
```

Implementación: el lexer ya tokenizaba las triples; solo había que
ajustar los offsets de cuerpo en `parsear_f_cadena()` y
`cadena_desde_lexema()`/evaluador para detectar el prefijo de tres
delimitadores.

### Fix: handler leak en `retornar` dentro de `intentar`

Bug preexistente expuesto al añadir slicing+re-raise: si una función
`retornar` desde dentro de un `intentar` (sin pasar por
`OP_INTENTAR_FIN`), el `HandlerFrame` quedaba registrado en
`vm->handlers`. La próxima excepción que aterrizase en el caller
disparaba ese handler obsoleto con un frame ya inexistente,
ejecutando el cuerpo del atrapar dos veces.

Fix en `OP_RETORNAR`: tras pop el frame, descartar todos los handlers
cuyo `frame_idx > vm->n_frames`. Una sola línea, propagación
limpia. También cubre `romper`/`continuar` que escapen de un
`intentar`.

Cambio colateral: varios errores de `OP_REBANADA` que antes eran
`return VM_ERROR_RUNTIME` directo ahora usan `RAISE_OR_DIE()` —
slicing inválido es atrapable.

### Tests

18 tests nuevos en
[tests/unit/test_bytecode_pulido.c](tests/unit/test_bytecode_pulido.c)
cubriendo las cuatro mejoras + el regression test del handler leak.
Total: **127 verde**.

Ejemplo: [examples/37_pulido.cor](examples/37_pulido.cor) con un
helper de slicing seguro que combina re-raise, slicing UTF-8 y
f-cadenas triples para reportes.

## [1.13.0] — 2026-05-10 — Context managers (`con`)

Llega la keyword `con` (`with` de Python) y los dunders
`__entrar__`/`__salir__`. Garantiza limpieza determinista de
recursos: locks, transacciones, conexiones, cualquier cosa que
necesite "abrir y siempre cerrar".

### Sintaxis

```cornamusa
con archivos.bloqueo("config.lock") como bloqueo:
    # Cuerpo. Si lanza excepción, __salir__() se ejecuta antes de propagar.
    procesar()
fin con
# Aquí el lock está garantizado liberado.
```

Equivale a:

```cornamusa
__cm_<linea>_<col> = archivos.bloqueo("config.lock")
bloqueo = __cm_<linea>_<col>.__entrar__()
intentar:
    procesar()
finalmente:
    __cm_<linea>_<col>.__salir__()
fin intentar
```

### Implementación: desugar en el parser

Sin nuevos opcodes ni cambios al compilador o VM. La función
`parsear_con()` en [src/parser.c](src/parser.c) construye el AST
equivalente de tres sentencias dentro de un `sent_bloque`:

1. Asignación del context manager a un nombre interno único
   (`__cm_<linea>_<columna>`) — colisión imposible con
   identificadores idiomáticos.
2. Llamada a `__entrar__`, opcionalmente asignada al alias.
3. `intentar/finalmente` con el cuerpo del usuario y la llamada a
   `__salir__` en `finalmente`.

Esto reusa toda la maquinaria existente. Las excepciones del cuerpo
se propagan tras `__salir__` (semántica idéntica a Python). Si
`__entrar__` lanza, `__salir__` NO se ejecuta — no entramos al
contexto.

### Cambio colateral: `OP_OBTENER_ATTR` ahora es atrapable

Al escribir tests para `con`, descubrimos que acceder a un atributo
inexistente (`obj.metodo_que_no_existe`) producía un error no
atrapable. Fix: las tres ramas de error de `OP_OBTENER_ATTR`
(módulo sin atributo, tipo sin atributos, instancia sin atributo)
usan ahora `RAISE_OR_DIE()`, consistente con el patrón de v1.10.
Esto significa:

```cornamusa
intentar:
    valor = obj.atributo_inexistente
atrapar Excepcion como e:
    imprimir(f"capturado: {e}")
fin intentar
```

ahora funciona como se espera.

### Limitaciones intencionales

- `__salir__` se invoca sin argumentos. Los context managers que
  necesitan distinguir "cuerpo terminó normal" vs "cuerpo lanzó"
  deberán esperar a v1.14+ con la firma extendida
  `__salir__(yo, tipo_exc, valor_exc, traceback)`.
- Sin lista de contextos: `con A, B:` no soportado. Encadenar
  manualmente con `con A: con B:`.
- Sin `archivos.abrir(ruta, modo)` que retorne handler. Eso requiere
  un nuevo tipo de Valor (file handle) — aplazado a v1.13.x con la
  introducción de `VAL_HANDLE_ARCHIVO` o equivalente.

### Tests

8 tests en [tests/unit/test_bytecode_con.c](tests/unit/test_bytecode_con.c):
con/sin alias, cuerpo lanza, `__entrar__` lanza (no llama a
`__salir__`), anidación con orden LIFO, errores cuando faltan
dunders, expresión compleja como context. Total: **124 verde**.

Ejemplo: [examples/36_con_recursos.cor](examples/36_con_recursos.cor)
con Lock simulado, Transacción y anidación.

## [1.12.0] — 2026-05-10 — Dunder `__iterar__`

Cualquier clase que defina `__iterar__` puede usarse en `para x en
obj`. Cierra el OOP idiomático: pilas, colas, rangos custom, árboles
recorridos, etc., son ahora directamente iterables sin código
auxiliar.

### Sintaxis

```cornamusa
clase Pila:
    funcion __iniciar__(yo):
        yo.items = []
    fin funcion

    funcion meter(yo, x):
        agregar(yo.items, x)
    fin funcion

    funcion __iterar__(yo):
        # Recorrido LIFO: del último al primero.
        resultado = []
        i = longitud(yo.items) - 1
        mientras i >= 0:
            agregar(resultado, yo.items[i])
            i = i - 1
        fin mientras
        retornar resultado
    fin funcion
fin clase

p = Pila()
p.meter(1); p.meter(2); p.meter(3)
para x en p:
    imprimir(x)  # 3, 2, 1
fin para
```

### Diseño: materialización, no streaming

`__iterar__` debe retornar **un iterable nativo** (lista, tupla,
conjunto, dicc, rango, cadena). La VM materializa al inicio del
bucle: el dunder se invoca una vez, y el iterador resultante recorre
ese valor con la maquinaria existente.

Ventajas:
- **Implementación trivial**: una sola modificación a `OP_ITER_INICIAR`
  con la técnica del rewind IP (retroceder 1 byte antes del dispatch
  para que el opcode se re-ejecute con el TOS reemplazado).
- **Sin nuevos opcodes**: `OP_ITER_SIGUIENTE` no cambia.
- **Encadenamiento gratis**: si `__iterar__` retorna otra instancia
  con `__iterar__`, la VM dispatcha hasta encontrar iterable nativo.
- **Errores atrapables**: clase sin `__iterar__` o `__iterar__` que
  retorna no-iterable producen `ErrorDeTipo` capturable.

Limitación intencional: no hay iteración lazy (`__siguiente__`). Para
colecciones gigantes el dunder materializa toda la secuencia en
memoria. Si llega demanda real (generadores, streams sobre archivos
grandes) se añadirá `__siguiente__` en una versión futura, probablemente
junto con `producir` (v1.15+).

### Implementación: rewind IP en OP_ITER_INICIAR

```c
case OP_ITER_INICIAR: {
    if (vm->tope[-1].tipo == VAL_INSTANCIA) {
        Closure *m = clase_obtener_metodo(...->clase, "__iterar__", 10);
        if (m) {
            frame->ip--;  /* re-ejecutar este opcode tras el dunder */
            ResultadoVM rc = ejecutar_dunder_unario(vm, &frame, m, ...);
            if (rc != VM_OK) RAISE_OR_DIE();
            break;
        }
        /* VAL_INSTANCIA sin __iterar__: ErrorDeTipo claro y atrapable. */
        ...
    }
    /* Path nativo (pre-v1.12). */
    ...
}
```

El rewind funciona porque `OP_RETORNAR` del dunder pop su frame y
deja el valor de retorno en el TOS, mientras el `frame->ip` del
caller apunta al byte que retrocedimos (el propio `OP_ITER_INICIAR`).
La segunda pasada del opcode ve el iterable nativo retornado por el
dunder y va por el camino normal — sin recursión ni sub-loops.

### Tests

13 tests en
[tests/unit/test_bytecode_iteradores.c](tests/unit/test_bytecode_iteradores.c):
listas, tuplas, rangos, cadenas, encadenamiento, anidación, iteración
sobre snapshot, errores. Total tests: **122 verde**.

Ejemplo:
[examples/35_iteradores.cor](examples/35_iteradores.cor) con Pila
LIFO, Cola FIFO, RangoPar, árbol binario in-order y errores
atrapables.

## [1.11.0] — 2026-05-10 — Funcionales y reflexión

Cierra el menú histórico de built-ins reservados en
[ESPEC §4.2](ESPEC.md#42-built-ins-planeados-no-en-v110): nueve nuevas
operaciones distribuidas entre el módulo `funcionales` (Cornamusa
puro) y seis built-ins en C. Sin sintaxis nueva — todo es valor
agregado a la stdlib.

### Nuevo módulo `funcionales`

```cornamusa
importar funcionales

dobles = funcionales.mapear(lambda x: x * 2, [1, 2, 3])           # [2, 4, 6]
pares  = funcionales.filtrar(lambda x: x % 2 == 0, rango(10))     # [0, 2, 4, 6, 8]
total  = funcionales.reducir(lambda a, x: a + x, [1, 2, 3, 4], 0) # 10

para par en funcionales.enumerar(["a", "b"]):
    imprimir(par[0], par[1])              # 0 a / 1 b
fin para

imprimir(funcionales.suma([1, 2, 3]))      # 6
imprimir(funcionales.minimo([3, 1, 2]))    # 1
imprimir(funcionales.maximo([3, 1, 2]))    # 3
imprimir(funcionales.cualquiera(lambda x: x > 5, [1, 2, 6]))  # verdadero
imprimir(funcionales.todos(lambda x: x > 0, [1, 2, 3]))       # verdadero
```

`enumerar_desde(xs, inicio)` y `suma_desde(xs, inicial)` son las
variantes con punto de partida explícito (los argumentos por defecto
todavía no funcionan en bytecode). Las funciones de orden superior
viven en [stdlib/funcionales.cor](stdlib/funcionales.cor) escritas en
Cornamusa puro: la firma actual de `FnNativa` no permite invocar
callables Cornamusa desde C, pero el bucle `para ... en` resuelve la
llamada en cada iteración sin sobrecoste.

### Built-ins globales nuevos en C

```cornamusa
imprimir(absoluto(-5))           # 5
imprimir(absoluto(-3.14))        # 3.14
imprimir(absoluto(-(2 ** 100)))  # bignum exacto

imprimir(redondear(3.7))         # 4
imprimir(redondear(2.5))         # 3   (half-away-from-zero, no banker's)
imprimir(redondear(-2.5))        # -3
imprimir(redondear(3.14159, 2))  # 3.14

imprimir(repr("hola"))           # "hola"  (con comillas, distingue de cadena())
imprimir(repr([1, "x"]))         # [1, "x"]
```

### Reflexión sobre clases

```cornamusa
clase Animal:
    funcion __iniciar__(yo, nombre):
        yo.nombre = nombre
    fin funcion
fin clase

clase Perro extiende Animal: ... fin clase

p = Perro("Toby")
imprimir(instancia_de(p, Perro))     # verdadero
imprimir(instancia_de(p, Animal))    # verdadero (vía herencia)
imprimir(instancia_de(5, Animal))    # falso
imprimir(subclase_de(Perro, Animal)) # verdadero
imprimir(subclase_de(Animal, Animal)) # verdadero (reflexivo)

# Identidad por referencia
otro = p
imprimir(id(p) == id(otro))     # verdadero
imprimir(id(p) == id(Perro("Toby")))  # falso (otra instancia)
```

`instancia_de` walks la cadena de superclases. Para tipos primitivos
(entero, cadena, etc.) retorna `falso` — usar `tipo(x) == "entero"`
para chequear primitivos. `id` retorna el puntero del objeto cast a
entero (estable durante la vida del objeto, no entre ejecuciones).

### Limitaciones documentadas

- Argumentos por defecto siguen sin funcionar en bytecode (heredado
  de v0.7). Las firmas opcionales se exponen como funciones distintas
  (`enumerar` / `enumerar_desde`).
- `mapear`/`filtrar`/`reducir` tienen overhead por invocación
  (cada elemento dispara un frame de bytecode). Para hot loops de
  millones de elementos un `para ... en` directo es más rápido.
- `redondear` con bignum convierte a double (puede perder precisión
  > 2^53). Ortogonalidad con `entero(decimal)`.
- `id` para tipos por valor (entero, decimal, cadena, etc.) retorna
  un valor derivado del contenido — no es identidad de referencia
  estable como en Python para inmutables.

### Tests

35 tests unit nuevos en
[tests/unit/test_bytecode_funcionales.c](tests/unit/test_bytecode_funcionales.c)
para los seis built-ins C. Las funciones `funcionales.*` se cubren
end-to-end vía [examples/34_funcionales.cor](examples/34_funcionales.cor).
Total tests: 120 (estable desde v1.10).

## [1.10.0] — 2026-05-01 — Errores atrapables en built-ins

Cierra una **limitación documentada desde v1.1**: los errores de
runtime en built-ins (`archivos.leer("no existe")`, `json.parsear("...")`,
`longitud(42)`, `5 + "hola"`, etc.) ahora son **atrapables** vía
`intentar/atrapar`. Combinado con v1.8 (`archivos`) y v1.9 (`json`),
esto desbloquea programas robustos: cargar config con fallback,
validar input, recuperarse de I/O fallido.

### Antes (v1.9)

```cornamusa
intentar:
    archivos.leer("/no/existe")
atrapar Excepcion como e:
    imprimir("nunca se ejecuta")
fin intentar
# El programa terminaba con ErrorDeIO antes de llegar a `atrapar`.
```

### Ahora (v1.10)

```cornamusa
funcion cargar_config(ruta):
    intentar:
        retornar json.parsear(archivos.leer(ruta))
    atrapar ErrorDeIO como e:
        imprimir(f"  ! archivo no encontrado, defaults")
        retornar {"puerto": 8080}
    atrapar ErrorDeValor como e:
        imprimir(f"  ! JSON corrupto: {e}")
        retornar {"puerto": 8080}
    fin intentar
fin funcion

cfg = cargar_config("/no/existe.json")  # cae al primer atrapar
```

### Implementación

Dos piezas en [src/vm.c](src/vm.c):

1. **`vm_lanzar_excepcion(vm, frame, e)`**: helper compartido que
   hace todo el unwind (cierra upvalues, descarta stack, pop frames),
   restaura `vm->globales` para cada frame descartado que swapeó
   (función de módulo importado), empuja la excepción al stack del
   handler y salta a `ip_handler`. Refactor del flujo previo de
   `OP_LANZAR` que tenía un bug preexistente (no restauraba globales).

2. **`intentar_atrapar_error_nativa(vm, &frame)`**: detecta error
   activo en `vm->error`, parsea el prefijo `"ClaseDeError: detalle"`,
   construye `Excepcion` con esa clase y mensaje, y dispatch al
   handler activo. Si no hay handler o falla la conversión, retorna
   false y el caller mantiene el comportamiento legacy (terminar).

3. **`RAISE_OR_DIE()` macro**: reemplaza el patrón
   `return VM_ERROR_RUNTIME` post-`VM_ERROR(...)` en el dispatch loop.
   Si hay handler, dispatch al handler con `goto raise_atrapado`. Si
   no, `return VM_ERROR_RUNTIME`.

Aplicado a opcodes con errores semánticamente atrapables:
- Operadores binarios slow path (`evaluador_aplicar_binario`).
- Operadores unarios (`OP_NEGAR`, `OP_NO`).
- `OP_LONGITUD` (atajo a `longitud(arg)`).
- `OP_ASEGURAR_CADENA` (validación de `__cadena__`).
- Llamadas a nativas (`ejecutar_llamar_nativa` actualizado a
  `frame_inout`).

Otros opcodes mantienen `return VM_ERROR_RUNTIME` directo (estado
interno corrupto, desbordamiento de pila — bugs no atrapables).

### Convención de mensajes

Las nativas siguen el patrón `"ClaseDeError: detalle"` desde v1.0+. El
parser de `intentar_atrapar_error_nativa` usa el primer `:` como
separador. Si no hay `:`, la clase es `"Excepcion"` genérica.

Clases observadas en runtime:
- `ErrorDeTipo`: argumento de tipo incorrecto.
- `ErrorDeValor`: tipo correcto pero valor inválido.
- `ErrorDeIndice`: índice fuera de rango.
- `ErrorDeClave`: clave no presente en diccionario.
- `ErrorDeIO`: error de archivo (no existe, permisos).
- `ErrorAritmetico`: división por cero, etc.

### Bug fix incidental

El unwind de `OP_LANZAR` no restauraba `vm->globales` cuando
descartaba frames de funciones importadas desde módulos. Si una
excepción se lanzaba desde dentro de `archivos.leer()` y se
atrapaba en el caller, `vm->globales` quedaba apuntando al dicc del
módulo `archivos`, y los globales del caller (incluido `archivos`
mismo) dejaban de ser accesibles. v1.10 lo corrige iterando los
frames descartados y restaurando cada `globales_pre_llamada`.

### Tests y ejemplo

- 8 tests unit en
  [tests/unit/test_bytecode_atrapar.c](tests/unit/test_bytecode_atrapar.c):
  errores de tipo (longitud, suma), I/O (archivo inexistente), valor
  (JSON inválido), atrapar por clase específica, múltiples atrapados
  consecutivos, globales preservados tras unwind a través de módulo,
  excepción como valor manipulable.
- [examples/33_atrapar_robusto.cor](examples/33_atrapar_robusto.cor):
  patrón típico `cargar_config con fallback` con archivo inexistente,
  JSON corrupto y JSON válido. Demuestra que el programa SIGUE VIVO
  tras múltiples errores.
- 116/116 tests pasan.

### Limitaciones

- **Solo se atrapan errores generados POR el dispatch loop principal**
  (operadores, OP_LONGITUD, llamadas a nativas, etc.). Algunos errores
  internos profundos (corrupción de estado) siguen siendo fatales — son
  bugs, no condiciones de error semántico.
- **El parser de `"Clase: mensaje"` es greedy del primer `:`**. Si un
  mensaje contiene `:` antes del prefijo de clase, podría parsearse
  raro. Las nativas siguen una convención estricta así que no es
  problema real.
- **Sin re-raise automático**: el bloque `atrapar` consume la
  excepción. Para re-lanzar usa `lanzar e` explícito.

## [1.9.0] — 2026-05-01 — Stdlib `json` (intercambio universal)

Complemento natural de v1.8: tras añadir lectura/escritura de
archivos, ahora un programa Cornamusa puede leer configs JSON y
emitir respuestas JSON. Resuelve la pregunta filosófica de cómo
mantener la identidad castellana del lenguaje frente a un formato
universal anglófono.

### Filosofía: identidad sin aislamiento

JSON es un formato de intercambio universal (RFC 8259) — no un
lenguaje. Cornamusa preserva su identidad castellana en CÓDIGO (los
literales `verdadero/falso/nulo` siguen igual) pero acepta JSON
estándar para interoperar con configs, APIs y datasets externos.

**El usuario nunca ve `true/false/null` en su código Cornamusa**, solo
en archivos JSON externos. La traducción es automática:

```cornamusa
importar archivos
importar json

cfg = json.parsear(archivos.leer("config.json"))
si cfg["debug"] == verdadero:           # ← castellano puro,
    imprimir(f"version: {cfg['version']}") #   aunque el JSON dijera "true"
fin si

archivos.escribir("salida.json", json.serializar({
    "estado": verdadero,
    "datos": [1, 2, nulo]
}))                                     # ← serializa a true/null automáticamente
```

### Built-ins nuevos (2)

- `json_parsear(cadena)` → Valor. Parsea JSON estándar y devuelve
  el valor Cornamusa correspondiente. Auto-traduce
  `null/true/false ↔ nulo/verdadero/falso`.
- `json_serializar(valor)` → cadena. Recursivo. Tipos no
  serializables (instancias, funciones, rangos, claves no-cadena en
  diccionarios) → ErrorDeTipo claro.

### Mapeo (Cornamusa ↔ JSON)

| Cornamusa | JSON |
|---|---|
| `nulo` | `null` |
| `verdadero` | `true` |
| `falso` | `false` |
| `entero`/`decimal` | `number` |
| `cadena` | `string` |
| `lista`/`tupla` | `array` (tupla → array; al re-parsear es lista) |
| `diccionario` con claves cadena | `object` |
| Otros (instancia, función, rango, conjunto) | ErrorDeTipo |

### Implementación

- Parser recursivo descendente en
  [src/nativos.c](src/nativos.c) `JsonParser`. Maneja todos los tipos
  RFC 8259 + escapes (`\n`, `\t`, `\"`, `\\`, `\/`, `\b`, `\f`,
  `\uXXXX` con conversión correcta a UTF-8 para BMP).
- Serializer con buffer dinámico `JsonOut` que escala hasta lo que
  necesite. Escapa caracteres de control y comillas; deja UTF-8
  multibyte tal cual (válido en JSON).
- Validación anti-NaN/Inf en serializer (no son JSON válido).

### Módulo `stdlib/json.cor`

```cornamusa
importar json

dato = json.parsear("...")
texto = json.serializar(obj)
```

### Tests y ejemplos

- 11 tests unit en
  [tests/unit/test_bytecode_json.c](tests/unit/test_bytecode_json.c):
  parse de primitivos, arrays, objects, anidación, escapes (incluido
  `\uXXXX`), round-trip preservando nulo/verdadero/falso, errores
  de sintaxis, errores de tipo en serializer.
- [examples/32_json_archivos.cor](examples/32_json_archivos.cor):
  patrón completo escribir → leer crudo → parsear → modificar → escribir.
- 113/113 tests pasan.

### Limitaciones documentadas

- **Surrogates UTF-16 no manejados**: `😀` (emoji) se
  parsea como dos codepoints separados. RFC 8259 los permite pero
  son raros; iterar a manejo completo si surge demanda.
- **Errores no atrapables**: `json.parsear("...")` con JSON inválido
  termina el programa (limitación preexistente de las nativas).
  Pendiente para v1.x: refactor que permita
  `intentar json.parsear(...) atrapar Excepcion como e: ...`.
- **Sin pretty-print**: el serializer emite JSON compacto sin
  indentación. Aceptable para intercambio entre programas; iterar
  a `json.serializar(obj, indentar=2)` si se requiere para humanos.

## [1.8.0] — 2026-05-01 — Stdlib `archivos` (I/O persistente)

Pivot de optimización a features tras 4 experimentos de perf que
mostraron returns decrecientes (v1.5–v1.7 + -O3). v1.8 añade lo que
los programas reales más necesitan: persistencia de datos.

### Built-ins nuevos (5)

- `archivo_leer(ruta)` → cadena con todo el contenido. Lectura
  binaria, dimensionada exactamente con `fseek`/`ftell`.
- `archivo_escribir(ruta, contenido)` → nulo. Trunca el archivo si
  existe.
- `archivo_lineas(ruta)` → lista de cadenas, una por línea (split por
  `\n`, sin la nueva línea final). La línea final con `\n` no produce
  cadena vacía adicional (estilo `readlines` Python).
- `archivo_existe(ruta)` → booleano. NO distingue entre "no existe"
  y "permisos negados" — ambos retornan falso.
- `archivo_agregar(ruta, contenido)` → nulo. Append; crea el archivo
  si no existe. Útil para logs.

### Módulo `stdlib/archivos.cor`

Reexporta los built-ins con nombres amigables:

```cornamusa
importar archivos

# Patrón típico:
si archivos.existe("config.txt"):
    contenido = archivos.leer("config.txt")
    para linea en archivos.lineas("config.txt"):
        imprimir(linea)
    fin para
fin si

archivos.escribir("salida.txt", "hola mundo")
archivos.agregar("log.txt", f"[{fecha}] evento\n")
```

### Limitaciones documentadas

- **Errores no atrapables**: si fopen/fread/fwrite falla, el programa
  termina con `ErrorDeIO`. Las nativas no soportan `intentar/atrapar`
  (limitación preexistente). Para v1.8 esto es aceptable: usa
  `archivos.existe(ruta)` para chequear antes de leer.
- **Encoding**: bytes crudos. Cornamusa cadenas son UTF-8; estas
  funciones no validan ni convierten. Archivos en otro encoding
  (UTF-16, latín-1) llegan como bytes mal formados.
- **Sin streaming**: `archivos.leer` carga todo el contenido en
  memoria. Para archivos muy grandes (>100MB) esta API no es
  apropiada — iterar `archivos.lineas` también materializa primero.

### Tests y ejemplos

- 6 tests unit en
  [tests/unit/test_bytecode_archivos.c](tests/unit/test_bytecode_archivos.c):
  round-trip, existe, lineas, agregar, errores de tipo, módulo wrapper.
- [examples/31_archivos.cor](examples/31_archivos.cor): demo
  completa con escribir/leer/lineas/append/existe.
- 110/110 tests pasan (nuevo `bc_run_31_archivos` integración).

### Direcciones para v1.9+

- **`json` stdlib**: parse + serialize. Combinable con `archivos` da
  un patrón completo para configs y persistencia ligera.
- **Errores atrapables en nativas**: refactor que permita
  `intentar archivos.leer(...) atrapar Excepcion como e: ...`.
- **Streaming**: API basada en handlers (`con archivos.abrir(ruta)
  como f:`) cuando lleguen los context managers.
- **Funcionales** (`mapear`/`filtrar`/`reducir`/`enumerar`).

## [1.7.0] — 2026-05-01 — Inline path con constructor (cierre del experimento OOP)

Última iteración de la serie de optimizaciones específicas de OOP
iniciada en v1.5. Implementa el patrón más ambicioso (`retornar
V(yo.A OP otro.B, yo.C OP2 otro.D)` con `__iniciar__` trivial) y
reporta honestamente los resultados.

### Patrón soportado

```cornamusa
clase V:
    funcion __iniciar__(yo, a, b):    # ← detectado: INIT_INLINE_TRIVIAL_2
        yo.a = a
        yo.b = b
    fin funcion

    funcion __sumar__(yo, otro):       # ← detectado: DUNDER_INLINE_BIN_CTOR_2
        retornar V(yo.a + otro.a, yo.b + otro.b)
    fin funcion
fin clase
```

Cuando AMBAS condiciones se cumplen, la VM ejecuta `v + w`
literalmente sin crear ningún `CallFrame`: aloca la instancia, lee
los 4 atributos, calcula los 2 args, asigna, push.

### Resultado medido (honestidad sobre returns decrecientes)

- v1.6 baseline (frame normal): mediana 0.1016s.
- v1.7 con fast path constructor: mediana 0.0960s.
- **Speedup ~1.06x — bordeando el ruido de medición.**

Este es el tercer experimento consecutivo en la serie OOP-perf:
- v1.5 (cache de lookup): ~5%, descartado.
- v1.6 (inline unario): ~17% en bucles tight con `__cadena__`/`__longitud__`.
- v1.7 (inline constructor): ~6%.

**Lección aprendida**: el bytecode dispatch de Cornamusa con sus IC
F10 (atributos cacheados) y opcodes especializados (OP_LLAMAR_CLASE)
ya es muy eficiente. Las micro-optimizaciones específicas para OOP
tienen returns decrecientes. Para acelerar OOP de verdad hace falta
atacar el VM dispatch global o el allocator de instancias —
optimizaciones VM-wide, no per-pattern.

### Implementación

- `TipoDunderInline`: nuevos valores `INIT_INLINE_TRIVIAL_2` y
  `DUNDER_INLINE_BIN_CTOR_2` en
  [src/chunk.h](src/chunk.h). Campos extras en `DunderInlineDesc`
  para nombre de clase y arg2 del constructor.
- Detector ampliado en
  [src/compilador.c](src/compilador.c):
  - `detectar_init_inline`: cuerpo es 2 asignaciones `yo.A = pK`.
  - `detectar_dunder_ctor`: cuerpo es `retornar IDENT(yo.A OP otro.B,
    yo.C OP2 otro.D)`.
  - Helper `extraer_attr_op_attr` reutilizable para los args.
- Fast path en
  [src/vm.c](src/vm.c) slow path de operadores binarios:
  resuelve la clase por nombre en globales, verifica que su
  `__iniciar__` sea trivial, lee 4 atributos, calcula 2 args, crea
  instancia y asigna. Si cualquier condición falla (clase no es VAL_CLASE,
  init no trivial, atributos faltantes), cae al frame normal.

### Restricciones del patrón

- Constructor debe tener exactamente 2 args (después de `yo`).
- `__iniciar__` debe ser exactamente `yo.A = p1; yo.B = p2`.
- Ambos args del constructor deben ser `yo.X OP otro.Y`.
- Los 4 atributos involucrados deben existir en las instancias en
  runtime.
- Operadores soportados: aritméticos y comparación.

### Tests y compatibilidad

- 2 tests nuevos en
  [tests/unit/test_bytecode_dunders.c](tests/unit/test_bytecode_dunders.c):
  fast path activo con `__iniciar__` trivial; fallback al frame
  normal cuando `__iniciar__` no es trivial.
- 109/109 tests pasan (sin regresión).

### Direcciones para v1.8+

Tras tres iteraciones, **es hora de pivotar**. Los siguientes pasos
con mayor ROI documentado:

1. **Threaded code dispatch (computed gotos)** — 10-15% global, no
   específico de OOP. Beneficia TODO el código.
2. **Pool de mp_int** — 1.2-1.5x en bignum-heavy.
3. **Volver a features**: dunders de iteración, stdlib (archivos,
   json), funcionales (mapear/filtrar).

## [1.6.0] — 2026-05-01 — Inline path unario (`__cadena__` y `__longitud__`)

Extiende la optimización de v1.5 al patrón unario `retornar yo.A`.
Aplica a `__cadena__` y `__longitud__` cuando el cuerpo del dunder
simplemente devuelve un atributo. La VM lee el atributo directo
desde el diccionario de la instancia y empuja el resultado, sin
crear `CallFrame`.

### Patrón soportado

```cornamusa
clase Wrapper:
    funcion __iniciar__(yo, t):
        yo.t = t
    fin funcion

    funcion __cadena__(yo):
        retornar yo.t       # ← detectado, fast path
    fin funcion

    funcion __longitud__(yo):
        retornar yo.t       # ← idem
    fin funcion
fin clase

w = Wrapper("hola")
imprimir(w)             # invoca fast path inline para __cadena__
imprimir(longitud(w))   # invoca fast path inline para __longitud__
```

### Implementación

- Nuevo valor `DUNDER_INLINE_UNARIO_ATTR` en `TipoDunderInline`
  ([src/chunk.h](src/chunk.h)).
- Detector ampliado en
  [src/compilador.c](src/compilador.c): si el cuerpo de un método
  con aridad 1 es exactamente `retornar yo.A`, llena el descriptor.
- Fast path en [src/vm.c](src/vm.c) en los handlers de `OP_FORMATO_F`
  y `OP_LONGITUD`: tras encontrar el dunder, si está marcado como
  unario inline, lee atributo via `dicc_obtener` y empuja sin frame.

### Restricciones

- Solo aridad 1 (`yo`).
- Cuerpo es exactamente UN `retornar EXPR_ATRIBUTO` sobre `yo`.
- Si el atributo no está en la instancia, cae al frame normal (sin
  regresión).

### Tests y compatibilidad

- 3 tests nuevos en
  [tests/unit/test_bytecode_dunders.c](tests/unit/test_bytecode_dunders.c):
  `__cadena__` inline, `__longitud__` inline, cuerpo no trivial
  fuera de patrón.
- 109/109 tests pasan.
- API: el descriptor reusa `DunderInlineDesc` ya existente con un
  nuevo tipo enum. Sin cambios en API pública.

### Aplazado a v1.7+

El patrón con constructor (`retornar V(yo.A OP otro.B, yo.C OP2 otro.D)`)
sería el siguiente paso lógico — aceleraría el caso real más común
(`Vector + Vector`). Requiere recursión en el detector y materialización
del constructor en VM, lo cual añade complejidad significativa. Queda
documentado para iteraciones futuras.

## [1.5.0] — 2026-05-01 — Inline path para dunders triviales

Primera optimización de rendimiento del modelo OOP. Tras profilear el
dispatch de dunders (commit 7cf37e9 con benchmarks dedicados) descubrí
que el cuello NO es el lookup de método (~50-100ns) sino la
preparación del CallFrame (~500ns: ~15 writes + memmove + swap
globales). Esta versión ataca exactamente ese cuello para los dunders
suficientemente simples.

### El plan original que fue descartado

Inicialmente intenté un cache de lookup (estilo F10 IC para
atributos): cachear el `Closure *` resuelto por `clase_obtener_metodo`
y saltar el lookup en hits subsiguientes. Speedup medido: ~5%, perdido
en ruido. El lookup ya era barato; el costo dominante estaba en otra
parte.

Los benchmarks dedicados quedaron en
[benchmarks/oo_dunder_aritmetico.cor](benchmarks/oo_dunder_aritmetico.cor)
y [benchmarks/oo_dunder_indice.cor](benchmarks/oo_dunder_indice.cor)
para validar futuras optimizaciones.

### Inline path para dunders triviales

Los dunders más simples encajan en patrones reconocibles en
compile-time. Para `retornar yo.A OP otro.B` (operación binaria
trivial entre atributos), el compilador detecta el patrón y la VM
ejecuta inline sin crear `CallFrame`:

```cornamusa
clase Persona:
    funcion __iniciar__(yo, edad):
        yo.edad = edad
    fin funcion

    funcion __menor__(yo, otra):
        retornar yo.edad < otra.edad     # ← detectado, fast path
    fin funcion
fin clase

# `ana < luis` evita memmove + swap globales + frame init.
```

**Speedup medido**: 1.17x para el caso trivial. Modesto pero real, y
acumulativo para programas que comparan/ordenan instancias en bucles
hot.

### Implementación

- Nuevo struct `DunderInlineDesc` en
  [src/chunk.h](src/chunk.h), incrustado en `FuncionBC`. Tipo + nombres
  de atributos (heap-duplicados) + token del operador.
- Detector `detectar_inline_dunder` en
  [src/compilador.c](src/compilador.c) inspecciona el AST del cuerpo
  tras compilarlo. Patrón soportado:
  `SENT_BLOQUE { SENT_RETORNAR { EXPR_BINARIO(EXPR_ATRIBUTO yo,
   op, EXPR_ATRIBUTO otro) } }` con `op` aritmético o de comparación.
- Fast path en
  [src/vm.c](src/vm.c) slow path de operadores binarios: si
  `m->plantilla->inline_desc.tipo == DUNDER_INLINE_BIN_ATTR_OP_ATTR`
  Y ambos operandos son `VAL_INSTANCIA`, lee atributos directos via
  `dicc_obtener` y aplica `evaluador_aplicar_binario` sin frame.

### Restricciones del patrón inline

- Solo aridad 2 (`yo, otro`).
- Cuerpo es exactamente UN `retornar` con expresión binaria.
- Operandos son `EXPR_ATRIBUTO` sobre los IDENT del primer y segundo
  parámetro respectivamente.
- Operadores soportados: `+`, `-`, `*`, `/`, `//`, `%`, `**`, `==`,
  `!=`, `<`, `<=`, `>`, `>=`.
- Si los atributos no están en la instancia (poco común), se cae al
  frame normal — el dunder real reporta el error correcto.

Casos NO inlinados (siguen por frame normal — sin regresión):
- Cuerpo con múltiples sentencias.
- Operandos que no son atributos directos (e.g. constructores como
  `V(yo.a + otro.a, ...)`).
- Llamadas a otros métodos.
- Dunders con aridad ≠ 2 (`__cadena__`, `__longitud__`, etc.).

### Tests y compatibilidad

- 3 tests nuevos en
  [tests/unit/test_bytecode_dunders.c](tests/unit/test_bytecode_dunders.c):
  inline path con suma, comparación y multiplicación + verificación de
  que cuerpos no triviales siguen funcionando.
- 109/109 tests pasan (sin regresión).
- API: el descriptor es interno; ningún cambio en API pública.

### Direcciones para v1.6+

El inline path actual cubre solo un patrón. Extensiones razonables:
- Patrón unario: `retornar yo.A` (`__cadena__` que envuelve un atributo).
- Patrón ternario: `retornar yo.A si cond sino yo.B`.
- Cuerpo con UNA llamada: `retornar V(yo.A OP otro.B, ...)` —
  aceleraría los casos "real" actuales (`Vector + Vector`) si se
  detectara el patrón de constructor.

## [1.4.0] — 2026-05-01 — `nolocal` + cobertura de tests

Cierra el modelo de closures con la declaración explícita `nolocal` y
añade tests adicionales recomendados por la revisión post-v1.2 sobre
herencia de dunders, `super.dunder`, aridad incorrecta y casos edge.

### `nolocal` (declaración de variable de scope envolvente)

```cornamusa
funcion contador(inicial):
    n = inicial
    funcion incrementar():
        nolocal n
        n = n + 1
        retornar n
    fin funcion
    retornar incrementar
fin funcion

c = contador(10)
imprimir(c())   # 11
imprimir(c())   # 12
```

**Diferencia con Python**: en Cornamusa la asignación a una variable
existente en un scope envolvente YA va a ese scope por default
(semántica Lua, no Python). `nolocal` no cambia el comportamiento;
es **declaración explícita** que valida que el nombre exista en
algún scope padre y documenta la intención del autor.

Ventajas:
- **Validación temprana**: `nolocal contador` cuando el padre tiene
  `cuenta` falla en compile-time, no en runtime.
- **Lectura más clara**: marca explícitamente "esta variable es del
  scope envolvente, no se va a crear local nueva".

Reglas:
- Solo se permite dentro de una función (no en el scope raíz).
- El nombre NO debe ya ser local del scope actual.
- El nombre DEBE existir como local en algún scope envolvente.
- Múltiples nombres separados por coma: `nolocal a, b, c`.

Implementación:
- El lexer y parser ya soportaban `nolocal` desde v0.x (lista de
  `Nombre`s reservada). v1.4 implementa el handler en
  [src/compilador.c](src/compilador.c) caso `SENT_NOLOCAL`.
- Nuevo array `nolocales[]` en `ScopeCompilador` (en
  [src/compilador.h](src/compilador.h)) que registra los nombres
  declarados.
- Sin nuevos opcodes — el efecto se obtiene reutilizando
  `resolver_upvalue` y `OP_ASIGNAR_UPVALUE` que ya existían.

### Tests adicionales (recomendaciones del review post-v1.2)

8 tests nuevos en [tests/unit/test_bytecode_dunders.c](tests/unit/test_bytecode_dunders.c):

- `test_herencia_de_dunder`: `Hijo extiende Base` hereda `__sumar__`.
- `test_super_dunder`: `super.__sumar__(otro)` desde un Hijo invoca
  el dunder del padre.
- `test_aridad_incorrecta`: `__sumar__(yo)` (aridad 1) → ErrorDeTipo
  con mensaje claro al invocar.
- `test_dunder_con_error_runtime`: `__sumar__` que divide por cero —
  el error se propaga limpiamente, sin corromper la VM.
- `test_indice_clave_no_entera`: `obj["clave"]` con `__indice__`
  definido pasa la cadena al dunder sin chequeo de tipo del path
  nativo de listas.
- `test_lado_derecho_sin_reflejado_da_error`: `5 + V(...)` sin
  `__sumar__` ni `__sumar_derecho__` da ErrorDeTipo.
- `test_nolocal_basico` y `test_nolocal_validacion`: contador
  clásico + validación temprana.

### Ejemplo

[examples/30_closures_nolocal.cor](examples/30_closures_nolocal.cor):
contador, sumador, memoización con `cache`, toggle con dos closures
que comparten estado.

### Compatibilidad

- Sintaxis: sin cambios. `nolocal` ya estaba reservada como palabra
  clave; programas v1.3 que (por error) usaron `nolocal` como
  identificador habrían fallado a parsear ya en v1.3.
- Comportamiento observable: programas v1.3 funcionan idéntico. Los
  que NO usaban `nolocal` siguen funcionando (la regla por default
  ya hacía closure-write). Los que SÍ la usaban antes (poco probable)
  ahora obtienen validación.

108/108 tests pasan.

## [1.3.0] — 2026-05-01 — Dunders avanzados (reflejados, __llamar__, __longitud__)

Cierra el modelo OOP de Cornamusa. v1.2 implementó los dunders básicos
(aritméticos, comparación, `__cadena__`, indexación). v1.3 añade los
restantes: operadores reflejados (5 + V), instancias callable
(`obj(args)` invoca `__llamar__`) y `longitud(obj)` invoca
`__longitud__`.

### Operadores reflejados

Cuando el lado izquierdo NO maneja el operador (no es instancia o no
tiene el dunder normal), se busca el dunder reflejado en el lado
derecho. Permite escribir `5 + V(...)` cuando V define
`__sumar_derecho__`.

Dunders reflejados implementados (solo aritméticos):
`__sumar_derecho__`, `__restar_derecho__`, `__multiplicar_derecho__`,
`__dividir_derecho__`, `__dividir_entero_derecho__`,
`__modulo_derecho__`, `__potencia_derecho__`.

Comparaciones (`<`, `>`, etc.) NO tienen reflejado dedicado — el
usuario invierte el operador (`b > a` en lugar de buscar
`__menor_derecho__` en a).

Implementación: nuevo helper `ejecutar_dunder_binario_reflejado` en
[src/vm.c](src/vm.c) que reorganiza la pila a `[..., closure, der, izq]`
(receptor=der, arg=izq). El slow path de operadores binarios
consulta `dunder_para_op_binario_reflejado` cuando el directo no
aplica.

### `__llamar__` (instancias callable)

```cornamusa
clase Multiplicador:
    funcion __iniciar__(yo, factor):
        yo.factor = factor
    fin funcion
    funcion __llamar__(yo, n):
        retornar n * yo.factor
    fin funcion
fin clase

doblar = Multiplicador(2)
imprimir(doblar(5))   # 10
```

Implementación: nuevo case `VAL_INSTANCIA` en `OP_LLAMAR` slow path
que delega a `ejecutar_llamar_instancia` — busca `__llamar__` en la
clase, prepara CallFrame con receptor=instancia y los args
desplazados un slot. NO promueve a opcode especializado (el camino es
poco frecuente; el dispatch genérico se mantiene).

### `__longitud__`

```cornamusa
clase Pila:
    funcion __iniciar__(yo):
        yo.items = []
    fin funcion
    funcion __longitud__(yo):
        retornar longitud(yo.items)
    fin funcion
fin clase

p = Pila()
imprimir(longitud(p))   # 0 via __longitud__
```

Implementación: nuevo opcode `OP_LONGITUD` (`src/vm.c`) y atajo del
compilador para `longitud(arg)` con un solo argumento (similar al
atajo de `imprimir`/`cadena`). El handler busca `__longitud__` en la
clase y delega al dunder, o cae a la lógica de la nativa para tipos
primitivos. Helper compartido `nativos_calcular_longitud`
(`src/nativos.h`) factoriza la lógica.

### `cadena()` ahora invoca `__cadena__`

Fix de inconsistencia detectado en revisión post-v1.2: `f"{obj}"` y
`imprimir(obj)` invocaban `__cadena__` pero `cadena(obj)` lo ignoraba.
v1.3 añade el atajo del compilador para `cadena(arg)` que emite
`OP_FORMATO_F + OP_ASEGURAR_CADENA`, igual que las otras dos formas.
La nativa `cadena` queda como fallback indirecto (`f = cadena; f(x)`).

### Tests y ejemplos

- 9 tests nuevos en [tests/unit/test_bytecode_dunders.c](tests/unit/test_bytecode_dunders.c):
  reflejados, `__llamar__` con varios arity, sin-dunder, `__longitud__`,
  `cadena()` con dunder.
- [examples/29_oop_avanzado.cor](examples/29_oop_avanzado.cor): Vec
  con suma reflejada, Multiplicador callable, Pila con
  `__longitud__`, Contador con estado interno.

107/107 tests pasan.

### Refactors menores

- Comentario obsoleto en `OP_FORMATO_F` actualizado para reflejar la
  coordinación con `OP_ASEGURAR_CADENA` (estaba diciendo que NO
  existía un opcode validador, cuando ya se había añadido en v1.2).
- `nativa_longitud` ahora delega en `nativos_calcular_longitud` —
  elimina la duplicación con el handler de `OP_LONGITUD`.
- Tests de IC (`test_bytecode_ic.c`): cambiado de `longitud` a `tipo`
  como ejemplo de "nativa cualquiera" (`longitud` ahora se atajea a
  OP_LONGITUD por el compilador).

### Compatibilidad

- API: nuevos `clase_obtener_metodo`, `nativos_calcular_longitud`. El
  resto de la API pública sin cambios.
- Comportamiento observable: programas v1.2 siguen funcionando idéntico.
  Adicionalmente ahora `cadena(obj)` invoca `__cadena__` y `5 + V(...)`,
  `obj(args)`, `longitud(obj)` funcionan donde antes daban ErrorDeTipo.

## [1.2.0] — 2026-05-01 — Dunders aritméticos y de coerción

Hace que el OOP de Cornamusa sea idiomático. HOY (v1.1) `Vector + Vector`
con `__sumar__` definido NO funcionaba — el operador `+` no buscaba el
dunder. v1.2 cierra esa promesa: los operadores binarios, las
comparaciones, `__cadena__` (en f-strings y `imprimir`) y `obj[k]` /
`obj[k] = v` ahora delegan en dunders cuando están definidos.

### Nuevos dunders soportados

- **Aritméticos binarios**: `__sumar__`, `__restar__`, `__multiplicar__`,
  `__dividir__`, `__dividir_entero__`, `__modulo__`, `__potencia__`.
- **Comparación**: `__igual__`, `__distinto__`, `__menor__`,
  `__menor_igual__`, `__mayor__`, `__mayor_igual__`.
- **Coerción a cadena**: `__cadena__` invocado por f-strings (`f"{obj}"`)
  e `imprimir(obj)`. Validación: si `__cadena__` retorna no-cadena
  → ErrorDeTipo claro vía nuevo opcode `OP_ASEGURAR_CADENA`.
- **Indexación**: `__indice__(yo, clave)` para `obj[k]`,
  `__asignar_indice__(yo, clave, valor)` para `obj[k] = v`.

### Fuera del alcance

- `__longitud__` (para `longitud(obj)`): requiere atajo del compilador
  + opcode dedicado, aplazado a v1.3.
- `__llamar__` (para `obj(args)` con instancia callable): aplazado.
- Operadores reflejados (e.g. `5 + V(...)` busca `__sumar_derecho__` en
  V): aplazado. Hoy solo el lado izquierdo dispara el dunder.
- Dunders en evaluador tree-walking: tree-walking nunca soportó clases
  (decisión Fase 2-5), así que dunders solo en bytecode VM.

### Arquitectura

Tres helpers de dispatch en [src/vm.c](src/vm.c) — `ejecutar_dunder_unario`
(arity 1, ej. `__cadena__`), `ejecutar_dunder_binario` (arity 2, ej.
`__sumar__`), `ejecutar_dunder_ternario` (arity 3, `__asignar_indice__`).
Todos preparan un CallFrame con la pila reorganizada como
`[..., closure, receptor, args...]` y devuelven al dispatch loop. El
resultado del dunder queda en el stack del caller vía `OP_RETORNAR`
del frame.

Helper compartido [`clase_obtener_metodo`](src/valor.c) para el lookup
no-owning sobre `clase->metodos`. Reusable para todos los dunders y
para futuras extensiones.

Mapper [`dunder_para_op_binario`](src/vm.c) traduce opcode → nombre del
dunder. El slow path de los operadores binarios consulta este mapper
antes de delegar al evaluador genérico.

### Compatibilidad con IC F10

Los fast paths `_INT_INT` (suma/resta/mult + comparaciones) siguen
funcionando exactamente igual. El IC se promueve cuando ambos operandos
son enteros y degrade al slow path si aparece una instancia. El
dispatch del dunder solo ocurre en el slow path, así que no hay
penalización para código aritmético puro.

### Optimización adicional

`OP_IMPRIMIR` ahora escribe directo desde el buffer de la cadena (sin
truncado a 1024 bytes) cuando el arg ya viene como cadena del
compilador. El compilador emite `OP_FORMATO_F + OP_ASEGURAR_CADENA`
antes de `OP_IMPRIMIR` para garantizar que cada arg pasa por
`__cadena__` si aplica.

### Tests y ejemplos

- [tests/unit/test_bytecode_dunders.c](tests/unit/test_bytecode_dunders.c)
  con 10 tests cubriendo los 14 dunders + IC + validación de tipo.
- [examples/28_dunders_jugable.cor](examples/28_dunders_jugable.cor) —
  Vector2D con OOP idiomático: `v + w`, `v * 5`, `v == w`, `f"v = {v}"`,
  además de TablaInversa con `__indice__`/`__asignar_indice__` y
  Persona con `__menor__` para comparaciones.

103/103 tests pasan (102 previos + nuevo `test_bytecode_dunders`).

## [1.1.1] — 2026-05-01 — Limpieza derivada de revisión

Patch sin features nuevas. Cierra la deuda técnica de la revisión
post-v1.1.0:

### Bugs

- **F-cadenas con cadenas internas que contienen llaves**: el lexer
  y el parser contaban `}` literal dentro de una cadena interna como
  cierre de la interpolación. Ahora `f"{ '}' }"`, `f"{eco('hola{')}"`
  y similares funcionan correctamente. Test de regresión añadido en
  [tests/unit/test_bytecode_fstrings.c](tests/unit/test_bytecode_fstrings.c)
  (`test_brace_en_cadena_interna`). Tocados:
  [src/lexer.c](src/lexer.c) `saltar_interpolacion` y
  [src/parser.c](src/parser.c) `parsear_f_cadena`.

- **Buffer de 4096 truncaba silenciosamente**: la coerción a cadena de
  colecciones grandes (listas/dicc con > ~4 kB de repr) cortaba la
  salida sin reportar. Nueva API `valor_a_cadena_alocada` en
  [src/valor.c](src/valor.c) escala el buffer dinámicamente hasta 16 MB,
  con dimensionamiento exacto vía `mp_radix_size` para enteros bignum
  y copia profunda directa para cadenas. Adoptada por `OP_FORMATO_F`
  (vm.c), evaluador tree-walking y `nativa_cadena`. Test de regresión
  con lista de 2000 elementos (`test_lista_grande_no_trunca`).

### Refactor

- **Helper compartido `valor_cadena_desde_escapes`**: el código de
  decodificación de escapes (`\n`, `\t`, etc.) estaba duplicado en
  `src/compilador.c` (`cadena_desde_slice`) y `src/evaluador.c`
  (`slice_a_cadena_eval`). Ahora vive en [src/valor.c](src/valor.c) con
  prototipo público y los dos motores delegan en él. Elimina el riesgo
  de divergencia accidental.

### Documentación

- **`leer()`**: comentario en código y nota en
  [docs/referencia.md](docs/referencia.md) aclarando que EOF inmediato
  y línea vacía son indistinguibles (ambos devuelven `""`). Si tu
  programa necesita detectar fin-de-stream, usa una sentinela.
- **`nativa_entero` rama decimal**: comentario sobre los magic numbers
  `9.2233720368547748e18` y `9.2233720368547758e18` — explica por qué
  son asimétricos (INT64_MAX no es exactamente representable en double;
  el redondeo a nearest cae en INT64_MAX + 1).

### Compatibilidad

- API: nuevo `valor_a_cadena_alocada` añadido a `valor.h`. El resto
  de la API pública sin cambios.
- Comportamiento observable: f-cadenas con cadenas internas que antes
  daban error de sintaxis ahora funcionan. Cadenas más largas en
  `cadena()` y f-strings preservan todo el contenido.

## [1.1.0] — 2026-05-01 — Built-ins esenciales

Primera entrega menor sobre v1.0.0. Cierra las tres promesas públicas
más visibles que aún no aterrizaban: conversores explícitos, lectura
desde stdin y f-cadenas con interpolación real. Sin cambios de
sintaxis (B10 respetado): la línea `f"hola {nombre}"` ya parseaba en
v1.0 pero no interpolaba; ahora sí.

### Built-ins añadidos (NATIVAS[] en `src/nativos.c`)

- `cadena(x)` — coerción a cadena. Idempotente sobre `VAL_CADENA`;
  para enteros bignum dimensiona el buffer con `mp_radix_size` para
  preservar todos los dígitos.
- `entero(x)` — desde int (no-op), decimal (truncar hacia cero),
  booleano (0/1), cadena (parse base 10 con signo opcional y `_`
  como separador). `ErrorDeValor` si la cadena no es entero válido o
  el decimal está fuera del rango int64; `ErrorDeTipo` para otros tipos.
- `decimal(x)` — desde decimal (no-op), int (vía `mp_get_double`),
  booleano, cadena (`strtod`). `ErrorDeValor`/`ErrorDeTipo` análogos.
- `booleano(x)` — siempre éxito; aplica las reglas de truthiness de
  ESPEC §6.2.
- `lista(iter)` — materializa cualquier iterable como lista. Acepta
  lista (copia), tupla, conjunto, cadena (caracteres), rango,
  diccionario (claves).
- `tupla(iter)` — igual a `lista` pero devuelve tupla inmutable.
- `diccionario(iter_de_pares)` — construye desde lista/tupla de pares
  `(clave, valor)`, o copia desde otro diccionario. Valida arity de
  pares y hashabilidad de claves.
- `leer([prompt])` — entrada interactiva desde stdin con buffer
  dinámico. Soporta CRLF (Windows). Sin args devuelve cadena vacía
  ante EOF inmediato; con un argumento cadena lo imprime sin newline
  como prompt antes de leer.

Tests unit en [tests/unit/test_bytecode_conversores.c](tests/unit/test_bytecode_conversores.c)
(38 casos cubriendo camino feliz + errores de tipo + errores de
valor + arity + idempotencia). El `leer()` se valida end-to-end con
[examples/26_leer_jugable.cor](examples/26_leer_jugable.cor)
(calculadora de IMC) y stdin redirigido vía script CMake.

### F-cadenas con interpolación real

El lexer y parser ya reconocían `f"..."` desde v0.2 (Fase 2), pero el
contenido se almacenaba crudo y el compilador/evaluador rechazaban con
"no implementado". v1.1 cierra el ciclo:

- **AST** ([src/ast.h](src/ast.h)): nuevo tipo `ParteFCadena` con
  `{ literal, longitud, expr }`. `EXPR_LITERAL_F_CADENA` ahora referencia
  un array de partes en lugar del lexema crudo.
- **Parser** ([src/parser.c](src/parser.c)): mini-parser interno que
  divide el lexema por `{...}` (respetando llaves balanceadas y `{{`/`}}`
  como literales), instancia un sub-lexer + sub-parser sobre cada slice
  de expresión, y verifica que consume EXACTAMENTE el slice (tokens
  sobrantes → error claro).
- **Compilador** ([src/compilador.c](src/compilador.c)): emite
  `OP_CONST` con cada parte literal (escapes ya decodificados) y, para
  partes expresión, compila + `OP_FORMATO_F` + `OP_SUMAR` para
  concatenar acumulando.
- **Tree-walking** ([src/evaluador.c](src/evaluador.c)): itera partes
  y concatena con buffer dinámico.
- **Nuevo opcode** `OP_FORMATO_F`: pop, convierte a cadena con
  representación canónica de `imprimir`, push.

Limitación conocida: triples (`f"""..."""`) no soportadas todavía
— el lexer las tokeniza pero el parser las rechaza con un error
explícito. Pendiente para v1.2.

Tests en [tests/unit/test_bytecode_fstrings.c](tests/unit/test_bytecode_fstrings.c)
(35+ casos: literal puro, escapes, llaves dobles, interpolación
simple/aritmética/llamadas, mezclas, coerción de tipos incluido bignum,
anidación, errores). Diferencial tree-walking ↔ bytecode con
[examples/27_fstrings_jugable.cor](examples/27_fstrings_jugable.cor).

### Refactors menores

- `cadena_desde_lexema` (compilador) y el procesamiento inline de
  `EXPR_LITERAL_CADENA` (evaluador) extraen un helper compartido
  `cadena_desde_slice` / `slice_a_cadena_eval` que también consume
  las partes literales de f-cadenas.
- Test infrastructure: nuevo script
  [tests/integracion/run_con_stdin.cmake](tests/integracion/run_con_stdin.cmake)
  que ejecuta un .cor con stdin redirigido y verifica regex sobre stdout
  (usado por el test `bc_run_26_leer_jugable`).

### Compatibilidad

- Sintaxis: sin cambios. Cualquier programa válido en v1.0 sigue
  válido en v1.1.
- Built-ins: solo añadidos, ninguno renombrado o eliminado.
- AST: el campo `como.literal` para `EXPR_LITERAL_F_CADENA` ya no
  contiene el lexema crudo — los consumidores externos del AST (poco
  probable fuera del proyecto) deben leer `como.f_cadena.partes`. La
  representación textual del `--ast` cambia (verificado en
  `tests/unit/test_parser_expresiones.c`).

## [1.0.0] — 2026-04-30 — Cornamusa estable

Primera versión **estable** de Cornamusa. El lenguaje es funcional,
documentado y usable. Marca el final del ciclo de optimización +
documentación iniciado tras v0.10.0.

### Hito v1.0 según plan B10

El plan original (referenciado en B2) listaba "GC generacional + docs
+ sitio web" para v1.0. Tras completar v0.7-v0.11 (clases, GC, módulos,
inline caching, small-int tagging) el rendimiento ya era razonable y
el cuello real era la documentación. La decisión [B10](decisiones/B10-scope-de-v1.md)
reorientó el scope:

- **GC generacional postergado** a post-v1.0 (decisión guiada por
  datos cuando programas reales lo justifiquen).
- **v1.0 enfocado en hacer el lenguaje USABLE para nuevos usuarios**:
  documentación, sitio web, ejemplos avanzados, fixes derivados de
  validar la documentación contra el intérprete.

### Cambios v1.0 (acumulados desde v0.11.0)

**Documentación**:
- `ESPEC.md` actualizado a v0.11.4 (clases, herencia, módulos, IC,
  small-int) — antes en "Fase 0 borrador" desde el inicio del proyecto.
- `docs/tutorial.md` (nuevo, ~600 líneas, 11 secciones) paso a paso
  desde "hola mundo" hasta clases y módulos. Cada bloque de código
  validado contra `cornamusa --bytecode`.
- `docs/referencia.md` (nuevo, ~640 líneas) cheatsheet con tablas
  densas: sintaxis, operadores, tipos, control, funciones, clases,
  excepciones, built-ins reales, stdlib, errores comunes.
- `FAQ.md` (nuevo, ~250 líneas) preguntas frecuentes para usuarios
  nuevos: por qué castellano, vs Python, instalación, sintaxis,
  rendimiento, desarrollo, curiosidades.
- `CONTRIBUTING.md` actualizado con referencias a tutorial, ADRs,
  tests diferenciales tree-walking vs bytecode.

**Sitio web**:
- `docs/SUMMARY.md` + `docs/introduccion.md` + `book.toml` →
  configuración mdBook.
- `.github/workflows/book.yml` → CI que construye y despliega a
  GitHub Pages.
- URL: https://david-castilla-gomez.github.io/Cornamusa/
- Apuntes técnicos (libros/papers de SELF, V8, Lua, CPython, etc.)
  movidos a `docs/papers/` — quedan en el repo pero no en el sitio
  público.

**Ejemplos avanzados** (no más micro-demos):
- `examples/24_notas_clase.cor` — análisis estadístico de notas
  con dicc, listas, ordenamiento, mediana.
- `examples/25_biblioteca_oop.cor` — simulación de biblioteca con
  herencia (Libro, Audiolibro, Revista), polimorfismo, validaciones.

**Fixes críticos descubiertos al validar docs/ejemplos contra el
intérprete real** (los 8 tests diferenciales existentes no los
detectaban porque no usaban los patrones):

- **v0.11.5** (commit d492da5): nuevo local en bucle dentro de
  función mantenía el valor de la primera iteración para siempre.
  Bug presente desde v0.6.0 (introducción del bytecode VM). Fix:
  emitir OP_NULO + agregar_local + push valor + OP_ASIGNAR_LOCAL en
  lugar de la "OLD convention" original.
- **v0.11.6** (commit 64d9555): el fix de v0.11.5 hacía crecer el
  stack +1 por iteración del `mientras`. Tras 2+ iters → desincroni-
  zación. Fix: `pre_reservar_locales(c, sent, linea)` recursivo que
  emite OP_NULO + agregar_local UNA vez ANTES del bucle por cada
  nuevo local detectado en el cuerpo. Las asignaciones dentro
  ejecutan plain OP_ASIGNAR_LOCAL.
- Ambos con tests de regresión específicos en
  `tests/unit/test_bytecode_ic.c::test_regresion_*`.

**Correcciones derivadas de validar docs**:
- `valor_a_int64_si_cabe` en `valor.c` aceptaba sólo magnitud ≤62
  bits para BIG (rango SMALL), creando hash divergente con DECIMAL
  en banda 2^62..2^63. Ampliado a magnitud ≤63 bits → hash convergen-
  te (v0.11.4).
- `quitar(dicc, clave)` documentado como reservado v1.x — solo
  funciona sobre listas en v1.0.
- ESPEC: diccionario "preserva orden de inserción" → NO en v1.0
  (era aspiración no aterrizada).

### Compromiso de estabilidad post-v1.0

Documentado en [B10](decisiones/B10-scope-de-v1.md):

- **Sintaxis del lenguaje**: congelada hasta v2.0. Cambios incompa-
  tibles requieren major version.
- **Built-ins y stdlib**: pueden añadir miembros entre minor versions;
  no pueden cambiar comportamiento de los existentes sin major.
- **AST, formato de chunks de bytecode, GC, IC, small-int**: detalles
  internos que pueden cambiar entre minor versions.
- **CLI flags públicas**: estables (`--bytecode`, `--version`,
  `--ast`, `--tokens`, `-h`).
- **Errores de runtime**: la categoría es estable, la redacción
  puede mejorar.

### Rendimiento (mediana de 5 corridas, v0.10.0 vs v1.0.0)

| Benchmark | v0.10 | v1.0 | Mejora |
|---|---|---|---|
| `fibonacci_recursivo` | 1.33 s | **228 ms** | **5.83x** |
| `globales_lookup` | 993 ms | **205 ms** | **4.84x** |
| `dicc_intensivo` | 121 ms | **40 ms** | **3.03x** |
| `oo_intensivo` | 44 ms | **23 ms** | **1.91x** |
| `bignum_factorial` | 29 ms | **16 ms** | **1.81x** |

Geomedia ≈ **3x**.

### Tests

- 96 tests verde (incluyendo 8 diferenciales tree-walking↔bytecode,
  14 boundaries de small-int, 11 de IC F10).
- ASan + UBSan en CI Linux.
- Build CI en Linux + Windows + macOS × Debug + Release.

### Después de v1.0

Roadmap post-v1.0 abierto, no comprometido:

- **v1.1+**: f-strings con interpolación real, dunders aritméticos
  (`__sumar__`, `__cadena__`, etc.), `nolocal` (escritura a upvalues),
  más stdlib (`archivos`, `json`, `regex`), generadores (`producir`),
  context managers (`con`).
- **v1.x**: pattern matching (`coincidir`).
- **v2.0** (lejano): concurrencia/async, NaN-boxing, posiblemente
  GC generacional si datos lo justifican.

### Agradecimientos

24 tags publicados desde v0.1.0 (2026-04-26) hasta v1.0.0
(2026-04-30). El proyecto comenzó como ejercicio personal de David
Castilla y se desarrolló con la asistencia constante de Claude
(Anthropic) como pareja de programación.

## [0.11.6] — 2026-04-30 — FIX: stack growth en mientras con nuevo local

Bug latente derivado del fix de v0.11.5 detectado al validar
`examples/25_biblioteca_oop.cor` durante la sesión 5 del plan v1.0.
Síntoma: "OP_ITER_SIGUIENTE sin iterador en slot N" tras 2+
iteraciones de un `mientras` que asignaba un nuevo local en su cuerpo.

### Bug

```cornamusa
funcion main():
    rondas = 3
    suma = 0
    mientras rondas > 0:
        n = rondas + 10        # nuevo local 'n' en cada iter
        suma = suma + n
        rondas = rondas - 1
    fin mientras
fin funcion
main()
```

Antes de v0.11.6: tras la 2ª iteración, error de runtime sobre el
stack desincronizado.

### Causa raíz

El fix de v0.11.5 emitía OP_NULO + push valor + OP_ASIGNAR_LOCAL
para nuevos locales, lo que crece el stack +1 por iteración. Para
`para` no había problema porque SENT_PARA pre-reserva el slot de su
variable de iteración fuera del cuerpo. Para `mientras` no había
pre-reserva — cada iteración acumulaba un OP_NULO sin consumir.

### Fix

Nueva función `pre_reservar_locales(c, sent, linea)` en
`src/compilador.c` que recorre el AST del cuerpo (recursivo por
SENT_BLOQUE y SENT_SI; no desciende en SENT_FUNCION/SENT_CLASE/
sub-bucles) buscando SENT_ASIGNAR a IDENT no-local existente. Por
cada uno emite OP_NULO + `agregar_local` UNA vez antes del cuerpo.

Llamada desde `compilar_mientras` y `compilar_para` antes de
compilar el cuerpo. Las asignaciones dentro del cuerpo ahora
encuentran "local existente" y emiten plain OP_ASIGNAR_LOCAL —
stack neutro por iteración.

### Tests

- Nuevo `test_regresion_mientras_con_nuevo_local`: ejercita el
  patrón exacto. 96 tests verde.
- `examples/25_biblioteca_oop.cor` restaurado con el algoritmo de
  selección iterativa (top-3 más prestados) que descubrió el bug,
  ahora funciona.

### Lección

Patrón repetido de v0.11.5: bug en un fix anterior detectado al
validar contra programas reales (no micro-tests). El test suite
ahora incluye un caso por cada bug histórico de scoping local.

## [0.11.5] — 2026-04-30 — FIX CRÍTICO: nuevo local en bucle dentro de función

Bug serio de correctness en el bytecode VM, descubierto al validar el
tutorial de v1.0 (sesión 3 del plan B10). El tree-walking interpreter
no tenía el bug; los 8 tests diferenciales tree-walking↔bytecode
existentes no lo detectaron porque sus ejemplos no usaban el patrón.

### Bug

Cuando una asignación a un **nuevo local** ocurre dentro de un bucle
dentro de una función, el slot del local quedaba con el valor de la
primera iteración para siempre.

```cornamusa
funcion main():
    para v en [1, 2, 3]:
        a = v + 10                  # nuevo local 'a'
        imprimir("v=", v, "a=", a)
    fin para
fin funcion
main()
```

Antes de v0.11.5:
```
v= 1 a= 11
v= 2 a= 11      ← bug: siempre 11
v= 3 a= 11      ← bug: siempre 11
```

Ahora v0.11.5 (correcto):
```
v= 1 a= 11
v= 2 a= 12
v= 3 a= 13
```

El bug afecta a **cualquier** programa con asignación a local nuevo
dentro de un bucle dentro de función. Es un patrón extremadamente
común en código real.

### Causa raíz

`compilar_asignar` en `src/compilador.c` usaba la "OLD convention"
para nuevos locales: empujar el valor + `agregar_local` SIN emitir
`OP_ASIGNAR_LOCAL`, asumiendo que el push deja el valor en el slot
recién creado. Eso solo es cierto en la PRIMERA ejecución del
bytecode emitido. Dentro de un bucle, el bytecode se ejecuta
múltiples veces y en iteraciones siguientes el push va a un stack
pos distinto del slot fijado en compile-time, dejando el slot con
el valor de la primera iteración.

### Fix

Para nuevos locales en función:

1. Emitir `OP_NULO` (reserva el slot en stack).
2. `agregar_local` (registra el nombre y fija el slot index).
3. Compilar la expresión del valor (push).
4. `OP_ASIGNAR_LOCAL` al slot recién creado (pop + asign).

El push del placeholder + asignación explícita funciona en cualquier
iteración porque el stack queda con el mismo n elementos al inicio
y al fin de cada iter.

### Impacto en otras versiones

Este bug ha estado presente desde **antes de v0.11** — probablemente
desde v0.6 cuando se introdujo el bytecode VM. Los benchmarks no lo
detectaron porque usaban variables globales (no locales nuevas en
bucles). Los tests integración no lo detectaron porque no usaban el
patrón. **v0.6.0 hasta v0.11.4 inclusive contienen este bug**.

Después de la sesión, el camino crítico de programas reales
funciona correctamente. Cualquier programa que demostraba algo
"raro" (resultado constante donde debía variar) probablemente
estaba afectado.

### Tests

- Nuevo `test_regresion_local_nuevo_en_bucle` en `test_bytecode_ic.c`:
  ejecuta el patrón exacto del bug y verifica `suma = 11 + 12 + 13 = 36`.
- 92 tests verde (incluye el nuevo).
- Bench sin regresiones: globales_lookup ~218ms (igual que v0.11.4).

### Lección

Tutorial validado contra el intérprete real es una práctica esencial.
El bug llevaba meses ahí; nadie lo había detectado porque los
ejemplos test eran demasiado micro. Lo capturamos porque escribir
un programa que un usuario humano escribiría reveló el patrón
inmediatamente.

## [0.11.4] — 2026-04-30 — fix hash divergente en banda 2^62..2^63

Cierra tech-debt #6 de la revisión post-release v0.11.1: bug latente
de hash entre `VAL_ENTERO` BIG y `VAL_DECIMAL` con el mismo valor
numérico cuando ambos caían en la banda `[2^62, 2^63)`.

### Bug corregido (v0.11.4)

- `valor_a_int64_si_cabe` (en `src/valor.c`) rechazaba BIG con
  `mp_count_bits > 62`, pero un DECIMAL del mismo valor pasaba por
  el camino i64 (rango ±9.2e18). Resultado: hashes divergentes.
  Ejemplo:
  ```cornamusa
  d = {}
  d[2 ** 62] = "uno"             # clave guardada como BIG
  imprimir(d[2.0 ** 62])          # antes: ErrorDeClave (slots distintos)
                                  # ahora: "uno" (mismo slot)
  ```
- Fix de 1 línea: `mp_count_bits > 62` → `mp_count_bits >= 64`.
  Esto acepta hasta magnitud 63 bits (rango int64 completo excepto
  INT64_MIN cuya magnitud es exactamente 64). Ahora BIGs en
  `[INT64_MIN+1, INT64_MAX]` y DECIMALs equivalentes hashean al
  mismo slot, manteniendo la invariante `a == b ⇒ hash(a) == hash(b)`.

### Tests (v0.11.4)

- Nuevo `test_hash_banda_2_62` en `test_small_int.c`: construye un
  BIG con valor 2^62 y un DECIMAL con valor 2^62.0; verifica que
  `dicc_asignar(dict, BIG)` permite recuperar con clave DECIMAL.
- 14 tests boundaries totales (de 13 en v0.11.3).
- 92 tests en suite completa.

### Pendiente para v0.12+

Tras este patch, los tech-debt restantes documentados son:
- #2: helpers `valor_entero_a_mp_int` (público, no acepta bool) vs
  `como_mp_int` (privado en evaluador.c, sí acepta bool). Aún
  duplicados; razón legítima de existir, pero la convergencia hacia
  un solo helper queda pendiente.
- #4: MSVC fallback de `__builtin_mul_overflow` con cota int31.
- #8: `long → int64_t` en indexación para Windows LLP64.
- Threaded code dispatch (computed gotos): refactor masivo de
  ~200 cambios al switch del VM por solo ~10-15% de ganancia.
  ROI/coste no compensa frente a un pivote a v1.0 (Fase 11.2).

## [0.11.3] — 2026-04-30 — constant folding en compilador

Pulido del pipeline de compilación. Expresiones cuyos operandos son
todos constantes ahora se reducen a un único `OP_CONST` en compile-time
en lugar de emitir bytecode aritmético.

### Añadido (v0.11.3)

- **`evaluar_constante` en `compilador.c`**: helper recursivo que
  intenta evaluar una expresión en compile-time. Soporta:
  - Literales (`NULO`, `BOOLEANO`, `ENTERO`, `DECIMAL`, `CADENA`).
  - `EXPR_GRUPO` (paréntesis) recursivo.
  - `EXPR_UNARIO` (`-x`, `+x`, `no x`, `~x`) cuando el operando es
    constante.
  - `EXPR_BINARIO` (todas las aritméticas/comparaciones/lógicas)
    cuando ambos lados son constantes.
- Reusa `evaluador_aplicar_unario` y `evaluador_aplicar_binario` —
  el folding produce semánticamente lo mismo que el runtime.
- **No foldeamos errores**: si la operación produciría un error
  (división por cero, tipos incompatibles), el folding aborta y
  dejamos al runtime reportar el error en su línea original.

### Patrones que ahora se foldean

```cornamusa
SEGUNDOS_DIA = 60 * 60 * 24      # → OP_CONST 86400
AREA = 3.14159 * 10 * 10         # → OP_CONST 314.159
MENSAJE = "Hola, " + "mundo"     # → OP_CONST "Hola, mundo"
LIMITE = 2 ** 16                 # → OP_CONST 65536
PUEDE = 5 < 10 y 3 > 1           # → OP_CONST true
```

### Tests (v0.11.3)

- Tests del IC actualizados (`test_bytecode_ic.c`): los casos que
  antes hacían `1 + 2` para verificar `OP_SUMAR_INT_INT` ahora usan
  variables intermedias (`k0 = 1; k1 = 2; a = k0 + k1`) para evitar
  que el folding eluda el opcode bajo prueba. La especialización
  IC sigue funcionando correctamente, pero se valida con código
  realista (variables locales/globales, no literales).

92 tests verde.

### Notas (v0.11.3)

- El impacto en benchmarks `benchmarks/*.cor` es marginal porque
  esos workloads no tienen aritmética constante en hot loops. El
  beneficio real es en código de aplicación: definiciones de
  constantes nombradas, fórmulas pre-computables, configs.
- Aún no foldeamos llamadas a built-ins puros como `longitud("hola")`
  o `mat.PI * 2`. Sería natural en una sesión futura — solo requiere
  whitelist de funciones puras.

## [0.11.2] — 2026-04-30 — fast-path int64 en iterador de `rango`

Tech-debt #5 de la revisión post-release de v0.11.1 cerrado. Programas
con loops grandes (`para i en rango(N)`) eran cuello porque el
iterador alocaba un `mp_int` nuevo cada paso aunque inicio/fin/paso
cupieran en SMALL.

### Mejoras (v0.11.2)

- **Camino rápido int64 en `iter_siguiente` para `VAL_RANGO`**: si
  inicio, fin y paso caben en `int64_t` (chequeado vía `mp_count_bits
  < 64` por valor), calculamos `inicio + cursor*paso` directamente
  con aritmética nativa. Detección de overflow vía
  `__builtin_mul_overflow`/`add_overflow` en GCC/Clang; en MSVC
  fallback con cota `int31` para cursor y paso.
- En overflow o si algún componente del rango excede `int64`, fallback
  al path bignum existente. Sin pérdida funcional.
- El resultado pasa por `valor_entero_de_i64` que produce SMALL
  cuando cabe, BIG si no.

### Corregido (v0.11.2)

- **Leak preexistente**: en `iter_siguiente` para `VAL_RANGO`, si
  `mp_init(resultado)` succeeds y `mp_copy(...)` falla, antes se
  llamaba `free(resultado)` sin `mp_clear` — perdía los `digits`
  alocados por `mp_init`. Ahora hace `mp_clear + free` correctamente.
  Reportado por la revisión post-release como tech-debt 5b.

### Rendimiento (v0.11.2)

Comparación contra v0.11.1 (mediana de 3 corridas):

| Benchmark             | v0.11.1  | v0.11.2  | Mejora |
|-----------------------|----------|----------|--------|
| globales_lookup       | 391 ms   | **218 ms** | **1.79x** |
| dicc_intensivo        | 59 ms    | **50 ms**  | 1.18x |
| fibonacci_recursivo   | 222 ms   | 235 ms   | (~igual; no usa rango) |
| bignum_factorial      | 17 ms    | 27 ms    | (variabilidad) |
| oo_intensivo          | 24 ms    | 32 ms    | (variabilidad) |

**Comparación acumulada vs v0.10.0 baseline**:

| Benchmark             | v0.10  | v0.11.2 | Total |
|-----------------------|--------|---------|-------|
| globales_lookup       | 993 ms | 218 ms  | **4.55x** |
| dicc_intensivo        | 121 ms | 50 ms   | 2.42x |
| fibonacci_recursivo   | 1.33 s | 235 ms  | 5.66x |

Geomedia consolidada: ~3.0x sobre v0.10.

92 tests verde.

## [0.11.1] — 2026-04-30 — fixes post-release (revisión crítica)

Code review crítica independiente del refactor B9 detectó tres
problemas de calidad y un bug latente. Esta versión los corrige
sin cambios de comportamiento observable.

### Corregido (v0.11.1)

- **Bug latente — `valor_entero_a_mp_int` no inicializaba `*propio`**
  cuando `nuevo_mp_int()` fallaba (OOM). Los callers que leyeran
  `propio` para decidir si liberar leerían memoria sin inicializar.
  Fix: `*propio = false` al inicio de la función. Severidad baja
  en la práctica (OOM raro) pero la API pública debe ser robusta.
- **Comentarios stale en `valor.c`** que describían "sesión 1, BIG
  siempre" cuando el código ya producía SMALL. Reescritos para
  reflejar el comportamiento actual de v0.11.0.
- **Comentario engañoso en `valor_entero_a_i64`** ("comparar
  mp_count_bits con 63") cuando el código compara `< 64`.
  Reescrito explicando que `< 64` significa "magnitud ≤ 63 bits"
  y que `INT64_MIN` queda excluido a propósito (SMALL_INT_MIN =
  -2^62, así no perdemos rango útil).
- **Función no usada `evaluador_valor_entero_de_mp` eliminada** del
  API pública. La función `static valor_entero_de_mp` también
  eliminada (warning `-Wunused-function`). Toda la producción
  pasa por `valor_entero_de_mp_normalizado` ahora.

### Tests reforzados (v0.11.1)

- **Nuevo test `test_smallmin_mult_neg1`**: cubre `SMALL_MIN * -1`,
  caso peligroso B9 §4 que no estaba en la suite original.
- **Validador común `verificar_overflow_promueve`** — los tests de
  overflow ahora validan EXPLÍCITAMENTE ambas ramas (`aplic=true`
  con BIG y valor correcto, o `aplic=false` con sentinel nulo). En
  v0.11.0 los tests usaban `if (aplic) { ... }` sin else, así
  que pasaban silenciosamente con cualquier implementación que
  jamás reportara `aplic=true`.
- 13 tests boundaries en total (de 12 en v0.11.0).

### Conocido para v0.12+ (post-release)

La revisión crítica también identificó tres tech-debt no urgentes:

- Iter de `VAL_RANGO` aloca `mp_int` por cada paso aunque inicio,
  fin y paso quepan en `int64_t` — fast-path SMALL no implementado
  en `valor.c::iter_siguiente`. Beneficiaría loops grandes como
  `para i en rango(1_000_000)`.
- MSVC fallback de `__builtin_mul_overflow` en `small_op_small` es
  conservador (rechaza si cualquier operando excede int32). En
  GCC/Clang ya está bien.
- Hash divergente en banda 2^62..2^63 entre BIG y DECIMAL con mismo
  valor numérico (preexistente, no introducido por B9). El refactor
  B9 era el momento natural de armonizarlo y se dejó pasar.
- Migración `long → int64_t` en indexación para Windows (LLP64).

Ninguno bloquea el uso de v0.11.1 — son optimizaciones y limpieza
para una sesión futura.

## [0.11.0] — 2026-04-30 — Small-int tagging (Fase 11.1)

Segunda fase de optimización de rendimiento, basada en la decisión
[B9](decisiones/B9-small-int-tagging.md). Enteros que caben en 63 bits
ahora viven inline en la unión `Valor.como.entero_small` (`int64_t`),
sin alocar `mp_int` ni invocar `mp_init`/`mp_clear`. La aritmética
SMALL+SMALL es directa en `int64_t` con detección de overflow.

Resultado: **~2.7x geomedia sobre v0.10**, **~6x en programas
recursivos numéricos**.

### Cambios principales (v0.11.0)

**Representación de Valor**:
- Nuevo tag `VAL_ENTERO_SMALL` en `TipoValor`. La unión `Valor.como`
  gana un campo `int64_t entero_small` junto al `mp_int *entero`
  existente.
- Rango: `CORNAMUSA_SMALL_INT_MAX = 2^62 - 1`, `_MIN = -2^62`.
  Reservamos 1 bit de margen respecto a `int64_t` para que la suma
  de dos SMALL caben sin UB en C — necesario para el camino rápido
  sin `__builtin_add_overflow` (MSVC fallback).
- `VAL_ENTERO` (BIG) sigue siendo `mp_int *` para enteros grandes.
  Las operaciones que producen ints normalizan: si el resultado cabe
  en SMALL se devuelve como tal; si no, BIG.

**API canónica** (en [src/valor.h](src/valor.h)):
- `valor_es_entero(v)`: predicado que sustituye `v->tipo == VAL_ENTERO`.
- `valor_entero_a_i64(v, *out)`: extrae como `int64_t` si cabe.
- `valor_entero_a_mp_int(v, *propio)`: extrae como `mp_int *`,
  alocando un temporal si es SMALL (flag *propio para liberar).
- `valor_entero_de_i64(n)`: constructor canónico — SMALL si cabe,
  BIG si no.
- `valor_entero_de_mp_normalizado(m)`: constructor desde mp_int* con
  demote automático a SMALL si el valor cabe.

**Aritmética SMALL+SMALL** (en [src/evaluador.c](src/evaluador.c)):
- Helper `evaluador_small_op_small` con dispatcher para `+`, `-`,
  `*`, `//`, `%`. Usa `__builtin_*_overflow` (GCC/Clang) o detección
  manual (MSVC).
- Casos especiales manejados:
  - Overflow → reportado como no-aplicable, fallback al path BIG.
  - `SMALL_MIN / -1` (UB en C, `INT_MIN/-1`) → no-aplicable.
  - División por cero → error explícito.
  - Módulo Python-style (signo del divisor): `-7 % 3 = 2`.
- `entero_op_entero` (path BIG existente) ahora normaliza el resultado
  con `valor_entero_de_mp_normalizado` — `100000 - 99999 = 1` se
  devuelve como SMALL, no como BIG.
- `comparar_valores`: camino rápido `int64_t` inline si ambos
  operandos caben en `i64`, fallback a `mp_cmp` si alguno es BIG
  fuera de rango.

**IC bytecode con SMALL** (en [src/vm.c](src/vm.c)):
- Macros `BIN_INT_INT_ARITH` y `BIN_INT_INT_CMP` reescritas con tres
  caminos: SMALL+SMALL inline, BIG+BIG via `mp_*`, mezcla degrada al
  slow path.
- El camino SMALL+SMALL invoca `evaluador_small_op_small`. Si
  overflow, fallback a `mp_int` temporales que normalizan resultado.
- Los literales numéricos del parser (`valor_entero_de_lexema`) se
  pasan por `valor_entero_de_mp_normalizado` — los literales
  pequeños son SMALL desde el principio.

**Migración masiva** (sesión 3): 72 sitios que comprobaban
`v.tipo == VAL_ENTERO` migrados a `valor_es_entero(&v)`. 22 sitios
adicionales que leían `v.como.entero` directo migrados a usar los
helpers (`valor_entero_a_i64` o `valor_entero_a_mp_int` con cleanup
de temporal). Cubren `valor_a_doble`, indexación, rebanada,
construcción de rango, comparador de ordenamiento, repetición de
cadena, unario `-` y `~`, etc.

### Tests (v0.11.0)

- **12 tests boundaries nuevos** en [tests/unit/test_small_int.c](tests/unit/test_small_int.c):
  - Constructor en frontera (SMALL_MIN/MAX caben; ±1 promueven a BIG).
  - `valor_entero_de_mp_normalizado` demote correcto.
  - Igualdad cross-tag: `SMALL(5) == BIG(5) == 5.0 == True`.
  - Hash equivalente: `dicc[SMALL(5)]` y `dicc[BIG(5)]` acceden al
    mismo slot — invariante crítica.
  - Overflow promueve correctamente a BIG (suma, resta, mult).
  - `SMALL_MIN / -1` reportado como no-aplicable.
  - División por cero, módulo Python-style.
  - Clone preserva tipo. Helpers de extracción funcionan para ambos
    tags.
- 92 tests verde en total (91 previos + el nuevo `test_small_int`).
- Tests existentes actualizados donde asumían representación
  pre-v0.11 (`test_runtime_valor`, `test_chunk_disasm`,
  `test_runtime_evaluador`).

### Rendimiento (v0.11.0)

Mediana de 5 corridas, binario v0.10.0 desde su tag vs binario v0.11.0
HEAD, ambos en CMake Release:

| Benchmark             | v0.10.0  | v0.11.0  | Mejora |
|-----------------------|----------|----------|--------|
| bignum_factorial      | 29 ms    | 17 ms    | **1.71x** |
| dicc_intensivo        | 121 ms   | 59 ms    | **2.05x** |
| fibonacci_recursivo   | 1.33 s   | 222 ms   | **5.98x** |
| globales_lookup       | 993 ms   | 391 ms   | **2.54x** |
| oo_intensivo          | 44 ms    | 24 ms    | **1.83x** |

**Geomedia ≈ 2.7x.**

`fibonacci_recursivo` excede el plan B9 (3-5x prometido). El cuello
ya no es allocación de `mp_int` — son las llamadas recursivas y el
dispatch general (que F10 ya optimizó hasta lo razonable).

`bignum_factorial` mejora menos en absoluto porque su loop interno
hace `r * i` con `r` que crece hasta 1000 dígitos (BIG persistente):
el ahorro está en `i` (SMALL) y en evitar alocaciones temporales,
pero `mp_mul` sigue dominando.

### Decisión arquitectónica (v0.11.0)

Opción **B** del documento [B9](decisiones/B9-small-int-tagging.md):
nuevo tag explícito en `TipoValor`, no tagged pointer.

Razones documentadas:
- Type-safety: el compilador C grita en cada switch que falte
  adaptarse (gracias a `-Wswitch`). El sistema de tipos hace de
  checklist.
- Auditable: cada acceso a `como.entero` queda visible en grep, y
  cualquier sitio que lea SMALL como `mp_int *` se manifiesta como
  segfault o test failure inmediato (no corrupción silenciosa).
- Migración progresiva: la API de helpers permitió migrar el código
  base en pasos pequeños, con tests verde después de cada commit.

Opción A (tagged pointer en bit 0 del `mp_int *`) rechazada por
riesgo de bugs silenciosos. Opción C (NaN-boxing) aplazada a un
hipotético v2.0 si se demuestra necesaria.

### Notas (v0.11.0)

- API pública (.cor scripts) sin cambios. El usuario no nota
  diferencia salvo en velocidad — programas que antes corrían
  correctamente siguen corriendo correctamente con los mismos
  resultados.
- Los tests diferenciales tree-walking vs bytecode (8 ejemplos)
  fueron la red de seguridad principal del refactor: cualquier
  divergencia entre paths SMALL y BIG se manifestaría como test
  rojo. Permanecieron verde durante todo el ciclo.
- ASan + UBSan en CI (job `sanitizers`) garantizó que las
  conversiones SMALL ↔ BIG no introdujeran heap corruption ni UB
  detectables.

### Pendiente para futuro (post-v0.11)

- **Threaded code dispatch** (computed gotos): ~10-15% global en
  GCC/Clang; MSVC requiere doble path. Considerar como v0.12.
- **Constant folding en compilador**: `1 + 2` se computa en
  compile-time. Pequeño pero gratis.
- **GC generacional + tier-2 IC + tracing**: trabajo mayor para
  v1.x.

## [0.10.0] — 2026-04-30 — Inline caching especializado tipo PEP 659 (Fase 10)

Primera fase de optimización de rendimiento. Cuatro tandas de
especializaciones implementadas en 5 sesiones de trabajo
(detalladas en [decisiones/B8-inline-caching.md](decisiones/B8-inline-caching.md)).
Quickening por reescritura del byte del opcode in-place; cache slots
inline en el bytecode para los opcodes con cache versionada (PEP 659
style, no side-table).

### Especializaciones nuevas (v0.10.0)

**Lookup de globales** (sesión 2):
- `OP_OBTENER_GLOBAL` ahora ocupa 6 bytes (opcode + name_idx + 4 bytes
  de cache). Slot inline guarda los 16 bits bajos de
  `Diccionario.version` y el slot_idx en `entradas`. Tras un acierto
  promueve a `OP_OBTENER_GLOBAL_CACHE` que lee directamente
  `entradas[slot_idx]` sin hashing ni probing. Miss → degrada y
  rebobina ip.
- `Diccionario.version` (uint64_t) bumpea solo en cambios
  estructurales (insert nuevo, remove, resize). NO en sobreescritura
  — preserva el cache para `contador = contador + 1` en hot loop.

**Llamadas a función** (sesión 3-4):
- 4 variantes especializadas, sin cache slot (el byte del opcode es
  el cache):
  - `OP_LLAMAR_NATIVA` para `VAL_NATIVA`
  - `OP_LLAMAR_BC` para `VAL_FUNCION_BC` (closure)
  - `OP_LLAMAR_CLASE` para `VAL_CLASE` (instanciación + `__iniciar__`)
  - `OP_LLAMAR_METODO_LIGADO` para `VAL_METODO_LIGADO`
- Refactor: cada cuerpo de rama de `OP_LLAMAR` extraído a helper
  `static` (`ejecutar_llamar_<tipo>`). Slow path captura el chunk del
  caller antes de que el helper push'ee un frame nuevo, para
  promover el opcode en el chunk correcto.

**Aritmética y comparaciones int+int** (sesión 5-6):
- `OP_SUMAR_INT_INT`, `OP_RESTAR_INT_INT`, `OP_MULTIPLICAR_INT_INT`
  llaman `mp_add`/`mp_sub`/`mp_mul` directamente, saltándose el
  switch general de tipos de `evaluador_aplicar_binario`.
- `OP_MENOR_INT_INT`, `OP_MENOR_IGUAL_INT_INT`, `OP_MAYOR_INT_INT`,
  `OP_MAYOR_IGUAL_INT_INT` con `mp_cmp` directo.
- Helpers de bignum (`nuevo_mp`, `liberar_mp`, `valor_entero_de_mp`)
  expuestos en `evaluador.h` para que `vm.c` no duplique gestión de
  `mp_int`.

**Acceso a atributos de instancia** (sesión 7-8):
- `OP_OBTENER_ATRIBUTO` ahora 6 bytes con cache de
  (clase_hash u16, slot_idx u16). Los 16 bits bajos del puntero a la
  clase filtran cross-class; el slot_idx apunta a `instancia.atributos`.
- `OP_OBTENER_ATRIBUTO_INSTANCIA` (fast path) verifica:
  1. obj es `VAL_INSTANCIA`
  2. low16(clase) coincide con cache
  3. slot ocupado
  4. clave guardada coincide con el nombre esperado (memcmp corto)
- El check (4) es esencial: instancias de la misma clase pueden
  tener layouts distintos si fueron mutadas dinámicamente.

### Infraestructura (v0.10.0)

- **ASan + UBSan en CI** (Linux/Clang Debug): job `sanitizers` en
  `.github/workflows/build.yml` que compila con
  `-fsanitize=address,undefined -fno-omit-frame-pointer` y corre
  todos los tests. Captura corrupciones de heap y UB que serían
  invisibles en builds Release. Importante para F10 que hace
  rewriting in-place de bytecode.
- **`CORNAMUSA_BYTECODE_VERSION = 1`** en `chunk.h` (decisión I7):
  marcador de formato. Se bumpea cuando el layout binario rompa
  compatibilidad. Hoy los chunks no se serializan a disco; la
  constante prepara el terreno para futuras herramientas (`.cornc`
  cache files, inspector externo).
- **Decisión [B8-inline-caching.md](decisiones/B8-inline-caching.md)**
  documentando arquitectura, riesgos, opcodes pendientes para
  post-v1.0 (tier-2, tracing, threaded code dispatch, JIT).

### Tests (v0.10.0)

- **11 tests unitarios nuevos en `test_bytecode_ic.c`**: validan
  quickening básico, hits múltiples estables, invalidación en
  insert nuevo, no-invalidación en sobreescritura, promoción de
  `OP_LLAMAR` a `_NATIVA`/`_BC`, degradación polimórfica,
  promoción de binarios a `_INT_INT`, mezcla de tipos en mismo site,
  shape cache de atributos.
- 91 tests verde totales (incluye los 8 diferenciales tree-walking
  vs bytecode — críticos para garantizar que el quickening no
  introduce divergencia semántica).

### Rendimiento (v0.10.0)

Mediana de 5 corridas, cada benchmark contra binario v0.9.2 y v0.10.0
construidos con CMake Release:

| Benchmark             | v0.9.2  | v0.10.0 | Mejora |
|-----------------------|---------|---------|--------|
| bignum_factorial      | 33 ms   | 18 ms   | **1.83x** |
| oo_intensivo          | 50 ms   | 37 ms   | **1.35x** |
| dicc_intensivo        | 157 ms  | 130 ms  | 1.21x |
| globales_lookup       | 1.18 s  | 1.04 s  | 1.14x |
| fibonacci_recursivo   | 1.47 s  | 1.44 s  | 1.02x |

**Geomedia ≈ 1.30x** (30% más rápido).

`fibonacci_recursivo` mejora poco porque su cuello dominante es la
asignación de `mp_int` por operación bignum, no el dispatch.
Optimizar eso requeriría small-int tagging (i63 para enteros que
caben en 63 bits) o pool de `mp_int` — quedan como trabajo
post-F10, posiblemente v0.11.

### Notas (v0.10.0)

- El IC introduce riesgo de bugs por cache mal invalidado. Los tests
  diferenciales son la red de seguridad principal — cualquier
  divergencia de salida entre tree-walking (sin IC) y bytecode (con
  IC) se manifiesta como test rojo.
- Todos los cache slots son zero-init en chunks recién emitidos. El
  primer hit del slow path los rellena. Si un programa solo ejecuta
  un site UNA vez, paga 4 bytes extra de chunk sin beneficiarse —
  aceptable.
- Sites polimórficos (que oscilan entre tipos) pagan el coste de
  rewrite en cada cambio. La detección de polimorfismo y degradación
  permanente se queda como trabajo post-v1.0 (tier-2 PEP 659).

## [0.9.2] — 2026-04-29 — pulido pre-v1.0: stdlib `sistema`, tests diferenciales, benchmarks

Pasada de madurez antes de decidir entre F10 (inline caching) y F11 (v1.0
final). **Sin nueva semántica de lenguaje** — solo herramientas alrededor
del intérprete que faltaban para que un usuario externo pueda llegar al
repo y orientarse. **90 tests verde**.

### Añadido (v0.9.2)
- **Stdlib `sistema`**: nuevo módulo en `stdlib/sistema.cor` que expone
  `sistema.argv` (lista de argumentos del programa) construida sobre el
  built-in nativo `obtener_argv()`.
- **Built-in `salir(codigo)`**: termina el proceso con el código
  indicado (entero o booleano). Disponible globalmente sin importar
  nada. Implementado como nueva nativa en `src/nativos.c`.
- **`nativos_set_argv(argc, argv)`**: hook que `main.c` llama tras
  parsear los flags, pasando los argumentos del programa (a partir
  del `.cor` ejecutado) para que `obtener_argv()` los devuelva.
- **Tests diferenciales tree-walking vs bytecode** (`tests/integracion/diff_motores.cmake`):
  para los 8 ejemplos compatibles con ambos motores, ejecutamos cada
  uno con `cornamusa` y `cornamusa --bytecode`, capturamos stdout en
  ficheros, y comparamos byte a byte con `cmake -E compare_files`.
  Red de seguridad ante regresiones semánticas.
- **Benchmarks baseline** en `benchmarks/`: 4 micro-benchmarks
  (fibonacci recursivo, dicc intensivo, OO intensivo, factorial
  bignum) + scripts `run.sh` y `run.ps1` para medir tiempos. Numbers
  baseline documentados en `benchmarks/README.md` para futura
  comparación con F10.
- **Ejemplo `examples/23_sistema_jugable.cor`** demostrando
  `sistema.argv` y `salir(0)`.
- **README al día**: badge actualizado a v0.9.2, características
  reflejan la realidad (clases, GC, módulos, stdlib, tests
  diferenciales, benchmarks), roadmap actualizado.

### Corregido (v0.9.2)
- **`OP_DESCARTAR` tras `OP_IMPORTAR`**: el frame del módulo retornaba
  `nulo` al stack del importador, dejando un valor sobrante. Causaba
  errores `OP_ITER_SIGUIENTE sin iterador en slot 0` cuando el código
  posterior usaba slots por posición. El compilador ahora descarta el
  valor explícitamente. (Bug presente desde v0.9.0 / refinado en v0.9.1
  pero el fix no entró en el commit del tag v0.9.1.)

### Notas (v0.9.2)
- 12 ejemplos podrían en teoría correr en ambos motores, pero 4
  (`03_fibonacci`, `05_listas`, `06_diccionarios`, `10_quicksort`)
  usan f-strings o desempaquetado de tuplas que ningún motor soporta
  todavía — fallan idénticamente al parsear, así que no aportan al
  diferencial. Los 8 restantes cubren básicos, control de flujo,
  listas, dicc, conjuntos, tuplas.
- `sistema.argv` solo se expone via `obtener_argv()` (no como variable
  fija), porque las globals del módulo se evalúan UNA vez al cargar
  el módulo. Si en el futuro se quiere argv reactivo, habrá que
  exponer `sistema.argv` como propiedad.
- `salir()` llama a `exit()` directamente, sin oportunidad de unwind
  ni `finalmente`. Es el comportamiento de Python `sys.exit()` con
  `os._exit()`, no con `SystemExit`. Si se necesitara un cierre
  ordenado en el futuro, habría que lanzar una excepción especial.

## [0.9.1] — 2026-04-29 — módulos completos + indexación de cadenas

Cierra la deuda funcional de v0.9.0: módulos con subsegmentos, alias,
y `desde X importar Y`. Indexación de cadenas `s[i]` ahora funciona
en bytecode con UTF-8. Stdlib `cadenas.cor` ampliada con funciones que
requieren indexación. **79 tests verde**.

### Añadido (v0.9.1)
- **`importar X.Y` (subsegmentos)**: `cargar_modulo_desde_archivo`
  traduce `.` a `/` antes del lookup. `importar mat.geometria` busca
  `./mat/geometria.cor` luego `stdlib/mat/geometria.cor`.
- **`importar X como Y` (alias)**: el módulo se carga y se cachea por
  su nombre real, pero se registra como global del importador bajo el
  alias.
- **`OP_IMPORTAR` ahora toma 2 operandos** (`module_idx`, `binding_idx`)
  para soportar alias/subsegmentos: `module_idx` es el nombre real del
  módulo (cache key), `binding_idx` es el nombre de la global (alias o
  último segmento).
- **`CallFrame.modulo_binding_name`** + `modulo_binding_len`:
  buffer heap-duplicated con el nombre del binding global, liberado en
  `OP_RETORNAR` tras registrar la global.
- **`desde X importar Y, Z` (selective import)**: nuevo opcode
  `OP_IMPORTAR_PARA_DESDE [name_idx]` que carga el módulo y lo deja
  en el tope del stack (sin registrar global). Para cada item, el
  compilador emite `OP_DUP`, `OP_OBTENER_ATRIBUTO [item_idx]`,
  `OP_DEFINIR_GLOBAL [binding_idx]`. Final `OP_DESCARTAR` retira el
  módulo. Nuevo flag `CallFrame.desde_import` para que `OP_RETORNAR`
  finalice el módulo poniéndolo en stack en vez de bindeándolo.
- **`OP_DUP`**: duplica el valor en el tope del stack (clone). Nuevo.
- **`OP_INDICE` ahora soporta `VAL_CADENA`**: indexación UTF-8 con
  `utf8proc_iterate`. Devuelve cadena de 1 carácter. Soporta índices
  negativos (cuentan desde el final). `ErrorDeIndice` si fuera de
  rango. `ErrorDeTipo` si índice no es entero.
- **`stdlib/cadenas.cor` ampliada**: funciones nuevas `caracter(s, i)`,
  `empieza_con(s, prefijo)`, `termina_con(s, sufijo)`, `contar(s, sub)`.
  Antes estaban deshabilitadas porque requerían `s[i]`.
- **9 tests nuevos** en `test_bytecode_modulos.c`: alias simple, alias
  no expone nombre original, subsegmentos compilan, `desde X importar`
  simple/multiple/alias, `desde` no expone módulo, función importada
  via desde, indexación cadena básica/negativa/UTF-8/fuera-de-rango.
- **Ejemplo `examples/22_modulos_avanzado.cor`** demostrando alias,
  desde-importar, y `s[i]` con UTF-8.

### Limitaciones documentadas en v0.9.1
- **Nuevos locales declarados dentro de cuerpos de bucles** (en función)
  no funcionan correctamente: el slot se desfasa entre iteraciones.
  Workaround: declarar el local antes del bucle. La función `contar`
  en `cadenas.cor` aplica este workaround. Resolver requiere un
  preamble de OP_NULOs en el chunk de la función + emit explícito de
  OP_ASIGNAR_LOCAL para todos los nuevos locales — refactor mediano,
  aplazado a v0.9.2 o v0.9.3.

## [0.9.0] — 2026-04-29 — módulos + stdlib mínima (Fase 9)

Cornamusa gana sistema de módulos: `importar matematicas` carga un
archivo `.cor` y expone sus globales como atributos del módulo.
Stdlib inicial con `matematicas` y `cadenas`. **78 tests verde**.

### Añadido (v0.9.0)
- **Tipo `VAL_MODULO`** + `struct Modulo` en `valor.{h,c}`: nombre +
  diccionario de atributos. Pretty-printed `<modulo X>`. tipo() reporta
  `"modulo"`. Ni hashable ni iguales por valor (identidad por puntero).
- **Opcode `OP_IMPORTAR [byte name_idx]`** en bytecode:
  1. Si el módulo está en cache (`vm->cache_modulos`), solo asigna la
     global del importador.
  2. Sino, busca el archivo (`./{nombre}.cor` luego
     `stdlib/{nombre}.cor`), lex+parse+compile.
  3. Crea un nuevo `Modulo` y un nuevo `Diccionario` para sus globales,
     poblado inicialmente con las nativas (imprimir, etc.).
  4. Empuja un sub-frame con el chunk del módulo y cambia
     `vm->globales` al dicc del módulo. El frame guarda
     `globales_pre_modulo` para restaurar al retornar.
  5. Cuando el frame del módulo termina (OP_RETORNAR detecta
     `modulo_en_carga`), captura el dicc de globales en
     `mod->atributos`, restaura el dicc principal y registra el módulo
     como global del importador + en cache.
- **`Closure.globales_definicion`**: cada closure captura el dicc de
  globales del scope donde fue creada. Crítico para módulos: una
  función definida en un módulo, cuando se invoca desde fuera, sigue
  viendo las globales del módulo (no las del importador). Sin esto,
  `mat.cuadrado(5)` daría `ErrorDeNombre` al intentar resolver `n`,
  `cuadrado`, etc., desde el contexto del importador.
- **`CallFrame.globales_pre_llamada`**: `OP_LLAMAR` (en sus tres
  variantes: closure, constructor, bound method) detecta si la closure
  tiene una `globales_definicion` distinta a la actual; si es así,
  guarda la actual y cambia. `OP_RETORNAR` restaura.
- **OP_OBTENER_ATRIBUTO** ahora despacha sobre `VAL_MODULO`: lookup en
  `modulo.atributos` con `ErrorDeAtributo` si no existe.
- **Compilación de `SENT_IMPORTAR`**: emite `OP_IMPORTAR [name_idx]`.
  Limitaciones documentadas: solo `importar X` simple (1 segmento, sin
  `como`); `importar X.Y` y `importar X como Y` rechazados con error
  claro (a cubrir en v0.9.x).
- **`stdlib/matematicas.cor`**: `PI`, `E`, `cuadrado`, `cubo`,
  `absoluto`, `maximo`, `minimo`, `signo`, `factorial`, `suma_rango`,
  `es_par`, `es_impar`, `mcd`. Funciones que se llaman entre sí
  (e.g. `mcd` usa `absoluto`) demuestran el cierre de globales.
- **`stdlib/cadenas.cor`**: `repetir`, `es_vacia`, `unir`. Operaciones
  que requieren indexación por carácter (`s[i]`) están aplazadas
  porque el bytecode no soporta indexación de cadenas en v0.9.0.
- **Cache global de módulos**: `VM.cache_modulos` evita re-cargar el
  mismo archivo en imports repetidos del mismo programa.
- **`tests/unit/test_bytecode_modulos.c`** con 9 tests cubriendo:
  importar constante, importar función, módulo no existe, atributo
  inexistente, `tipo()` reporta "modulo", cache (doble import en mismo
  programa registra una sola vez), subsegmentos rechazados, alias
  rechazado, aislamiento (las globales del módulo no son visibles sin
  prefijo).
- **`examples/21_modulos_jugable.cor`** que importa `matematicas` y
  `cadenas`, ejercita constantes, funciones simples, funciones que
  llaman a otras del mismo módulo, y `tipo()` sobre un módulo.

### Correcciones
- **Bug crítico en OP_IMPORTAR**: el frame del módulo no inicializaba
  `globales_pre_llamada`, que en OP_RETORNAR es leído como puntero
  para restaurar globales. Memoria sin inicializar contenía a veces
  basura no-NULL → vm->globales se sobrescribía con un puntero
  inválido → crash en heap corruption. Detectado por iteración
  infinita en `dicc_liberar` durante `vm_destruir`.

### Limitaciones documentadas (a resolver en v0.9.x)
- Sin subsegmentos en path: `importar mat.geometria` no busca
  `mat/geometria.cor`. Llega en v0.9.1.
- Sin alias: `importar mat como m` rechazado. Llega en v0.9.1.
- Sin `desde X importar Y`: la sentencia se reconoce en parser pero el
  compilador rechaza con error explícito. Llega en v0.9.1.
- `cadenas.cor` está limitado por la falta de `s[i]` en bytecode (que
  funciona en tree-walking pero no en bytecode). Resolver requiere
  añadir VAL_CADENA al case OP_INDICE.

## [0.8.3] — 2026-04-29 — excepciones polish

Completa el modelo de excepciones que llevaba postergado desde
v0.6.3: discriminación por tipo, `sino`, `finalmente`, y `lanzar`
re-raise. **75 tests verde**.

### Añadido (v0.8.3)
- **`atrapar Tipo como e:`** discriminado: el atrapador solo coincide
  si la clase de la excepción coincide con el nombre del tipo
  (comparación por cadena del identificador). `atrapar Excepcion`
  funciona como tipo genérico (atrapa cualquier excepción).
- **Múltiples atrapadores** en un mismo `intentar` ahora se compilan
  todos: el primero que coincide se ejecuta; si ninguno coincide, la
  excepción se re-lanza al handler exterior.
- **`sino:`** ejecuta solo si el cuerpo del `intentar` terminó sin
  excepción.
- **`finalmente:`** ejecuta SIEMPRE: tras salida limpia, tras cada
  atrapar exitoso, y antes del re-lanzar si ningún atrapador
  coincide. (Limitación: NO se ejecuta cuando hay `retornar`,
  `romper` o `continuar` que sale del intentar — llega en una versión
  posterior si se necesita.)
- **`lanzar` sin valor (re-raise)**: dentro de un `atrapar Tipo como e:`
  re-emite la excepción capturada. El compilador rastrea aliases de
  atrapadores activos en una pila (`Compilador.atrapador_alias_slots`)
  para que `lanzar` sin valor compile correctamente. Fuera de un
  atrapar con alias, error de compilación claro.
- **Opcode nuevo `OP_COMPROBAR_TIPO_EXC [byte name_idx]`**: peek la
  excepción top, compara su `clase` con la cadena en
  constantes[name_idx], empuja un bool sin descartar la excepción.
  Permite que el handler chequee el tipo antes de decidir si atrapa
  o re-lanza.

### Correcciones críticas en compilar_intentar/compilar_para
- **Bug del aliasing en `OP_ASIGNAR_LOCAL`**: cuando el slot de
  destino y el slot que se acababa de pop coincidían (caso muy común
  en top-level cuando el handler asignaba la excepción al slot 0),
  `valor_destruir(destino)` liberaba el valor que `nuevo` aún
  apuntaba — use-after-free. Detectamos `destino == vm->tope` tras
  `sacar` y saltamos el destruir.
- **Bug del scope persistente**: el compilador acumulaba locals en el
  scope top-level entre bloques `intentar`/`para`, pero el runtime
  stack es transitorio. Ahora `compilar_intentar` y `compilar_para`
  guardan `n_locales` al entrar y emiten `OP_DESCARTAR` por cada
  local introducido al salir (cuerpo, alias, $iter, target). Sin
  esto, bloques posteriores leían valores stale del slot 0.
- **Bug de aliasing entre atrapadores**: si dos atrapadores usaban el
  mismo nombre de alias (`e` por convención), el segundo encontraba
  el slot del primero via `buscar_local` y emitía `ASIGNAR_LOCAL`
  que dropeaba `tope` por debajo del local. Ahora cada atrapador
  añade un local fresco; entre atrapadores se resetea `n_locales` al
  valor de entrada al handler para que cada alias caiga en el mismo
  slot consistente con la posición real en stack.

### Tests nuevos
- `test_atrapar_por_tipo`, `test_atrapar_excepcion_atrapa_todo`,
  `test_atrapar_sin_match_propaga`: discriminación por tipo y
  fallback a re-raise.
- `test_sino`: ejecuta solo si no hubo excepción (positivo y negativo).
- `test_finalmente`: tras salida limpia y tras atrapar exitoso.
- `test_lanzar_reraise`: re-raise dentro de función propaga al
  llamador.
- `test_lanzar_reraise_sin_alias_es_error`: `lanzar` sin valor sin
  contexto → error de compilación.
- `test_intentar_blocks_repetidos`: bloques intentar consecutivos en
  top-level no se contaminan (test del bug de scope persistente).

## [0.8.2] — 2026-04-29 — super multinivel correcto

Resuelve la limitación de `super` que llevaba arrastrándose desde
v0.7.1: ahora `super.metodo()` funciona correctamente con cualquier
profundidad de herencia, no solo 1 nivel.

### Añadido (v0.8.2)
- **Campo `Clase *clase_definicion` en `Closure`**: la clase donde el
  closure fue registrado como método. NULL si no es método (función
  top-level, lambda, función anidada).
  - Antes de v0.8.2 (refcount sin GC), este campo crearía un ciclo
    `Clase → metodos[m] → Closure → clase_definicion → Clase` que el
    refcount no podía romper. Ahora con GC mark-sweep (v0.8.0+), el
    ciclo se rompe automáticamente cuando la clase deja de ser
    alcanzable. La razón por la que esta sesión es post-v0.8.1.
- **`OP_METODO`** ahora set `closure->clase_definicion = clase` (con
  retención) tras meter la closure en `clase.metodos`. Cada
  declaración o redefinición de método actualiza este campo en el
  closure correspondiente.
- **`OP_HEREDAR`** preserva el `clase_definicion` heredado: cuando el
  hijo hereda un método del padre via copia, el closure compartido
  mantiene `clase_definicion = Padre`. Esto es lo que queremos —
  `super` dentro de un método heredado busca en el padre original,
  no en la clase del hijo.
- **`OP_SUPER_INVOCAR`** ahora resuelve
  `frame->closure->clase_definicion->superclase` en lugar de
  `yo.clase.superclase`. Resultado correcto para varios niveles:
  - `Abuelo → Padre → Nieto`. Si el método actual fue declarado en
    Padre, super busca en Abuelo, **incluso si `yo` es un Nieto**.
  - Si `yo.clase` se usara (v0.7.1), `super` desde Padre.metodo en una
    instancia de Nieto resolvería incorrectamente a Padre, causando
    recursión infinita.
  - Fallback al esquema antiguo (`yo.clase.superclase`) si el closure
    no tiene `clase_definicion` set (caso edge: función llamada como
    método sin pasar por una declaración de clase).
- **`gc_marcar_objeto` para closure** ahora también propaga la marca a
  `clase_definicion` para que la clase no sea barrida mientras el
  método siga vivo.
- **`closure_liberar`** decrementa el refcount de `clase_definicion`
  además de la plantilla y los upvalues.
- **Tests nuevos** en `test_bytecode_clases.c`:
  - `test_super_multinivel`: Abuelo → Padre → Nieto. Padre.via_super()
    llama super.m(); con un Nieto como receptor, el resultado es
    `"abuelo"` (no `"padre"` como sería con la implementación
    incorrecta).
  - `test_super_multinivel_constructor`: Cadena de constructores
    Nieto→Padre→Abuelo via super.__iniciar__(), cada uno añadiendo un
    atributo. Verificar que los tres atributos se asignan.
- **Versión** bump a `0.8.2`.

### Limitaciones que aún quedan
- `__cadena__` y otros dunders runtime aún sin implementar — llegan
  en v0.8.3 si se sigue por esta línea, o se aplazan a Fase 9.

## [0.8.1] — 2026-04-29 — GC automático + recolectar() built-in

Activa el trigger automático del recolector que en v0.8.0 quedó
diferido por el problema de las factories anidadas. Añade
`recolectar()` como built-in callable desde código Cornamusa.
**75 tests verde**, **incluido test_bytecode_gc.c bajo `--gc-stress`**.

### Añadido (v0.8.1)
- **Modelo "deferred-to-opcode-boundary"** en `gc_alocar`: cuando
  detecta que el GC debería correr (umbral cruzado o `gc_stress`),
  no ejecuta la recolección inmediatamente — solo marca un flag
  `Memoria.trigger_pendiente`. El dispatch loop de la VM lo chequea
  al inicio de cada iteración (cuando el stack está consistente entre
  opcodes) y ejecuta la recolección en ese punto seguro. Resuelve el
  problema de las factories anidadas (clase_nueva → dicc_nuevo, etc.)
  porque cualquier alocación dentro de un opcode termina antes del
  trigger.
- **Built-in `recolectar()`**: ejecuta un ciclo de mark-sweep manual
  desde código Cornamusa. Devuelve el número de objetos heap liberados
  durante la pasada (entero ≥ 0). Acepta 0 args; aridad incorrecta
  produce `ErrorDeTipo` claro. Funciona también para limpiar ciclos
  intencionalmente desde el usuario.
- **Flag `--gc-stress` ahora funcional**: compilando con
  `cmake -DCORNAMUSA_GC_STRESS=ON` hace que cada `gc_alocar` marque el
  flag pendiente, que el siguiente opcode dispatch dispara. Útil para
  validar que cada alocación es segura y todas las raíces se marcan
  correctamente.
- **`gc_marcar_raices(VM*)`** ahora también marca las constantes del
  chunk de cada frame activo (incluido el frame top-level cuyo closure
  es NULL). Las constantes incluyen plantillas y cadenas dueñas que
  deben sobrevivir.
- **`tests/unit/test_bytecode_gc.c`** con 7 tests end-to-end:
  `recolectar()` devuelve entero, aridad incorrecta, libera ciclo de
  diccionarios, libera ciclo de instancias, no toca objetos vivos,
  carga pesada (50 listas en bucle) bajo gc_stress no explota,
  métodos en bucle (20 instancias + dispatch) funcionan.
- **Refactor `vm_ejecutar` → `vm_ejecutar_dispatch` interno + wrapper
  público**: el wrapper activa `gc_habilitado=true` al entrar y
  `false` al salir, garantizando que el trigger automático solo opere
  durante la ejecución (no durante la fase de compilación entre
  `vm_iniciar` y `vm_ejecutar`).
- **Versión** bump a `0.8.1`.

### Limitaciones que aún quedan (a resolver en v0.8.2+)
- `super` multinivel sigue restringido a 1 nivel (limitación de v0.7.1).
  Resolver requiere `clase_definicion` en Closure que ahora con GC es
  posible sin leak; el cambio se aplaza a v0.8.2.
- `__cadena__` y otros dunders runtime aún sin implementar.

## [0.8.0] — 2026-04-29 — GC mark-sweep tri-color (Fase 7)

Sustituye al refcount como fundamento del modelo de memoria, sin
eliminarlo todavía: refcount sigue siendo el liberador primario y el
GC complementa para limpiar ciclos. **74 tests verde**.

Esta versión es principalmente infraestructura — no hay cambios
visibles al usuario en el lenguaje. Habilita correcciones futuras
(super multinivel, `__cadena__`, etc.) que requieren ciclos seguros
en el modelo de memoria.

### Añadido (v0.8.0)
- **`src/memoria.{h,c}`** con la infraestructura completa de GC
  mark-sweep tri-color simplificado (white/black, sin gris explícito):
  - `GCObject` header (siguiente, marcado, tipo): primer campo de cada
    struct heap-rastreado.
  - `Memoria` con linked-list `cabeza` de objetos vivos + estadísticas
    (total_alocado, total_objetos, umbral_gc) + flag `gc_stress`.
  - `gc_alocar(size, tipo)`: alocator central que enlaza el objeto a
    la lista de la `Memoria` global instalada via `gc_instalar`.
  - `gc_desenlazar(GCObject *)`: usado por los `*_liberar` de refcount
    para sacar el objeto de la lista cuando el refcount los libera.
  - `gc_marcar_valor(Valor *)` y `gc_marcar_objeto(GCObject *)`:
    propagación recursiva idempotente (corta ciclos via flag marcado).
  - `gc_barrer(Memoria *)`: recorre la lista, libera no-marcados con
    un destructor "no recursivo" que solo libera partes propietarias
    no-GC (mp_int, char* dueño, buffers de tablas hash) y la struct
    misma. NO decrementa refcounts de hijos heap-rastreados — esos se
    procesan en la misma pasada cuando el barrido los alcance.
  - `gc_recolectar(Memoria *, FnMarcarRaices, void *ctx)`: orquesta el
    ciclo completo (desmarcar + marcar raíces + barrer).
  - `gc_set_marcador_raices(Memoria *, FnMarcarRaices, void *ctx)`:
    registra el callback que `gc_alocar` usaría para gatillar
    recolección automática (deshabilitado en v0.8.0 — ver limitaciones).
- **Migración de los 12 tipos heap del runtime** a usar `GCObject obj`
  como primer campo: Lista, Diccionario, Conjunto, Tupla, FuncionBC,
  Closure, Upvalue, Iterador, Excepcion, Clase, Instancia,
  MetodoLigado. Sus factory functions usan `gc_alocar`; sus liberadores
  llaman `gc_desenlazar` antes de `free`.
- **VM con `Memoria` propia**: `vm_iniciar` la inicializa e instala
  como global. `vm_destruir` la barre y desinstala — defensa contra
  ciclos refcount cuando el cliente destruye la VM sin haber
  recolectado manualmente.
- **`gc_marcar_raices(VM *)`** en `src/vm.c`: marca el stack
  (pila..tope), las globales (Diccionario), los closures de cada frame
  + las constantes del chunk activo (incluido el frame top-level cuyo
  closure es NULL), y los open_upvalues.
- **Flag `--gc-stress` (CMake `CORNAMUSA_GC_STRESS=ON`)** activable en
  build para habilitar el trigger automático en cada `gc_alocar`.
  Compila pero NO funciona correctamente todavía (ver limitaciones).
- **`tests/unit/test_memoria.c`** con 17 tests cubriendo: alocación con
  y sin Memoria instalada, enlace y desenlace correctos, destrucción
  masiva, integración con cada tipo migrado, mark de valores planos
  (no-op), recursión via lista anidada, dicc clave/valor, clase +
  instancia + superclase + atributos, idempotencia, ciclos sin
  recursión infinita, raíces de la VM real, sweep libera no marcados,
  recolección rompe ciclos refcount, recolección preserva marcados,
  destrucción de Memoria sin leaks visibles.

### Limitaciones conocidas en v0.8.0 (a resolver en v0.8.x)
- **Trigger automático del GC deshabilitado**. La razón: muchas factory
  functions anidan llamadas a `gc_alocar` (ej. `clase_nueva` aloca la
  Clase y luego un Diccionario para sus métodos; `instancia_nueva`
  igual). Tras la primera alocación el objeto está en la lista pero
  todavía no es alcanzable desde ninguna raíz; un trigger interno lo
  barrería incorrectamente. La solución limpia es añadir paréntesis
  `gc_pausar/gc_reanudar` en cada factory, o un modelo de trigger a
  nivel de opcode-boundary. En v0.8.0 el GC se invoca solo manualmente
  via `gc_recolectar` desde C; el built-in `recolectar()` para código
  Cornamusa llega en v0.8.1.
- **Refcount sigue siendo primario**. El GC limpia solo lo que el
  refcount no liberó (típicamente ciclos). Eliminar el refcount por
  completo requiere primero arreglar el trigger automático.
- **`super` multinivel sigue restringido a 1 nivel** (limitación
  documentada de v0.7.1). Resolver requiere `clase_definicion` en
  Closure que crea un ciclo refcount; ahora con GC es posible, pero el
  cambio se aplaza a v0.8.x junto con la activación automática.
- **`__cadena__` y otros dunders runtime** siguen sin implementar —
  llegan en v0.8.x ahora que GC permite invocar métodos durante
  `imprimir()` sin riesgo de leaks.

### Cambios internos
- `chunk.c` ahora `#include "memoria.h"` para gc_alocar/gc_desenlazar
  en `funcion_bc_nueva`/`closure_nuevo`/`upvalue_nuevo` y sus
  liberadores.
- `valor.h` `#include "memoria.h"` para que cada struct heap pueda
  tener `GCObject obj` como primer campo.
- Refactor `vm_ejecutar` → `vm_ejecutar_dispatch` (interno) +
  `vm_ejecutar` (wrapper público que activaría el flag `gc_habilitado`
  cuando el trigger automático esté disponible).

## [0.7.1] — 2026-04-29 — super en bytecode

Cierra el ciclo OOP en bytecode añadiendo `super.metodo(args)` para
herencia simple. Ejemplos como `examples/20_clases_jugable.cor` ahora
encadenan constructores hijo/padre. **73 tests verde**.

### Añadido (v0.7.1)
- **Palabra clave `super`** activa en el parser (el lexer ya tenía `TT_SUPER` desde Fase 2). Solo válida en la forma `super.metodo(args)`.
- **Nuevo nodo AST `EXPR_SUPER`**: guarda el nombre del método tras el punto. El receptor (`yo`) es implícito (slot 1 del frame del método actual).
- **`parsear_super`** registrada como prefix-rule en `obtener_regla(TT_SUPER)`. Espera `super.identificador`; si no hay `.` o no hay identificador tras el punto, error de sintaxis claro.
- **Compilación de `EXPR_LLAMADA(EXPR_SUPER, args)`**: emite `OP_OBTENER_LOCAL 1` (push `yo`) + args + `OP_SUPER_INVOCAR [name_idx] [n_args]`. Validación: solo dentro de un scope de función (método); fuera de un método o sin llamada inmediata, error claro de compilación.
- **Nuevo opcode `OP_SUPER_INVOCAR [byte name_idx] [byte n_args]`**:
  - Stack al ejecutar: `[..., yo, arg1, ..., argN]`.
  - Resuelve `yo.clase.superclase.metodos[name]`, valida aridad incluyendo `yo` (error reporta cifras sin el receptor).
  - Despacha igual que un bound method: `memmove` args un slot arriba, reemplaza el callee con la closure y pone receptor (clonado) en slot 1.
  - Errores claros: `'super' solo puede usarse en metodos de instancia`, `la clase '...' no tiene superclase`, `ErrorDeAtributo: la superclase '...' no tiene metodo '...'`.
- **Limitación documentada**: la búsqueda de super usa `yo.clase.superclase`, no la clase donde el método actual fue definido. Para herencia de un solo nivel (Padre → Hijo) coincide; para varios niveles (Padre → Hijo → Nieto, `super` dentro de un método de Hijo) se requiere almacenar `clase_definicion` en `Closure`, lo que crea un ciclo refcount → llega en v0.8.0 con GC mark-sweep.
- **Tests nuevos** (en `test_bytecode_clases.c`): `super.metodo()` simple, `super.__iniciar__(args)` en constructor del hijo (con campo extra propio), aridad incorrecta vía super, super sin superclase, método inexistente en superclase, super fuera de método (error compilación), super sin punto (error sintaxis).
- **Ejemplo `20_clases_jugable.cor`** actualizado: `Perro` ahora tiene su propio `__iniciar__(yo, nombre, edad, raza)` que llama a `super.__iniciar__(nombre, edad)` antes de asignar `yo.raza`.
- **Versión** bump a `0.7.1`.

## [0.7.0] — 2026-04-29 — clases, métodos, herencia (Fase 8)

Cornamusa pasa a ser un lenguaje OOP completo: clases definibles por
el usuario con atributos mutables, métodos con `yo` autoinyectado,
constructor `__iniciar__`, y herencia simple por copia de métodos.
**71 tests verde**.

### Añadido (v0.7.0)
- **Tipos nuevos en `valor.{h,c}`**:
  - **`VAL_CLASE`** + `struct Clase`: nombre heap-duplicado, `metodos` (Diccionario cadena → VAL_FUNCION_BC), `superclase` opcional, refcount. Pretty-printed `<clase Foo>`.
  - **`VAL_INSTANCIA`** + `struct Instancia`: referencia compartida a su `Clase`, `atributos` (Diccionario propio modificable), refcount. Pretty-printed `<instancia de Foo>`.
  - **`VAL_METODO_LIGADO`** + `struct MetodoLigado`: receptor (Valor con refcount) + método (Closure con refcount). Construido al acceder a `instancia.metodo` cuando el nombre está en `clase.metodos`. Pretty-printed `<metodo nombre>`. `valor_nombre_tipo` lo reporta como `"funcion"`.
  - Lifecycle (destruir/clonar/iguales/es_verdadero/es_hashable/nombre_tipo) wired para los tres tipos. Identidad por puntero; ninguno hashable.
- **5 opcodes nuevos** en bytecode:
  - **`OP_CLASE [byte name_idx]`**: crea `Clase` con el nombre indicado y la empuja.
  - **`OP_OBTENER_ATRIBUTO [byte name_idx]`**: lookup de instancia con fallback. Primero busca en `instancia.atributos` (override); si no está, busca en `instancia.clase.metodos` y, si encuentra una closure, crea un `MetodoLigado(instancia, closure)`. `ErrorDeAtributo` si no existe en ninguno; `ErrorDeTipo` si el objeto no es instancia.
  - **`OP_ASIGNAR_ATRIBUTO [byte name_idx]`**: pop valor, pop instancia, set `atributos[nombre] = valor`, push nulo (la sentencia descarta).
  - **`OP_METODO [byte name_idx]`**: con stack `[..., clase, closure]`, pop closure y guardarla en `clase.metodos[name]`; clase queda en el tope para más métodos.
  - **`OP_HEREDAR`** (sin operando): con stack `[..., clase, super]`, pop super, copia `super.metodos → clase.metodos` (los OP_METODO posteriores sobrescriben para implementar override) y enlaza `clase.superclase = super`.
- **`OP_LLAMAR` despacha sobre tres nuevos tipos de callee**:
  - **`VAL_CLASE`**: instancia la clase. Si tiene `__iniciar__`, lo invoca como método con la instancia recién creada como receptor; aridad chequeada incluyendo `yo` (el error reporta cifras sin el receptor). Sin `__iniciar__` y `n_args > 0` → error claro. La llamada `Foo(args)` siempre devuelve la instancia, no lo que `__iniciar__` retorne.
  - **`VAL_METODO_LIGADO`**: inserta el receptor como primer argumento del frame (`memmove` los args un slot arriba, reemplaza el callee con la closure y pone el receptor en slot 1). Aridad chequeada con receptor incluido; el error reporta cifras sin él.
- **Nuevo flag `CallFrame.es_constructor`**: marca el frame de `__iniciar__` para que `OP_RETORNAR` descarte el valor de retorno y devuelva la instancia (slot 1) en su lugar.
- **Compilación de `SENT_CLASE`** completa:
  - Cuerpo admite `SENT_PASAR` y `SENT_FUNCION` (métodos); cualquier otra sentencia produce error claro.
  - Para cada método: emite la closure vía el nuevo helper `emitir_closure_de_funcion` (refactor de `compilar_funcion`, factor común) + `OP_METODO [name_idx]`.
  - `extiende Padre` (un solo padre): emite la expresión del padre + `OP_HEREDAR` antes de los métodos. Herencia múltiple rechazada en compilación.
- **Compilación de `EXPR_ATRIBUTO`** (lectura) y **`obj.attr = valor`** (escritura):
  - `obj.attr` lectura → `obj` + `OP_OBTENER_ATRIBUTO [idx]`.
  - `obj.attr = valor` → `obj` + `valor` + `OP_ASIGNAR_ATRIBUTO [idx]` + `OP_DESCARTAR`.
- **`yo` por convención** (decisión B5+B6): el primer parámetro de un método (idiomáticamente `yo`) recibe la instancia automáticamente al llamarlo via `instancia.metodo(args)`. No es palabra reservada.
- **Constructor `__iniciar__`** (dunder en castellano, decisión B5+B6) reemplaza el patrón `__init__` de Python.
- **Métodos encadenables**: `obj.m1().m2().m3()` retornando `yo`.
- **Herencia simple**: el hijo recibe los métodos del padre (por copia al ejecutar `OP_HEREDAR`); puede sobrescribir en su propio cuerpo. Hereda también `__iniciar__` si no lo redefine. Polimorfismo: cada subclase dispatcha a su propio método al ser invocado.
- **Limitaciones documentadas v0.7.0** (a cubrir en v0.7.x patches):
  - Sin `super` (la palabra clave existe en el lexer pero no se usa todavía).
  - Sin `__cadena__` (usar `imprimir(obj.atributo)` o métodos custom mientras tanto).
  - Sin operator overloading (otros dunders como `__sumar__`, `__igual__`, etc.).
  - Sin atributos de clase (solo de instancia).
  - Solo herencia simple (parser admite múltiples padres pero el compilador rechaza).
- **`tests/unit/test_bytecode_clases.c`** con 18 grupos cubriendo: definición y `tipo()`, instanciación, atributos (lectura/escritura/sobrescritura/mutación compartida), errores runtime (atributo inexistente / asignación a no-instancia / lectura de no-instancia / llamada con args), métodos (sin args, con args, mutación via `yo`, chaining, aridad incorrecta, sombrea con atributo, tipo correcto), constructor `__iniciar__` (con y sin args, retorno ignorado, aridad incorrecta, combinado con métodos), herencia (métodos heredados, override, constructor heredado, polimorfismo, mezcla override/heredado), errores compilación (herencia múltiple, heredar de no-clase), identidad por `es`.
- **Versión** bump a `0.7.0`.

## [0.6.3] — 2026-04-29 — excepciones en bytecode

El motor bytecode ahora maneja `intentar`/`atrapar` y `lanzar`. Cierra
el motor casi-completo módulo atributos (Fase 8) y módulos.

### Añadido (v0.6.3)
- **`VAL_EXCEPCION`** y **`struct Excepcion`** en `valor.{h,c}`: tipo runtime con clase + mensaje (cadenas heap-duplicadas) y refcount. Pretty-printer formato `<clase>: <mensaje>`.
- **Built-ins de construcción de excepciones**:
  - **`Excepcion(clase, mensaje)`**: constructor genérico.
  - **`ErrorAritmetico("...")`**, **`ErrorDeTipo("...")`**, **`ErrorDeValor("...")`**, **`ErrorDeIndice("...")`**, **`ErrorDeClave("...")`**, **`ErrorDeNombre("...")`**: atajos con clase prerellenada (1 argumento = mensaje).
- **3 opcodes nuevos** y `HandlerFrame` stack en VM:
  - **`OP_INTENTAR_INICIAR [u16 offset_handler]`**: empuja un `HandlerFrame` con el snapshot del estado (n_frames, tope, n_open_upvalues) y la dirección del handler. Hasta 64 handlers anidados (`VM_HANDLERS_MAX`).
  - **`OP_INTENTAR_FIN`**: pop el handler frame al salir limpio del bloque `intentar`.
  - **`OP_LANZAR`**: pop la excepción del tope. Si es cadena, se envuelve como `Excepcion("Excepcion", cadena)`. Busca el handler frame top: si no hay → error en VM con clase y mensaje. Si hay → cierra upvalues abiertos por encima del handler, descarta slots del stack hasta el `tope_offset`, hace pop de frames hasta `frame_idx`, y empuja la excepción para que el handler la consuma. Salta a `ip_handler`.
- **Compilación de `SENT_INTENTAR`**:
  - Estructura: `INTENTAR_INICIAR offset → cuerpo → INTENTAR_FIN → SALTAR fin → handler: → atrapador → fin:`.
  - Soporta `atrapar [Tipo] [como alias]:`. El alias se registra como local (mismo patrón que `x = 5` creando un local nuevo: el valor de la excepción ya está en su slot final tras el `OP_LANZAR`).
  - El tipo, si está presente, se evalúa pero **no se compara** todavía (limitación v0.6.3 — `atrapar Excepcion como e:` atrapa cualquier cosa). Atrapar discriminando por tipo llega en v0.6.4.
- **Compilación de `SENT_LANZAR`**: compila la expresión + `OP_LANZAR`. `lanzar` desnudo (re-raise) aún no soportado.
- **Limitaciones documentadas v0.6.3** (todas a cubrir en v0.6.4+):
  - Solo el primer atrapador de un `intentar` se compila (los demás se aceptan en parser pero se ignoran).
  - Tipo de excepción (`atrapar ErrorAritmetico:`) no discrimina; el handler atrapa todo.
  - Cláusula `sino` (rama "sin excepción") aún no compila — error explícito.
  - Cláusula `finalmente` aún no compila — error explícito.
  - `lanzar` sin valor (re-raise) aún no compila.
  - Tree-walking sigue sin implementar excepciones (decisión B2).
- **`tests/unit/test_bytecode_excepciones.c`** con 9 grupos: construcción de excepciones (genérica + atajos), atrapar simple con alias, mensaje del alias, cuerpo sin excepción no entra al atrapar, excepción dentro de función propaga al llamador, programa `dividir` robusto (con/sin cero), excepciones no atrapadas como error VM, anidamiento de `intentar` (interno atrapa / interno re-lanza al externo), `lanzar` cadena (azúcar a `Excepcion("Excepcion", cadena)`).
- **Versión** bump a `0.6.3`.
- **70 tests verde** (27 unit + 43 integración).

## [0.6.2] — 2026-04-29 — closures + lambdas + slicing en bytecode

El motor bytecode ahora ejecuta el lenguaje completo módulo
excepciones, atributos y módulos. **8 de 9 ejemplos jugables
v0.5 + el nuevo 19_closures corren con `--bytecode`**.

### Añadido (v0.6.2)
- **`OP_REBANADA`** y compilación de `EXPR_REBANADA`: slicing `lista[a:b:c]` con cualquier campo opcional. Operandos faltantes se emiten como `OP_NULO` (sentinela). VM despacha con la misma semántica que el evaluador tree-walking: defaults dependientes del signo del paso, índices negativos cuentan desde el final, fuera de rango se clampea silenciosamente, paso 0 → `ErrorDeValor`.
- **Closures con upvalues** (estilo clox cap. 25). Refactor mayor:
  - **Separación `FuncionBC` (plantilla, en pool) vs `Closure` (instancia, en stack)**:
    - `FuncionBC` mantiene chunk + nombre + aridad + metadata de upvalues (`info_upvalues[]`).
    - `Closure` envuelve un `FuncionBC` y añade `Upvalue **upvalues` runtime.
    - Nuevo `VAL_PLANTILLA_BC` para el constant pool; `VAL_FUNCION_BC` ahora apunta a `Closure`.
  - **`Upvalue` runtime**: linked-list ordenada por posición decreciente en stack (`vm->open_upvalues`). Refcount para compartir entre múltiples closures que capturan la misma variable.
  - **`OP_CLOSURE [byte fn_idx] [n_upvalues * (es_local, indice)]`**: lee la plantilla del pool, crea Closure nuevo, conecta cada upvalue a su slot del frame actual (vía `capturar_upvalue`) o a un upvalue existente.
  - **`OP_OBTENER_UPVALUE [slot]`** y **`OP_ASIGNAR_UPVALUE [slot]`**: lectura/escritura via `frame->closure->upvalues[slot]->posicion`.
  - **`OP_RETORNAR` cierra upvalues** del frame que termina con `cerrar_upvalues_hasta`. Al cerrar, el valor del slot se **transfiere** (no se duplica) al campo `cerrado` del upvalue, y el slot original se vacía a `nulo` para evitar double-free.
  - `CallFrame` ahora tiene un puntero `closure` (NULL en frame top-level) que la VM usa para resolver upvalues durante la ejecución.
- **Compilador con resolución de upvalues recursiva**: `EXPR_IDENT`, `compilar_asignar` y `compilar_asignar_aug` siguen el orden **local → upvalue → global**. La función `resolver_upvalue(scope)` busca la variable en el scope padre directo (como local) y, si no la encuentra, recurre subiendo la cadena de scopes (capturando como "upvalue de upvalue"). El compilador rellena la metadata de upvalues en la `FuncionBC` para que `OP_CLOSURE` la use en runtime.
- **Funciones anidadas dentro de función ahora se registran como locales** (no como globales como antes en S5). El compilador detecta `c->actual->es_funcion` y emite `OP_DEFINIR_GLOBAL` solo en top-level.
- **Lambdas (`EXPR_LAMBDA`)** compiladas como funciones anónimas con cuerpo expresión:
  - Crea `FuncionBC` con nombre `"lambda"`.
  - Abre scope hijo con parámetros como locales.
  - Compila el cuerpo como expresión y emite `OP_RETORNAR`.
  - Emite en el padre `OP_CLOSURE` con metadata de upvalues capturados.
  - Sin valores por defecto (mismo que `SENT_FUNCION` en bytecode); error explícito si se usan.
- **Nuevo ejemplo `examples/19_closures_jugable.cor`** que demuestra contadores con estado y lambdas factory. Bytecode-only (decisión B2: el tree-walking no implementa closures).
- **`tests/unit/test_bytecode_closures.c`** con 7 grupos:
  - Captura simple de local desde función anidada.
  - Contador clásico (3 invocaciones).
  - Closures independientes (factoría produce instancias con estado separado).
  - Captura de dos niveles arriba (upvalue de upvalue).
  - Lambda sin captura.
  - Lambda con captura (factory de multiplicadores).
  - Slicing varios casos (omisiones, paso negativo, fuera de rango).
- **`bc_run_*` ampliados**: `16_lista_busqueda` (slicing) y `19_closures_jugable` ahora se ejecutan con `--bytecode` y verifican salida.
- **Versión** bump a `0.6.2`.
- **69 tests verde** (26 unit + 43 integración).

### Aplazado a v0.6.3+
- **Excepciones** (`intentar`/`atrapar`/`finalmente`/`lanzar`).
- **Atributos** (`obj.attr`) — necesita primero el sistema de objetos (Fase 8).
- **Módulos** (`importar`).
- **`global`/`nolocal`** declaraciones explícitas.
- **Valores por defecto** en parámetros bytecode (sí en tree-walking).

## [0.6.1] — 2026-04-29 — bytecode con iteración

Bytecode amplía soporte: `SENT_PARA` con iteradores genéricos sobre
listas, tuplas, cadenas (UTF-8), rangos, diccionarios y conjuntos.
Asignación aumentada con destino índice (`dicc[k] += 1`). 7 de 9
ejemplos jugables corren ya por bytecode.

### Añadido (v0.6.1)
- **`VAL_ITERADOR`** y **`struct Iterador`** en `valor.{h,c}`: tipo VM-only (no expuesto al usuario) que mantiene el estado de iteración. Campos: copia con refcount del iterable + cursor int. La función `iter_siguiente` despacha por tipo:
  - **Lista/Tupla**: cursor = índice.
  - **Cadena**: cursor = byte position; avanza por code points UTF-8 con `utf8proc_iterate`.
  - **Diccionario/Conjunto**: cursor = slot interno; salta entradas vacías; emite claves (dict) o elementos (conjunto).
  - **Rango**: cursor = número de iteración; calcula `inicio + cursor*paso` cada vez.
- **`OP_ITER_INICIAR`**: pop iterable, push iterador (validado con `valor_es_iterable`).
- **`OP_ITER_SIGUIENTE [byte slot] [u16 offset]`**: lee el iterador del slot dado del frame actual. Si tiene siguiente, push valor; si no, salta `offset` bytes (los `OP_ASIGNAR_LOCAL` siguientes no se ejecutan, el slot iterador se libera con el frame).
- **`SENT_PARA` en compilador**:
  - Compila iterable + `OP_ITER_INICIAR`.
  - Reserva un local oculto `$iter` con el iterador en su slot.
  - Si el objetivo es local: pre-asigna con `OP_NULO` y emite `OP_ASIGNAR_LOCAL` en cada iteración (evita el bug "asignar al top sobre sí mismo").
  - Si es top-level: emite `OP_DEFINIR_GLOBAL` en cada iteración.
  - Soporta `romper`/`continuar` y cláusula `sino` (ejecutada solo al agotarse el iterador, no por break).
- **Locales en scope top-level**: el compilador permite registrar locales (vía `agregar_local`) incluso en `es_funcion=false`. Esto habilita el slot oculto `$iter` y evita interferencias con globales del usuario.
- **`OP_DUP_2`**: duplica los dos valores del tope (`a, b → a, b, a, b`). Necesario para implementar aug-assign en índice sin reevaluar `obj` y `key`.
- **`compilar_asignar_aug` con destino `EXPR_INDICE`**: compila como `obj key OP_DUP_2 OP_INDICE valor OP_op OP_ASIGNAR_INDICE OP_DESCARTAR`. Permite `dicc[k] += 1`, `lista[i] *= 2`, etc.
- **`OP_ES` y `OP_EN`** añadidos al enum `OpCode` y mapeados desde `TT_ES`/`TT_EN` en el compilador. La VM los despacha a `evaluador_aplicar_binario` igual que las comparaciones — la lógica completa (identidad, membership en cadena/lista/dicc/conjunto/tupla) se reusa del refactor de S2.
- **Versión** bump a `0.6.1`.
- **`tests/unit/test_bytecode_iter.c`** con 10 grupos: `para` sobre cadena (incluido UTF-8), rango (con paso negativo y `rango(n)`), lista/tupla, dicc/conjunto, `romper`, `continuar`, cláusula `sino`, `para` dentro de función (conteo de vocales en `"murcielago"`), aug-assign con índice (frecuencia de letras en `"abracadabra"`, mutación de lista), factorial(25) iterativo via bytecode (26 dígitos).
- **`bc_run_*` ampliados**: `02_fizzbuzz`, `14_contar_vocales`, `15_fizzbuzz_jugable`, `17_dicc_frecuencia`, `18_conj_y_tupla` ahora se ejecutan con `--bytecode` y verifican misma salida que tree-walking.
- **65 tests verde** (25 unit + 40 integración: 12 lex + 8 parse + 13 run + 7 bc_run).

### Aplazado a v0.6.2+
- **Closures con upvalues** (estilo clox cap. 25): funciones anidadas que capturan locales del scope enclosing. Requiere `OP_CLOSURE`, `OP_GET_UPVALUE`, `OP_SET_UPVALUE`, `OP_CLOSE_UPVALUE` y tracking runtime de upvalues abiertos.
- **Lambdas** (`lambda x: x*2`): mismo modelo que `SENT_FUNCION` pero como expresión.
- **Slicing** (`lista[a:b:c]`): `OP_REBANADA`.
- **Atributos** (`obj.attr`): hace falta primero el sistema de objetos (Fase 8).
- **Excepciones** (`intentar`/`atrapar`/`finalmente`): tabla de excepciones por chunk, manejo de stack unwinding.

## [0.6.0] — 2026-04-29 — motor bytecode (opt-in)

Cierre de Fase 6 según el plan: compilador AST → bytecode + VM
stack-based ejecutando expresiones, sentencias, control de flujo,
funciones con recursión y colecciones. Motor opt-in con flag
`--bytecode`; el tree-walking sigue siendo el por defecto en v0.6.0
para preservar la cobertura completa del lenguaje (incluida la
iteración `para`, que el bytecode aún no soporta).

### Añadido (Fase 6 sesión 6)
- **`FnNativa` refactorizado para `EvalError *`**: las funciones nativas (built-ins) ya no dependen del struct `Evaluador` — toman `EvalError *` directamente. Esto permite invocarlas tanto desde el evaluador tree-walking como desde la VM bytecode sin acoplarlas a uno de los dos motores. Cambio de firma propagado a todas las nativas (`imprimir`, `longitud`, `tipo`, `rango`, `agregar`, `quitar`, `insertar`, `invertir`, `ordenar`, `claves`, `valores`, `conjunto`).
- **`nativos_registrar_dicc(Diccionario *globales)`** en paralelo a `nativos_registrar(Entorno *)`: ambos iteran una **lista canónica única** de nativas (`NATIVAS[]` en `nativos.c`) garantizando que tree-walking y bytecode ofrezcan los mismos built-ins.
- **VM `vm_iniciar` ahora registra los built-ins** en `vm->globales` automáticamente — los programas bytecode ya tienen acceso a `imprimir`, `longitud`, `tipo`, `rango`, etc. desde el primer byte de ejecución.
- **`OP_LLAMAR` en VM extendido para `VAL_NATIVA`**: cuando el callee es una nativa, la VM la invoca pasando `&vm->error` y limpia los args/callee del stack al volver. Las funciones definidas por el usuario (`VAL_FUNCION_BC`) siguen creando un `CallFrame` nuevo como en S5.
- **Colecciones literales en bytecode** con cuatro nuevos opcodes:
  - `OP_BUILD_LISTA [n]`: pop n elementos, push lista.
  - `OP_BUILD_TUPLA [n]`: pop n elementos, push tupla.
  - `OP_BUILD_DICC [n_pares]`: pop 2n elementos (k,v intercalados), push diccionario.
  - `OP_BUILD_CONJUNTO [n]`: pop n elementos hashables, push conjunto.
- **Indexación en bytecode**:
  - `OP_INDICE`: pop key, pop obj, push obj[key]. Despacha por tipo (lista/tupla/diccionario) en runtime con mensajes de error específicos (`ErrorDeIndice`, `ErrorDeClave`).
  - `OP_ASIGNAR_INDICE`: pop value, pop key, pop obj — `obj[key] = value`. Soporta listas y diccionarios.
- **Compilador con `EXPR_LISTA`/`EXPR_TUPLA`/`EXPR_DICCIONARIO`/`EXPR_CONJUNTO`/`EXPR_INDICE`**: producen los nuevos opcodes. Los literales con más de 255 elementos producen error explícito (limitación del operando byte).
- **`SENT_ASIGNAR` con destino `EXPR_INDICE`**: el compilador emite el bytecode `obj key valor OP_ASIGNAR_INDICE OP_DESCARTAR`.
- **`VM_PILA_MAX`** ampliado a 8192, **`VM_FRAMES_MAX`** a 256 para acomodar recursión profunda como `factorial(100)` (64 dígitos vía bytecode).
- **`OP_ITER_INICIAR` y `OP_ITER_SIGUIENTE`** reservados en el enum pero no implementados todavía — `SENT_PARA` queda aplazado a v0.6.1 (necesita un VAL_ITERADOR ad hoc o desazucar a `mientras` con manejo de slots temporales). El motor tree-walking sigue siendo el camino para programas que usan `para`.
- **Integración con `cornamusa`** (motor opt-in):
  - **Flag `--bytecode`** en `main.c` activa el pipeline `lex → parse → compilar → vm_ejecutar`. Sin la flag, el motor sigue siendo tree-walking (default en v0.6.0).
  - Errores de compilación o de runtime se reportan con `imprimir_error_runtime` (mismo formato MENSAJES.md §2 con caret).
- **Tests `bc_run_*`**: ejemplos `01_hola_mundo` y `13_factorial_jugable` (recursión que deja `100!` con 158 dígitos vía bytecode) se ejecutan también con `--bytecode` y verifican misma salida que el tree-walking.
- **`tests/unit/test_bytecode_colecciones.c`** con 9 grupos: literales (lista, tupla, dicc, conjunto), indexación (lista/tupla/dicc + errores), asignación a índice, nativas sobre colecciones via OP_LLAMAR, programas mixtos (función que indexa, dicc acumulador).
- **`test_bytecode_funciones.c` extendido** con grupo de nativas vía OP_LLAMAR (longitud sobre cadena UTF-8, rango, tipo).
- **Limitaciones documentadas para v0.6.0**:
  - `SENT_PARA` (`para X en Y`) **no compila a bytecode**; se ejecuta solo en tree-walking.
  - **Closures con upvalues** no implementadas (decisión B2 ya las excluía del tree-walking; las añadiremos en v0.6.1+).
  - Atributos (`obj.attr`), lambda, slicing, f-string interpolada, intentar/atrapar, importar — todos siguen aplazados.
- **Versión** bump a `0.6.0` en `common.h`, `CMakeLists.txt`, smoke test.
- **59 tests verde** (24 unit + 35 integración: 12 lex + 8 parse + 13 run tree-walking + 2 bc_run bytecode).

### Añadido (Fase 6 sesión 5)
- ⏳ Sesión 6: colecciones + transición a tagged i63 + flag `--tree-walking` + tests diferenciales + tag v0.6.0.

### Añadido (Fase 6 sesión 1)
- **`src/chunk.{h,c}`**: estructura `Chunk` con bytecode, pool de constantes y array paralelo de números de línea fuente. Tres arrays paralelos siguiendo clox cap. 14 (renombrado al castellano):
  - `codigo[]` (uint8_t): instrucciones y operandos inline.
  - `constantes[]` (Valor): pool referenciado por `OP_CONST` y `OP_CONST_LARGO`. El chunk es DUEÑO de los Valores y los destruye al liberarse.
  - `lineas[]` (int): un número de línea por byte de código. Sin compresión (suficiente para v0.6; se podrá optimizar a run-length encoding más adelante).
- **`enum OpCode`** con 32 instrucciones reservadas para toda la fase: literales (`OP_CONST`, `OP_CONST_LARGO` para >256 constantes, `OP_NULO`, `OP_VERDADERO`, `OP_FALSO`), aritmética (`+`, `-`, `*`, `/`, `//`, `%`, `**`, negación), lógica/comparación (`no`, `==`, `!=`, `<`, `<=`, `>`, `>=`), stack (`DESCARTAR`), control de flujo (`SALTAR`, `SALTAR_SI_FALSO`, `BUCLE`), variables (locales y globales), llamadas y retorno. Reservadas ahora para que el orden quede estable; se implementan progresivamente en S2-S5.
- **API**:
  - `chunk_iniciar`/`chunk_destruir` (idempotente).
  - `chunk_emitir_byte` y `chunk_emitir_byte2` con crecimiento ×2 amortizado.
  - `chunk_agregar_constante` devuelve índice; `chunk_emitir_constante` elige entre `OP_CONST` (1 byte de índice) y `OP_CONST_LARGO` (3 bytes little-endian) automáticamente cuando el pool supera 255 entradas.
  - `opcode_nombre` para inspección/debug.
- **`src/debug.{h,c}`**: disassembler estilo clox. Formato:
  ```
  == nombre ==
  0000  123 OP_CONST            7 '42'
  0002    | OP_RETORNAR
  ```
  - Offset (4 dígitos), línea fuente (con `|` cuando coincide con la anterior), nombre del opcode alineado a 20 caracteres, operandos formateados según el tipo de instrucción (constante, byte, u16 con destino calculado para saltos).
  - `desensamblar_chunk(chunk, nombre, salida)` y `desensamblar_instruccion(chunk, offset, salida)` (devuelve siguiente offset).
  - Constantes se imprimen con `valor_a_repr` (cadenas con comillas).
- **Limitación documentada**: este es solo el armazón. Sin compilador ni VM funcional aún — eso llega en S2. La idea de S1 es congelar el formato del chunk antes de añadir muchos consumidores.
- **`tests/unit/test_chunk_disasm.c`** con 11 tests: chunk vacío + idempotencia destruir, crecimiento de capacidad (100 bytes), `emitir_byte2`, ownership de constantes (entero/decimal/cadena), `emitir_constante` con índice corto y largo (forzando el cambio a `OP_CONST_LARGO` con 256 constantes previas), disassembler simple/aritmético, marca `|` para línea repetida, `opcode_nombre`.
- **52 tests verde** (19 unit + 33 integración).

### Añadido (Fase 6 sesión 2)
- **Refactor del evaluador** para que la lógica de operadores sea reutilizable desde la VM bytecode sin duplicación:
  - Nueva función pública `evaluador_aplicar_binario(EvalError *err, int op_token, Valor a, Valor b, int linea, int columna)` que toma posesión de `a`/`b` y devuelve un Valor nuevo. Mismo modelo de error que la versión interna pero desacoplado del struct `Evaluador` (toma `EvalError *` directamente).
  - Análoga `evaluador_aplicar_unario(EvalError *err, ...)`.
  - Helpers internos (`entero_op_entero`, `decimal_op_decimal`, `cadena_concatenar`, `cadena_repetir`, `evaluar_comparacion`, `evaluar_en`) cambiados a tomar `EvalError *err` y `int linea, int columna` en lugar de `Evaluador *ev` y `const Expr *e`. El evaluador tree-walking sigue funcionando idénticamente — solo se ha movido el acoplamiento.
  - Wrapper interno `aplicar_binario(Evaluador *ev, ...)` y `aplicar_unario_pos` para que los call-sites del tree-walking (que tienen `Expr *e` a mano) no se vean obligados a desempaquetarlo.
- **`src/vm.{h,c}`**: máquina virtual stack-based estilo clox cap. 15.
  - Pila de 256 slots (capacidad fija por ahora; será dinámica con frames cuando lleguen llamadas en S5).
  - Dispatch loop con `for(;;) switch(*ip++)`. Tracking de línea fuente vía `chunk->lineas[ip - codigo - 1]` para mensajes de error.
  - Implementa: `OP_CONST`/`OP_CONST_LARGO` (con `valor_clonar` del pool), `OP_NULO`/`OP_VERDADERO`/`OP_FALSO`, los 7 operadores aritméticos, las 6 comparaciones, `OP_NEGAR`/`OP_NO`, `OP_DESCARTAR`, `OP_RETORNAR` (extrae el tope y lo devuelve al cliente). Opcodes reservados (saltos, locales, llamadas) emiten error explícito "no implementado en v0.6 sesión 2".
  - Reusa `evaluador_aplicar_binario`/`unario` con un mapeo `OpCode → TipoToken`. Cero duplicación de la aritmética bignum / comparaciones / cadenas.
- **`src/compilador.{h,c}`**: visita el AST y emite bytecode al chunk.
  - Compila `EXPR_LITERAL_*` (entero/decimal/cadena/booleano/nulo), `EXPR_BINARIO`, `EXPR_UNARIO`, `EXPR_GRUPO`.
  - Las constantes se almacenan en el pool del chunk; el chunk es DUEÑO y las destruye al liberarse (incluyendo `mp_int*` y cadenas con dueño).
  - Compilación de cadena literal procesa los escapes mínimos (`\n \t \r \\ \' \"`) igual que el evaluador tree-walking — ambos motores producen el mismo Valor cadena para la misma fuente.
  - Aplazadas con error explícito: `EXPR_IDENT`, `EXPR_LOGICA`, `EXPR_LLAMADA`, `EXPR_LAMBDA`, colecciones, indexación, slicing, f-strings, atributos.
- **`tests/unit/test_bytecode_expr.c`** con 8 grupos de tests end-to-end (`lex → parse → compilar → vm_ejecutar`): literales (cada tipo + escapes), aritmética bignum (precedencia, asociatividad, floor div Python, 2^100 = 31 dígitos), aritmética decimal y mixta (true div siempre decimal, promoción), comparaciones (todas + cross-tipo entero=decimal + lexicográfico de cadenas + tipos incomparables → error), unarios (`-`, `+`, `no`, doble negación), realistas (Pitágoras, promedio decimal, semántica izquierda-a-derecha de Cornamusa sin chained comparisons), errores de runtime (división por cero, tipo incompatible) y errores de compilación (identificadores/lambda/lógica explícitamente no implementados todavía).
- **53 tests verde** (20 unit + 33 integración).

### Añadido (Fase 6 sesión 3)
- **Variables globales en la VM**: la `VM` ahora contiene un `Diccionario *globales` (refcount) que persiste entre llamadas a `vm_ejecutar` (útil para REPL futuro). Inicializado en `vm_iniciar`, liberado en `vm_destruir`.
  - **`OP_DEFINIR_GLOBAL [idx]`**: lee el nombre de `chunk->constantes[idx]`, saca el valor del tope, define o sobrescribe en globales. Cornamusa no distingue declaración de asignación, así que esta es la operación habitual.
  - **`OP_OBTENER_GLOBAL [idx]`**: empuja al stack una copia del valor; si la clave no existe, `ErrorDeNombre: nombre 'X' no esta definido`.
  - **`OP_ASIGNAR_GLOBAL [idx]`**: variante estricta (la clave debe existir); reservada para futuras semánticas más rigurosas, no usada por el compilador en S3.
- **Built-in `imprimir(...)` en bytecode**: nuevo `OP_IMPRIMIR [n]` que saca `n` valores del stack, los imprime separados por espacio + newline, y empuja `nulo` (porque `imprimir(...)` es expresión y `SENT_EXPR` la envuelve con `OP_DESCARTAR`). Soporta hasta 255 argumentos.
- **`compilador_compilar_sent`** soporta:
  - **`SENT_PASAR`**: no-op explícito.
  - **`SENT_EXPR`**: compila la expresión y emite `OP_DESCARTAR`.
  - **`SENT_ASIGNAR`** con destino `EXPR_IDENT`: compila el valor y emite `OP_DEFINIR_GLOBAL` con el nombre como constante. Tuple destructuring, atributos e índices como destino quedan para S6+.
  - **`SENT_BLOQUE`**: compila secuencialmente cada sentencia.
  - Resto de sentencias (`if/while/for/funcion/clase/intentar/lanzar/importar`) producen error explícito con su sesión objetivo.
- **`compilador_compilar_programa(c, sents, n)`**: compila cada sentencia y emite `OP_NULO + OP_RETORNAR` al final, dejando el chunk listo para `vm_ejecutar`.
- **`EXPR_IDENT`** ahora se compila a `OP_OBTENER_GLOBAL` (lookup en globales).
- **`EXPR_LLAMADA` con callee `imprimir`** se detecta como caso especial en el compilador y emite `OP_IMPRIMIR [n]`. Otras llamadas siguen produciendo error "no implementado en bytecode v0.6 sesión 3" — el sistema completo de funciones definidas por el usuario llega en S5.
- **Limitación documentada**: nombres de globales con índice >255 en el pool del chunk dan error explícito ("demasiadas constantes para v0.6 (operando byte)"). Se resolverá con variantes `*_LARGO` cuando sea necesario.
- **`tests/unit/test_bytecode_programa.c`** con 5 grupos: asignación a global (incluye reasignación, varias variables, cambio de tipo libre), error de nombre no definido en runtime, captura de stdout para `imprimir(...)` (con redirección dup/dup2 portable Windows/POSIX), `pasar` y un programa combinado (Pitágoras 3-4-5 imprimiendo "hipotenusa: 5.0").
- **54 tests verde** (21 unit + 33 integración).

### Añadido (Fase 6 sesión 4)
- **`OP_SALTAR`, `OP_SALTAR_SI_FALSO`, `OP_BUCLE`** implementados en la VM con operandos `u16` big-endian. `OP_SALTAR_SI_FALSO` hace **PEEK** (no pop) — el compilador inserta `OP_DESCARTAR` donde toca. Estilo clox cap. 23.
- **Helpers de salto en el compilador**:
  - `emitir_salto(op, linea)`: emite el opcode con placeholder `0xffff`, devuelve el offset para parchear después.
  - `parchear_salto(offset)`: rellena el placeholder con la distancia hasta la posición actual del chunk. Reporta error si excede `UINT16_MAX`.
  - `emitir_bucle(inicio)`: emite `OP_BUCLE` con offset hacia atrás.
- **Stack de bucles abiertos** en el `Compilador` (`BucleAbierto bucles[16]`):
  - `inicio_continuar`: offset al que `continuar` debe saltar (la condición del `mientras`).
  - `parches_romper[]`: array dinámico de offsets de `OP_SALTAR` emitidos por `romper`, parcheados al cerrar el bucle.
  - `empujar_bucle` / `cerrar_bucle` mantienen la pila al entrar/salir.
- **`EXPR_LOGICA` con cortocircuito real**:
  - `a y b`: si `a` es falso, salta sobre `b` dejando `a` en stack; si verdad, descarta `a` y evalúa `b`.
  - `a o b`: si `a` es verdadero, salta sobre `b` dejando `a` en stack; si falso, descarta `a` y evalúa `b`.
  - Verificable porque `verdadero o (1 // 0)` no produce error de división por cero — la rama no se compila a saltar, sino a un OP_SALTAR_SI_FALSO + OP_SALTAR que evita ejecutar el lado derecho.
- **`SENT_SI`** con cadena arbitraria de `si` / `sino si` / `sino`:
  - Cada rama compila `cond → OP_SALTAR_SI_FALSO else → OP_DESCARTAR → cuerpo → OP_SALTAR fin`.
  - La rama final `sino` no tiene condición ni descart.
  - Hasta 64 ramas en una cadena (límite arbitrario, suficiente).
- **`SENT_MIENTRAS`** con cláusula `sino` y `romper`/`continuar`:
  - Loop estándar: `inicio: cond → OP_SALTAR_SI_FALSO salir → OP_DESCARTAR → cuerpo → OP_BUCLE inicio`.
  - `romper` emite `OP_SALTAR` patcheable al fin del bucle (DESPUÉS de la cláusula `sino` para que `romper` salte sobre ella, semántica Python).
  - `continuar` emite `OP_BUCLE` al inicio de la condición.
  - Cláusula `sino` ejecutada solo si terminamos por condición falsa (no por break) — gracias a que el OP_SALTAR_SI_FALSO `salir` apunta antes de `sino` y los `romper` saltan después.
- **`SENT_ASIGNAR_AUG`** (`x op= expr`) compila como `x = x op expr`: emite `OP_OBTENER_GLOBAL` + compilar expr + opcode binario + `OP_DEFINIR_GLOBAL`. Soporta `+= -= *= /= //= %= **=`.
- **Limitación documentada**: `SENT_PARA` queda para S6 (necesita iteración sobre cadena/rango/lista que se conectará con las colecciones en bytecode).
- **`tests/unit/test_bytecode_control.c`** con 8 grupos: lógica con cortocircuito demostrado, `si`/`sino si`/`sino`, `mientras` con romper/continuar/sino, asignación aumentada (todas las variantes), y programas realistas: factorial(25)=26 dígitos, Fibonacci(30)=832040, 2^64 (20 dígitos), anidamiento `mientras` en `mientras`.
- **55 tests verde** (22 unit + 33 integración).

### En desarrollo (Fase 6 — Compilador + VM bytecode)
- ✅ Sesión 1: infraestructura `Chunk` + enum `OpCode` + disassembler.
- ✅ Sesión 2: refactor del evaluador (helpers reutilizables) + compilador para expresiones + VM stack-based con dispatch loop.
- ✅ Sesión 3: variables globales (DEFINIR/OBTENER/ASIGNAR), `imprimir(...)` como built-in en bytecode, sentencias básicas (asignación, expresión, pasar, bloque).
- ✅ Sesión 4: control de flujo en bytecode (`si`/`mientras` con `romper`/`continuar`/`sino`, lógica con cortocircuito, asignación aumentada).
- ✅ Sesión 5: funciones top-level con recursión + variables locales + llamadas en bytecode.
- ✅ Sesión 6: nativas en VM via OP_LLAMAR + colecciones (lista/tupla/dicc/conjunto) en bytecode + indexación + flag `--bytecode` + tag v0.6.0.
- ⏳ Aplazado a v0.6.1: `SENT_PARA`, closures con upvalues, atributos, slicing, lambda, intentar/atrapar.

### Añadido (Fase 6 sesión 5)
- **`VAL_FUNCION_BC`** y **`struct FuncionBC`** en `chunk.{h,c}`: función compilada a bytecode con su propio `Chunk`, nombre (heap-duplicated), aridad y refcount. Comparte refcount con el resto de tipos colección. Diferente de `VAL_FUNCION` (que es para tree-walking) — coexisten para no romper el evaluador antiguo.
- **CallFrame stack en la VM**: refactor de la VM para soportar llamadas anidadas. Cada `CallFrame` contiene chunk activo, ip y `base_pila`. Stack de hasta 64 frames; pila de Valores ampliada a 1024 slots para acomodar varias llamadas. El frame[0] es el del chunk top-level.
- **`OP_LLAMAR [n_args]`** en la VM: lee el callee del slot `tope - n - 1`, valida que es `VAL_FUNCION_BC`, valida la aridad, crea un nuevo `CallFrame` con `base_pila = tope - n - 1`. Slot 0 del frame contiene el callee, slots 1..n los args, slots posteriores las locales.
- **`OP_RETORNAR` multi-frame**: pop el resultado, libera todos los slots del frame que termina (callee + args + locales) limpiamente, push el resultado en el frame anterior, decrementa `n_frames`. Si era el frame top-level, devuelve el resultado al cliente.
- **`OP_OBTENER_LOCAL [slot]`** y **`OP_ASIGNAR_LOCAL [slot]`**: acceso/escritura a `frame->base_pila[slot]`. Locales viven en el stack del frame; al retornar se liberan junto con el frame.
- **`ScopeCompilador`** en el compilador: representa una función en construcción. Mantiene el chunk de la función, la lista de locales (slot 0 = callee, slots 1..aridad = parámetros, posteriores = locales declaradas dinámicamente), y el stack de bucles abiertos para `romper`/`continuar` dentro de la función.
- **Lookup de identificadores con prioridad local → global**: `EXPR_IDENT` busca primero en `c->actual->locales`; si no encuentra, emite `OP_OBTENER_GLOBAL`.
- **Asignación con dispatch local/global**:
  - En el scope raíz (top-level): siempre globales.
  - Dentro de función: primera asignación a un nombre nuevo lo crea como **local** (sin emitir bytecode adicional — el valor ya quedó en el slot del stack); reasignaciones emiten `OP_ASIGNAR_LOCAL`. Mismo modelo para `+=`, `-=`, etc.
- **`SENT_FUNCION`** compilada con scope anidado:
  - `funcion_bc_nueva(nombre, aridad)` con chunk vacío.
  - Scope hijo con slot 0 = callee, slots 1..n = parámetros como locales.
  - Compila el cuerpo en el chunk hijo.
  - Emite `OP_NULO + OP_RETORNAR` implícitos al final (si el cuerpo no terminaba con `retornar`, esto cubre el caso `funcion f(): pasar fin funcion` → devuelve nulo).
  - Vuelve al scope padre y emite `OP_CONST <fn>` + `OP_DEFINIR_GLOBAL <nombre>`.
  - Limitación documentada: parámetros con valor por defecto NO soportados todavía en bytecode (sí en tree-walking) — error explícito.
- **`SENT_RETORNAR`**: compila el valor opcional + `OP_RETORNAR`. Error si está fuera de función.
- **`EXPR_LLAMADA` general**: callee + args + `OP_LLAMAR [n]`. El caso especial `imprimir(...)` sigue emitiendo `OP_IMPRIMIR` directamente (corto-circuitado solo cuando no hay un local llamado `imprimir` que sombrear).
- **Sin closures todavía**: una función definida dentro de otra NO captura las locales de la enclosing — solo accede a sus propias locales y a globales. Las closures con upvalues están planeadas para una sesión adicional o F6 S6.
- **`tests/unit/test_bytecode_funciones.c`** con 8 grupos: función básica con args, recursión (factorial 10/20, fib 10), variables locales (declaración + reasignación + sombrear global con local), aridad mal con mensaje específico, retornar con/sin valor + dentro de bucle + fuera de función, no invocable, locales aisladas (no filtran a globales), y factorial(50) bignum (64 dígitos) recursivo end-to-end por bytecode.
- **56 tests verde** (23 unit + 33 integración).

Cierre de Fase 5: tree-walking interpreter con todas las colecciones
básicas. **Último release con tree-walking activo** según decisión
[B2](decisiones/B2-tree-walking-vs-bytecode.md): desde v0.6 el motor
de producción será la VM bytecode y el tree-walking se congela como
referencia ejecutable de regresión.

### Añadido (Fase 5 sesión 5)
- **Versión `0.5.0`** en `common.h` y `CMakeLists.txt`. Smoke test ajustado.
- **Tres ejemplos jugables nuevos** que ejercitan las nuevas colecciones end-to-end:
  - [`16_lista_busqueda.cor`](examples/16_lista_busqueda.cor): construcción incremental, slicing, inversa con `[::-1]`, `ordenar()`, función auxiliar de búsqueda lineal.
  - [`17_dicc_frecuencia.cor`](examples/17_dicc_frecuencia.cor): conteo de letras en `"abracadabra"` con `dicc[c] += 1`, iteración `para letra en dicc`, `claves()`/`valores()`.
  - [`18_conj_y_tupla.cor`](examples/18_conj_y_tupla.cor): deduplicación con `conjunto(lista)`, mapa de coordenadas con tuplas como claves de diccionario.
- **Tres tests `run_X`** con `PASS_REGULAR_EXPRESSION` para verificar que cada ejemplo produce la salida esperada.
- **51 tests verde** (18 unit + 33 integración).

### Añadido (Fase 5 sesión 1)
- **`VAL_LISTA`** y **`struct Lista`** en `valor.{h,c}`: array dinámico de `Valor` con refcount manual (sin GC todavía — Fase 7). Operaciones: `lista_nueva`, `lista_retener` (++ref), `lista_liberar` (--ref + free si llega a 0), `lista_agregar` (toma posesión, crece ×2 amortizado), `lista_obtener_ref`, `lista_asignar`. Capacidad inicial 4.
- **Semántica de referencia compartida**: `valor_clonar` para `VAL_LISTA` hace `lista_retener` (no deep copy) → asignar `b = a` comparte el mismo objeto Python-style. Las cadenas dentro de la lista siguen su propia ownership (cadena con `dueno_cadena=true` se duplica al clonar, las referencias al fuente no).
- **Limitación documentada**: el refcount no detecta ciclos. Una lista que se contiene a sí misma filtrará memoria; aceptable hasta Fase 7 (mark-sweep real).
- **`valor_a_cadena` para lista**: produce `[a, b, c]` usando una nueva función `valor_a_repr` que envuelve cadenas en comillas (`[1, "hola"]` en lugar de `[1, hola]`). Recursivo para listas anidadas.
- **`valor_iguales` para lista**: comparación element-wise; mismo objeto (puntero) → `true` por short-circuit; longitudes distintas → `false`.
- **`valor_es_verdadero` para lista**: `cuenta > 0`. Lista vacía es falsa, no vacía es verdadera (Python).
- **`EXPR_LISTA` en evaluador**: evalúa cada elemento de izquierda a derecha; si alguno falla, libera lista parcial y propaga error. Trailing comma del parser ya soportada en sintaxis.
- **`EXPR_INDICE` en evaluador**: `lista[i]` con `i` entero o booleano. Soporta índice negativo (cuenta desde el final). Bounds check con `ErrorDeIndice` específico que reporta el índice y el tamaño. Cadenas, diccionarios y otros tipos quedan para sesiones siguientes.
- **`+` de listas**: `[1, 2] + [3, 4]` → `[1, 2, 3, 4]`. Lista NUEVA con refcount 1, deep-clona elementos (cadenas con dueño se duplican; bignum se copia; listas internas comparten refcount).
- **`*` de listas**: `[1, 2] * 3` y `3 * [1, 2]` → `[1, 2, 1, 2, 1, 2]`. Repetición negativa o por cero produce `[]`. Detecta overflow de tamaño.
- **`en` extendido** para listas: `valor en lista` con búsqueda lineal usando `valor_iguales`. Mantiene también `subcadena en cadena`.
- **`para x en lista`**: itera elementos en orden; cada iteración asigna un clon del elemento al objetivo. Soporta `romper`/`continuar` y cláusula `sino` con la misma semántica que sobre cadenas y rangos.
- **`longitud(lista)`** built-in: devuelve `cuenta` como entero. `tipo(lista)` devuelve `"lista"`.
- **`tests/unit/test_runtime_listas.c`** con 8 grupos: literal (vacío, mixto, anidado, trailing comma), indexación (positivo/negativo/fuera-de-rango, no-entero, anidado), operadores (`+`/`*`/`en`/`no en`/`==` con cross-tipo entero=decimal), iteración (suma, concat, romper, sino), built-ins, referencia compartida (asignar `b = a` no rompe), programa promedio decimal, construcción de cuadrados con `rango()` y concat.
- **42 tests verde** (15 unit + 27 integración).

### Añadido (Fase 5 sesión 2)
- **Mutación `lista[i] = valor`**: extendido `SENT_ASIGNAR` para aceptar `EXPR_INDICE` como destino. Soporta índices negativos. `ErrorDeIndice` específico cuando fuera de rango. La asignación destruye el valor previo y toma posesión del nuevo.
- **Mutación aumentada `lista[i] op= valor`**: `SENT_ASIGNAR_AUG` extendido con misma lógica. Lee, computa con `aplicar_binario`, escribe atómicamente. Funciona para todas las variantes (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`).
- **Semántica de referencia confirmada por tests**: `b = a; b[0] = 99` cambia también `a[0]` (Python-like). `agregar(b, 4); longitud(a)` reporta 4.
- **Slicing `lista[a:b:c]`** (`EXPR_REBANADA`) con semántica Python:
  - Cualquier campo opcional (`[:]`, `[a:]`, `[:b]`, `[::c]`, `[a:b:c]`).
  - Defaults dependientes del signo del paso: paso > 0 → `inicio=0`, `fin=cuenta`; paso < 0 → `inicio=cuenta-1`, `fin=-1`.
  - Índices negativos cuentan desde el final.
  - Índices fuera de rango se *clampean* silenciosamente (no error).
  - Paso negativo invierte: `[1,2,3,4,5][::-1] == [5,4,3,2,1]`.
  - Paso 0 produce `ErrorDeValor`.
- **Built-ins de mutación de listas** (estilo función-libre hasta que F8 traiga método-syntax `lista.agregar(x)`):
  - **`agregar(lista, x)`**: añade al final. Devuelve nulo.
  - **`quitar(lista, indice=-1)`**: elimina y devuelve el elemento. Sin índice quita el último. Negativos cuentan desde el final. Lista vacía → `ErrorDeIndice`.
  - **`insertar(lista, indice, valor)`**: inserta antes del índice. Indices fuera de rango se clampean a [0, cuenta] (Python `list.insert`).
  - **`invertir(lista)`**: invierte en sitio (O(n/2) swaps).
  - **`ordenar(lista)`**: ordena in-place con `qsort` libc + comparador propio. Numéricos (entero/decimal/booleano) por valor; cadenas lexicográfico. Tipos mixtos no comparables → error explícito.
- **`tests/unit/test_runtime_listas_mut.c`** con 11 grupos: mutación simple/aug, referencia compartida en mutación, slicing básico (omisiones, negativos, clamping), slicing con paso (positivo/negativo, paso 0 → error), `agregar`, `quitar` (con/sin índice, vacía), `insertar` (con clamping), `invertir`, `ordenar` (numérico/cadenas/mixto/incomparable), y un quicksort recursivo end-to-end como prueba de integración.
- **43 tests verde** (16 unit + 27 integración).

### Añadido (Fase 5 sesión 3)
- **`VAL_DICCIONARIO`** y **`struct Diccionario`** en `valor.{h,c}`: tabla hash con probing lineal, capacidad potencia de 2, factor de carga 0.75, refcount manual (mismo patrón que `Lista`). Funciones: `dicc_nuevo`, `dicc_retener`, `dicc_liberar`, `dicc_asignar` (toma posesión de clave/valor), `dicc_obtener` (devuelve clon), `dicc_contiene`, `dicc_quitar`.
- **Hash genérico de Valores** que cumple la invariante `a == b ⇒ hash(a) == hash(b)`:
  - Booleanos, enteros (que quepan en `int64`) y decimales con valor entero exacto comparten el camino rápido `hash_int64`. Por eso `dicc[1]`, `dicc[1.0]` y `dicc[verdadero]` acceden al mismo slot.
  - Bignums grandes hashean por dígitos + signo. Decimales no enteros por bit pattern del double. Cadenas con FNV-1a 64-bit. Funciones por puntero.
  - `valor_es_hashable` rechaza `lista`, `diccionario`, `rango` como claves.
- **`EXPR_DICCIONARIO` literal** `{clave: valor, ...}`. Diccionario vacío `{}` (resuelto por el parser distinguiéndolo del conjunto).
- **`dicc[clave]` (lectura)**: extiende `EXPR_INDICE`. Clave inexistente produce `ErrorDeClave: <repr>` con la representación del valor que faltaba.
- **`dicc[clave] = valor` (asignación e inserción)**: extiende `SENT_ASIGNAR` con destino `EXPR_INDICE` para diccionarios. Crea la entrada o sobrescribe el valor existente.
- **`dicc[clave] op= valor` (asignación aumentada)**: extiende `SENT_ASIGNAR_AUG` análogamente. La clave debe existir o se reporta `ErrorDeClave`.
- **`clave en dicc` y `clave no en dicc`**: extiende `evaluar_en` con búsqueda hash O(1) amortizado.
- **`para clave en dicc`**: itera las claves del diccionario en orden de slot (no inserción — limitación documentada). Soporta `romper`/`continuar`/cláusula `sino` igual que las otras iteraciones.
- **Igualdad estructural** dos diccionarios son iguales si tienen las mismas claves con valores iguales (orden irrelevante).
- **Built-ins nuevos**:
  - **`claves(dicc)`**: devuelve una lista con las claves.
  - **`valores(dicc)`**: devuelve una lista con los valores.
  - `longitud(dicc)` extendido para devolver `cuenta`.
  - `tipo(dicc)` devuelve `"diccionario"`.
- **Pretty-printer** produce `{"clave": valor, ...}` usando `valor_a_repr` en claves y valores.
- **Limitación documentada**: el orden de iteración (y de `claves`/`valores`) sigue el layout interno del hash table — NO el orden de inserción como Python 3.7+. Aceptable hasta v1.0; se puede cambiar a hash-table ordenada en una versión futura.
- **`tests/unit/test_runtime_diccionarios.c`** con 12 grupos: literal y acceso, asignación/aumentada, membership, iteración, `longitud`/`tipo`, `claves`/`valores`, igualdad estructural (orden distinto), hash unificado entero/decimal/booleano (`dicc[1]` y `dicc[1.0]` mismo slot), tipos no hashables como clave, referencia compartida (`b = a` y mutar `b` afecta `a`), programa de conteo de caracteres en `"abracadabra"` (resultado: `a` aparece 5 veces), diccionario anidado de personas.
- **44 tests verde** (17 unit + 27 integración).

### Añadido (Fase 5 sesión 4)
- **`VAL_CONJUNTO`** y **`struct Conjunto`** en `valor.{h,c}`: hash set construido con la misma estrategia de probing lineal y refcount que `Diccionario`. API: `conj_nuevo`, `conj_retener`, `conj_liberar`, `conj_agregar` (toma posesión, deduplica si ya existe), `conj_contiene`, `conj_quitar`. Sólo elementos hashables (no listas/diccionarios/conjuntos).
- **`VAL_TUPLA`** y **`struct Tupla`** en `valor.{h,c}`: secuencia inmutable con refcount. Sin operaciones de mutación (no hay agregar/asignar). API: `tupla_nueva` (aloca slots no inicializados), `tupla_retener`, `tupla_liberar`. Hashable si todos sus elementos lo son — combinable con `Diccionario` y `Conjunto` como clave.
- **`EXPR_CONJUNTO` literal `{a, b, c}`**: evalúa de izquierda a derecha y deduplica con la igualdad estructural. Vacío explícito requiere `conjunto()` porque `{}` es diccionario vacío.
- **`EXPR_TUPLA` literal**:
  - `()` tupla vacía.
  - `(x,)` tupla de un elemento (coma obligatoria).
  - `(a, b, ...)` tupla múltiple.
  - **Distinción** `(x)` (grupo) vs `(x,)` (tupla 1) ya manejada por el parser desde la sesión 3 sesión 5.
- **Pretty-printer** específico:
  - Conjunto vacío imprime `conjunto()` (no `{}`, que es diccionario).
  - Conjunto no vacío `{a, b, c}`.
  - Tupla `(a, b, c)`, vacía `()`, de uno `(x,)`.
- **`hash_valor` extendido** para tuplas: combina los hashes de cada elemento estilo Python `tuplehash`.
- **`valor_es_hashable` actualizado**: tupla es hashable si todos sus elementos lo son (recursivo); conjuntos NO son hashables.
- **`EXPR_INDICE` para tupla**: `t[i]` con índice positivo/negativo y `ErrorDeIndice` específico.
- **Operador `en`** extendido para conjunto (búsqueda hash O(1)) y tupla (búsqueda lineal).
- **Iteración `para x en conjunto`** y **`para x en tupla`** con la misma semántica que el resto: clon por iteración, soporte de `romper`/`continuar`/cláusula `sino`. El conjunto itera en orden de slot interno (no de inserción — limitación documentada).
- **`longitud(conjunto)`** y **`longitud(tupla)`** funcionan; `tipo()` devuelve `"conjunto"` o `"tupla"`.
- **`conjunto()` built-in** con dos formas:
  - `conjunto()` → conjunto vacío.
  - `conjunto(iterable)` con iterable lista o tupla → conjunto con sus elementos deduplicados.
- **`agregar(conjunto, x)` extendido**: ahora acepta listas Y conjuntos como primer argumento; sobre conjunto deduplica al añadir.
- **Igualdad estructural**: dos conjuntos iguales si tienen los mismos elementos (orden irrelevante); dos tuplas iguales si tienen los mismos elementos en el mismo orden.
- **`tests/unit/test_runtime_conj_tup.c`** con 12 grupos: literal y deduplicación, membership, iteración, igualdad (con hash unificado entero/decimal/booleano), tipos no hashables; tupla literal (vacía/uno/varios) con distinción grupo, indexación, iteración, igualdad cross-tipo (tupla != lista), membership, tupla como clave de dict (con error si contiene lista), `conjunto()` constructor, programa de palabras únicas.
- **45 tests verde** (18 unit + 27 integración).

## [0.4.0] — 2026-04-28 — primer release jugable

Cierre de Fase 4 según el plan: tree-walking interpreter completo y
jugable end-to-end. Decisión [B2](decisiones/B2-tree-walking-vs-bytecode.md):
este release sirve como referencia ejecutable y se congelará en v0.5
tras añadir colecciones; desde v0.6 el motor de producción será la VM
bytecode.

### Añadido (Fase 4 sesión 5)
- **`cornamusa <archivo.cor>`** ahora ejecuta el programa con el evaluador tree-walking en lugar de solo lexarlo. Errores de runtime se reportan con caret indicators reusando `error_imprimir` (formato MENSAJES.md §2). Exit codes: 0 OK, 64 uso, 65 error de parseo, 70 error de runtime, 74 error de E/S.
- **`cornamusa --tokens <archivo>`** (anteriormente el modo por defecto) sigue disponible para inspección del lexer.
- **`cornamusa --ast <archivo>`** sin cambios — vuelca el AST en S-expression.
- **REPL interactivo funcional**: `cornamusa` sin argumentos abre un prompt `>>> ` y `... ` para continuación. Variables y funciones definidas persisten entre líneas. Heurística de continuación: línea acabada en `:` abre bloque, `fin` lo cierra; al volver a profundidad 0 se ejecuta el buffer acumulado. Una línea vacía con buffer ejecuta y reinicia. `salir` o EOF terminan.
- **Persistencia REPL**: arena compartida durante toda la sesión + `strdup` de cada bloque ejecutado para que las claves del entorno y los nodos AST de funciones definidas previamente sigan vivos al ejecutar líneas posteriores. Las cadenas duplicadas se filtran deliberadamente — viven hasta el fin del proceso.
- **Tres ejemplos jugables nuevos**:
  - [`13_factorial_jugable.cor`](examples/13_factorial_jugable.cor): factorial recursivo con bignum (`100!` = 158 dígitos exactos).
  - [`14_contar_vocales.cor`](examples/14_contar_vocales.cor): `para letra en cadena` UTF-8 + acumulador + `o` encadenado + `longitud()`.
  - [`15_fizzbuzz_jugable.cor`](examples/15_fizzbuzz_jugable.cor): FizzBuzz clásico con `rango()`, `si`/`sino si`/`sino`, `%`.
- **Tests de ejecución end-to-end**: nuevos `run_X` con `PASS_REGULAR_EXPRESSION` para verificar que cada ejemplo jugable produce la salida esperada. Cubren los 4 casos representativos (hola_mundo, factorial, contar_vocales, fizzbuzz).
- **Tests `lex_X` ahora usan `--tokens`**: la verificación lexicográfica de los 12 ejemplos sigue intacta como paso de regresión, pero independiente del runtime — un ejemplo puede usar features futuras (closures, listas) y aún así pasar `lex_X`.
- **Versionado**: `CORNAMUSA_VERSION` actualizado a `"0.4.0"` en `common.h` y `CMakeLists.txt`. Smoke test ajustado.
- **41 tests verde** (14 unit + 27 integración: 12 lex + 8 parse + 4 run + 3 examples nuevos).

### Añadido (Fase 4 sesión 4)
- **Tipo `VAL_RANGO`**: nuevo variante en `Valor` con tres `mp_int *` (inicio, fin, paso). Iterable con bignum, ascendente o descendente. `valor_clonar` hace deep copy; `valor_destruir` libera los tres mp_int. `valor_es_verdadero` devuelve `true` si la iteración produciría al menos un elemento. `rango(a, b, paso)` se imprime como `"rango(a, b, paso)"`.
- **Refactor de `VAL_FUNCION` y `VAL_NATIVA`** a estructuras inline (sin allocations heap):
  - `VAL_FUNCION` referencia un `const Sent *def` del AST + un `Entorno *entorno_definicion`. Sin closures (decisión B2): el entorno_definicion siempre es global en S4 — campo reservado para closures futuros en Fase 6+.
  - `VAL_NATIVA` contiene nombre + puntero `FnNativa` (typedef en `valor.h`).
  - Ambos son trivialmente clonables (struct copy), no requieren ownership tracking, y `valor_iguales` compara por referencia subyacente.
- **`src/nativos.{h,c}`** con la API `nativos_registrar(globales)` que añade los built-ins al entorno:
  - **`imprimir(*args)`**: variádica. Imprime cada argumento separado por espacio + `\n`. Sin kwargs (`separador`, `final`) — se añadirán cuando lleguen kwargs al lenguaje. Devuelve nulo.
  - **`longitud(x)`**: cadena → número de **code points UTF-8** (no bytes); rango → número de elementos producidos. Otros tipos producen `ErrorDeTipo`.
  - **`tipo(x)`**: devuelve cadena con el nombre del tipo en castellano (`"entero"`, `"decimal"`, `"cadena"`, `"booleano"`, `"nulo"`, `"funcion"`, `"rango"`).
  - **`rango([inicio,] fin [, paso])`**: tres formas. Acepta solo enteros (booleano se promueve a 1/0). `paso == 0` produce `ErrorDeValor`. Bignum-friendly: `rango(0, 10**100, 1)` es válido (aunque su iteración tarde una eternidad).
- **`SENT_FUNCION` en evaluador**: crea un `VAL_FUNCION` y lo asigna en el entorno actual. La función puede llamarse a sí misma porque el nombre está definido antes de cualquier llamada.
- **`SENT_RETORNAR`**: evalúa la expresión opcional (`retornar` desnudo → nulo), guarda el valor en `ev->valor_retorno` y marca `ev->control = EJEC_RETORNAR`. El bucle envolvente o `llamar_usuario` la consume.
- **`EXPR_LLAMADA` en evaluador**: evalúa el callee, evalúa cada argumento, despacha:
  - `VAL_NATIVA`: invoca el puntero a función C con los args ya evaluados (ownership del cliente).
  - `VAL_FUNCION`: crea un nuevo `Entorno` hijo del entorno_definicion, liga parámetros (con valores por defecto si faltan), ejecuta el cuerpo, recoge `ev->valor_retorno` si apareció `EJEC_RETORNAR`, restaura entorno y control. Aridad validada con mensaje específico ("`f()` esperaba N argumentos, recibió M").
- **Recursión funcional**: factorial(50)=64 dígitos y factorial(100)=158 dígitos pasan tests recursivos sin stack-smashing (depth ≈ 100 frames).
- **`para` ahora itera también `VAL_RANGO`**: usa `mp_add` para avanzar y `mp_cmp` para terminar. Soporta paso ascendente y descendente. Combinable con `romper`/`continuar`/cláusula `sino` igual que con cadenas.
- **`Evaluador` ahora es `typedef struct Evaluador { ... }`** (con nombre explícito) para permitir forward declaration desde `valor.h` en la firma de `FnNativa`.
- **`tests/unit/test_runtime_funciones.c`** con 11 grupos de tests: definición y llamada simple, recursión (factorial 10/50, fib 15), parámetros con defaults, aridad mal con mensaje específico, no invocable, `retornar` con/sin valor y dentro de bucle, `tipo()` para cada tipo, `longitud()` UTF-8 + rangos, `rango()` 1/2/3 args con paso negativo y cero iteraciones, `imprimir()` no rompe, programa de pares con función auxiliar, factorial(100) recursivo (158 dígitos).
- **34 tests verde** (14 unit + 20 integración).

### Añadido (Fase 4 sesión 3)
- **Evaluador de sentencias** en `evaluador.{h,c}`: nueva API `evaluador_ejecutar_sent` y `evaluador_ejecutar_programa`. Modelo de control de flujo sin `setjmp`: nuevo enum `ControlFlujo` (`EJEC_NORMAL`, `EJEC_ROMPER`, `EJEC_CONTINUAR`, `EJEC_RETORNAR`). Las construcciones envolventes (bucles, llamadas) inspeccionan y resetean `ev->control`.
- **`SENT_ASIGNAR`**: solo destino `EXPR_IDENT` en v0.4 (tuple destructuring, atributos e índices como destino quedan para v0.3.1+/F5). La asignación crea o sobrescribe en el entorno actual con `entorno_definir`. Sin tipos: la misma variable puede pasar de entero a cadena a decimal.
- **`SENT_ASIGNAR_AUG`** (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`): obtiene el valor actual (clon) del entorno, evalúa el operando derecho, aplica el operador binario equivalente y reasigna. La variable debe estar previamente definida (semántica Python: `ErrorDeNombre` si no existe). `x /= 2` produce decimal aunque `x` sea entero.
- **Refactor de `eval_binario`**: extraída `aplicar_binario(ev, op, a, b, e)` que toma posesión de dos valores ya evaluados. Reutilizada por `SENT_ASIGNAR_AUG` para no duplicar la lógica.
- **`SENT_PASAR`**: no-op explícito.
- **`SENT_ROMPER` / `SENT_CONTINUAR`**: marcan `ev->control` y dejan que el bucle envolvente lo gestione. Si `evaluador_ejecutar_programa` detecta control de flujo no consumido al volver al top-level, produce error explícito ("control de flujo fuera de su contexto").
- **`SENT_SI`**: itera sobre la cadena de `RamaSi` (`si` + `sino si`* + `sino`?) y ejecuta la primera rama cuya condición sea verdadera; la rama final `sino` tiene `condicion=NULL` y siempre se toma si se llega.
- **`SENT_MIENTRAS`**: bucle clásico con `romper`/`continuar`. Cláusula `sino` con semántica Python: se ejecuta sólo si el bucle terminó por condición falsa, NO si se rompió.
- **`SENT_PARA`** sobre cadenas: itera **code points UTF-8** (no bytes), de modo que `"niño"` produce 4 iteraciones (`'n'`, `'i'`, `'ñ'`, `'o'`). Cada iteración crea un nuevo `Valor` cadena de 1 code point y lo asigna al objetivo. `romper`, `continuar` y cláusula `sino` con la misma semántica que `mientras`. Otros iterables (rango, lista, diccionario) llegarán en S4/F5. Iterable no soportado produce `ErrorDeTipo` específico.
- **`SENT_BLOQUE`**: secuencia de sentencias; para al primer error o cuando aparece control de flujo no normal (que el bloque envolvente recogerá).
- **Aplazadas con error explícito**: `SENT_FUNCION`/`SENT_RETORNAR` (S4), `SENT_CLASE`/`SENT_INTENTAR`/`SENT_LANZAR`/`SENT_IMPORTAR`/`SENT_DESDE_IMPORTAR`/`SENT_GLOBAL`/`SENT_NOLOCAL` (F5+).
- **`tests/unit/test_runtime_sentencias.c`** con 12 grupos de tests sobre programas completos parseados y ejecutados:
  - Asignación simple, múltiples variables, cambio de tipo libre.
  - Asignación aumentada (todas las variantes incluyendo concatenación de cadenas con `+=` y true-div con `/=`).
  - `si`/`sino si`/`sino` en cascada y one-liner.
  - `mientras` clásico (suma 1..10), `romper`, `continuar` (suma de pares), cláusula `sino` ejecutada y NO ejecutada.
  - `para` sobre cadena ASCII y UTF-8 (`"niño"` → 4 iteraciones), concatenación durante iteración, `romper`, cláusula `sino`, error con iterable entero.
  - Programas realistas: factorial(25) con bignum (26 dígitos), conteo de vocales en `"murcielago"`, Fibonacci(30) iterativo, 2^64 (20 dígitos).
  - Anidamiento: `si` en `mientras`, `mientras` en `para`.
- **33 tests verde** (13 unit + 20 integración).

### Añadido (Fase 4 sesión 2)
- **`src/evaluador.{h,c}`** — evaluador tree-walking de expresiones. Modelo de errores sin `setjmp`: cada función devuelve `Valor` y rellena `Evaluador.error` (con línea, columna y mensaje) en caso de fallo. El cliente comprueba `evaluador_tiene_error` tras cada evaluación.
- **Literales**: `EXPR_LITERAL_ENTERO` parsea decimal/hex/oct/bin con `_` separadores; `EXPR_LITERAL_DECIMAL` con notación científica; `EXPR_LITERAL_CADENA` quita comillas y procesa escapes mínimos (`\n \t \r \\ \' \"`); `EXPR_LITERAL_BOOLEANO`, `EXPR_LITERAL_NULO`. `EXPR_LITERAL_F_CADENA` produce error explícito (interpolación llega en F4 S5 + parser de sub-expresiones).
- **Identificadores**: lookup en el entorno actual con scope chain por punteros a padre. Si el nombre no existe, error `ErrorDeNombre: nombre 'X' no esta definido`.
- **Aritmética entero⊕entero** vía libtommath: `+`, `-`, `*`, `//` (floor division estilo Python para negativos), `%` (módulo matemático con resultado siempre del signo del divisor), `**` (potencia con exponente que cabe en `int`; exponente negativo promociona a decimal `pow()`). Sin overflow: `2 ** 100` da el bignum exacto de 31 dígitos, `10 ** 100` el gugol completo.
- **True division `/`**: siempre produce `VAL_DECIMAL` (estilo Python 3), incluso para enteros divisibles (`6 / 2` → `3.0`).
- **Promoción mixta entero/decimal**: cualquier operación con un decimal convierte el otro operando a doble. `1 + 2.5` → `3.5`. Para enteros muy grandes la conversión a doble pierde precisión, conducta documentada y consistente con Python.
- **Aritmética decimal⊕decimal** con `pow()`, `floor()` y módulo Python (`a - floor(a/b)*b` — resultado del signo del divisor: `-7.5 % 3.0 == 1.5`).
- **Bitwise**: `&`, `|`, `^` vía `mp_and`/`mp_or`/`mp_xor`. `<<` (`mp_mul_2d`) y `>>` (`mp_div_2d` con ajuste a floor para negativos). `~` (complemento a uno) vía `mp_complement`. Booleanos se promueven a entero (1/0). Errores específicos para desplazamiento negativo o demasiado grande.
- **Comparaciones**: `==`, `!=`, `<`, `<=`, `>`, `>=`. Función `comparar_valores` con `Orden` (LT/EQ/GT/INCOMP). `==` y `!=` permiten tipos distintos (devuelven `false`); `<` etc. dan `ErrorDeTipo` si los tipos no son comparables. Cross-tipo numérico: entero/decimal/booleano se comparan matemáticamente. Cadenas: lexicográfico byte a byte (UTF-8 preservado).
- **`valor_iguales` extendido**: ahora trata `verdadero == 1`, `falso == 0`, `verdadero == 1.0` como verdadero (Python: bool es subclase de int).
- **Lógica con cortocircuito**: `y` y `o` evalúan el operando derecho solo si el izquierdo no decide. Devuelven el **valor decisor original** (no booleano), igual que Python: `0 o 42` → `42`, `1 y "x"` → `"x"`. El test `verdadero o (1 // 0)` pasa porque la división por cero nunca se evalúa.
- **Unarios**: `-x` (negación numérica con `mp_neg`), `+x` (identidad), `no x` (negación lógica usando `valor_es_verdadero`), `~x` (complemento a uno).
- **Cadenas**: `+` concatena (nuevo buffer en heap, `dueno_cadena=true`), `*` con entero repite (con detección de overflow del tamaño total), comparaciones lexicográficas, `subcadena en cadena` mediante búsqueda lineal.
- **`es` (identidad)**: para funciones/nativas compara puntero. Para inmutables (entero, decimal, cadena, booleano, nulo) coincide con `valor_iguales` por ahora — se refinará cuando lleguen instancias y objetos heap.
- **`en` (membership)**: solo soportado para `subcadena en cadena` en esta sesión. Listas/diccionarios llegan en F5.
- **Aplazadas a sesiones siguientes** (devuelven error explícito): `EXPR_LLAMADA`, `EXPR_ATRIBUTO`, `EXPR_LAMBDA`, colecciones (`EXPR_LISTA`, `EXPR_DICCIONARIO`, `EXPR_CONJUNTO`, `EXPR_TUPLA`), `EXPR_INDICE`, `EXPR_REBANADA`, f-string con interpolación parseada.
- **`tests/unit/test_runtime_evaluador.c`** con ~70 verificaciones agrupadas en 14 grupos: literales (cada base, escapes), aritmética entera (precedencia, asociatividad, bignum 31 dígitos), división y mixto, decimales, comparaciones (mismo tipo y cross-tipo), bitwise (incluido `~`), unarios (incluida doble negación), lógica con cortocircuito demostrado, cadenas (concat/repetición/membership), identidad (`es`, `no es`, `es no`), identificadores con entorno definido, errores (división por cero, nombre, tipo), y combinaciones realistas (gugol, promedio, condiciones encadenadas).
- **32 tests verde** (12 unit + 20 integración).

### Añadido (Fase 4 sesión 1)
- **Vendoreado [libtommath 1.3.0](https://github.com/libtom/libtommath)** en `vendor/libtommath/` (~150 archivos `.c`, Public Domain). Bignum desde día 1 según decisión [B3](decisiones/B3-representacion-numerica.md). Compilado como librería estática separada en CMake.
- **`src/valor.{h,c}`** — tipo `Valor` con tagged union de 7 variantes:
  - `VAL_NULO`, `VAL_BOOLEANO`, `VAL_DECIMAL` (IEEE 754 double).
  - `VAL_ENTERO` con `mp_int *` boxed (precisión arbitraria; `factorial(100)` produce número de 158 dígitos sin overflow).
  - `VAL_CADENA` con bandera `dueno_cadena` (referencia al buffer fuente vs heap).
  - `VAL_FUNCION`, `VAL_NATIVA` (preparados para sesión 4).
- **Constructores**: `valor_nulo()`, `valor_booleano()`, `valor_decimal()`, `valor_decimal_de_lexema()`, `valor_entero_de_long()`, `valor_entero_de_lexema()` (acepta decimal, hex `0xff`, octal `0o755`, binario `0b1010`, con `_` separadores), `valor_cadena_referencia()`, `valor_cadena_duplicar()`.
- **Operaciones**: `valor_destruir`, `valor_clonar` (deep), `valor_imprimir`, `valor_a_cadena`, `valor_nombre_tipo`, `valor_es_verdadero` (truthiness ESPEC §6.2), `valor_iguales` (igualdad ESPEC §6.3 incluyendo `1 == 1.0`).
- **`src/entorno.{h,c}`** — `Entorno` (scope chain) con tabla hash de probing lineal:
  - Hash FNV-1a 32-bit, factor de carga 0.75, redimensionamiento dinámico.
  - API: `entorno_iniciar`, `entorno_destruir`, `entorno_definir`, `entorno_obtener` (devuelve clon), `entorno_asignar` (mutación), `entorno_existe`.
  - Scope chain por puntero a `padre`: una variable se busca aquí y, si no, en los entornos enclosing.
  - El entorno es **dueño** de los Valores; al destruirse libera todos sus mp_int y cadenas con dueño.
- **Sin GC** en Fase 4 (decisión B2 + B3): liberación eager. Cuando un entorno se destruye, todos los valores locales se liberan. En Fase 7 se añade GC mark-sweep.
- **`tests/unit/test_runtime_valor.c`** con ~25 tests: construcción de cada tipo, bignum (factorial 100 = 158 dígitos), verdadez, igualdad (incluyendo `1 == 1.0`), clonación, operaciones de entorno (definir, obtener, asignar, scope chain con padre, shadowing, redimensionamiento al añadir 100 variables).
- **31 tests verde** (11 unit + 20 integración).

### En desarrollo (Fase 3 — Parser y AST, objetivo v0.3.0)
- ✅ Sesión 1: AST + arena allocator + Pratt parser para expresiones.
- ✅ Sesión 2: sentencias simples + control de flujo + validación `fin <etiqueta>`.
- ✅ Sesión 3: funciones, clases, lambda.
- ✅ Sesión 4: excepciones, módulos, global/nolocal.
- ✅ Sesión 5: literales de colección, indexación, slicing, operadores de identidad/membership, `--ast` flag, tests de integración del parser, tag v0.3.0.

### Añadido (Fase 3 sesión 5)
- **Literales de colección** (`EXPR_LISTA`, `EXPR_DICCIONARIO`, `EXPR_CONJUNTO`, `EXPR_TUPLA`) con todas las variantes:
  - `[1, 2, 3]` lista; `[]` lista vacía; trailing comma permitida.
  - `{"k": "v"}` diccionario; `{}` diccionario vacío.
  - `{1, 2, 3}` conjunto.
  - `()` tupla vacía; `(x,)` tupla de 1; `(a, b)` tupla de 2+.
  - **Distinción tupla vs grupo**: `(x)` es grupo, `(x,)` es tupla.
- **Indexación** (`EXPR_INDICE`): `lista[0]`, `dicc[clave]`, `obj.attr[i]`, encadenamientos `matriz[i][j]`.
- **Slicing** (`EXPR_REBANADA`): `lista[a:b]`, `lista[a:b:c]`, con omisiones (`[:b]`, `[a:]`, `[:]`, `[::c]`).
- **Operadores de identidad y membership** (ESPEC §5 `op_comp`):
  - `a es b` → identidad.
  - `a es no b` → identidad negada (forma ESPEC).
  - `a no es b` → identidad negada (forma natural castellana).
  - `a en b` → pertenencia.
  - `a no en b` → pertenencia negada.
  - Las formas con `no` se desazucaran a `(uop "no" (op "es" / "en" izq der))`.
- **Flag `--ast`** en `cornamusa` que vuelca el AST del programa en formato S-expression. Ejemplo: `cornamusa --ast programa.cor`.
- **`tests/unit/test_parser_colecciones.c`** con ~30 tests cubriendo cada forma de literal, indexación, slicing, distinción tupla/grupo, anidamiento.
- **Tests de integración del parser**: 8 ejemplos parsean correctamente con `--ast`:
  - ✅ 01_hola_mundo, 02_fizzbuzz, 04_factorial, 07_clases_herencia, 08_excepciones, 09_closures, 11_iterador, 12_modulos.
- **30 tests verde** (10 unit + 12 integración del lexer + 8 integración del parser).

### Aplazado a v0.3.1 (parsean en sesiones futuras de Fase 3)
- **Multi-target assignment** (`a, b = b, a + b`) — usado en 03_fibonacci.
- **Iteración con tuple destructuring** (`para palabra, conteo en pares.elementos():`) — usado en 06_diccionarios.
- **List comprehensions** (`[x*x para x en y si cond]`) — usadas en 05_listas y 10_quicksort.
- **f-strings con interpolación parseada** (actualmente `EXPR_LITERAL_F_CADENA` almacena el lexema completo; las expresiones `{...}` no se parsean como sub-AST todavía).

### Añadido (Fase 3 sesión 4)
- **AST de excepciones, módulos y declaraciones**:
  - `SENT_INTENTAR`: cuerpo + lista de cláusulas `atrapar` + `sino` opcional + `finalmente` opcional.
  - `SENT_LANZAR`: expresión opcional (NULL = re-raise).
  - `SENT_IMPORTAR`: ruta dotted + alias opcional.
  - `SENT_DESDE_IMPORTAR`: ruta + items con aliases opcionales (o `*`).
  - `SENT_GLOBAL` / `SENT_NOLOCAL`: lista de nombres.
- **Tipos auxiliares**: `Nombre` (puntero+longitud al lexema), `ItemImportado` (nombre + alias opcional), `ClausulaAtrapar` (tipo + alias + cuerpo).
- **Parser de excepciones**:
  - `intentar:` con cero o más `atrapar [TipoExc [como alias]]:`, opcional `sino:` (rama sin excepción), opcional `finalmente:`, cerrado con `fin intentar`.
  - Validación: `intentar` requiere al menos un `atrapar` O `finalmente`. Error específico si ambos faltan.
  - `atrapar`/`finalmente` ahora son terminadores válidos de bloque (extendido `en_inicio_de_termino`).
- **Parser de `lanzar`**: `lanzar expr` con expresión, o `lanzar` desnudo en la misma línea de un atrapar como re-raise. Heurística para detectar bare lanzar: nuevo line o token de cierre tras el keyword.
- **Parser de imports**:
  - Helper `parsear_ruta_modulo` consume `IDENT ('.' IDENT)*`.
  - `importar X.Y.Z [como W]`.
  - `desde X.Y importar A [como A2], B, C` o `desde X importar *`.
- **Parser de `global`/`nolocal`**: lista de identificadores separados por coma.
- **`tests/unit/test_parser_excepciones_modulos.c`** con ~22 tests cubriendo: cada forma de `intentar`/`atrapar`/`finalmente`/`sino`, `lanzar` con valor y bare, imports simples/dotted/con-alias, `desde X importar Y` con uno/varios items/alias/`*`, `global` y `nolocal` con uno/varios nombres, anidamiento realista (función con `intentar` dentro como en `examples/08_excepciones.cor`, closure con `nolocal` como en `examples/09_closures.cor`), y errores específicos.
- **21 tests verde** (9 unit + 12 integración).

### Añadido (Fase 3 sesión 3)
- **`SENT_FUNCION`** en AST: nombre, parámetros, anotación de retorno opcional, cuerpo.
- **`SENT_CLASE`** en AST: nombre, lista de superclases (`extiende A, B, C`), cuerpo.
- **`EXPR_LAMBDA`** en AST: parámetros + cuerpo (una sola expresión, no bloque).
- **`Parametro`** struct: nombre + anotación de tipo opcional + valor por defecto opcional.
- **Parser de funciones**:
  - `funcion nombre(p1, p2, ...) [-> tipo]:`
  - Parámetros con anotación de tipo (`n: entero`) y valor por defecto (`idioma="es"`) en cualquier combinación.
  - Anotación de retorno con `-> tipo`.
  - Cuerpo: bloque multilínea cerrado con `fin funcion`, o one-liner.
- **Parser de clases**:
  - `clase Nombre [extiende A, B, ...]:`
  - Multi-herencia sintácticamente aceptada (semántica MRO en runtime).
  - Cuerpo cerrado con `fin clase`. Métodos son sentencias `funcion` dentro.
- **Parser de lambda**:
  - `lambda x, y, n=10: x + y + n`
  - Parámetros sin paréntesis. Defaults permitidos. **Anotaciones de tipo NO permitidas** en lambda (el `:` siempre es terminador).
  - Cuerpo es una sola expresión.
- **Pretty-printer extendido**: `(funcion nombre (param x) (param y (defecto ...)) (retorno ...) (bloque ...))`, `(clase Nombre (extiende ...) (bloque ...))`, `(lambda (param x) <expr-cuerpo>)`.
- **Validación de etiquetas extendida**: `fin funcion` y `fin clase` ahora se validan correctamente. `fin si` cerrando una función produce mensaje específico.
- **`tests/unit/test_parser_funciones.c`** con ~20 tests cubriendo:
  - Funciones con 0/1/varios parámetros, anotaciones de tipo, defaults, anotación de retorno, one-liner.
  - Clases vacías, con métodos, con herencia simple y múltiple, ejemplo realista del `examples/07_clases_herencia.cor`.
  - Lambdas vacías, con uno/varios parámetros, con defaults, anidadas en llamadas (`mapear(lambda x: x*2, lista)`).
  - Validación: `fin funcion` no cierra `si` (y viceversa).
  - Errores: función sin nombre, sin `(`, clase sin nombre, lambda sin cuerpo.
  - Anidamiento realista: función con `si` dentro (patrón fibonacci).
- **Limitación documentada**: las palabras `y`, `o`, `no`, `en`, `es` (operadores lógicos/comparativos como palabra) **son keywords y no se pueden usar como identificadores**. Tests usan nombres alternativos (`z`, `n`).
- **20 tests verde** (8 unit + 12 integración).

### Añadido (Fase 3 sesión 2)
- **AST de sentencias** en `ast.{h,c}`: 11 variantes (`SENT_EXPR`, `SENT_ASIGNAR`, `SENT_ASIGNAR_AUG`, `SENT_PASAR`, `SENT_ROMPER`, `SENT_CONTINUAR`, `SENT_RETORNAR`, `SENT_SI` con cadena de `RamaSi`, `SENT_MIENTRAS`, `SENT_PARA`, `SENT_BLOQUE`). Pretty-printer en S-expression.
- **Parser de sentencias**: `parser_parsear_sentencia` y `parser_parsear_programa`. Maneja:
  - Sentencias simples: `pasar`, `romper`, `continuar`, `retornar [expr]`.
  - **Asignación simple** (`x = expr`) y **aumentada** (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`).
  - **Sentencia-expresión** (cualquier expresión usada como sentencia: `imprimir(x)`).
  - **Bloques `si`/`sino si`/`sino`** con cadena completa de ramas, cerrado con `fin si`.
  - **`mientras`/`fin mientras`** con cláusula `sino` opcional.
  - **`para X en Y:`/`fin para`** con cláusula `sino` opcional.
- **Detección de one-liners**: si tras `:` el siguiente token está en la misma línea, se parsea una sola sentencia sin requerir `fin <X>`. Si va a línea siguiente, se exige bloque multilínea cerrado con `fin <etiqueta>`.
- **Validación de `fin <etiqueta>`** mediante stack de bloques abiertos en el parser (`pila_bloques[64]`):
  - `fin si` solo cierra `si`. `fin para` solo cierra `para`. Etc.
  - Mensaje específico cuando la etiqueta no coincide:
    *"se esperaba 'fin si' (bloque abierto en línea 9), encontrado 'fin para'"*.
  - Mensaje específico cuando falta el `fin`:
    *"se esperaba 'fin si' para cerrar el bloque abierto en línea 9"*.
- **Recuperación de errores** con panic mode: tras un error, el parser sale del modo pánico al inicio de cada sentencia para poder reportar varios errores en un programa.
- **Anidamiento arbitrario**: `si` dentro de `para` dentro de `mientras` funciona; cada bloque tiene su propia entrada en el stack.
- **`tests/unit/test_parser_sentencias.c`** con ~30 tests cubriendo: cada sentencia simple, asignaciones, todas las variantes de `si`/`mientras`/`para` (con/sin `sino`, one-liner vs multilínea), anidamiento, validación de etiquetas (`fin para` cerrando un `si` da error, etc.), errores de sintaxis (`fin` desnudo, falta `:`, falta `fin`), y un programa completo de varias sentencias.
- **19 tests verde** (7 unit + 12 integración del lexer).

### Añadido (Fase 3 sesión 1)
- **`src/arena.{h,c}`** — arena allocator con bloques crecientes (~80 líneas). Aloca alineado a 8 bytes, libera todo en una sola llamada con `arena_destruir`. Patrón estándar para ASTs (lo usan V8, GCC, LLVM).
- **`src/ast.{h,c}`** — AST tipado con tagged union. Esta sesión define **expresiones** con 13 variantes:
  - Literales: `EXPR_LITERAL_ENTERO`, `EXPR_LITERAL_DECIMAL`, `EXPR_LITERAL_CADENA`, `EXPR_LITERAL_F_CADENA`, `EXPR_LITERAL_BOOLEANO`, `EXPR_LITERAL_NULO`.
  - `EXPR_IDENT`, `EXPR_BINARIO`, `EXPR_UNARIO`, `EXPR_LOGICA` (`y`/`o`).
  - `EXPR_LLAMADA`, `EXPR_ATRIBUTO`, `EXPR_GRUPO`.
  - Pretty-printer en formato S-expression (`(op "+" (lit-int 1) (lit-int 2))`) para tests y depuración.
- **`src/parser.{h,c}`** — Parser estilo **Pratt** con tabla de reglas (prefijo, infijo, precedencia). Maneja:
  - **14 niveles de precedencia** desde `o` (más bajo) hasta llamada/atributo (más alto).
  - **Asociatividad correcta**: izquierda para `+ - * / // % == != < > <= >= y o & | ^ << >>`, derecha para `**`.
  - **Llamadas con argumentos** (0 o más, separados por coma).
  - **Acceso a atributo encadenado** (`a.b.c`).
  - **Operadores unarios**: `-x`, `+x`, `no x`, `~x`.
  - **Recuperación de errores** con panic mode + flag `tuvo_error`.
  - **Mensajes de error con caret** reusando `error_imprimir_token` de Fase 2.
- **`tests/unit/test_parser_expresiones.c`** — 35+ tests cubriendo: literales (cada tipo), identificadores, operadores con precedencia y asociatividad correctas (`1 + 2 * 3` → `1 + (2*3)`; `2 ** 3 ** 4` → `2 ** (3**4)`), unarios anidados, lógicas (`y`/`o` con precedencia entre ellos y vs `no`), agrupación, llamadas con varios args y anidadas, atributos encadenados, métodos (`obj.metodo(arg)`), combinaciones realistas extraídas de ejemplos (`tipo(yo).__nombre__`, `n * factorial(n - 1)`, `x > 0 y x < 100`), y errores (paréntesis sin cerrar, atributo sin nombre, operador sin operando).
- Build verde con flags estrictos. **18 tests verde** (6 unit + 12 integración del lexer).

### En desarrollo (Fase 2 — Lexer, objetivo v0.2.0)
- ✅ Sesión 1: esqueleto del lexer + tokens simples (símbolos, operadores, comentarios).
- ✅ Sesión 2: literales numéricos y cadenas básicas.
- ✅ Sesión 3: identificadores Unicode + NFC + tabla de keywords.
- ✅ Sesión 4: f-strings y triple-quoted strings.
- ✅ Sesión 5: mensajes de error pulidos siguiendo MENSAJES.md + tests exhaustivos.

### Añadido (Fase 2 sesión 5)
- **Refactor `Token`**: nuevo campo `mensaje` (NULL para tokens normales, contiene el mensaje de error para `TT_ERROR`). El campo `inicio`/`longitud` ahora describe siempre el span en la fuente — para errores, el fragmento problemático que producirá el caret indicator. Esto permite mensajes de error con calidad de Rust/Python 3.10.
- **`struct Token` con nombre** (en lugar de typedef anónimo) para permitir forward declarations entre módulos.
- **`error_imprimir_token`** en `errores.{h,c}`: formatea un token de error siguiendo MENSAJES.md §2 con anatomía completa:
  ```
  ErrorDeSintaxis en archivo.cor:3:18
          retornar 1__2
                   ^^
  no se permiten guiones bajos consecutivos en literales numéricos
  ```
  Carets dibujados a partir de `columna` y `longitud` del token. La línea de fuente se localiza en el buffer original sin copiar.
- **`error_imprimir`** extendida para aceptar `fuente` y `longitud_span` opcionales. Si se proporcionan, dibuja el contexto de línea + carets.
- **`main.c` reescrita**: pipeline completo `archivo → fuente_cargar_archivo (NFC) → Lexer → tokens`. Reporta errores léxicos con `error_imprimir_token`. Nuevo flag `--tokens` que vuelca todos los tokens en formato debug `LINEA:COL TIPO "lexema"`.
- **Tests de integración**: 12 tests CTest (uno por ejemplo en `examples/`) que invocan `cornamusa <archivo.cor>` y verifican exit code 0 (sin errores léxicos). Etiquetados con label `integracion` en CTest.
- Tests unitarios actualizados: `t.inicio` → `t.mensaje` en las verificaciones de mensajes de error (4 archivos, ~15 ocurrencias).
- Verificado manualmente: los 12 ejemplos en `examples/` lexán sin error. El error de muestra (`1__2` en código) produce el caret indicator correcto bajo el span ofensivo.

**Total tests al cerrar Fase 2:** 17 (5 unit + 12 integración), 100% verde con build Release y -O3.

### Añadido (Fase 2 sesión 4)
- Lexer reconoce **f-strings** (`TT_F_CADENA`):
  - Prefijo `f` o `F` inmediatamente seguido de comilla simple o doble.
  - Interpolación `{expresión}` con tracking de profundidad de llaves balanceadas.
  - `{{` y `}}` son llaves literales (no abren ni cierran interpolación).
  - El lexema completo (incluyendo `f` y comillas) se almacena en el token; el parser/AST hará el mini-parse de cada interpolación cuando llegue Fase 3.
- Lexer reconoce **cadenas triple-quoted** (`"""..."""` y `'''...'''`):
  - Multilínea: el contador de líneas avanza correctamente al ver `\n` interno.
  - Comillas dobles o simples sueltas dentro no cierran la triple (solo tres consecutivas idénticas a la apertura).
  - Compatible con prefijo `f`: `f"""..."""` y `f'''...'''` se reconocen como `TT_F_CADENA`.
- Refactor interno: `escanear_cadena` es ahora dispatcher entre `escanear_cadena_simple` y `escanear_cadena_triple`. Helpers `procesar_escape` y `saltar_interpolacion` factorizan la lógica de escapes y brace tracking. Firma `bool` para señalar errores limpiamente.
- Errores nuevos:
  - `f"hola {sin cerrar` → "interpolación de f-cadena sin cerrar antes del fin de archivo".
  - `f"hola {x\ny}"` (newline dentro de interp en f-string simple) → mensaje específico.
  - `f"hola }"` → "'}' inesperado en f-cadena (usa '}}' para llave literal)".
  - `"""sin cerrar` → "cadena triple sin cerrar antes del fin de archivo".
- `tests/unit/test_lexer_f_cadenas.c` añadido con 36 tests cubriendo: f-strings sin/con interpolación, mayúsculas (`F`), comillas simples, llaves literales, triple-quoted con conteo de líneas correcto, combinación f+triple, escapes, errores específicos, distinción `f"..."` vs `f` + `"..."` (ident + cadena), lexemas y secuencias realistas inspiradas en `examples/03_fibonacci.cor` y `06_diccionarios.cor`.
- `tests/unit/test_lexer_literales.c` renombrado a `tests/unit/test_lexer_numeros_cadenas.c` por consistencia (el nombre describe mejor el contenido).
- 5/5 tests verde con build Release optimizado: smoke + simbolos + numeros_cadenas + identificadores + f_cadenas.

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

