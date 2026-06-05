# Registro de cambios

Todos los cambios notables a este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto adhiere a [Versionado Semántico](https://semver.org/lang/es/).

## [No publicado]

## [1.134.0] — 2026-06-05 — Destructuring en el bucle `para`

Limitación documentada desde el principio del proyecto: el bucle
`para` solo aceptaba **un único identificador** como variable de
iteración. El propio parser lo decía:

```c
/* Objetivo: por ahora un único identificador. Multi-objetivo
   (`a, b en pares`) llega en sesión 5. */
```

Sesión 5 nunca llegó. Hasta v1.134.

### Lo que ahora funciona

```cornamusa
# Multi-destino clasico (tupla de pares)
para a, b en [(1, 10), (2, 20), (3, 30)]:
    imprimir(a, "->", b)
fin para

# Star al final — primero + resto
para primero, *resto en filas:
    imprimir(primero, "tiene", longitud(resto), "amigos")
fin para

# Star al inicio — todo menos el ultimo
para *previos, ultimo en historial:
    imprimir("ultimo:", ultimo)
fin para

# Star en medio
para apertura, *cuerpo, cierre en lineas:
    procesar(cuerpo)
fin para

# Dentro de funciones, anidado, etc.
funcion sumar_pares(lst):
    total = 0
    para mm, n en lst:
        total = total + mm + n
    fin para
    retornar total
fin funcion
```

### Implementación

Refactor del parser sin tocar el compilador de `para`. Si el
encabezado tiene más de un destino, se reescribe el AST a:

```
para $item_L_C en iterable:
    (destinos) = $item_L_C
    <cuerpo original>
fin para
```

donde `$item_L_C` es un identificador temporal único por posición
textual del `para` (línea + columna) para evitar colisiones entre
bucles anidados. El bucle exterior queda como un `SENT_PARA`
clásico con objetivo `EXPR_IDENT`. El destructuring vive como una
`SENT_ASIGNAR` adicional al inicio del cuerpo y se compila
exactamente como cualquier otra asignación con LHS tupla — toda la
maquinaria de `emitir_destructuring` (validación de aridad, star
binding, slots locales) se aplica sin cambios.

**Cambios concretos**:

- `src/parser.c::parsear_para`: detecta `*` o coma tras el primer
  destino y entra a la rama de destructuring. Construye
  `EXPR_TUPLA` con los destinos y envuelve el cuerpo en
  `SENT_BLOQUE([SENT_ASIGNAR(patron = $item), cuerpo])`. Nuevo
  helper `parsear_destino_para` para parsear un `IDENT` o `*IDENT`.

- `src/compilador.c::pre_reservar_locales`: en la rama de
  destructuring tupla, extendida para reconocer `EXPR_STAR_BIND`
  además de `EXPR_IDENT`. Sin esto el slot del star se creaba
  dentro del loop (el `OP_NULO` se reejecutaría en cada iteración,
  creciendo el stack — mismo bug que arregló v1.130 en
  `compilar_mientras`).

Sin tocar `compilar_para` ni `emitir_destructuring`.

### Cobertura

Nuevo fichero `tests/unit/test_bytecode_para_destructuring.c` con
9 casos:

- Multi-destino clásico (tupla de pares).
- Star al inicio, al final y en medio sobre listas de listas.
- Dentro de función (slots locales sin colisión).
- Star dentro de función con varios `_` ignorados.
- Loop largo (100 iteraciones) que verifica que no hay crecimiento
  del stack — patrón de bug pasado al destructuring en loops.
- Aridad incorrecta lanza `ErrorDeValor` atrapable.
- Regresión: single ident y one-liner clásicos.
- Anidados: `para` externo con destructuring + `para` interno
  clásico.

Suite: **330 tests verde** (329 + el nuevo).

### Notas

- El nombre temporal `$item_LINEA_COLUMNA` empieza con `$` para no
  colisionar con identificadores válidos en Cornamusa (que no
  permiten `$`). Aparece en pilas de traza si una excepción ocurre
  durante el destructuring; se documenta como detalle conocido.
- `_` no es un identificador especial en Cornamusa — funciona como
  cualquier nombre regular. Si lo usas en dos posiciones del mismo
  destructuring, la segunda asignación pisa la primera (esperado).

## [1.133.0] — 2026-06-05 — Star binding en primera posición del destructuring

v1.129 introdujo `*nombre` en destructuring pero solo aceptaba el star
en posiciones media y final:

```cornamusa
a, *r = it           # ✅ desde v1.129
a, *m, z = it        # ✅ desde v1.129
*r, z = it           # ❌ ErrorDeSintaxis: "se esperaba una expresión"
```

La limitación se documentó explícitamente en el header del test
`test_bytecode_star_destructuring.c`:

> *Star en primera posición (`*r, a = it`) no se reconoce porque
> el parser empieza llamando a `parser_parsear_expr`; el `*` se
> interpreta como factor.*

v1.133 cierra ese caso. Ahora todos los formatos funcionan:

```cornamusa
*previos, ultimo = [1, 2, 3, 4]
# previos = [1, 2, 3], ultimo = 4

*todo_menos_dos, penultimo, ultimo = "abcde"
# todo_menos_dos = "abc", penultimo = "d", ultimo = "e"

# Tambien dentro de funciones, sobre tuplas, etc.
funcion procesar(it):
    *iniciales, marca = it
    ...
fin funcion
```

### Implementación

Dos cambios pequeños en `src/parser.c` — el compilador ya soportaba
`star_idx == 0` desde v1.129 (el cálculo de índices reales
`i - n` para `i > star_idx` da `-n+1, -n+2, ...` que apuntan
correctamente al final del iterable).

**1. `parsear_asignar_o_expr` (sentencia de asignación)**: detectar
`TT_ASTERISCO` antes de invocar `parser_parsear_expr`. Si la
sentencia empieza con `*`, validar que sea `* IDENT ,` (forma
rigurosa de destructuring inicial) y construir el primer destino
como `EXPR_STAR_BIND`. Cualquier otro uso (`*r = ...` sin coma, o
`* sin identificador`) emite error claro.

**2. Heurística de fin-de-sentencia (`parsear_precedencia`)**:
añadir `TT_ASTERISCO` a la lista de tokens que rompen la
continuación cuando arrancan línea distinta a la anterior. Sin esto:

```cornamusa
x = 1
*r, z = [10, 20, 30]
```

se parseaba como `x = 1 * r` (multiplicación que cruza líneas) y
explotaba en la coma. La heurística ya cubría `[`, `(`, `si` por
razones equivalentes (v1.21 + v1.44); `*` se suma a la lista.

Caso límite: una multiplicación legítima que cruce líneas
(`x = a` + `* b` en línea aparte) deja de parsearse, pero nunca
fue idiomática — siempre se puede forzar con paréntesis o llevando
el `*` al final de la línea anterior.

### Tests

Se añadieron 9 casos al fichero `test_bytecode_star_destructuring.c`
(ya existente desde v1.129):

- Star inicial sobre lista con uno y con dos finales.
- Star inicial vacío (aridad mínima exacta).
- Star inicial sobre tupla.
- Star inicial dentro de función.
- Heurística: `x = 1` + `*r, z = ...` no roba el asterisco.
- Multiplicación en misma línea sigue funcionando.
- `*` sin identificador y `*r` sin coma rechazados.
- Aridad insuficiente con star inicial sigue siendo `ErrorDeValor`
  atrapable.

### Limitación pendiente

El `para` loop con star inicial (`para *previos, ultimo en it:`)
aún no funciona — el parser de `para` tiene una rama distinta que
no comparte código con `parsear_asignar_o_expr`. Queda para una
release futura junto con otras mejoras del bucle `para`.

Suite: **329 tests verde**.

## [1.132.0] — 2026-06-05 — Comprehensions con múltiples `para` y `si`

Antes de v1.132, `[(x, y) para x en xs para y en ys]` daba
`ErrorDeSintaxis: se esperaba ']' al final de la comprehension`.
Solo se aceptaba **una** cláusula `para X en Y [si Z]`. Esta release
añade soporte para N cláusulas anidadas (producto cartesiano).

```cornamusa
# Producto cartesiano
xs = [1, 2, 3]; letras = ["a", "b"]
pares = [(x, l) para x en xs para l en letras]
# [(1, "a"), (1, "b"), (2, "a"), (2, "b"), (3, "a"), (3, "b")]

# Guarda en la 2ª cláusula
sin_diagonal = [(i, j) para i en rango(0, 3) para j en rango(0, 3) si i != j]

# Guarda en la 1ª cláusula (filtra antes del 2º bucle)
m = [(a, b) para a en rango(0, 3) si a > 0 para b en rango(0, 2)]

# Dict y set comprehension multi-para
d = {f"{i}-{j}": i + j para i en [1, 2] para j en [10, 20]}
s = {i * j para i en [1, 2] para j en [3, 4]}

# Tres clausulas anidadas
tres = [(a, b, c) para a en [1, 2] para b en [3, 4] para c en [5, 6]]
# 8 tuplas
```

### Implementación

**AST** (`src/ast.h`/`src/ast.c`): nueva struct `ClausulaComp` con
`{nombre_var, longitud_var, iterable, guarda}`. La struct
`comprehension` añade `clausulas_extra: ClausulaComp *` y
`n_extras: int`. La PRIMERA cláusula sigue en los campos legacy
(`nombre_var`, `iterable`, `guarda`); si `n_extras == 0` el
comportamiento es idéntico al previo, sin cambios de compatibilidad.

**Parser** (`src/parser.c`): `parsear_comprehension_cola` ahora
acepta dos parámetros adicionales (out-only) para devolver las
cláusulas extra. Después de procesar la primera cláusula y su `si`
opcional, entra en un bucle `while (check(p, TT_PARA))` que recolecta
más `para X en Y [si Z]` en el array. Los 4 call sites (genex `()`,
list `[]`, dict `{k:v}`, set `{x}`) pasan `&extras, &n_extras` excepto
el genex (paréntesis) que pasa `NULL, NULL` — los generators no
soportan extras todavía.

**Compilador** (`src/compilador.c`): refactor completo del case
`EXPR_COMPREHENSION`. Para N cláusulas:

1. Pre-emit `OP_NULO` + `agregar_local` para TODOS los slots (var
   de cláusula 0, y iter+var de cada extra) **antes del primer
   `inicio_loop`**.
2. Para cada cláusula `i`:
   - Si `i > 0`: emit `iterable_i`, `OP_ITER_INICIAR`,
     `OP_ASIGNAR_LOCAL slot_iter[i]` — sobrescribe el slot
     pre-reservado.
   - `inicio_loop[i] = chunk->cuenta`.
   - `OP_ITER_SIGUIENTE slot_iter[i]`, `OP_ASIGNAR_LOCAL slot_var[i]`.
   - Si hay guarda: eval + `OP_SALTAR_SI_FALSO continuar[i]`.
3. Cuerpo: agregar al acumulador.
4. Cleanup en orden inverso: `OP_BUCLE inicio_loop[i]`, aterrizaje
   del `continuar`, patch del `offset_fin[i]`.

El patrón "pre-emit todos los slots ANTES del loop padre" es el
mismo que arregló v1.130 para `compilar_mientras`: evita que las
cláusulas internas hagan `OP_NULO + agregar_local` dentro del bucle
padre (lo que crecería el stack +N por iter). Sin esto, dos
cláusulas anidadas crasheaban silenciosamente con resultados raros
(la primera iteración del bug original imprimía solo el último
valor del var más interno).

### Limitaciones

- **Máximo 16 cláusulas anidadas** (`para` x16). El bytecode indexa
  locales con `u8`; arrays C de tamaño fijo en el compilador.
  Más allá → `ErrorDeCompilacion: demasiadas clausulas en comprehension`.
- **Generator expressions** `(expr para v en it)` aún no aceptan
  cláusulas extra. Es un refactor adicional del path de generators.

### Tests

`tests/unit/test_bytecode_comprehension_multi_para.c` con 9 bloques:
producto cartesiano, guarda en 2ª cláusula, guarda en 1ª cláusula
(filtra antes del segundo bucle), dict y set multi-para, tres
cláusulas anidadas con cardinalidad 8, y 3 regresiones de
comprehension simple (lista, guarda, dict).

Suite: **329 tests verde**.

## [1.131.0] — 2026-06-05 — `OP_INDICE` y `OP_REBANADA` sobre `rango`

Cierra la limitación que documenté honestamente en v1.129: el
destructuring star sobre rango (`a, *m, b = rango(0, 5)`) fallaba con
`'rango' no soporta slicing`, obligando a envolver en
`lista(rango(...))`. También `rango(0, 10)[3]` daba `'rango' no es
indexable`.

```cornamusa
r = rango(0, 10)
imprimir(r[3])         # 3
imprimir(r[-1])        # 9
imprimir(r[2:5])       # [2, 3, 4]
imprimir(r[::2])       # [0, 2, 4, 6, 8]
imprimir(r[::-1])      # [9, 8, 7, 6, 5, 4, 3, 2, 1, 0]

# Con paso > 1:
imprimir(rango(0, 20, 3)[2])   # 6 (valores: 0, 3, 6, 9, ...)

# Destructuring star directo, sin envolver en lista():
a, *m, b = rango(0, 5)         # a=0, m=[1, 2, 3], b=4
```

### Implementación

**`OP_INDICE` sobre `VAL_RANGO`** (`src/vm.c`): extrae `inicio`,
`fin`, `paso` como `int64` usando `mp_get_i64`. Calcula
`total = ceil((fin - inicio) / paso)` con cuidado del signo del paso.
Si el índice es negativo suma `total`. Devuelve `inicio + idx * paso`
como entero (normalizado a `VAL_ENTERO_SMALL` si cabe).

**`OP_REBANADA` sobre `VAL_RANGO`**: materializa el rango en una
`Lista` temporal con `valor_entero_de_i64` por cada valor, luego
reusa el código existente de slicing de listas (devuelve una nueva
lista con los elementos seleccionados). Más simple que duplicar la
lógica de clamp e iteración.

### Limitaciones

Solo funciona si `inicio`, `fin` y `paso` caben en `int64`
(`mp_count_bits <= 62`). Rangos bignum masivos
(`rango(0, 2**100, 1)`) rechazados con error claro:
`ErrorDeValor: rango bignum no es slice-able directamente`. Para
rangos así, materializar antes con `lista(rango(...))` (que también
fallaría por OOM en el caso real).

### Tests

`tests/unit/test_bytecode_rango_indexable.c` con 10 bloques:
índice positivo/negativo, con paso > 1, descendente, slice básico,
slice con paso, slice invertido `[::-1]`, destructuring star sobre
rango (la motivación), destructuring sin star, regresión de
`longitud(rango)`.

Suite: **328 tests verde**.

## [1.130.0] — 2026-06-05 — Cierra el bug "OP_ITER_SIGUIENTE sin iterador en slot N"

Bug documentado en v1.119 (`stdlib/grafos.cor:componentes`). La versión
idiomática con `para` anidados crasheaba en la segunda iteración del
exterior con:

```
estado interno corrupto
OP_ITER_SIGUIENTE sin iterador en slot N
```

El workaround de v1.119 fue reescribir `componentes` y todos sus
bucles internos como `mientras + índice manual` (`i = 0; mientras
i < longitud(xs): ...; i = i + 1`), un patrón claramente no idiomático
para un lenguaje cuyo selling point es la legibilidad. Documentado
honestamente en el CHANGELOG de v1.119 como "bug del compilador para
investigar más adelante".

### Caso mínimo

```cornamusa
funcion f():
    nodos = ["a", "b"]
    para n en nodos:
        cola = coleccion.Cola()
        cola.poner(n)
        mientras no cola.vacia():
            cur = cola.sacar()       # asignacion NUEVA dentro del while
            imprimir(cur)
            para v en [1, 2]:        # `para` interno crashea aqui
                imprimir(v)
            fin para
        fin mientras
    fin para
fin funcion
```

Antes de v1.130 imprimía `a, 1, 2, b` y crasheaba al entrar al `para v`
de la segunda iteración. Tras v1.130 imprime `a, 1, 2, b, 1, 2`
correctamente.

### Root cause

`compilar_mientras` en `src/compilador.c` llamaba a
`pre_reservar_locales` para los locales del cuerpo (`cur`, etc.),
emitiendo `OP_NULO + agregar_local` por cada uno — **pero nunca emitía
los `OP_DESCARTAR` correspondientes al salir del bucle** (a diferencia
de `compilar_para` que sí lo hace).

Consecuencia: cuando el `mientras` está dentro de un `para` exterior,
cada iteración del exterior re-ejecuta el `OP_NULO` del pre-reservado
y el stack crece +N. El `compilar_para` interno calcula `slot` para su
`$iter` en compile-time asumiendo `tope == n_locales`; en la segunda
iteración del exterior ese slot apunta al `OP_NULO` del pre-reservado
del while de iteraciones anteriores, no al iterador real.

### Fix

Añadir el mismo cleanup que `compilar_para` ya tenía al final de
`compilar_mientras`:

```c
{
    int drops = c->actual->n_locales - n_locales_entrada;
    for (int j = 0; j < drops; j++) {
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
    }
}
c->actual->n_locales = n_locales_entrada;
```

Cuatro líneas. La constante `n_locales_entrada` ya se capturaba al
inicio del helper pero nunca se usaba — solo había un `(void)
n_locales_entrada` placeholder.

### Limpieza posterior

Restaurada la versión idiomática de `stdlib/grafos.cor:componentes`
con `para n en g.nodos()`, `mientras no cola.vacia():`,
`para vec en g.vecinos(cur):` y `para otro en g.nodos():` anidados.
El código pasó de 47 líneas con índices manuales a 35 líneas con
bucles `para` legibles.

### Tests

`tests/unit/test_bytecode_para_anidado_en_mientras.c` con 5 bloques:
- Caso mínimo del bug (3 niveles de anidación).
- Variante con tres niveles `para → mientras → para`, contando líneas
  para verificar que NO hay crash silencioso.
- Regresión: `mientras` simple sigue funcionando.
- Regresión: `mientras` con `romper`.
- Regresión: `grafos.componentes` (que ahora usa la versión idiomática).

Suite: **327 tests verde**.

## [1.129.0] — 2026-06-05 — Star binding en destructuring (`a, *resto, c = lista`)

El destructuring soportaba `a, b = par` y `a, b = b, a + b` desde v1.21,
incluyendo el caso patológico de v1.122/v1.123 (variables nuevas en bucle).
La pieza que faltaba para paridad con Python era el **star binding**:
`a, *resto, c = lista`, idiomático para *primero, intermedios y último*.

Curiosidad: la sintaxis YA existía en pattern matching (`cuando [a, *r, b]:`)
desde v1.16.2, pero no en destructuring de asignación.

```cornamusa
a, *r, c = [1, 2, 3, 4, 5]       # a=1, r=[2, 3, 4], c=5
a, b, *r = [1, 2, 3, 4, 5]       # a=1, b=2, r=[3, 4, 5]
a, *r, c = [1, 2]                # a=1, r=[], c=2  (star vacío)
a, b, *r, c, d = [1, 2, 3, 4]    # a=1, b=2, r=[], c=3, d=4

# Sobre tupla:
a, *r, c = (10, 20, 30, 40)      # r es lista, no tupla

# Sobre cadena (slice de code points):
a, *r, c = "hola"                # a='h', r='ol', c='a'
```

### Implementación

`src/ast.h`/`src/ast.c`: nuevo nodo `EXPR_STAR_BIND { nombre, longitud }`
+ helper `expr_star_bind`. Solo válido como destino dentro de un
destructuring; cualquier otro uso lo rechaza el compilador.

`src/parser.c` (`parsear_asignar_o_expr`): dentro del bucle que recolecta
destinos tras `,`, si el siguiente token es `*` lo consume y exige un
`IDENT` después. Construye un `EXPR_STAR_BIND`. Restricción
documentada: el primer destino sí pasa por `parser_parsear_expr`
normal, así que `*r, a = it` (star en posición inicial) no se reconoce
— sólo `a, *r, b = it`.

`src/compilador.c` (`emitir_destructuring`):
- Detecta `star_idx` en una primera pasada. Si hay más de uno → error.
- Pre-reserva slots para destinos `EXPR_IDENT` **y** `EXPR_STAR_BIND` (mismo
  manejo: reusa locales existentes, marca upvalues con `-100 - upv`).
- Verificación de aridad: sin star → `==` n; con star → `>=` n - 1
  (emite `OP_MAYOR_IGUAL` en vez de `OP_IGUAL`).
- En el loop de extracción:
  - `i < star_idx`: `OP_INDICE` con i positivo.
  - `i == star_idx`: emite `OP_REBANADA [star_idx : longitud(it) - tail]`
    con `tail = n - 1 - star_idx`. El resultado se asigna al slot del star.
  - `i > star_idx`: `OP_INDICE` con `i - n` (índice negativo) — desde el
    final de la lista.

`src/vm.c` (`OP_REBANADA`):
- Antes solo aceptaba `VAL_LISTA` y `VAL_CADENA`. Ahora también `VAL_TUPLA`:
  `total = tupla->cuenta`, mismas operaciones de clamp e iteración, pero
  el resultado es **siempre una lista** (no una tupla nueva) porque el
  star binding lo necesita así.

### Limitaciones

- **Star solo en posición no-inicial** (`a, *r, b = it` sí; `*r, a = it`
  no). El parser empieza por una expresión normal antes del primer `,`.
- **Iterable debe soportar `OP_LONGITUD` y `OP_INDICE`**: lista, tupla
  y cadena. Rango no porque `OP_INDICE` no lo acepta — para usar rango,
  envolver en `lista(rango(...))`.
- Sólo un star por destructuring (validado en `emitir_destructuring`).

### Tests

`tests/unit/test_bytecode_star_destructuring.c` con 13 bloques:
star en medio/final, vacío, dos lados, aridad insuficiente lanza,
sobre tupla/cadena, reasignar variables existentes, dentro de función,
múltiples stars rechazado, regresión sin star.

Suite: **326 tests verde**.

## [1.128.0] — 2026-06-05 — Métodos sobre `conjunto`, `tupla` y `lista.indice_de`

v1.122 introdujo la tabla `METODOS_NATIVOS` con 13 entradas iniciales
(lista/cadena/dict). Pero **`conjunto` y `tupla` quedaron con 0
entradas** — `{1, 2}.union({3})` fallaba con `'conjunto' no tiene
atributos accesibles`. Esta release cierra esos huecos siguiendo el
mismo patrón seguro: implementar nativas en C, registrarlas en la
tabla, y un test unitario por método.

### `conjunto` (9 entradas nuevas)

- `agregar(x)` / `añadir(x)` (alias) — la nativa global `agregar` ya
  soportaba conjuntos desde v1.16.1; ahora se expone como método.
- `quitar(x)` — la nativa global `quitar` ya soportaba conjuntos.
- `union(otro)` — conjunto nuevo con todos los elementos de ambos.
- `interseccion(otro)` — conjunto nuevo con los elementos comunes,
  iterando sobre el más pequeño (O(min)).
- `diferencia(otro)` — conjunto nuevo con elementos en `self` pero no en `otro`.
- `es_subconjunto(otro)` — booleano `self ⊆ otro`. Early-out si `|self| > |otro|`.
- `contiene(x)` — booleano. Si `x` no es hashable devuelve `falso` (no lanza).
- `copiar()` — shallow copy.

### `tupla` (3 entradas nuevas)

Las tuplas son inmutables, así que el set se limita a métodos de
secuencia de consulta:

- `contar(x)` — apariciones por igualdad.
- `contiene(x)` — booleano (equivale a `x en t`).
- `indice_de(x)` — primer índice o `-1`.

### `lista.indice_de(x)` (bonus)

Faltaba complemento a `lista.contar(x)` / `lista.contiene(x)` de
v1.122 — para paridad con `cadena.indice_de` y la nueva
`tupla.indice_de`. Devuelve la posición de la primera aparición o `-1`.

### Implementación

Las nativas nuevas (`nativa_conjunto_*`, `nativa_tupla_*`,
`nativa_lista_indice_de`) están en `src/nativos.c` con un helper
interno `_conj_copiar_todos` que clona todos los elementos de un
conjunto en otro. `union`/`copiar` usan ese helper; `interseccion` y
`diferencia` iteran y filtran con `conj_contiene`. Las tuplas y listas
usan `valor_iguales` (mismo despachador de igualdad por dunder
`__igual__` que el operador `==`).

Tabla `METODOS_NATIVOS` pasa de **24 entradas a 37**.

### Tests

`tests/unit/test_bytecode_metodos_conjunto_tupla.c` con 12 bloques,
~25 asserts:
- Cardinalidad y pertenencia post-`union`/`interseccion`/`diferencia`.
- `es_subconjunto` con casos verdadero/falso y conjunto vacío.
- `copiar` es independiente (mutar el original no afecta a la copia).
- `agregar`/`quitar` vía método (no solo global).
- `tupla.contar` / `contiene` / `indice_de` con casos hit y miss.
- `lista.indice_de` con cadenas.
- Método no existente sobre conjunto sigue lanzando `ErrorDeTipo` atrapable.

Suite: **325 tests verde**.

## [1.127.0] — 2026-06-05 — Completion en el LSP

El LSP de Cornamusa ya implementaba `diagnostics`, `hover`, `definition`
y `formatting`. Faltaba `completion` — la pieza más usada del LSP por
los editores.

### Capabilities

`initialize` ahora anuncia:

```json
{ "completionProvider": { "resolveProvider": false } }
```

Sin `triggerCharacters`: la completion se dispara con Ctrl-Space o
cuando el cliente la pide explícitamente.

### Items devueltos

`textDocument/completion` responde con una `CompletionList`
(`isIncomplete: false`) que incluye:

1. **Todas las nativas globales** (~119) con `kind: 3` (Function) y
   `detail: "funcion nativa"`. Se itera con la nueva API pública
   `nativos_iterar_nombres` en `src/nativos.h`.
2. **36 keywords del lenguaje** (`si`, `sino`, `mientras`, `funcion`,
   `clase`, etc.) con `kind: 14` (Keyword). Lista alineada con
   `buscar_keyword` en `src/lexer.c`.
3. **Funciones y clases top-level** del documento abierto, parseando
   el texto on-demand con el mismo arena + parser que ya usan `hover`
   y `definition`. `kind: 3` para funciones, `kind: 7` para clases,
   `detail: "funcion/clase (este archivo)"`.

`isIncomplete = false` significa que devolvemos toda la lista; el
cliente filtra por prefijo. Es el approach simple y barato que funciona
bien para corpus pequeño/medio — ~160 items por completion request.

### Tests

`tests/integracion/lsp_completion.py` lanza el LSP como subproceso,
hace `initialize` + `didOpen` con un .cor que define `funcion saludar`
y `clase Persona`, pide completion, verifica que la respuesta incluye
nativas conocidas (`imprimir`, `longitud`), keywords (`si`, `funcion`)
y los símbolos top-level (`saludar`, `Persona`).

Registrado en CMake con `find_package(Python3 ... QUIET)`; si Python
no está disponible el test se omite silenciosamente.

Suite: **324 tests verde**.

## [1.126.0] — 2026-06-05 — Autocompletado con TAB en el REPL

El REPL ya tenía historial persistente, edición de línea con cursores
y multilínea. Faltaba el clásico TAB para autocompletar — uno de los
quality-of-life más visibles desde el primer segundo de uso.

```
>>> impr<TAB>
imprimir  imprimir_error
>>> imprimi<TAB>
>>> imprimir
>>> imp<TAB><TAB>
imprimir  imprimir_error  importar
>>> imp
```

### Comportamiento

- **0 candidatos**: silencio (sin beep).
- **1 candidato**: el sufijo restante se inserta directamente.
- **N candidatos**: se inserta el **prefijo común más largo**; si no
  extiende lo escrito, los candidatos se listan en una línea debajo
  separados por dos espacios y la línea actual se reimprime.
- **Cursor al principio o tras un espacio**: TAB inserta 4 espacios
  (comodidad para identar dentro de bloques en multilínea).

### Implementación

`src/repl_line.{h,c}` expone una API minimalista de autocompletado:

```c
typedef void (*ReplEmitirCandidatoFn)(const char *cand, int cand_len,
                                          void *emit_ctx);
typedef void (*ReplCompletarFn)(const char *prefijo, int prefijo_len,
                                   void *ctx,
                                   ReplEmitirCandidatoFn emitir,
                                   void *emit_ctx);

void repl_set_completar(ReplCompletarFn fn, void *ctx);
```

El editor de línea detecta TAB y extrae el token de identificador antes
del cursor (`prefijo_token_len`). `es_char_ident` reconoce ASCII
alfanumérico, `_` y todo byte `>= 0x80` — preserva identificadores con
tildes/eñes (`añadir`, `función`). El callback es idempotente y puede
llamarse varias veces por sesión.

En `src/main.c:correr_repl` se registra un callback que itera el
`Entorno` global (todos los nativos + variables definidas por el
usuario) y filtra por prefijo. Adicionalmente añade las 36 keywords
del lenguaje (`si`, `sino`, `mientras`, etc.) — la lista está alineada
con `buscar_keyword` en `src/lexer.c`.

### Tests

`tests/unit/test_repl_completar.c` valida la API pública + un callback
de ejemplo con 5 casos: prefijo `imp` → 3 candidatos, `long` → 1,
`l` → 3, `xyz` → 0, prefijo vacío → todos. La rama TAB del editor de
línea requiere TTY interactivo y queda cubierta solo por verificación
manual; las funciones internas `prefijo_token_len` y `prefijo_comun`
son aritmética trivial sobre arrays.

Suite: **323 tests verde**.

## [1.125.0] — 2026-06-04 — Typo suggestions en ErrorDeClave

Cierra un hueco detectado en la fase 5 del plan corpus:
`ErrorDeAtributo` y `ErrorDeNombre` ya invocaban sugerencias estilo
*"¿quisiste decir 'X'?"* con Levenshtein, pero `ErrorDeClave` (en
`OP_INDICE` y `OP_BORRAR_INDICE` sobre diccionarios) solo imprimía la
clave `repr` sin sugerencia. Los typos en claves de dict eran de los
errores menos amigables.

```cornamusa
d = {"nombre": "Ana", "edad": 30, "ciudad": "Sevilla"}

# Antes:
d["nomre"]    # ErrorDeClave: "nomre"
borrar d["ciuda"]  # ErrorDeClave: clave "ciuda" no presente en diccionario

# Tras v1.125:
d["nomre"]    # ErrorDeClave: "nomre" (¿quisiste decir 'nombre'?)
borrar d["ciuda"]  # ErrorDeClave: clave "ciuda" no presente en diccionario (¿quisiste decir 'ciudad'?)
```

### Fix

En `src/vm.c`, dos sitios — `OP_INDICE` sobre `VAL_DICCIONARIO` cuando
`dicc_obtener` falla, y `OP_BORRAR_INDICE` cuando `dicc_quitar` falla.
Si la clave es `VAL_CADENA`, invocar `sugerir_nombre_cercano` sobre
`obj.como.dicc` (reusa la infra de Levenshtein adaptativo que ya cubría
nombres globales). Si la clave no es cadena, fallback al mensaje
clásico (las sugerencias de Levenshtein no aplican).

### Detalles ya cubiertos por la infra existente

- Umbral adaptativo: distancia ≤ 2 para claves ≥ 4 chars, ≤ 1 para más cortas.
- Case-insensitive ASCII tiene prioridad alta: `d["NOMBRE"]` sugiere `nombre`.
- No sugiere identidad: si la clave es exactamente igual a una clave existente, no se sugiere a sí misma.
- Salta nombres internos `$...`.

### Tests

`tests/unit/test_bytecode_typo_clave.c` con 5 casos:
- Levenshtein 1 (`nomre` → `nombre`)
- Case-insensitive (`NOMBRE` → `nombre`)
- Claves muy distintas (sin sugerencia)
- `borrar d["ciuda"]` también sugiere
- Clave no-cadena (`d[42]`): mensaje clásico, no crashea

Suite: **322 tests verde**.

## [1.124.0] — 2026-06-04 — Mensajes de error en kwargs de constructor

Cierra el detalle de pulido documentado en v1.121: cuando un kwarg
fallaba al construir una instancia, el mensaje decía `__iniciar__()`
en vez del nombre de la clase.

```
# Antes:
Persona(nombre="Ana", edad=30, profesion="ing")
-> ErrorDeTipo: __iniciar__() no acepta keyword 'profesion'

# Tras v1.124:
Persona(nombre="Ana", edad=30, profesion="ing")
-> ErrorDeTipo: Persona() no acepta keyword 'profesion'
```

### Fix

En `src/vm.c:ejecutar_llamar_kw`, capturar `nombre`/`longitud_nombre`
de la clase ANTES de la transformación `VAL_CLASE → __iniciar__`.
Variables `err_nombre` / `err_long_nombre` se usan en los cuatro
`snprintf` de error de matching kw. Si la llamada no es a un
constructor, se inicializan al nombre de la función como antes.

Errores cubiertos:
- `%.*s() recibio %d posicionales pero solo acepta %d`
- `%.*s() no acepta keyword 'X'`
- `%.*s() recibio multiple valor para 'X'`
- `%.*s() falta argumento 'X'`

### Tests

`tests/unit/test_bytecode_kwargs_clase_error_nombre.c` con 4 bloques:
kwarg duplicado, kwarg desconocido, falta argumento, y regresión
para funciones normales (siguen diciendo su nombre, no rompemos
ese path).

Suite: **321 tests verde**.

## [1.123.0] — 2026-06-04 — Destructuring de variables nuevas dentro de bucle

Cierra el caso patológico residual documentado en v1.122 fase 1: cuando
los destinos de un destructuring eran **nuevas** variables dentro de un
bucle dentro de una función, cada iteración acumulaba `+(1 + n_destinos)`
slots fantasma en el stack y la lectura `OP_OBTENER_LOCAL slot_iter`
siempre devolvía la tupla de la PRIMERA iter. Síntoma observable:

```cornamusa
funcion f():
    para i en rango(0, 5):
        a, b = i, i * 2
        imprimir(a, b)
    fin para
fin funcion
```

Antes de v1.123 imprimía `0 0` cinco veces (las nuevas asignaciones
no surtían efecto). Tras v1.123 imprime `0 0`, `1 2`, `2 4`, `3 6`, `4 8`.

### Fix

Extender `pre_reservar_locales` (en `src/compilador.c`) para reconocer
`SENT_ASIGNAR` con destino `EXPR_TUPLA` o `EXPR_LISTA`, no solo
`EXPR_IDENT`. Recorre cada destino del patrón; si es `IDENT` y no es
local existente, emite `OP_NULO + agregar_local`; si es a su vez una
tupla anidada, recurre. Cuando `emitir_destructuring` se ejecuta dentro
del bucle, ve los destinos como locales existentes, reusa sus slots y
descarta `slot_iter` (la rama `n_nuevos_slots == 0` del fix de v1.122).

### Tentativa descartada

Una primera versión hacía que `pre_reservar_locales` descendiera en
`SENT_PARA`/`SENT_MIENTRAS`/`SENT_INTENTAR` y se invocara desde el
inicio de `compilar_funcion`. Eso movía las variables al scope de la
función — incluyendo las simples — y hacía que `a` sobreviviera tras
`para i: a = i fin para` (semántica Python). Pero rompió
`bc_run_30_closures_nolocal`: `funcion inc(): nolocal n; n = n + 1
fin funcion` lanzaba `'n' es local del scope actual` porque la
pre-reserva creaba `n` como local antes de procesar `nolocal n`.

Decisión: revertir esa parte y mantener el scoping clásico de Cornamusa
(variables del cuerpo de un bucle/intentar no sobreviven al control de
flujo). El bug específico de "stack crece dentro del bucle" se resuelve
con el cambio mínimo en `SENT_ASIGNAR + EXPR_TUPLA`.

### Tests

`tests/unit/test_bytecode_destructuring_bucle_nuevas.c` con 5 bloques:
- destructuring de nuevas en bucle, valores correctos por iter
- 1000 iters retornando `[999, 1000]` (stack no acumula)
- destructuring anidado `a, (b, c) = ...` dentro de bucle
- regresión `nolocal n` (la tentativa descartada lo rompía)
- destructuring dentro de `si`/`sino` dentro de `para`

Suite: **320 tests verde**.

## [1.122.0] — 2026-06-04 — Plan corpus (fases 0-5) + métodos nativos extendidos

Esta release agrupa dos bloques de trabajo simultáneos:

### Plan de 6 fases sobre el corpus pedagógico

Detectado al generar masivamente código Cornamusa: `examples/03_fibonacci`
imprimía `fib(n)=0` para todo n; 4 ejemplos crasheaban con
`ErrorDeTipo` por métodos sobre nativos; `examples/12_modulos`
usaba nombres inexistentes de stdlib; ejecutar `cornamusa X.cor`
desde otro cwd no resolvía `importar funcionales`.

- **Fase 0** Golden tests: `cmake/golden_one.cmake` + GLOB sobre
  `examples/esperado/*.salida`. Compara stdout byte a byte con
  normalización CRLF→LF.
- **Fase 1** Bug del destructuring `a, b = b, a + b` en bucle dentro
  de función. Causa raíz: `slot_iter` (la tupla RHS) persistía en
  el stack al final del destructuring cuando los destinos eran
  variables existentes. En cada iteración se acumulaba un slot
  fantasma y el bytecode leía siempre la tupla de la PRIMERA iter.
  Fix en `src/compilador.c:emitir_destructuring`: reusar slot de
  variables existentes (locales y upvalues), contar `n_nuevos_slots`,
  y descartar `slot_iter` cuando todos los destinos preexistían.
- **Fase 2** Métodos sobre tipos nativos: nuevo `VAL_METODO_NATIVO_LIGADO`
  + tabla `METODOS_NATIVOS` en `nativos.c`. `xs.añadir(4)`,
  `"hola".minusculas()`, `d.claves()` etc. funcionan. 13 métodos
  iniciales sobre lista/cadena/dict.
- **Fase 3** Atributos sintéticos `.nombre` y `.__nombre__` sobre
  `VAL_CLASE`. Patrón `f"{clase_de(yo).nombre}(...)"` para
  `__cadena__` polimórfico. (`tipo(instancia)` sigue devolviendo
  la cadena "instancia" — cambiarlo rompía 8 tests.)
- **Fase 4** stdlib resoluble relativa al binario y a `$CORNAMUSA_RUTA`.
  `vm_set_ruta_binario` en `main.c` (Windows usa `GetModuleFileNameA`).
  Cinco intentos: cwd, cwd/stdlib, `$CORNAMUSA_RUTA`,
  `dir_binario/stdlib`, `dir_binario/../stdlib`.
- **Fase 5** `docs/referencia-stdlib.md` verificado automáticamente
  con `smoke_referencia_stdlib.cor`. 3 ejemplos arreglados (05, 08,
  12) y validados con golden output.

Resultado del plan: **105 de 106 ejemplos ejecutan correctamente**
con `--bytecode` (el restante `26_leer_jugable` requiere stdin y
su test diferencial inyectándolo sigue pasando).

### Ampliación de métodos nativos (10 nuevos)

Tras la fase 2 quedaban huecos visibles. Nuevas nativas y entradas
en `METODOS_NATIVOS`:

**Cadena**:
- `cadena.separar(sep)` — O(n) (la versión pure-Cornamusa era O(n²)).
  `sep=""` separa por code-point UTF-8.
- `cadena.reemplazar(viejo, nuevo)` — O(n) replace all.
- `cadena.recortar()` — trim ASCII espacios (` \t\n\r\f\v`).
- `cadena.contiene(sub)` — booleano (wrapper de `indice_de >= 0`).
- `cadena.unir(lista)` — receptor es el separador (Python `sep.join(lst)`).

**Lista**:
- `lista.contar(x)` — apariciones por igualdad.
- `lista.contiene(x)` — booleano (igual semántica que `x en xs`).
- `lista.copiar()` — shallow copy. Equivalente a `xs[0:]`.

**Diccionario**:
- `dict.items()` — lista de `[clave, valor]` en orden de inserción.
  Permite `para par en d.items(): par[0]; par[1] fin para`.
- `dict.obtener(clave, defecto)` — devuelve el valor o el defecto
  sin lanzar `ErrorDeClave`. Patrón canónico de Python `dict.get`.

Tests: `tests/unit/test_bytecode_metodos_nativos_v123.c` con 22+
asserts. Las 13 entradas previas (v1.122 fase 2) tienen su test
separado `test_bytecode_metodos_nativos.c`.

### Suite

**316 tests verde** (308 base + 8 nuevos golden/integración del
plan + el unit test de las nativas extras).

## [1.121.0] — 2026-06-04 — Kwargs en constructores de clase

Cierra deuda técnica documentada en v1.120: hasta hoy
`Persona(nombre="Ana", edad=30)` o `Heap(clave=lambda x: x)`
lanzaban `ErrorDeTipo: keyword args solo soportados para
funciones bytecode (no 'clase')`. La instanciación con kwargs
era el único hueco que quedaba en la paridad de llamada
posicional/keyword entre funciones y constructores.

### Implementación

En `src/vm.c`, el helper `ejecutar_llamar_kw` detecta `VAL_CLASE`
antes de la verificación `VAL_FUNCION_BC`:

1. Crea la instancia con `instancia_nueva`.
2. Recupera `__iniciar__` del dict de métodos.
3. Si no existe y se pasaron args (positionals o kwargs), lanza
   `ErrorDeTipo: X() no acepta argumentos (sin __iniciar__)`.
4. Si existe, hace espacio para `yo` desplazando los args en el
   stack un slot hacia arriba, sustituye el callee `clase` por la
   closure de `__iniciar__`, e inserta `yo = instancia` como pos0.
5. Marca el frame nuevo como `es_constructor = true` para que el
   retorno descarte el valor del cuerpo y devuelva la instancia
   (semántica Python).

A partir de ahí, el flujo es idéntico al de cualquier llamada con
kwargs: matching nombre→slot, defaults para faltantes, errores
por duplicados o kwargs desconocidos.

### Patrones desbloqueados

```cornamusa
# Heap con clave nombrada (motivación original).
h = coleccion.Heap(clave=lambda p: p[0])

# Constructores con muchos parámetros legibles en su sitio:
clase Persona:
    funcion __iniciar__(yo, nombre, edad, ciudad="Madrid"):
        ...
    fin funcion
fin clase

p = Persona(nombre="Ana", edad=30)
p = Persona("Luis", edad=25, ciudad="Sevilla")  # mezcla pos+kw
```

### Errores que sí lanza correctamente

- `Vacia(x=1)` con clase sin `__iniciar__` → `X() no acepta argumentos`.
- `Persona(1, nombre="Otro")` → kw duplicado.
- `Persona(nombre="Ana", profesion="ing")` → kw desconocido (sin `**kw`).

### Tests

- `tests/unit/test_bytecode_kwargs_clase.c` — 10 bloques, 12+ asserts:
  cubre construcción por kwargs, mezcla pos+kw, defaults
  rellenados, override de default, clase sin `__iniciar__` (con
  y sin args), kw duplicado, kw desconocido, `Heap(clave=lambda)`
  como caso real, y la semántica de constructor que descarta
  `retornar` del cuerpo y devuelve la instancia.

Suite total: **304 tests, 100% verde**.

### Nota sobre el mensaje de error

Cuando el matching falla dentro del constructor, el mensaje dice
`__iniciar__() recibio multiple valor para 'x'` y no `Persona()`.
Es técnicamente honesto (la función que falla es `__iniciar__`)
y se puede pulir en una release menor; no bloquea esta release.

### Ejemplo actualizado

`examples/106_heap_clave.cor` ahora usa `Heap(clave=lambda t: t["prioridad"])`
en la sección 1 (cola de prioridad de tareas), demostrando el patrón
directamente con kwargs en lugar de posicional.

## [1.120.0] — 2026-06-04 — `Heap` con clave + Dijkstra O((V+E) log V)

Cierra deuda técnica documentada en v1.119: `coleccion.Heap` acepta
un callable opcional como `clave` que extrae el valor de comparación,
y `grafos.dijkstra` / `camino_mas_corto` se refactorizan para usarlo.

### `coleccion.Heap(clave=nulo)`

Nuevo segundo parámetro opcional. Cuando se proporciona, las
comparaciones internas usan `clave(a) < clave(b)` en vez de `a < b`.
Compatible hacia atrás — sin clave el Heap se comporta exactamente
igual que antes.

```cornamusa
# Cola de prioridad de tareas (prioridad 1 = más urgente):
tareas = coleccion.Heap(lambda t: t["prioridad"])
tareas.poner({"prioridad": 3, "tarea": "limpiar"})
tareas.poner({"prioridad": 1, "tarea": "incendio"})
tareas.poner({"prioridad": 2, "tarea": "deadline"})
imprimir(tareas.sacar())   # {"prioridad": 1, "tarea": "incendio"}
```

Patrones desbloqueados:
- Heap de listas/tuplas (`Heap(lambda p: p[0])`).
- Heap de instancias o dicts por campo (`Heap(lambda x: x["nota"])`).
- Max-heap negando la clave (`Heap(lambda x: -x)`).

### `grafos.dijkstra` ahora es O((V+E) log V)

Reescrito sobre `Heap(lambda par: par[0])` con entries `[distancia, nodo]`.
Patrón clásico de **lazy deletion**: cuando sacamos una entrada cuyo
nodo ya tiene mejor distancia en el dict, la descartamos sin
explorar — evita tener que implementar `decrease-key` en el heap.

Resultados verificados idénticos a la versión O(V²) sobre el mismo
grafo (mapa de carreteras de Castilla: Madrid→Salamanca por Ávila,
213 km).

### Limitación que sigue: kwargs en constructores de clase

`coleccion.Heap(clave=lambda x: x)` actualmente NO funciona — devuelve
`ErrorDeTipo: keyword args solo soportados para funciones bytecode (no 'clase')`.
Hay que usar posicional: `coleccion.Heap(lambda x: x)`. Es un hueco del
runtime de clases, independiente de esta release. Documentado en el
módulo y en el ejemplo.

### Tests

- `tests/unit/test_bytecode_heap_clave.c` — 8 bloques, 17+ asserts:
  - Regresión: Heap sin clave sigue funcionando.
  - Heap con clave sobre listas, dicts, negada (max-heap).
  - `vista()` con clave, empates por clave.
  - Regresión: `dijkstra` y `camino_mas_corto` con heap dan los
    mismos resultados que la versión O(V²) anterior.

Suite total: **303 tests, 100% verde**.

### Ejemplo

`examples/106_heap_clave.cor` — 4 secciones: cola de prioridad,
top-K menores (peores notas), max-heap por ventas, merge de 3 listas
ordenadas (algoritmo clásico que requiere comparar por valor pero
mantener índices).

## [1.119.0] — 2026-06-04 — Stdlib `grafos`

Nuevo módulo `stdlib/grafos.cor` con clase `Grafo` (dirigido o no
dirigido, con pesos) y los algoritmos clásicos: BFS, DFS, Dijkstra,
camino más corto, orden topológico, detección de ciclos y
componentes conexas. Pure-Cornamusa, sobre `coleccion.Cola` y
`coleccion.Pila`.

### `Grafo(dirigido=verdadero)`

Representación interna: `dict` de adyacencia `nodo → dict(vecino → peso)`. Permite recorrer aristas en O(grado) y actualizar/quitar aristas en O(1). Nodos pueden ser cualquier valor hashable (cadena, entero, tupla).

API:
- `agregar_nodo(n)`, `agregar_arista(u, v, peso=1)`, `quitar_arista(u, v)`.
- `nodos()` → lista (orden de inserción).
- `aristas()` → lista de `[u, v, peso]`. En no dirigidos cada par aparece una sola vez.
- `vecinos(n)`, `peso(u, v)` (devuelve `nulo` si no existe), `contiene(n)`.
- `__longitud__`, `__cadena__`.

En grafos no dirigidos, `agregar_arista(u, v, p)` crea automáticamente la arista inversa con el mismo peso.

### Algoritmos

- `bfs(g, inicio)` → lista de nodos en orden de visita. Usa `coleccion.Cola`.
- `dfs(g, inicio)` → lista de nodos en preorden iterativo. Usa `coleccion.Pila`. Apila vecinos en orden inverso para que el primer vecino se procese antes.
- `dijkstra(g, inicio)` → `dict` nodo→distancia. Solo nodos alcanzables aparecen (no se usa `INFINITO` como sentinela). Lanza `ErrorDeValor` si encuentra arista con peso negativo durante la exploración.
- `camino_mas_corto(g, inicio, finn)` → lista `[inicio, ..., finn]` o `[]` si no hay camino. Reconstruye guardando predecesores.
- `componentes(g)` → lista de listas de nodos. En grafos dirigidos calcula componentes **débilmente conexas** (trata aristas como no dirigidas). Para fuertemente conexas se necesitaría Tarjan/Kosaraju (pendiente).
- `topologico(g)` → orden topológico por Kahn (cola de nodos con grado entrante 0). Lanza `ErrorDeValor` si el grafo tiene ciclo o no es dirigido.
- `tiene_ciclo(g)` → booleano. En no dirigidos usa DFS con marca de padre; en dirigidos usa Kahn.

### Decisión: Dijkstra O(V²) lineal, no O((V+E) log V) con heap

`coleccion.Heap` solo compara valores escalares nativamente (números, cadenas). Para usar heap de pares `[distancia, nodo]` haría falta un comparador por primer elemento — funcionalidad que no existe todavía. La versión O(V²) sobre `dict` de pendientes es suficiente para grafos pedagógicos típicos (hasta ~1000 nodos cómodos). Documentado en el módulo como deuda técnica.

### Tropezón conocido: `para` anidado en `coleccion.Cola`

Una versión inicial de `componentes` con `coleccion.Cola()` + `para` anidado producía `"OP_ITER_SIGUIENTE sin iterador en slot N"`. El compilador del bytecode tiene un edge case con la interacción entre slots de iteradores y métodos de clases internos. Solución: reescribir `componentes` con lista plana + índice de cabecera (`mientras cabeza < longitud(cola)`). Posible bug del compilador para investigar más adelante; no bloquea esta release.

### Tests y ejemplo

- `tests/unit/test_bytecode_grafos.c` — 17 bloques, 22+ asserts. Cubre clase Grafo (dirigido y no dirigido), BFS/DFS orden, Dijkstra con camino indirecto más corto (`A→C→B = 3` vs `A→B = 4`), peso negativo lanza, camino_mas_corto reconstrucción, topológico DAG + ciclo lanza, tiene_ciclo, componentes (3 grupos), quitar_arista, contiene.
- `examples/105_grafos.cor` — 6 secciones: mapa de carreteras de Castilla con Dijkstra real (Madrid→Salamanca por Ávila, 213 km), red social con BFS/DFS, orden topológico para preparar café, detección de ciclo, componentes en red de amigos, errores típicos (peso negativo, nodo ausente).

Suite total: **301 tests, 100% verde**.

## [1.118.0] — 2026-06-04 — Stdlib `iteradores`

Nuevo módulo `stdlib/iteradores.cor`: combinatoria y herramientas de
iteración inspiradas en `itertools` de Python pero en castellano,
pure-Cornamusa. Complementa `funcionales` (mapear/filtrar/reducir/
agrupar_por/tomar/saltar) con las primitivas que faltaban para
permutaciones, ventanas deslizantes y procesamiento por lotes.

### Combinatoria

- `producto(xs, ys)` — producto cartesiano de dos iterables → lista de `[a, b]`.
- `producto3(xs, ys, zs)` — análogo para 3 iterables.
- `producto_repeticion(xs, r)` — `xs^r` (todas las r-tuplas con repetición). Útil para enumerar todas las claves binarias / configuraciones.
- `permutaciones(xs, r=-1)` — sin repetición; `r=-1` (default) usa `longitud(xs)`.
- `combinaciones(xs, r)` — sin repetición, orden lexicográfico por índices. `r=0` → `[[]]`; `r>n` → `[]`.
- `combinaciones_con_repeticion(xs, r)` — multiconjuntos.

### Iteración

- `concatenar(xs, ys)` — encadena dos iterables. (Nombre `concatenar` en vez de `cadena` para no sombrear el built-in de conversión a texto.)
- `repetir(valor, n)` — lista con `valor` repetido `n` veces.
- `ventana(xs, n)` — ventanas deslizantes de tamaño `n`. `n > longitud(xs)` → `[]`. `n <= 0` lanza `ErrorDeValor`.
- `pares_consecutivos(xs)` — atajo para `ventana(xs, 2)`.
- `agrupar_consecutivos(xs)` — `[[clave, sub-lista], ...]` por igualdad de adyacentes. Base para run-length encoding.
- `comprimir(xs, selectores)` — `xs[i]` cuando `selectores[i]` es verdadero. Hasta agotar la lista más corta.
- `dividir_en(xs, n)` — particiones consecutivas de tamaño `n`; la última puede ser más corta.

### Por qué eager y no lazy

Todas las funciones devuelven `lista`, no generadores. Decisión consciente
para muestras pequeñas (anagramas, combinaciones de pocos elementos) — el
caso 99%. Para casos masivos (`producto_repeticion([0,1], 30)` son mil
millones de tuplas), el usuario puede escribir un `funcion*` con `dar`
directamente. No queremos forzar la abstracción para el caso pedagógico
común.

### Tests y ejemplo

- `tests/unit/test_bytecode_iteradores_stdlib.c` — 21 bloques, 30+ asserts. Cardinales conocidos (`2x3=6`, `2^4=16`, `3!=6`, `C(4,2)=6`, `P(4,2)=12`), primer/último elemento por orden lexicográfico, casos límite (`r=0`, `r>n`, lista vacía, n=0 lanza). Nombre con sufijo `_stdlib` porque ya existía `test_bytecode_iteradores.c` para el dunder `__iterar__`.
- `examples/104_iteradores.cor` — 8 secciones: combinaciones prenda+color, anagramas, equipos de 2, claves binarias de 4 bits, media móvil de 3 días con `funcionales.suma`, run-length encoding (`aaabbcdddde → a3b2c1d4e1`), paginación en bloques, filtro por máscara.

Suite total: **299 tests, 100% verde**.

## [1.117.0] — 2026-06-04 — Stdlib `estadisticas`

Nuevo módulo `stdlib/estadisticas.cor` con análisis estadístico
descriptivo e inferencial básico sobre listas de números, en
pure-Cornamusa. Reusa `funcionales` para agregaciones, `matematicas`
para `raiz`/`ln`/`exp`/`suelo`/`techo`, y `coleccion.Contador` para
las modas — es decir, no toca el runtime ni añade built-ins. Es el
módulo `statistics` de Python pero en castellano.

### Medidas de centralidad

- `media(xs)` — promedio aritmético.
- `mediana(xs)` — valor central; promedio de los dos centrales si `n` es par.
- `mediana_baja(xs)` / `mediana_alta(xs)` — el menor / mayor de los dos centrales en listas pares. En impares coinciden con `mediana`.
- `moda(xs)` — valor más frecuente (Contador interno; primero en empate).
- `multimodal(xs)` — lista de todas las modas en caso de empate.
- `media_armonica(xs)` — `n / Σ(1/xi)`. Útil para promediar tasas y velocidades. Lanza si algún `xi <= 0`.
- `media_geometrica(xs)` — `(Π xi)^(1/n)`, calculada en espacio log para evitar overflow.

### Medidas de dispersión

- `varianza(xs)` — muestral (denominador `n-1`, estimador insesgado). Lanza si `n < 2`.
- `varianza_pob(xs)` — poblacional (denominador `n`).
- `desviacion(xs)` / `desviacion_pob(xs)` — raíces de las anteriores.
- `amplitud(xs)` — `max - min`. Llamada así porque `rango` ya es built-in para iteración numérica.

### Medidas de posición

- `percentil(xs, p)` — interpolación lineal estilo numpy/Python. `0` → mín, `100` → máx, `50` → mediana.
- `cuartiles(xs)` → `[Q1, Q2, Q3]`.

### Relación entre dos series

- `covarianza(xs, ys)` — muestral.
- `correlacion(xs, ys)` — Pearson, valor en `[-1, 1]`.
- `regresion_lineal(xs, ys)` → `{"pendiente": m, "intercepto": b}` por mínimos cuadrados.

### Resumen rápido

- `resumen(xs)` → `dict` con `n`, `min`, `max`, `media`, `mediana`, `Q1`, `Q3`, `desviacion`. Útil para REPL e inspección.

### Por qué `amplitud` y no `rango`

`rango(a, b)` es built-in para iteración numérica (`para i en rango(0, 10):`).
Sobrecargarlo con la semántica estadística de `max - min` rompería todos
los bucles. `amplitud` es término aceptado en estadística castellana.

### Tests y ejemplo

- `tests/unit/test_bytecode_estadisticas.c` — 19 bloques, 25+ asserts. Cubre cada función + 3 casos de error (lista vacía, `n=1` en varianza muestral, valores `<= 0` en media armónica).
- `examples/103_estadisticas.cor` — 6 secciones: notas de examen, cuartiles con outliers, correlación estudio↔nota + regresión, muestra `azar.normal` con estimación de parámetros, multimodal sobre dado, resumen completo como dict.

Suite total: **297 tests, 100% verde**.

## [1.116.0] — 2026-06-03 — Stdlib `coleccion` extendida: `Heap` + `Contador`

Dos estructuras de datos clásicas que faltaban en `stdlib/coleccion.cor`,
implementadas en pure-Cornamusa sobre listas y diccionarios nativos.
Cierran huecos visibles cada vez que se quería hacer un top-N por
frecuencia o un priority queue.

### `Heap` — min-heap binario

Operaciones `poner(x)`/`sacar()` en O(log n), `vista()` en O(1). Por
defecto extrae el menor; para max-heap, insertar valores negados.

```cornamusa
importar coleccion

h = coleccion.Heap()
para x en [5, 3, 8, 1, 9, 2, 7]:
    h.poner(x)
fin para

mientras no h.vacia():
    imprimir(h.sacar())   # 1, 2, 3, 5, 7, 8, 9
fin mientras
```

API completa: `poner`, `sacar`, `vista`, `vacia`, `__longitud__`,
`__cadena__`. Implementación binaria estándar con `_subir`/`_bajar`
sobre lista; sin overhead de wrapper-de-nodo.

**Limitación documentada**: solo valores comparables con `<` nativamente
(números y cadenas). Para priority queues con tuplas, usar dos
estructuras separadas (heap de prioridades + dict de payloads).

### `Contador` — multiset estilo Counter

Cuenta apariciones de cada valor hashable. API inspirada en
`collections.Counter` de Python: `incrementar`, `decrementar`,
`obtener` (con defecto 0), `mas_comunes(n)`, `total`, `items`.

```cornamusa
importar coleccion

c = coleccion.Contador(["sol", "luna", "sol", "estrella", "sol"])
imprimir(c.obtener("sol"))      # 3
imprimir(c.mas_comunes(2))      # [["sol", 3], ["luna", 1]]
imprimir(c.total())             # 5
```

`decrementar` elimina la entrada cuando llega a 0 — el multiset no
queda con valores espurios. Acepta inicialización con lista para el
patrón frecuente "contar frecuencias de tokens".

### Por qué `Heap.poner` en lugar de `Heap.agregar`

`agregar` es built-in nativo (`agregar(lista, x)`). Si la clase
define un método con el mismo nombre, la resolución `agregar(yo._items, x)`
dentro del método se ambigua: en Cornamusa, los nombres globales se
resuelven antes que los locales-de-clase y por consistencia con
`Pila.poner`/`Cola.poner`/`ColaDoble.poner_*` se eligió `poner`.

### Tests y cookbook

- `tests/unit/test_bytecode_heap_contador.c` — 11 bloques, 16 asserts
  (Heap orden asc, vista no consume, cadenas lexicográfico, vacío
  lanza, longitud; Contador incrementos, total, mas_comunes,
  decrementar elimina, init con lista, items iter).
- `examples/102_heap_contador.cor` — 7 secciones: heap sort,
  top-N menores, heap de cadenas, contador básico, frecuencia de
  letras, histograma textual, top-3 palabras con split.

Suite total: **295 tests, 100% verde** (sin regresión).

## [1.115.0] — 2026-06-03 — Cookbook ampliado a 25 recetas

Cinco recetas nuevas validadas contra el intérprete usando
features de v1.107-v1.114 que no tenían pattern documentado. El
cookbook pasa de 20 a **25 recetas**. Release de pedagogía pura
— sin cambios en runtime ni stdlib. Refleja que las últimas 8
releases (typo sugg, sistema completo, OOP setter, distribuciones,
FS modificadores, f-string debug, walrus, anotaciones) añadieron
features útiles que ahora tienen pattern documentado.

### Recetas nuevas (21-25)

| # | Receta | Features que combina |
|---|---|---|
| 21 | Pipeline con walrus operator | `(item := f()) != "STOP"` (v1.113), `(n := longitud()) > 5` |
| 22 | Print debugging con `f"{x=}"` | f-string debug (v1.112) + format specs (v1.45) |
| 23 | Sandbox temporal con cleanup garantizado | `sistema.usuario/directorio_temp` (v1.108) + `crear_arbol`/`eliminar_arbol` (v1.102) + `tiempo.epoch_ms` (v1.73) + `intentar/atrapar` con re-lanzar |
| 24 | Clase como contrato (anotaciones) | Anotaciones en parámetros/retorno/variables (v1.114) + clase con métodos públicos + `f"{x=}"` para debug |
| 25 | Propiedad de solo lectura con `@propiedad` | `@propiedad`/`@escritor` (v1.78/v1.109) + `ErrorDeAtributo` atrapable + validación en setter |

### Receta destacada: 21 (walrus pipeline)

Muestra el contraste antes/después:

```cornamusa
# Antes (verboso):
i = 0
mientras i < longitud(fuente):
    item = fuente[i]
    si item == "STOP": romper fin si
    procesar(item)
    i = i + 1
fin mientras

# Con walrus (v1.113):
i = 0
mientras (item := fuente[i]) != "STOP":
    procesar(item)
    i = i + 1
fin mientras
```

Plus el equivalente con `si (n := f()) > 5:` y la limitación
documentada (la variable destino debe pre-existir si va a usarse
en un bucle, mismo bug v0.11.5).

### Receta destacada: 23 (sandbox temporal)

Helper `en_sandbox(prefijo, accion)` que crea directorio temporal
único con `usuario@timestamp`, ejecuta callback, y limpia
SIEMPRE incluso si lanza excepción. Patrón clásico para tests
aislados y procesamiento sin contaminar el FS.

### Receta destacada: 25 (propiedad solo lectura)

Clase `Rectangulo` con `base`/`altura` (read+write con
`@escritor` y validación negativos) y `area`/`perimetro` (solo
lectura sin `@escritor`). Asignar a `area` lanza
`ErrorDeAtributo` atrapable.

### Validación

Las 5 recetas se ejecutaron en `cornamusa --bytecode` durante la
redacción. Los outputs mostrados son los reales del intérprete:

- Receta 22: `precio=100, porcentaje=15, descuento=15.0, final=85.0`
- Receta 23: `Sandbox: C:/Users/.../ejemplo_david_1780497252091`
- Receta 25: `r.base=5, r.altura=3, r.area=15, r.perimetro=16`

### Sin cambios de código

293 tests verde sin cambios desde v1.114. Bump por convención de
release: el cookbook es parte del proyecto y merece versión propia
cuando crece.

### Archivos

- `docs/cookbook.md` — 5 secciones nuevas + índice actualizado.
  Pasó de ~720 líneas a ~990.
- `README.md`, `docs/introduccion.md`: entrada de release.

### Estado

293 tests verde, lint+fmt limpios. **Cookbook: 25 recetas
validadas contra el intérprete real.**

---

## [1.114.0] — 2026-06-03 — Tipos opcionales sintácticos (PEP 484-style)

Cornamusa permite anotaciones de tipo en parámetros, retornos y
asignaciones de variables, sin verificación runtime. Útil para
documentación, contratos legibles entre módulos y futuras
herramientas externas (linter de tipos, generadores de docs).

```cornamusa
# Parametros y retorno
funcion sumar(a: entero, b: entero = 0) -> entero:
    retornar a + b
fin funcion

# Variables top-level y locales
nombre: cadena = "Ana"
edades: lista = [25, 30, 35]

funcion procesar(items: lista) -> nulo:
    total: entero = 0
    para item en items:
        total = total + item
    fin para
    imprimir(total)
fin funcion
```

### Lo que ya estaba

**Parámetros y retorno ya se parseaban** desde versiones previas
— el parser de `funcion` aceptaba `nombre: tipo` para cada
parámetro (guardado en `Parametro.anotacion_tipo`) y `-> tipo`
para el retorno. El token `TT_FLECHA` para `->` ya existía. El
problema: no estaba documentado, no había tests dedicados, y los
ejemplos del repo nunca usaban anotaciones.

v1.114 **documenta** la feature, **añade tests** para prevenir
regresiones, y **completa el set** con anotaciones en
asignaciones de variables (la pieza que faltaba).

### Lo nuevo: anotaciones en asignaciones

```cornamusa
# Top-level
config: diccionario = {"modo": "produccion"}

# Locales dentro de funcion
funcion calcular():
    contador: entero = 0
    para i en rango(10):
        contador = contador + i
    fin para
    retornar contador
fin funcion
```

**Implementación** (`src/parser.c`, función
`parsear_asignar_o_expr`):

```c
if (primero->tipo == EXPR_IDENT && check(p, TT_DOS_PUNTOS)) {
    avanzar(p);  /* consume ':' */
    Expr *anot = parser_parsear_expr(p);
    if (anot == NULL) return NULL;
    /* anot se ignora; debe seguir un '=' con valor. */
    if (!consumir(p, TT_ASIGNAR, ...)) return NULL;
    Expr *valor = parser_parsear_expr(p);
    return sent_asignar(p->arena, primero, valor, linea, col);
}
```

Detección: si `primero` (el destino) es un `EXPR_IDENT` puro y el
siguiente token es `:`, parsear anotación y descartar. Después
exigir `=` y continuar como asignación normal.

### Filosofía: sin verificación runtime

Las anotaciones son **documentación con sintaxis**. El compilador
las descarta tras parsear:

- `x: entero = "cadena"` NO falla en runtime.
- `funcion f(x: TipoQueNoExiste)` parsea aunque `TipoQueNoExiste`
  no esté definido.
- `tipo(x)` devuelve el tipo real, no el anotado.

Mismo enfoque que Python con type hints. Permite que las
herramientas externas (linter de tipos al estilo `mypy`,
generadores de docs, IDEs) lean las anotaciones sin imponer
overhead runtime.

### Limitaciones declaradas

- **No anotación en atributos**: `yo.x: tipo = ...` NO parsea.
  Necesitaría extender el parser de `EXPR_ATRIBUTO`.
- **No anotación en destructuring**: `a: tipo, b: tipo = par` NO
  parsea. La detección de anotación es solo para IDENT puro
  pre-coma.
- **Anotaciones son cualquier expresión**: el parser solo verifica
  que sea una expresión válida. `x: lista[entero, cadena]` parsea
  aunque `lista[entero, cadena]` no tenga sentido como expresión
  ejecutable.

### Tests

15 asserts en `test_bytecode_anotaciones.c`:

- Anotaciones en parámetros (básico, con default, mixto).
- Retorno `-> tipo`.
- Anotaciones en variables top-level y locales.
- Tipos compuestos como anotación (`lista`, `diccionario`).
- **No verificación runtime**: `x: entero = "cadena"` no falla.
- Función completa con todos los elementos anotados.
- Compatibilidad: código sin anotaciones sigue compilando.
- Anotación como identificador en posición de tipo (incluso usando
  nombre de función built-in como `longitud` — pasa porque es
  solo documentación).

### Ejemplo

`examples/101_anotaciones.cor` con 7 secciones:

1. Funciones anotadas (`saludar`, `contar_letras`, `sumar`).
2. Con valores por defecto.
3. Variables anotadas (5 tipos distintos).
4. Mezcla parcial.
5. **Estructura tipo data class** (clase `Punto` con anotaciones).
6. Demostración de NO-verificación runtime.
7. **Patrón contrato legible** — clase `Cuenta` con métodos
   completamente anotados como documentación pública.

### Archivos

- `src/parser.c` — detección de anotación en
  `parsear_asignar_o_expr` (~15 líneas nuevas).
- `tests/unit/test_bytecode_anotaciones.c` — 10 bloques,
  15 asserts.
- `examples/101_anotaciones.cor` — 7 secciones demo.
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación nueva de la feature completa.

### Estado

293 tests verde, lint+fmt limpios. Compatibilidad total con
código pre-v1.114.

---

## [1.113.0] — 2026-06-03 — Walrus operator `:=` (asignación como expresión)

Añade el walrus operator de Python (PEP 572): `nombre := valor`
es una **expresión** que asigna `valor` a `nombre` y deja el
valor en stack. Permite patrones como `si (n := f()) > 0:` y
`mientras (item := siguiente()) != nulo:`.

```cornamusa
# Llamar UNA vez y usar el valor en condicion + cuerpo
xs = [1, 2, 3, 4, 5, 6, 7, 8]
si (n := longitud(xs)) > 5:
    imprimir(f"lista grande con {n} elementos")
fin si

# Patron procesador con sentinela
fuente = ["dato1", "dato2", "STOP"]
i = 0
mientras (item := fuente[i]) != "STOP":
    procesar(item)
    i = i + 1
fin mientras

# Validacion con conversion en una linea
si (edad := entero(input_str)) < 0 o edad > 150:
    lanzar ErrorDeValor(f"{edad=} fuera de rango")
fin si
```

### Sintaxis

Solo identificador como destino: `nombre := valor`. No se admite
`obj.x := v` ni `xs[0] := v` (atributos e índices usan `=`
normal). El parser detecta `:=` como infix tras un IDENT,
asociativo a la derecha por el recursive descent que delega en
`parser_parsear_expr`.

### Cambios

**Lexer** (`src/lexer.c` + `src/lexer.h`):
- Nuevo token `TT_WALRUS` para `:=`.
- En el case de `:`, mirar el siguiente char con `coincidir('=')`.

**AST** (`src/ast.h` + `src/ast.c`):
- Nuevo `EXPR_WALRUS` con `{nombre, longitud, valor}`.
- Constructor `expr_walrus()`.
- Dump `(walrus nombre valor)` para `--ast`.

**Parser** (`src/parser.c`):
- En `parsear_ident`, si el siguiente token es `TT_WALRUS`,
  consumir y parsear el lado derecho como expresión completa.
  Devolver `EXPR_WALRUS` en lugar de `EXPR_IDENT`.

**Compilador** (`src/compilador.c`):
- Nuevo caso `EXPR_WALRUS`:
  1. Compilar el valor → stack `[v]`.
  2. Si la variable es local existente: `OP_DUP` + `OP_ASIGNAR_LOCAL`.
  3. Si es upvalue: `OP_DUP` + `OP_ASIGNAR_UPVALUE`.
  4. Si es nuevo local en función: `agregar_local` (el valor en TOS
     ya es el slot del nuevo local) + `OP_DUP` para empujar la copia.
  5. Si top-level: `OP_DUP` + `OP_DEFINIR_GLOBAL`.

El resultado en todos los casos: el valor queda en stack como
resultado de la expresión walrus.

### Limitación documentada

Dentro de bucles, crear una variable **nueva** con `:=` falla
porque el slot del compilador se fija en la primera iteración
(mismo bug que v0.11.5 que motivó la solución `OP_NULO +
agregar_local` para `si` en v1.95).

**Workaround**: si la variable es nueva y va a usarse con walrus
en un loop, pre-declararla antes del bucle:

```cornamusa
# BUG potencial:
mientras (tmp := f()) > 0:    # tmp se "fija" en iter 1
    ...
fin mientras

# Solucion: pre-declarar
tmp = 0
mientras (tmp := f()) > 0:    # OK: tmp ya existe
    ...
fin mientras
```

Un fix futuro podría aplicar la misma técnica de v1.95
(pre-pass que recolecta nuevos locales y emite `OP_NULO +
agregar_local` antes del bucle).

### Tests

12 asserts en `test_bytecode_walrus.c`:

- Walrus top-level crea global.
- Valor de la expresión walrus se usa en aritmética.
- Walrus en condición de `si`.
- Walrus en condición de `mientras` (variable existente).
- Walrus dentro de función: nuevo local + reasignar existente.
- Walrus con expresión compuesta (`longitud(lista)`).
- Walrus anidado en aritmética: `3 + (x := 10) * 2 == 23`.
- Walrus con cadena.
- Dict literal `{"clave": 42}` NO se rompe con la nueva token.
- Código sin walrus sigue compilando.

### Ejemplo

`examples/100_walrus.cor` con 8 secciones:

1. Idea básica.
2. En condiciones (caso clásico, evita doble cálculo).
3. En bucles con sentinela.
4. Dentro de funciones (`validar_edad` con conversión y rangos).
5. Cache temporal de cálculo costoso.
6. Anidación en aritmética.
7. Limitación documentada (variable nueva en loop).
8. Combinación con f-string debug (`f"{datos[0]=}"`) — v1.112 + v1.113.

### Archivos

- `src/lexer.c`/`src/lexer.h` — `TT_WALRUS`.
- `src/ast.h`/`src/ast.c` — `EXPR_WALRUS` + `expr_walrus()` +
  dump.
- `src/parser.c` — detección en `parsear_ident`.
- `src/compilador.c` — caso `EXPR_WALRUS` con `OP_DUP` +
  asignación.
- `tests/unit/test_bytecode_walrus.c` — 11 bloques, 12 asserts.
- `examples/100_walrus.cor` — 8 secciones demo.
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

291 tests verde, lint+fmt limpios. Compatibilidad total con
código pre-v1.113 (TT_WALRUS solo se genera con `:=` literal).

---

## [1.112.0] — 2026-06-03 — F-string debug format `f"{x=}"`

Añade el patrón de print-debugging conciso de Python 3.8: sufijo
`=` dentro de una interpolación emite la expresión textual + `=` +
valor. Sin ambigüedad con operadores `==`/`!=`/`<=`/`>=`.

```cornamusa
x = 5
imprimir(f"{x=}")            # "x=5"
imprimir(f"{x*2=}")          # "x*2=10"
imprimir(f"{x = }")          # "x = 5"   (espacios preservados)
imprimir(f"{x=:>5}")         # "x=    5" (combinable con spec v1.45)

# Multiple debugs en una linea
a, b = 1, 2
imprimir(f"{a=}, {b=}, {a+b=}")    # "a=1, b=2, a+b=3"
```

### Antes vs después

```cornamusa
# Antes (v1.45-v1.111):
imprimir(f"precio={precio}, descuento={descuento}, final={final}")

# Ahora (v1.112):
imprimir(f"{precio=}, {descuento=}, {final=}")
```

Reduce muchísimo la verborrea típica de print-debugging.
Especialmente útil dentro de funciones donde se quieren imprimir
varios valores intermedios.

### Detección de `=` sin ambigüedad

El parser detecta `=` como debug **solo** si el carácter no está
precedido por `=`, `!`, `<` o `>`. Esto evita falsos positivos:

```cornamusa
f"{a == b}"   # operador == → comparación, NO debug
f"{a != b}"   # operador != → comparación
f"{a <= b}"   # operador <= → comparación
f"{a >= b}"   # operador >= → comparación
```

Compatible 100% con código pre-v1.112: cualquier f-string que ya
funcionaba sigue funcionando idéntico. La detección es puramente
sintáctica al final de la expresión, antes de `}` o `:`.

### Implementación

**`src/ast.h`** — `ParteFCadena` extendida con `debug_texto` y
`debug_longitud`. NULL si no es debug.

**`src/parser.c`** — tras extraer la expresión, escanear hacia
atrás desde el final saltando espacios. Si el último carácter
no-blanco es `=` y no es operador, marcar como debug y guardar el
slice completo (con espacios preservados).

**`src/compilador.c`** — antes de compilar `p->expr`, si es debug,
emitir el `debug_texto` como literal en stack. Tras formatear el
valor (`OP_FORMATO_F` o `OP_FORMATO_F_SPEC`), `OP_SUMAR` para
fusionar `"expr=" + valor` en un solo string. El OP_SUMAR exterior
del bucle concatena este resultado al acumulado.

**`src/evaluador.c`** — actualizado para coherencia con
tree-walking (excepto specs que están congelados en v0.5).

### Espacios preservados

El texto debug se copia literalmente desde el código fuente. Si
hay espacios entre la expresión, el `=` y el cierre `}`, se
preservan:

| Código | Output |
|---|---|
| `f"{x=}"` | `"x=5"` |
| `f"{x =}"` | `"x =5"` |
| `f"{x= }"` | `"x= 5"` |
| `f"{x = }"` | `"x = 5"` |

### Combinación con format specs

`f"{x=:>5}"` aplica el spec `>5` (alineación derecha, ancho 5) al
**valor**, no al debug literal:

```cornamusa
x = 5
n = 10
imprimir(f"{x=:>5}")   # "x=    5"
imprimir(f"{n=:0>4}")  # "n=0010"
```

Coherente con Python.

### Tests

12 asserts en `test_bytecode_fstring_debug.c`:

- Básico `f"{x=}"`.
- Preservación de espacios (3 variantes).
- Expresión compuesta `f"{x*2=}"`.
- Debug + spec `f"{x=:>5}"`.
- Múltiples debugs en una f-string.
- Mezcla debug + interpolación normal + literal.
- Operadores `==`/`!=`/`<=`/`>=` NO emiten literal (no son debug).
- Debug con cadena, atributo, función `longitud`.
- Compatibilidad: f-strings sin `=` siguen funcionando.

### Ejemplo

`examples/99_fstring_debug.cor` con 8 secciones que cubren todos
los patrones: básico, espacios, spec, múltiples, operadores, debug
de función real (`calcular_descuento`), atributos/índices, mezcla
con literales.

### Archivos

- `src/ast.h` — `ParteFCadena.debug_texto` y `debug_longitud`.
- `src/parser.c` — detección del `=` debug + verificación de
  no-operador.
- `src/compilador.c` — emisión del literal `"expr="` antes del
  valor + `OP_SUMAR` para fusionar.
- `src/evaluador.c` — coherencia con tree-walking.
- `tests/unit/test_bytecode_fstring_debug.c` — 12 bloques.
- `examples/99_fstring_debug.cor` — 8 secciones demo.
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

289 tests verde, lint+fmt limpios. Compatibilidad total con
código pre-v1.112.

---

## [1.111.0] — 2026-05-24 — FS modificadores: `archivo_mover`, `set_mtime`, `tocar`

Cierra los pendientes declarados en CHANGELOG de v1.105:
**`archivo_mover`** (rename atómico) y **`archivo_set_mtime`**
(touch / restaurar mtime). Con esto el módulo FS tiene el ciclo
completo de operaciones para cualquier script de mantenimiento:

```
crear   → crear_directorio (v1.97), crear_arbol (v1.102), escribir (v1.8)
leer    → leer, lineas (v1.8), info (v1.99), listar/recorrer (v1.97/v1.100)
copiar  → copiar (v1.105), copiar_arbol (v1.105)
mover   → mover (v1.111)                    ← NUEVO
mtime   → set_mtime, tocar (v1.111)         ← NUEVO
borrar  → eliminar (v1.99), eliminar_arbol (v1.102)
```

```cornamusa
importar archivos
importar ruta

# Rename atomico
archivos.mover("borrador.txt", "publicado.txt")

# touch: actualizar mtime al ahora
archivos.tocar("hito.txt")

# Restaurar mtime preciso (preservar al copiar)
archivos.copiar("orig.dat", "back.dat")
archivos.set_mtime("back.dat", archivos.info("orig.dat")["mtime_epoch_ms"])

# Via Ruta encadenable
nueva = ruta.Ruta("origen").mover("destino")
nueva.set_mtime(1577836800000)   # 2020-01-01
```

### Nativas C nuevas

| Nativa | Devuelve | Implementación |
|---|---|---|
| `archivo_mover(orig, dest)` | `nulo` | `rename()` POSIX, `MoveFileExA` con `MOVEFILE_REPLACE_EXISTING` Windows |
| `archivo_set_mtime(ruta, ms)` | `nulo` | `utimes()` POSIX, `SetFileTime()` Windows con conversión epoch |

Ambas lanzan `ErrorDeIO` si la operación falla (origen inexistente,
destino no escribible, etc.).

**Sobrescritura coherente entre plataformas**: en POSIX `rename()`
sobrescribe destino si existe. En Windows, `rename()` falla si el
destino existe; por eso usamos `MoveFileExA` con
`MOVEFILE_REPLACE_EXISTING` para mantener la misma semántica.

**Conversión epoch Windows**: `SetFileTime` usa `FILETIME` =
intervalos de 100ns desde 1601-01-01 UTC. La conversión:
`(mtime_ms * 10000) + 116444736000000000` (diferencia 1601 → 1970
en intervalos de 100ns).

**Preservar atime en POSIX**: `utimes` cambia atime y mtime
juntos. Para preservar el atime original leemos primero con
`stat` y se lo pasamos de vuelta.

### Wrappers en `stdlib/archivos`

```cornamusa
funcion mover(origen, destino):
    archivo_mover(origen, destino)
fin funcion

funcion set_mtime(ruta, mtime_ms):
    archivo_set_mtime(ruta, mtime_ms)
fin funcion

funcion tocar(ruta):
    importar tiempo
    archivo_set_mtime(ruta, tiempo.epoch_ms())
fin funcion
```

`tocar` compone `set_mtime` con `tiempo.epoch_ms` (v1.73): touch
de Unix sin código duplicado.

### Métodos en `Ruta`

- `r.mover(destino)` → devuelve `Ruta(destino)` para encadenar.
  `destino` puede ser cadena o `Ruta`.
- `r.tocar()` → atajo de `archivos.tocar(r.s)`.
- `r.set_mtime(ms)` → atajo de `archivos.set_mtime(r.s, ms)`.

### Patrones documentados (ejemplo 98)

1. **Renombrar simple** con verificación de existencia.
2. **Promoción staging → publicado** entre directorios (atómica
   en mismo FS).
3. **Touch** para actualizar mtime al ahora.
4. **Rotación de logs**: `app.log` → `app.log.1`, crear nuevo
   `app.log` vacío.
5. **Preservar mtime al copiar**: copiar + `set_mtime` con el
   mtime del origen. La copia simple no preserva mtime, pero
   esta composición sí.
6. **Encadenamiento via `Ruta`**.

### Tests

22 asserts en `test_bytecode_mover.c`:

- `archivo_mover`: origen desaparece, destino aparece, contenido
  preservado.
- Sobrescritura: mover sobre destino existente reemplaza.
- Origen inexistente lanza `ErrorDeIO`.
- `archivo_set_mtime`: timestamp 2020-01-01 (1577836800000 ms) se
  lee correcto con `archivo_info`.
- `set_mtime` sobre inexistente lanza `ErrorDeIO`.
- Wrappers `archivos.mover/set_mtime/tocar`.
- Métodos `Ruta.mover/set_mtime/tocar`.
- Mover entre directorios (mismo FS).

### Lo que sigue pendiente del set FS

- Symlinks (lstat, readlink, crear).
- Watch (notificaciones de cambio).
- `archivo_chmod` (permisos POSIX / atributos Windows).
- Cross-FS atomic rename con fallback automático a copiar+borrar
  (hoy `MoveFileExA` lo gestiona en Windows; POSIX `rename()`
  retorna `EXDEV` para cross-FS y habría que detectarlo).

### Archivos

- `src/nativos.c` — 2 nativas nuevas (~200 líneas C con `#ifdef`
  POSIX/Windows, incluye conversión epoch).
- `stdlib/archivos.cor` — `mover`, `tocar`, `set_mtime`.
- `stdlib/ruta.cor` — 3 métodos nuevos en clase `Ruta`.
- `tests/unit/test_bytecode_mover.c` — 8 bloques, 22 asserts.
- `examples/98_mover_tocar.cor` — 6 secciones (incluye rotación
  de logs y preservación de mtime al copiar).
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

287 tests verde, lint+fmt limpios. **Ciclo FS completo**: crear,
leer, escribir, listar/info, copiar, **mover**, **tocar mtime**,
borrar.

---

## [1.110.0] — 2026-05-24 — Distribuciones azar + constantes mat (TAU/INF/NaN)

Cierra el pendiente declarado en CHANGELOG de v1.103: tres
distribuciones adicionales en `stdlib/azar` (exponencial,
binomial, Poisson) plus constantes especiales en
`stdlib/matematicas` (TAU, INFINITO, NO_NUMERO) y predicados para
detectar inf/NaN. Release pequeña tras dos grandes (v1.108
sistema completo, v1.109 deuda OOP).

```cornamusa
importar azar
importar matematicas

# Distribuciones nuevas
azar.exponencial(2.0)        # tiempo hasta evento, media 0.5
azar.binomial(100, 0.3)      # exitos de 100 ensayos Bernoulli
azar.poisson(5)              # eventos por intervalo, media 5

# Constantes especiales
matematicas.TAU              # 2*PI
matematicas.INFINITO         # decimal infinito positivo
matematicas.NO_NUMERO        # NaN

# Predicados (NaN != NaN, hay que usar el predicado)
matematicas.es_no_numero(matematicas.NO_NUMERO)   # verdadero
matematicas.es_infinito(matematicas.INFINITO)     # verdadero
matematicas.es_finito(5)                          # verdadero
```

### Nativas C nuevas (4)

| Nativa | Devuelve | Notas |
|---|---|---|
| `mat_infinito()` | decimal `+inf` | `INFINITY` de `<math.h>` |
| `mat_no_numero()` | decimal `NaN` | `NAN` de `<math.h>` |
| `mat_es_infinito(x)` | booleano | `isinf(x)` |
| `mat_es_no_numero(x)` | booleano | `isnan(x)` |

Necesarias porque en Cornamusa `1.0 / 0.0` lanza
`ErrorAritmetico` (no devuelve infinito). Estas son la única forma
de obtener inf/NaN reales.

### Constantes nuevas en `stdlib/matematicas.cor`

```cornamusa
TAU = 6.283185307179586    # 2 * PI
INFINITO = mat_infinito()
NO_NUMERO = mat_no_numero()
```

TAU es conveniente para ángulos completos (una vuelta = TAU
radianes) — recomendado por la "Tau Manifesto". PI sigue
existiendo para retrocompatibilidad y casos típicos.

### Wrappers de predicados

```cornamusa
funcion es_infinito(x): retornar mat_es_infinito(x) fin
funcion es_no_numero(x): retornar mat_es_no_numero(x) fin
funcion es_finito(x):
    retornar no mat_es_infinito(x) y no mat_es_no_numero(x)
fin
```

**IEEE 754**: `NaN != NaN`. Una comparación directa como
`x == NO_NUMERO` SIEMPRE es falsa, incluso si `x` ES NaN. La única
forma correcta de detectar NaN es `es_no_numero(x)`. Igual con
infinito: aunque `INFINITO == INFINITO` sí es verdadero, usar
`es_infinito(x)` también captura `-infinito` y es más legible.

### Distribuciones en `stdlib/azar.cor`

**`exponencial(tasa)`** — pure-Cornamusa, ~10 líneas.

Modelo: tiempo hasta el próximo evento en un proceso Poisson con
tasa `lambda`. Media = `1/tasa`. Soporte `[0, +inf)`.

Implementación: inversa de la CDF acumulada:
`X = -ln(U)/lambda` donde `U ~ Uniform(0, 1)`. La protección
contra `U=0` (que daría `ln(0) = -inf`) hace re-sortear.

**`binomial(n, p)`** — pure-Cornamusa, ~15 líneas.

Modelo: número de éxitos en `n` ensayos Bernoulli independientes
con probabilidad `p` por éxito. Soporte `{0, 1, ..., n}`,
media = `n*p`.

Implementación: cuenta éxitos sumando `n` Bernoulli(p). O(n).
Para n muy grande (>10000) sería mejor usar aproximación normal
o Poisson, pero la implementación directa es suficiente para
casos comunes y mantiene legibilidad.

**`poisson(media)`** — pure-Cornamusa, ~15 líneas.

Modelo: número de eventos en intervalo fijo. Media = `lambda`,
soporte `{0, 1, 2, ...}` (en teoría sin tope).

Implementación: **algoritmo de Knuth**. Multiplicar uniformes
hasta que el producto baje del umbral `e^-lambda`. Performance
buena para `media <= 30`; para valores mayores tendría sentido
usar PTRS (más complejo) o aproximación normal (más sencilla),
pero los casos con media > 30 son raros en simulaciones típicas.

### Verificación estadística

Cada distribución tiene un test que comprueba que la media
empírica converge al valor teórico con tolerancia razonable
sobre 2000-5000 muestras y semilla fija:

| Distribución | Media teórica | Tolerancia |
|---|---|---|
| `exponencial(2)` | `0.5` | `0.05` |
| `binomial(100, 0.3)` | `30` | `0.5` |
| `poisson(7)` | `7` | `0.2` |

Plus casos borde: `binomial(n, 0) == 0` siempre, `binomial(n, 1) == n` siempre, `poisson(0) == 0` siempre.

### Tests

22 asserts en `test_bytecode_distribuciones.c`:

- Constantes: `TAU == 2*PI`, predicados sobre `INFINITO`/`NO_NUMERO`.
- `NaN != NaN` confirmado (regla IEEE 754).
- Tres distribuciones: media empírica converge a la teórica.
- `exponencial` siempre devuelve `>= 0`.
- `binomial(n, 0)` siempre `0`; `binomial(n, 1)` siempre `n`.
- `poisson(0)` siempre `0`.
- Errores: rechazos por parámetros inválidos lanzan `ErrorDeValor`.

### Ejemplo

`examples/97_distribuciones.cor` con 5 secciones:

1. Constantes especiales y predicados (`NaN != NaN`).
2. **Exponencial**: tiempos entre llamadas a un centro
   (`exponencial(3.0)` con media 1/3 min).
3. **Binomial**: aciertos en 10 partidas de 20 disparos con
   tirador al 75% (media teórica = 15).
4. **Poisson**: clientes que entran a una tienda 24 horas, media
   5/hora. Incluye **histograma horizontal ASCII** por hora.
5. **Simulación**: 1000 llegadas exponencial(2) ≈ 500s; comparar
   con `poisson(200)` para 100s del mismo proceso.

### Pendientes futuros

- Más distribuciones: gamma, beta, chi-cuadrado, log-normal.
- Eficiencia: PTRS para Poisson con media grande.
- Cachear segundo componente Box-Muller en `normal` (mitad de
  llamadas a libm).

### Archivos

- `src/nativos.c` — 4 nativas matemáticas nuevas (~70 líneas C).
- `stdlib/matematicas.cor` — `TAU`, `INFINITO`, `NO_NUMERO` +
  `es_infinito`, `es_no_numero`, `es_finito`.
- `stdlib/azar.cor` — `exponencial`, `binomial`, `poisson`
  (~50 líneas pure-Cornamusa).
- `tests/unit/test_bytecode_distribuciones.c` — 11 bloques,
  22 asserts.
- `examples/97_distribuciones.cor` — 5 secciones (incluye
  histograma ASCII Poisson + comparación exponencial/Poisson).
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

285 tests verde, lint+fmt limpios. Módulo `azar` completo para
modelado estadístico básico (uniforme, normal, exponencial,
binomial, Poisson) + módulo `matematicas` con constantes
especiales IEEE 754.

---

## [1.109.0] — 2026-05-24 — `@propiedad` con setter: cierra el modelo OOP

Cierra la **deuda OOP grande declarada desde v1.78**: ahora una
propiedad puede tener setter además de getter. Sintaxis
Python-style con un segundo método del mismo nombre decorado con
`@escritor`. Plus excepción `ErrorDeAtributo` atrapable.

```cornamusa
clase Termometro:
    funcion __iniciar__(yo):
        yo._c = 0
    fin funcion

    @propiedad
    funcion celsius(yo):
        retornar yo._c
    fin funcion

    @escritor
    funcion celsius(yo, c):
        si c < -273.15:
            lanzar ErrorDeValor("bajo cero absoluto")
        fin si
        yo._c = c
    fin funcion
fin clase

t = Termometro()
t.celsius = 100        # ejecuta el setter, valida
t.celsius              # 100 (via getter)
t.celsius = -300       # lanza ErrorDeValor
```

### Diseño: `@escritor` como decorator marker

El decorator `@escritor` crea una `Propiedad` con `getter == NULL`
y `setter == closure` — un **marker** que OP_METODO detecta y
**fusiona** con la propiedad ya guardada bajo el mismo nombre.

Razonamiento: Python usa `@x.setter` que requiere que `x` (el
objeto propiedad) sea accesible dentro del cuerpo de la clase para
acceder a su atributo `.setter`. En Cornamusa el cuerpo de clase
solo admite métodos (no asignaciones top-level ni acceso a
identificadores). Por eso `@escritor` se aplica sin argumentos y
el OP_METODO hace la fusión por nombre.

Patrón: declarar getter y setter como dos métodos con el mismo
nombre, decorados con `@propiedad` y `@escritor` respectivamente.
Idéntico ergonómicamente a Python.

### Cambios C

**`src/valor.h`**: `Propiedad` extendida con `Closure *setter`
(NULL si solo lectura). Helper `propiedad_vincular_setter`.

**`src/valor.c`**: `propiedad_nueva` inicializa `setter = NULL`.
`propiedad_liberar` libera setter si lo hay. Ambos closures
soportan NULL.

**`src/memoria.c`**: GC mark de `Propiedad` ahora marca también el
setter.

**`src/nativos.c`**: nueva nativa `escritor(fn)` que envuelve fn
en marker `Propiedad{getter=NULL, setter=fn}`. Nueva excepción
canónica `ErrorDeAtributo` (sigue el patrón de `DEFINIR_EXC_NATIVA`).

**`src/vm.c`** (dos cambios):

1. **OP_METODO**: si el valor a guardar es un marker `@escritor`
   (propiedad con getter=NULL pero setter!=NULL), busca la
   propiedad existente con ese nombre y la fusiona con
   `propiedad_vincular_setter`. Si no hay propiedad previa, error
   claro: `"@escritor requiere una @propiedad previa con el mismo
   nombre"`. Setea `clase_definicion` del setter para que `super`
   funcione correctamente.

2. **OP_ASIGNAR_ATRIBUTO**: peek antes de sacar — si la clase de
   la instancia tiene una `Propiedad` con ese nombre:
   - Con setter: `ejecutar_dunder_binario(vm, &frame, setter,
     "setter", 6)` — el dunder ya espera el stack `[obj, valor]`
     que tenemos. El setter ejecuta y su retorno (típicamente
     `nulo`) queda en TOS para que OP_DESCARTAR del compilador lo
     saque.
   - Sin setter: `VM_ERROR("ErrorDeAtributo: 'X' es una propiedad
     de solo lectura")` + `RAISE_OR_DIE()` para que sea atrapable.
   
   Si no hay propiedad: comportamiento estándar (asigna a
   `instancia->atributos`).

### Excepción nueva: `ErrorDeAtributo`

Atrapable con `atrapar ErrorDeAtributo como e:`. Se lanza desde
OP_ASIGNAR_ATRIBUTO cuando se intenta asignar a una propiedad de
solo lectura. Reservada para uso futuro en otros casos relacionados
con atributos.

### Pitfall encontrado durante el desarrollo

`ejecutar_dunder_binario` solo **prepara** el frame del setter; no
lo ejecuta inline. Tras `break`, el ciclo principal continúa
ejecutando el bytecode del setter. Mi código inicial sacaba un
`Valor ret` y empujaba `nulo` justo tras el dunder — pero eso
sacaba el closure del nuevo frame antes de ejecutarse,
corrompiendo el stack del setter. El setter llamaba con
`valor = nulo` y `yo` desplazado. Fix: no manipular el stack
después del dunder. El setter ya deja su retorno en TOS, y el
`OP_DESCARTAR` que el compilador emite tras `OP_ASIGNAR_ATRIBUTO`
lo limpia naturalmente.

### Tests

13 asserts en `test_bytecode_propiedad_setter.c`:

- Setter invocado: `c.x = 10` ejecuta el setter (verificado por
  efecto secundario: el setter duplica el valor).
- Validación: setter lanza `ErrorDeValor` y el valor NO se
  modifica.
- Propiedad sin setter: `ErrorDeAtributo` atrapable.
- `@escritor` sin `@propiedad` previa: error de compilación
  visible al ejecutar.
- Atributos normales no afectados (caso sin propiedad sigue
  funcionando).
- Setter puede leer otros atributos de `yo` (closure de yo).

### Ejemplo

`examples/96_propiedad_setter.cor` con 3 secciones:

1. **Validación en setter**: clase `Edad` rechaza valores fuera
   de rango y no-enteros con excepciones atrapables.
2. **Termómetro multi-propiedad**: `celsius`/`fahrenheit`/`kelvin`
   con getters y dos setters que componen. `kelvin` queda solo
   lectura sin `@escritor`.
3. **Cuenta bancaria con protección**: setter de `saldo` que
   **bloquea cualquier asignación directa** (`c.saldo = 999999`
   lanza), forzando el uso de `depositar/retirar` que mantienen
   historial. Patrón clásico de encapsulación.

### Estado del modelo de objetos

Tras v1.109 Cornamusa tiene paridad con Python 3.10 en OOP:

- ✓ Herencia simple (v0.7)
- ✓ Dunders aritméticos, comparación, contenedor, conversión (v1.2+)
- ✓ `__hash__` + `__igual__` (v1.42)
- ✓ `__siguiente__` iteradores lazy (v1.43)
- ✓ Decoradores (v1.72)
- ✓ `@propiedad` solo lectura (v1.78)
- ✓ `@estaticometodo` (v1.84) / `@clasemetodo` (v1.85)
- ✓ Atributos dinámicos: `tiene_atributo`/`obtener_atributo`/
  `asignar_atributo` (v1.86)
- ✓ `inspeccion` (v1.91)
- ✓ **`@propiedad` con setter (v1.109)**

Sin pendientes en OOP. Lo único que falta para "Python completo"
es múltiple herencia con MRO C3 — decisión de diseño deliberada
(complejidad innecesaria para un lenguaje pedagógico).

### Archivos

- `src/valor.h` / `valor.c` — `Propiedad.setter`,
  `propiedad_vincular_setter`.
- `src/memoria.c` — GC mark del setter.
- `src/nativos.c` — `nativa_escritor`, `nativa_exc_ErrorDeAtributo`
  + 2 entradas en la tabla NATIVAS.
- `src/vm.c` — OP_METODO con fusión + OP_ASIGNAR_ATRIBUTO con
  dispatch al setter.
- `tests/unit/test_bytecode_propiedad_setter.c` — 6 bloques,
  13 asserts.
- `examples/96_propiedad_setter.cor` — 3 secciones (validación,
  termómetro multi-prop, cuenta bancaria).
- `README.md`, `docs/introduccion.md`: entrada de release.

### Estado

283 tests verde, lint+fmt limpios. **Deuda OOP de v1.78
oficialmente cerrada.**

---

## [1.108.0] — 2026-05-23 — Sistema completo: usuario, host y directorio temporal

Cierra los tres huecos declarados como pendientes en el CHANGELOG
de v1.104. Ahora `stdlib/sistema` cubre el set completo de
identidad y entorno del proceso: argv, variables, home, **usuario,
host, dir temporal**.

```cornamusa
importar sistema

sistema.usuario()           # "david"
sistema.host()              # "MI-LAPTOP"
sistema.directorio_temp()   # "C:/Users/david/AppData/Local/Temp"
```

### Nativas C nuevas

| Nativa | Devuelve | Errores |
|---|---|---|
| `usuario_actual()` | cadena | `ErrorDeSistema` si no se determina |
| `hostname()` | cadena | `ErrorDeSistema` si la syscall falla |
| `directorio_temporal()` | cadena con separadores `/` | siempre devuelve algo (con fallback hardcodeado) |

Portabilidad:

- `usuario_actual`: `getenv("USER")` con fallback a `getenv("LOGNAME")` en POSIX; `getenv("USERNAME")` en Windows.
- `hostname`: `gethostname(buf, len)` POSIX (vía `<unistd.h>`); `GetComputerNameA(buf, &len)` Windows (vía `<windows.h>`).
- `directorio_temporal`: `TMPDIR` env con fallback a `/tmp` (POSIX); `TEMP`/`TMP` con fallback a `C:/Windows/Temp` (Windows). Normaliza `\` a `/` antes de devolver — coherente con `obtener_cwd` (v1.97) y `directorio_inicio` (v1.104).

### Wrappers en `stdlib/sistema.cor`

- `sistema.usuario()` → wrapper de `usuario_actual`.
- `sistema.host()` → wrapper de `hostname`.
- `sistema.directorio_temp()` → wrapper de `directorio_temporal`.

Nombres más cortos en la API de stdlib que en las nativas — sigue
el patrón de `sistema.inicio()` vs `directorio_inicio()`.

### Patrón documentado: sandbox temporal con cleanup

`examples/95_identidad_sistema.cor` introduce el helper
`en_sandbox(prefijo, accion)`:

```cornamusa
funcion en_sandbox(prefijo, accion):
    nombre = prefijo + "_" + sistema.usuario() + "_" +
             cadena(tiempo.epoch_ms())
    sandbox = ruta.Ruta(sistema.directorio_temp()).unir(nombre)
    archivos.crear_arbol(sandbox.cadena())
    error_cap = nulo
    intentar:
        accion(sandbox)
    atrapar Excepcion como e:
        error_cap = e
    fin intentar
    si sandbox.es_directorio():
        sandbox.eliminar_arbol()
    fin si
    si error_cap != nulo:
        lanzar error_cap
    fin si
fin funcion
```

Combina v1.108 (usuario/temp), v1.102 (`crear_arbol`/`eliminar_arbol`),
v1.73 (`tiempo.epoch_ms`) para crear directorios temporales únicos
con cleanup garantizado incluso si la acción lanza excepción.

### Tests

14 asserts en `test_bytecode_sistema_v108.c`:

- Cada nativa devuelve cadena no vacía.
- `directorio_temporal` tiene separadores `/` (no `\`).
- `directorio_temporal` apunta a un directorio existente
  (verificado con `archivo_es_directorio`).
- Wrappers `sistema.*` devuelven cadenas.
- Errores: pasar argumentos a funciones sin argumentos lanza
  `ErrorDeTipo`.
- Combinación realista: crear archivo en `directorio_temp` con
  nombre `_test_v108_<usuario>.tmp`, escribir, leer, borrar.

### Ejemplo

`examples/95_identidad_sistema.cor` con 5 secciones:

1. Identidad básica (usuario, host, home, cwd, temp).
2. Huella del entorno (`usuario@host` como marca).
3. Archivo temporal con nombre único combinando usuario + timestamp.
4. Directorio de trabajo por usuario (`~/.miapp/{usuario}/{cache,datos}`).
5. **Sandbox temporal con cleanup garantizado** (patrón
   `en_sandbox(prefijo, accion)`).

### Pendientes ahora cerrados

Del CHANGELOG de v1.104 había tres ítems en "Lo que sigue
pendiente":

- ~~`usuario_actual()`~~ ✓
- ~~`hostname()`~~ ✓
- ~~`directorio_temporal()`~~ ✓

Los tres cerrados en esta release.

### Archivos

- `src/nativos.c` — 3 nativas nuevas (~80 líneas C con
  `#ifdef _WIN32` para portabilidad POSIX/Windows). `<windows.h>`
  / `<unistd.h>` incluido en la sección v1.108 para que las
  declaraciones (`DWORD`, `gethostname`) estén disponibles.
- `stdlib/sistema.cor` — 3 wrappers + docs inline. Pasa de ~70 a
  ~100 líneas.
- `tests/unit/test_bytecode_sistema_v108.c` — 8 bloques, 14 asserts.
- `examples/95_identidad_sistema.cor` — 5 secciones (incluye el
  patrón sandbox temporal con cleanup garantizado).
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

281 tests verde, lint+fmt limpios.

---

## [1.107.0] — 2026-05-23 — Typo suggestions: case-insensitive + filtro de idéntico

Dos mejoras al "¿quisiste decir...?" en `ErrorDeNombre`,
motivadas por casos reales observados durante el desarrollo.

```cornamusa
# Antes de v1.107:
IMPRIMIR("hola")
# ErrorDeNombre: nombre 'IMPRIMIR' no esta definido

# Despues:
# ErrorDeNombre: nombre 'IMPRIMIR' no esta definido
#   (¿quisiste decir 'imprimir'?)
```

### Cambio 1: filtro de sugerencia idéntica

`escanear_dicc_cercano` ahora descarta candidatos con
`distancia == 0` (el mismo nombre que el objetivo). Motivado por
el bug visto en v1.104: el `obtener_variable_entorno` se registró
con longitud incorrecta (25 en vez de 24), así que el lookup
exacto fallaba pero la clave en el dict era idéntica al
objetivo. Resultado patológico: `"no está definido (¿quisiste
decir 'obtener_variable_entorno'?)"` — el sugeridor encontraba el
nombre exacto pero la búsqueda exacta no lo cogía. Mensaje
confuso para el usuario.

Tras este filtro, ningún sugeridor (`sugerir_nombre_cercano`,
`sugerir_atributo_cercano`) puede ofrecer un nombre idéntico al
buscado.

### Cambio 2: case-insensitive ASCII

Cuando el usuario escribe en case incorrecto (`IMPRIMIR`,
`Longitud`, `MI_CONTADOR`), la distancia Levenshtein normal es
grande (un cambio por carácter) — el umbral de 2 no cubre, y no
se sugiere nada.

Fix: helper `_iguales_ci_ascii(a, alen, b, blen)` que compara
case-insensitive sobre ASCII (`A-Z` ↔ `a-z`). En el scan, si las
longitudes coinciden y son case-insensitive iguales, el candidato
recibe **distancia artificial 1** — alta prioridad por encima de
otras coincidencias léxicas.

Solo ASCII intencionalmente: los built-ins y stdlib están en ASCII,
y los identificadores de usuario normalmente siguen un estilo
consistente. Hacer Unicode lowercasing añadiría dependencia de
utf8proc en el camino caliente del sugeridor. Pragmático.

### Casos cubiertos

| Entrada | Sugerencia |
|---|---|
| `imprimr` (typo 1 char) | `imprimir` (Levenshtein 1) |
| `lojitud` (typo 2 chars) | `longitud` (Levenshtein 2, dentro del umbral) |
| `IMPRIMIR` (todo upper) | `imprimir` (CI match, dist artificial 1) |
| `Longitud` (capitalize) | `longitud` (CI match) |
| `MI_CONTADOR` (variable usuario) | `mi_contador` (CI sobre user-defined) |
| `cosa_inexistente_xyz` (sin similitud) | sin sugerencia |

Funciona para built-ins, variables del usuario, atributos de
instancia y métodos — todos usan el mismo `escanear_dicc_cercano`.

### Tests

5 tests nuevos en `test_bytecode_sugerencias.c`:

- `IMPRIMIR` mayúsculas → sugiere `imprimir`.
- `LONGITUD` → `longitud`.
- `Longitud` mixto → `longitud`.
- `MI_CONTADOR` sobre variable user-defined → `mi_contador`.
- Variables similares (`foobar2026` y `foobar2027` falla): no
  sugiere el nombre buscado a sí mismo.

Total tests del archivo: 15 (los 10 anteriores + 5 nuevos).

### Limitación documentada

`_iguales_ci_ascii` no maneja Unicode (tildes, `ñ`). Si un
identificador usa `imprimirá` y otro `IMPRIMIRÁ`, no se detectarán
como case-insensitive iguales. Caso raro porque los nombres de
identificadores suelen seguir un estilo consistente. Si surge
demanda, se puede integrar `utf8proc_tolower` (vendored).

### Archivos

- `src/vm.c` — helper `_iguales_ci_ascii` + filtro `dist == 0` y
  detección case-insensitive en `escanear_dicc_cercano`.
- `tests/unit/test_bytecode_sugerencias.c` — 5 tests nuevos al
  final, llamadas en `main`.
- `README.md`, `docs/introduccion.md`: entrada de release.

### Estado

279 tests verde, lint+fmt limpios.

---

## [1.106.0] — 2026-05-23 — Cookbook ampliado a 20 recetas

Cinco recetas nuevas validadas contra el intérprete + refactor de
la receta 15 para usar `funcionales.ordenar_por` (v1.101) en
lugar del bubble sort manual original. El cookbook pasa de 15 a
**20 recetas**, cubriendo las features añadidas entre v1.96 y
v1.105 que no tenían pattern documentado.

Release de pedagogía pura — sin cambios en runtime ni stdlib.
Refleja la situación real: 11 releases han añadido
features (FS, glob, sort, pruebas, matemáticas, entorno, copy) que
faltaban en el cookbook. Esta release cierra esa brecha.

### Recetas nuevas (16-20)

| # | Receta | Features que combina |
|---|---|---|
| 16 | Suite de tests para tu propio código | `pruebas.Suite` (v1.96), exit code para CI |
| 17 | Limpieza de archivos viejos por fecha | `ruta.encontrar` (v1.100) + `mtime_ms` (v1.99) + `eliminar` |
| 18 | Backup incremental de un proyecto | `copiar_arbol` (v1.105) + `sistema.inicio()` (v1.104) + `tiempo.epoch_ms()` |
| 19 | Configuración desde variables de entorno | `sistema.obtener_variable` (v1.104) + clase `Config` + helper `config(n, def)` |
| 20 | Estadística de muestras normales | `azar.normal` (v1.103) + `ordenar_por` (v1.101) para percentiles + `matematicas.raiz` |

### Receta 15 actualizada

`ordenar_por` (v1.101) reemplaza el bubble sort manual:

**Antes (v1.90)**:
```cornamusa
funcion ordenar_por(lst, campo):
    copia = lst[:]
    n = longitud(copia)
    para i en rango(n):
        para j en rango(0, n - i - 1):
            si copia[j][campo] > copia[j + 1][campo]:
                tmp = copia[j]
                copia[j] = copia[j + 1]
                copia[j + 1] = tmp
            fin si
        fin para
    fin para
    retornar copia
fin funcion
```

**Ahora (v1.106)**:
```cornamusa
importar funcionales
por_precio = funcionales.ordenar_por(productos, lambda p: p["precio"])
```

El mergesort estable O(n log n) que `funcionales.ordenar_por` ofrece
desde v1.101 es mejor en performance Y legibilidad. Se documenta
además el **truco de las dos pasadas estables** para ordenar por
dos campos.

### Validación

Las 5 recetas nuevas se ejecutaron en `cornamusa --bytecode` durante
la redacción. Los outputs mostrados son los reales del intérprete
(p.ej. la receta 20 muestra `media: 170.17` para semilla 42 —
valor exacto del Box-Muller con esa semilla).

### Patrón destacado: la receta 19 demuestra "12-factor app"

```cornamusa
funcion config(nombre, defecto):
    v = sistema.obtener_variable(nombre)
    si v == nulo:
        retornar defecto
    fin si
    retornar v
fin funcion

clase Config:
    funcion __iniciar__(yo):
        yo.host = config("APP_HOST", "localhost")
        yo.puerto = entero(config("APP_PUERTO", "8080"))
        ...
```

Patrón estándar para apps que se configuran via env vars sin
recompilar (clave 12-factor). Encapsula los defaults razonables y
las conversiones de tipo necesarias (todas las env vars son
cadenas).

### Sin cambios de código

279 tests verde sin cambios desde v1.105. Bump por convención de
release: el cookbook es parte del proyecto y merece versión propia
cuando crece.

### Archivos

- `docs/cookbook.md` — 5 secciones nuevas + receta 15 reescrita.
  Pasa de ~540 líneas a ~720.
- `README.md`, `docs/introduccion.md`: entrada de release.

### Estado

279 tests verde, lint+fmt limpios. **Cookbook: 20 recetas
validadas contra el intérprete real.**

---

## [1.105.0] — 2026-05-23 — Filesystem: copia de archivos y árboles

Cierra el set de FS abierto en v1.97: ahora un script puede hacer
**crear → listar/info → leer/escribir → copiar → borrar** sobre
archivos y árboles enteros. Una nativa C nueva
(`archivo_copiar`) + función pure-Cornamusa `copiar_arbol`
recursiva sobre las primitivas existentes.

```cornamusa
importar archivos
importar ruta

# Copia simple (bytes literales, destino se trunca si existe)
archivos.copiar("config.toml", "config.toml.bak")

# Copia recursiva con mkdir -p implicito
archivos.copiar_arbol("proyecto", "back/up/2026/proyecto")

# Encadenable via Ruta
r = ruta.Ruta("origen.dat").copiar("respaldo.dat")
imprimir(r.tamano())          # ya es Ruta del destino
```

### Nativa C nueva

| Nativa | Devuelve | Errores |
|---|---|---|
| `archivo_copiar(origen, destino)` | `nulo` | `ErrorDeIO` si origen no existe o destino no escribible |

Implementación: `fread`/`fwrite` con buffer de 64 KiB en heap.
Modo binario (`"rb"`/`"wb"`) para preservar bytes literales sin
traducción CRLF. Errores de I/O durante la copia (disco lleno,
fallo de medio) se detectan via `ferror`/`fwrite` mismatch y
lanzan `ErrorDeIO`. NO preserva mtime ni permisos — pendiente para
una release futura junto con `archivo_chmod` / `archivo_set_mtime`.

### Funciones nuevas en `stdlib/archivos.cor`

```cornamusa
archivos.copiar(origen, destino)         # wrapper directo
archivos.copiar_arbol(origen, destino)   # recursivo, pure-Cornamusa
```

`copiar_arbol` se implementa así (~30 líneas):

1. Si `origen` es archivo regular, fallback a `archivo_copiar`.
2. Si `origen` no existe, lanzar `ErrorDeIO`.
3. Si `destino` no existe como directorio, llamar a `crear_arbol`
   (mkdir -p implícito — usa la v1.102).
4. Por cada entrada en `directorio_listar(origen)`: recursión si
   es directorio, `archivo_copiar` si es archivo.

Idempotente: si el destino ya existe como directorio, la copia
sigue (no falla). Útil para copias incrementales.

### Métodos nuevos en `Ruta`

- `r.copiar(destino)` → copia archivo. `destino` puede ser cadena
  o `Ruta`. Devuelve `Ruta(destino)` para encadenar (p.ej.
  `r.copiar("x").tamano()`).
- `r.copiar_arbol(destino)` → análogo recursivo.

Ambos aceptan `Ruta` o cadena como `destino`, normalizando con un
chequeo de `tipo(destino) == "instancia"`.

### Patrones documentados (ejemplo 94)

1. **Copia simple con truncación**: el destino se sobrescribe
   completamente, no se append-ea.
2. **Backup con timestamp**: combinar `archivos.copiar_arbol` con
   `tiempo.epoch_ms()` para nombres únicos.
3. **mkdir -p implícito**: `copiar_arbol("src", "back/up/2026")`
   crea todos los padres necesarios.
4. **Encadenamiento via Ruta**: `Ruta(x).copiar(y).tamano()`.

### Tests

22 asserts en `test_bytecode_copiar.c`:

- `archivo_copiar`: contenido y tamano coinciden.
- Destino se trunca si existía con contenido más largo.
- Origen inexistente lanza `ErrorDeIO`.
- Wrapper `archivos.copiar`.
- `copiar_arbol`: árbol con archivos en varios niveles.
- Mkdir -p implícito en destino (crea padres profundos).
- Fallback: `copiar_arbol` con archivo regular = `copiar`.
- Origen inexistente y ruta vacía lanzan `ErrorDeIO`/`ErrorDeValor`.
- Métodos `Ruta.copiar` devuelven `Ruta` encadenable.

### Ejemplo

`examples/94_copiar.cor` con 7 secciones:

1. Copia simple + sobrescritura.
2. Copia de árbol completo con verificación via `recorrer`.
3. mkdir -p implícito en destino profundo.
4. Fallback con archivo regular.
5. **Backup con timestamp** usando `tiempo.epoch_ms()`.
6. Encadenamiento via `Ruta`.
7. Errores atrapables.

### Lo que sigue pendiente del set FS

- Symlinks (lstat, readlink, crear).
- Watch (notificaciones de cambio).
- `archivo_chmod` / `archivo_set_mtime` — modificar permisos y
  fecha.
- Preservar mtime/permisos en `copiar` (opcional via flag).
- Move/rename: `archivo_mover(orig, dest)`. Hoy se hace con
  `copiar` + `eliminar`, pero `rename()` es atómico en mismo FS.

### Archivos

- `src/nativos.c` — `nativa_archivo_copiar` (~100 líneas C con
  buffer 64 KiB y manejo de error de I/O).
- `stdlib/archivos.cor` — `copiar` (wrapper) + `copiar_arbol`
  (recursivo pure-Cornamusa, ~30 líneas).
- `stdlib/ruta.cor` — métodos `r.copiar(destino)` y
  `r.copiar_arbol(destino)` aceptando cadena o `Ruta`.
- `tests/unit/test_bytecode_copiar.c` — 10 bloques, 22 asserts.
- `examples/94_copiar.cor` — 7 secciones demo (incluye backup
  con timestamp y encadenamiento).
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

279 tests verde, lint+fmt limpios. **Ciclo FS completo**: crear,
listar, info, leer, escribir, **copiar**, borrar — todo
disponible y atrapable.

---

## [1.104.0] — 2026-05-23 — Variables de entorno y directorio de inicio

Acceso al entorno del proceso desde scripts Cornamusa: cuatro
nativas C nuevas (`obtener_variable_entorno`,
`establecer_variable_entorno`, `variables_entorno`,
`directorio_inicio`) más wrappers idiomáticos en `stdlib/sistema`.
Hueco real para scripts CLI que necesitan `$HOME`, `$PATH`, o
configuración via env vars con defaults.

```cornamusa
importar sistema

# Home directory
home = sistema.inicio()                    # "C:/Users/david" o "/home/user"

# Variables tipicas con default
puerto = sistema.obtener_variable("MI_APP_PUERTO")
si puerto == nulo:
    puerto = "8080"
fin si

# Set / unset
sistema.establecer_variable("MI_VAR", "valor")
sistema.establecer_variable("MI_VAR", nulo)   # unset

# Listar todas
para nombre, valor en sistema.variables():
    imprimir(nombre, "=", valor)
fin para
```

### Nativas C nuevas

| Nativa | Devuelve | Errores |
|---|---|---|
| `obtener_variable_entorno(nombre)` | cadena o `nulo` | `ErrorDeTipo` si `nombre` no es cadena |
| `establecer_variable_entorno(nombre, valor)` | `nulo` | `nombre` cadena; `valor` cadena o `nulo` (= unset). `ErrorDeSistema` si falla |
| `variables_entorno()` | dict `{nombre: valor}` | — |
| `directorio_inicio()` | cadena con separadores `/` | `ErrorDeSistema` si no se determina |

Portabilidad:
- Lectura: `getenv()` (estándar C, portable).
- Escritura: `setenv()` POSIX, `_putenv_s()` Windows.
- Unset: `unsetenv()` POSIX, `_putenv_s(nombre, "")` Windows.
- Listado: `environ` POSIX, `_environ` Windows (declarados `extern char **`).
- Home: `HOME` POSIX, `USERPROFILE` con fallback a `HOMEDRIVE` en Windows.

`directorio_inicio()` normaliza `\` a `/` antes de retornar, coherente con la convención de `obtener_cwd()` (v1.97) y `stdlib/ruta`.

### Wrappers en `stdlib/sistema`

API idiomática en castellano:

- `sistema.obtener_variable(nombre)`
- `sistema.establecer_variable(nombre, valor)` — `valor=nulo` borra.
- `sistema.variables()` — dict completo.
- `sistema.inicio()` — home como cadena.

### Pitfall encontrado y corregido

El registro inicial de `obtener_variable_entorno` tenía longitud
incorrecta (puse 25 en vez de 24). El lookup de la tabla de
globales usa la longitud para buscar, así que el nombre nunca
hacía match. El mensaje de error fue elocuente: `nombre
'obtener_variable_entorno' no esta definido (¿quisiste decir
'obtener_variable_entorno'?)` — el sugeridor encontraba el nombre
correcto pero la búsqueda exacta fallaba por la longitud. Fix
trivial: contar bien (`obtener_variable_entorno` son 24 caracteres).

### Patrón documentado: configuración con defaults

`examples/93_entorno.cor` muestra el patrón típico de configurar
una app vía env vars con fallback a defaults sensatos:

```cornamusa
funcion config_o(nombre, defecto):
    v = sistema.obtener_variable(nombre)
    si v == nulo:
        retornar defecto
    fin si
    retornar v
fin funcion

modo = config_o("APP_MODO", "dev")
puerto = config_o("APP_PUERTO", "8080")
```

### Tests

18 asserts en `test_bytecode_entorno.c`:

- `obtener_variable_entorno` devuelve `nulo` para inexistentes.
- Round-trip set + get con valor de texto.
- Borrar pasando `nulo`: la variable desaparece.
- `variables_entorno()` retorna dict no vacío.
- `variables_entorno()` refleja cambios inmediatamente tras `set`.
- `directorio_inicio()` es cadena no vacía.
- `directorio_inicio()` normaliza separadores (sin `\`).
- Wrappers `sistema.*`.
- Errores de tipo (argumento no-cadena lanza `ErrorDeTipo`).

Todos los tests usan prefijo `_CORNAMUSA_TEST_` en nombres de
variable para evitar choque con el entorno real, y limpian
después.

### Ejemplo

`examples/93_entorno.cor` con 6 secciones:

1. `sistema.inicio()` y existencia.
2. Inspección de variables típicas (`PATH`, `USER`, `LANG`,
   `SHELL`, `HOME`) — truncadas si son largas.
3. Ciclo set / overwrite / unset.
4. Patrón configuración con defaults (helper `config_o`).
5. Buscar variables por prefijo (`PATH*`, `USER*`, `PROGRAM*`,
   `TEMP*`, `PYTHON*`).
6. Construir rutas relativas al home: `~/.cornamusa/config.cor`
   con `ruta.Ruta(sistema.inicio()).unir(...)`.

### Lo que sigue pendiente

- `usuario_actual()` → nombre del usuario (`whoami`). Trivial sobre
  `USER`/`USERNAME` env var; conviene como helper para no replicar
  la lógica de detección.
- `hostname()` → nombre de la máquina. POSIX `gethostname()`,
  Windows `GetComputerNameA()`.
- `directorio_temporal()` → `TMPDIR`/`TEMP` con fallback. Útil
  para `tempfile`-style.

### Archivos

- `src/nativos.c` — 4 nativas con `#ifdef _WIN32` para portabilidad
  POSIX/Windows (~150 líneas C).
- `stdlib/sistema.cor` — 4 wrappers + docs inline. Pasa de 23
  líneas a ~70.
- `tests/unit/test_bytecode_entorno.c` — 10 bloques, 18 asserts.
- `examples/93_entorno.cor` — 6 secciones (incluye patrón
  config-con-defaults y composición con `ruta`).
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

277 tests verde, lint+fmt limpios.

---

## [1.103.0] — 2026-05-23 — Matemáticas: trig, log, raíz, redondeo + `azar.normal()` Box-Muller

Cierra el TODO declarado en `stdlib/azar.cor:108` (de v1.27): para
implementar Box-Muller hacían falta built-ins `sqrt`, `ln`, `cos` que
no se exponían. Esta release añade **15 nativas matemáticas** sobre
libm, las expone en `stdlib/matematicas.cor`, y completa
`stdlib/azar.cor` con `normal(mu, sigma)`.

```cornamusa
importar matematicas
importar azar

# Funciones continuas
matematicas.raiz(2)                    # 1.41421...
matematicas.ln(matematicas.E)          # 1.0
matematicas.seno(matematicas.PI / 6)   # 0.5
matematicas.arco_tangente(1) * 4       # PI
matematicas.hipotenusa(3, 4)           # 5.0

# Distribucion normal
azar.semilla(2024)
altura = azar.normal(170, 8)           # ~N(170, 8) cm
```

### Nativas C nuevas (15)

| Nativa | Comportamiento | Errores |
|---|---|---|
| `mat_raiz(x)` | `sqrt(x)` | `ErrorDeValor` si `x < 0` |
| `mat_ln(x)` | `log(x)` natural | `ErrorDeValor` si `x <= 0` |
| `mat_log10(x)` | `log10(x)` | `ErrorDeValor` si `x <= 0` |
| `mat_exp(x)` | `e^x` | — |
| `mat_potencia(x, expo)` | `pow(x, expo)` | — |
| `mat_seno(x)`, `mat_coseno(x)`, `mat_tangente(x)` | trig (radianes) | — |
| `mat_arco_seno(x)`, `mat_arco_coseno(x)` | inversas | `ErrorDeValor` si fuera de `[-1, 1]` |
| `mat_arco_tangente(x)`, `mat_arco_tangente2(dy, dx)` | `atan`, `atan2` | — |
| `mat_techo(x)`, `mat_suelo(x)`, `mat_redondear(x)` | `ceil`, `floor`, `round` half-away-from-zero | — |

Las 15 aceptan entero/decimal/booleano vía helper interno
`_val_a_double` y devuelven `decimal`. `<math.h>` se incluye una
sola vez en `nativos.c`. Sin manejo especial de NaN/inf — comportamiento de libm.

### Wrappers en `stdlib/matematicas.cor`

API en castellano para uso normal:

- `raiz(x)`, `potencia(x, expo)`, `hipotenusa(a, b)`.
- `ln(x)`, `log10(x)`, `log(x, base)`, `exp(x)`.
- `seno(x)`, `coseno(x)`, `tangente(x)`.
- `arco_seno(x)`, `arco_coseno(x)`, `arco_tangente(x)`,
  `arco_tangente2(dy, dx)`.
- `grados_a_radianes(g)`, `radianes_a_grados(r)`.
- `techo(x)`, `suelo(x)`, `redondear(x)`.

`log(x, base)` se compone como `ln(x) / ln(base)` — no es nativo.
`hipotenusa(a, b)` se compone como `raiz(a*a + b*b)`.

### `azar.normal(mu, sigma)` con Box-Muller

```cornamusa
funcion normal(mu, sigma):
    importar matematicas
    si sigma == 0:
        retornar mu * 1.0
    fin si
    si sigma < 0:
        lanzar ErrorDeValor("normal: sigma debe ser >= 0")
    fin si
    u1 = azar_decimal()
    mientras u1 == 0:
        u1 = azar_decimal()      # evitar log(0)
    fin mientras
    u2 = azar_decimal()
    z = matematicas.raiz(-2.0 * matematicas.ln(u1)) *
        matematicas.coseno(2.0 * matematicas.PI * u2)
    retornar mu + sigma * z
fin funcion
```

Box-Muller estándar: dos uniformes en `(0, 1]` producen una
muestra estándar `Z ~ N(0, 1)`. Se descarta el componente `sin(2*PI*u2)`
(más simple que cachear). El `mientras u1 == 0` evita
`log(0) = -infinito`. `sigma == 0` corto-circuita devolviendo
`mu` exacto (convención degenerada).

### Naming: por qué `y` cambió a `expo` y `dy`/`dx`

`y` es palabra reservada del lenguaje (operador booleano `y`).
Inicialmente puse `potencia(x, y)` y `arco_tangente2(y, x)` —
fallaba con `ErrorDeSintaxis: se esperaba un nombre de parámetro`.
Fix: `potencia(x, expo)` y `arco_tangente2(dy, dx)`. Pitfall que
se ha visto otras veces; documentado en el código.

### Tests

26 asserts en `test_bytecode_matematicas.c`:

- Cada función con casos típicos: `raiz(16)`, `ln(E)`, `seno(0)`,
  `coseno(0)`, conversión grados↔radianes.
- Identidad `4 * arco_tangente(1) ≈ PI` con tolerancia `1e-4`.
- Redondeo half-away-from-zero: `redondear(-2.5) == -3.0`.
- Errores: `raiz(-1)`, `ln(0)`, `arco_seno(2)` lanzan
  `ErrorDeValor` atrapable.
- `azar.normal(0, 1)` con 5000 muestras: media ≈ 0 (tol 0.1),
  desviación ≈ 1 (tol 0.1).
- `azar.normal(100, 5)` con 5000 muestras: media ≈ 100 (tol 0.5).
- `azar.normal(7, 0)` devuelve 7 exacto.
- `azar.normal(0, -1)` lanza `ErrorDeValor`.

### Ejemplo

`examples/92_matematicas.cor` con 6 secciones:

1. Raíz, potencia, hipotenusa.
2. Logaritmos y exponencial.
3. Trigonometría + inversas + conversiones grados/radianes.
4. Redondeo (techo, suelo, redondear).
5. Distribución normal: 10 muestras individuales + verificación
   estadística sobre 10000 muestras (media ≈ 0, desv ≈ 1).
6. **Histograma ASCII** de alturas `N(170, 8)`: 1000 muestras
   agrupadas en bins de 10 cm, ordenadas con
   `funcionales.ordenar_por` (v1.101), barras con `#`. Sale una
   campana de Gauss perfectamente reconocible.

### Pendiente futuro

- Más distribuciones: `exponencial(lambda)`, `gamma`, `beta`,
  `binomial(n, p)`, `poisson(lambda)`.
- Cachear el segundo componente Box-Muller (`sin(2*PI*u2)`) para
  reducir llamadas a libm a la mitad. No prioritario.
- Constantes adicionales: `TAU = 2*PI`, `INF`, `NaN`.

### Archivos

- `src/nativos.c` — 15 nativas matemáticas + helper `_val_a_double`
  + macro `MAT_UNARIA` para reducir boilerplate (~200 líneas C).
- `stdlib/matematicas.cor` — 18 wrappers nuevos.
- `stdlib/azar.cor` — `normal(mu, sigma)` reemplaza el TODO de v1.27.
- `tests/unit/test_bytecode_matematicas.c` — 11 bloques, 26 asserts.
- `examples/92_matematicas.cor` — 6 secciones (incluye histograma
  ASCII de campana de Gauss).
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

275 tests verde, lint+fmt limpios. TODO declarado en
`stdlib/azar.cor:108` — **cerrado**.

---

## [1.102.0] — 2026-05-23 — Filesystem: `eliminar_arbol` (rm -rf) + `crear_arbol` (mkdir -p)

Cierra dos huecos declarados explícitamente en v1.99 ("Pendiente
futuro"): borrado recursivo y creación con padres. Pure-Cornamusa
sobre las nativas FS de v1.97 y v1.99 — sin código C nuevo. Ahora
los scripts pueden hacer scaffolding completo + cleanup con dos
llamadas, sin componer bucles manuales.

```cornamusa
importar archivos

# mkdir -p en una linea
archivos.crear_arbol("proyecto/src/utils")
archivos.crear_arbol("proyecto/tests")
archivos.crear_arbol("proyecto/docs/api")

# Idempotente: si ya existe, no falla
archivos.crear_arbol("proyecto/src/utils")    # OK

# rm -rf con guardrails
archivos.eliminar_arbol("proyecto")           # borra todo el arbol
archivos.eliminar_arbol("")                   # ErrorDeValor (guardrail)
archivos.eliminar_arbol("/")                  # ErrorDeValor (guardrail)
```

### API

| Función | Hace |
|---|---|
| `archivos.crear_arbol(ruta)` | Crea ruta y todos los padres intermedios. Idempotente. Acepta `/` y `\` mezclados. |
| `archivos.eliminar_arbol(ruta)` | Borra recursivamente. Si ruta es archivo, fallback a `eliminar`. |

Wrappers correspondientes en `Ruta`:

| Método | Equivalente |
|---|---|
| `r.crear_arbol()` | `archivos.crear_arbol(r.cadena())` |
| `r.eliminar_arbol()` | `archivos.eliminar_arbol(r.cadena())` |

### Guardrails de seguridad

`eliminar_arbol` rechaza tres rutas peligrosas con `ErrorDeValor`:

- `""` (cadena vacía)
- `"/"` (raíz POSIX)
- `"\\"` (raíz Windows)

Esto evita el accidente clásico de borrar todo el filesystem por
una variable mal pasada. Si el usuario realmente quiere borrar la
raíz, puede llamar a `directorio_listar("/")` + bucle manual — la
acción consciente.

Además:

- Si la ruta no existe → `ErrorDeIO` atrapable.
- Si un componente intermedio en `crear_arbol` existe como
  archivo (no directorio), el `directorio_crear` interno fallará
  con `ErrorDeIO`. Esto es intencional — no se sobrescriben
  archivos.

### Implementación

Pure-Cornamusa, ~80 líneas en `stdlib/archivos.cor`.

**`eliminar_arbol(ruta)`**:
1. Validar guardrails.
2. Si es archivo regular, fallback a `archivo_borrar`.
3. Si es directorio, iterar `directorio_listar(ruta)`. Por cada
   entrada: si es directorio, recursión; si es archivo,
   `archivo_borrar`.
4. Finalmente `directorio_borrar` sobre el directorio ya vacío.

**`crear_arbol(ruta)`**:
1. Normalizar separadores `\` → `/`.
2. Detectar prefijo absoluto (`/`, drive letter `C:`).
3. Iterar caracteres acumulando componentes. Por cada `/`, llamar
   a `directorio_crear` sobre el path acumulado (saltando si ya
   existe vía `archivo_es_directorio`).
4. Componente final tras el último separador.

### Por qué pure-Cornamusa y no nativa C

Implementar `rmdir -r` en C con portabilidad POSIX/Windows es
~200 líneas de `#ifdef _WIN32` con `FindFirstFile`/`opendir` +
recursión + manejo de errores en cada nivel. Pure-Cornamusa son
~80 líneas legibles que reusan exactamente las primitivas que ya
existen (`archivo_es_directorio`, `directorio_listar`,
`archivo_borrar`, `directorio_borrar`, `directorio_crear`). La
performance es perfectamente adecuada para casos típicos
(scaffolding, cleanup de directorios temporales, sandboxes en
tests). Patrón ya establecido en v1.100 (glob) y v1.101 (ordenar).

### Tests

8 bloques en `test_bytecode_arbol.c`:

- `crear_arbol` crea 4 niveles correctamente.
- `crear_arbol` idempotente: dos llamadas seguidas no fallan.
- `eliminar_arbol` recursivo con archivos en cada nivel.
- Guardrails: `""` y `"/"` lanzan `ErrorDeValor`.
- Fallback: `eliminar_arbol(archivo)` borra el archivo.
- Inexistente: `eliminar_arbol("no_existe")` lanza `ErrorDeIO`.
- Métodos `Ruta.crear_arbol()` y `Ruta.eliminar_arbol()`.
- `crear_arbol` acepta separadores `\\` mezclados.

### Ejemplo

`examples/91_arbol_fs.cor` con 7 secciones:

1. Crear estructura de proyecto en un solo paso, recorrer para
   verificar.
2. Poblar con archivos en distintos niveles + listado de tamaños.
3. Borrado recursivo.
4. Idempotencia de `crear_arbol`.
5. Guardrails: `""`, `"/"`, inexistente.
6. Patrón **sandbox temporal**: función que crea un dir, ejecuta
   un callback, y limpia siempre (incluso si lanza).
7. Métodos sobre `Ruta`.

### Lo que sigue pendiente del set FS

- Symlinks (`lstat`, `readlink`, crear).
- Watch (notificaciones de cambio en FS).
- Modificar mtime/permisos.
- Copy (`archivo_copiar`, `eliminar_arbol_copiar`).

### Archivos

- `stdlib/archivos.cor` — `eliminar_arbol` + `crear_arbol`
  (~80 líneas pure-Cornamusa, con guardrails).
- `stdlib/ruta.cor` — métodos `r.eliminar_arbol()` y
  `r.crear_arbol()`.
- `tests/unit/test_bytecode_arbol.c` — 8 bloques.
- `examples/91_arbol_fs.cor` — 7 secciones demo (incluye patrón
  sandbox temporal con cleanup garantizado).
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

273 tests verde, lint+fmt limpios.

---

## [1.101.0] — 2026-05-18 — `funcionales.ordenar_por`: sort estable con clave

Añade `ordenar_por(xs, clave)` y `ordenar_por_inverso(xs, clave)`
a `stdlib/funcionales.cor`. Soluciona la limitación del built-in
`ordenar` que solo compara números y cadenas directamente — ahora
es trivial ordenar diccionarios por campo, instancias por
atributo, tuplas por posición, etc. Limitación detectada en
`examples/89_glob_recorrer.cor` (v1.100) al querer ordenar Rutas
por `mtime_ms`.

```cornamusa
importar funcionales

# Ordenar diccionarios por campo
empleados = [
    {"nombre": "Ana",    "salario": 35000},
    {"nombre": "Bruno",  "salario": 42000},
    {"nombre": "Carlos", "salario": 28000},
]
por_salario = funcionales.ordenar_por(empleados, lambda e: e["salario"])

# Ordenar Rutas por fecha de modificacion (descendente)
recientes = funcionales.ordenar_por_inverso(
    ruta.encontrar("examples", "*.cor"),
    lambda r: r.mtime_ms()
)
```

### API

| Función | Devuelve |
|---|---|
| `ordenar_por(xs, clave)` | nueva lista con `xs` ordenada ascendente por `clave(x)` |
| `ordenar_por_inverso(xs, clave)` | nueva lista con `xs` ordenada descendente |

`clave(x)` debe devolver un valor comparable con `<=` (número o
cadena). Si dos elementos producen claves iguales, su orden
relativo en la entrada se preserva: el sort es **estable** —
propiedad del mergesort que se usa.

### Por qué pure-Cornamusa y no nativa C

Implementar `ordenar(xs, key=fn)` en el nativo C requeriría
invocar callbacks de Cornamusa desde dentro de `qsort`, lo cual
necesita infraestructura no trivial (`vm_llamar_callback`,
manejo de stack/excepciones desde C, reentrancia). Por contra,
mergesort en cornamusa puro es:

- **80 líneas legibles** que cualquier usuario puede leer y
  modificar.
- **Performance comparable** para listas <10k (donde el
  bottleneck es la llamada `clave(x)`, no las comparaciones).
- **Cero infraestructura C nueva** — un cambio aditivo en
  `stdlib/funcionales.cor`.

### Optimización: precomputo de claves

`ordenar_por` invoca `clave(x)` **una sola vez** por elemento
antes del mergesort (no por comparación). El mergesort luego
opera sobre índices comparando las claves pre-computadas. Esto
es importante cuando `clave` es costosa (acceso a FS, llamada a
red, computación pesada).

### Estabilidad: aplicación práctica

Como el sort es estable, dos pasadas dan sort por dos claves:

```cornamusa
paso1 = funcionales.ordenar_por(equipo, lambda p: p["salario"])
# Segunda pasada por depto. Preserva orden por salario dentro
# de cada depto, porque el sort es estable.
paso2 = funcionales.ordenar_por(paso1, lambda p: p["depto"])
```

### Tests

11 bloques en `test_bytecode_ordenar_por.c`:

- Enteros por identidad.
- Cadenas por longitud.
- Lista vacía y de un elemento.
- No muta la lista original.
- Diccionarios por campo.
- **Estabilidad** confirmada con elementos de misma clave.
- `ordenar_por_inverso`.
- Clave compuesta (dos pasadas estables = sort por dos campos).
- Función key con nombre (no lambda).
- Lista grande (>10 elementos para recurrir varios niveles).

### Ejemplo

`examples/90_ordenar_por.cor` con 6 secciones:

1. Cadenas por longitud.
2. Diccionarios por campo (ascendente y descendente).
3. Tuplas por posición — ranking de puntuaciones.
4. Sort en dos pasadas (por depto, dentro por salario).
5. Función key con lógica custom (vocales primero).
6. Patrón top-N (los 3 mayores).

También actualicé `examples/89_glob_recorrer.cor` para usar
`ordenar_por_inverso` en lugar del bucle manual que tenía antes.

### Archivos

- `stdlib/funcionales.cor` — `ordenar_por`, `ordenar_por_inverso`,
  helpers internos `_mergesort_idx` y `_merge_idx` (~80 líneas).
- `tests/unit/test_bytecode_ordenar_por.c` — 11 bloques.
- `examples/90_ordenar_por.cor` — 6 secciones demo.
- `examples/89_glob_recorrer.cor` — sección 7 actualizada.
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

271 tests verde, lint+fmt limpios.

---

## [1.100.0] — 2026-05-18 — Glob recursivo en `stdlib/ruta`

Añade matcher glob básico (`*`, `?`) y dos funciones de recorrido
recursivo del filesystem a `stdlib/ruta`. Pure-Cornamusa sobre las
nativas FS de v1.97 (`directorio_listar`, `archivo_es_directorio`).
Habilita scripts tipo "encontrar todos los `*.cor` del proyecto"
en una sola línea.

```cornamusa
importar ruta

# Recorrer recursivamente
todos = ruta.recorrer("examples")           # lista de Rutas (DFS)

# Encontrar por patron glob
cors = ruta.encontrar("examples", "*.cor")  # solo .cor
tests = ruta.encontrar("tests", "test_*.c") # recurre tests/unit/

# Metodos sobre Ruta
r = ruta.Ruta("stdlib")
r.recorrer()                # lista
r.encontrar("*.cor")        # filtrado
r.coincide("*.cor")         # un solo path
```

### Matcher glob

Implementación iterativa O(n·m) con backtracking en `*`. Soporta:

- `*` — cero o más caracteres cualesquiera.
- `?` — exactamente un carácter.

NO soporta:

- `**` recursivo (toda búsqueda con `encontrar` ya es recursiva).
- Clases `[abc]`.
- Alternancias `{a,b}`.
- Negación `[!abc]`.

Para necesidades más complejas, usar `stdlib/regex`. La función
interna es `_coincide_glob(cadena, patron)`.

### Recorrido recursivo

`ruta.recorrer(directorio)` devuelve una **lista** (no generador
lazy) con todas las Rutas alcanzables desde `directorio`
recursivamente, en orden DFS. Incluye archivos Y directorios. Si la
ruta no es un directorio accesible, retorna lista vacía
silenciosamente — para distinguir "vacío" de "inaccesible", usar
`archivos.es_directorio` antes.

`ruta.encontrar(directorio, patron)` es equivalente a
`recorrer + filter` por glob sobre el **nombre** (no la ruta
completa). Útil para `*.cor`, `test_*.txt`, etc.

### Métodos en `Ruta`

| Método | Equivalente |
|---|---|
| `r.recorrer()` | `ruta.recorrer(r.cadena())` |
| `r.encontrar(patron)` | `ruta.encontrar(r.cadena(), patron)` |
| `r.coincide(patron)` | `_coincide_glob(r.nombre(), patron)` |

### Pitfall encontrado durante el desarrollo

Inicialmente el método `Ruta.recorrer(yo)` llamaba a la función
módulo `recorrer(yo.s)`. Pero dentro del método de la clase, el
nombre `recorrer` se resolvía al propio método (vía `yo`), causando
una llamada infinita con un argumento de tipo incorrecto. Fix:
llamar al helper `_recorrer_aux(yo.s, salida)` directamente, sin
pasar por el wrapper del módulo. Mismo patrón que se usó en
v1.95+ para `Ruta.unir` con `unir_partes`.

Además: `intentar/atrapar` tiene un sub-scope donde las variables
declaradas dentro NO escapan al exterior (a diferencia de `si/sino`
desde v1.95). Por eso `_recorrer_aux` pre-declara `entradas = []`
y `fallo = falso` antes del `intentar`.

### Tests

10 bloques en `test_bytecode_glob.c`:

- Glob básico con extensiones (`*.cor` matchea `hola.cor` pero no
  `hola.txt`).
- Glob con `?` (`???` matchea `abc` exacto, no `ab` ni `abcd`).
- Prefijo + asterisco intermedio (`test_*.c` matchea
  `test_main.c` pero no `main_test.c`).
- Patrón con solo asteriscos matchea cualquier cosa, incluso vacío.
- `recorrer` cuenta entradas de un directorio real.
- `encontrar` filtra por patrón.
- Recorrido recursivo entra en sub-directorios.
- `recorrer` sobre archivo (no directorio) → lista vacía.
- Métodos `Ruta.recorrer`/`Ruta.encontrar`.
- `encontrar` devuelve `Ruta`s encadenables (no cadenas).

### Ejemplo

`examples/89_glob_recorrer.cor` con 7 secciones:

1. Matcher glob sobre nombres.
2. Recorrer `stdlib/` (~23 entradas).
3. Encontrar `*.cor` en `examples/` (89 archivos).
4. `test_*.c` recursivo en `tests/` (104 archivos).
5. Total de bytes en `stdlib/*.cor` (~103 KiB).
6. Filtrar via método `coincide()` para archivos `*.md`.
7. Encontrar el archivo más reciente por `mtime_ms`.

### Pendiente futuro

- Generador lazy (`producir`) para no cargar árboles enormes en
  memoria.
- `**` recursivo en patrones (`src/**/*.cor`).
- Clases `[a-z]` en glob.
- Patrón con paths (no solo nombres), tipo `examples/test_*.cor`.

### Archivos

- `stdlib/ruta.cor` — matcher `_coincide_glob`, helper
  `_recorrer_aux`, funciones módulo `recorrer`/`encontrar`, 3
  métodos nuevos en `Ruta` (~80 líneas pure-Cornamusa añadidas).
- `tests/unit/test_bytecode_glob.c` — 10 bloques.
- `examples/89_glob_recorrer.cor` — 7 secciones demo.
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

269 tests verde, lint+fmt limpios.

### Nota sobre el número de versión

Saltamos de v1.99 a v1.100 (no v2.0). v2.0 está reservada para el
hito gordo (concurrencia, async/await, NaN-boxing); las releases
incrementales siguen como 1.NNN.

---

## [1.99.0] — 2026-05-18 — Filesystem completo: borrado e info

Cierra el set de operaciones de FS abiertas en v1.97. Añade
borrado de archivos y directorios, plus `archivo_info` con
metadata (tamano, mtime, tipo). Ahora un script Cornamusa puede
hacer ciclo completo: crear, listar, leer, escribir, modificar,
borrar — con `ErrorDeIO` atrapable en todos los puntos.

```cornamusa
importar ruta
importar archivos

# Info de un archivo
i = archivos.info("README.md")
imprimir(i["tamano"], "bytes,",
         "mtime =", i["mtime_epoch_ms"], "ms epoch")

# Listado con tamano (estilo `ls -l`)
para entrada en ruta.cwd().listar_rutas():
    si entrada.es_archivo():
        imprimir(entrada.tamano(), entrada.nombre())
    fin si
fin para

# Borrar despues de procesar
r = ruta.Ruta("temp.log")
si r.existe():
    r.eliminar()
fin si

# Limpieza de directorios (deben estar vacios)
archivos.crear_directorio("trabajo")
# ... procesamiento ...
archivos.eliminar_directorio("trabajo")
```

### Nativas C nuevas

| Nativa | Retorna | Errores |
|---|---|---|
| `archivo_borrar(ruta)` | `nulo` | `ErrorDeIO` si no existe / es directorio |
| `directorio_borrar(ruta)` | `nulo` | `ErrorDeIO` si no existe / no vacío |
| `archivo_info(ruta)` | dict | `ErrorDeIO` si no existe |

`archivo_info` retorna un dict con cuatro claves:

- `"tamano"` — bytes (entero, hasta 2^62 vía `valor_entero_de_i64`,
  importante para archivos >2GB en Windows MinGW donde `long` es
  32-bit y truncaría).
- `"mtime_epoch_ms"` — milisegundos desde UNIX epoch. Precisión
  por-segundo en Windows; sub-segundo en POSIX (lossy a ms en
  ambos casos).
- `"es_archivo"` / `"es_directorio"` — booleanos.

Portabilidad: usa `<sys/stat.h>` (universal). En Windows:
`struct _stat64` para soporte de archivos grandes; `_rmdir` para
borrar directorio; `remove()` portable. POSIX: `struct stat`,
`rmdir()`, `remove()`.

### Wrappers en `stdlib/archivos.cor`

- `archivos.eliminar(ruta)` — wrapper de `archivo_borrar`.
- `archivos.eliminar_directorio(ruta)` — wrapper de `directorio_borrar`.
- `archivos.info(ruta)` — wrapper de `archivo_info`.

**Nota de naming**: `borrar` es palabra reservada del lenguaje
(`borrar d[k]`, `borrar obj.attr`), así que aquí se usa `eliminar`
para evitar choques de parsing. Mismo motivo que llevó a renombrar
`obtener_atributo` / `asignar_atributo` antes.

### Métodos nuevos en `Ruta` (`stdlib/ruta.cor`)

- `r.eliminar()` — quita el archivo.
- `r.eliminar_directorio()` — quita el directorio (debe estar vacío).
- `r.info()` — dict completo.
- `r.tamano()` — atajo a `info()["tamano"]`.
- `r.mtime_ms()` — atajo a `info()["mtime_epoch_ms"]`.

### Bug encontrado y arreglado durante el desarrollo

Inicialmente `mtime_epoch_ms` se construía con
`valor_entero_de_long((long)mtime_ms)`. En Windows MinGW64, `long`
es 32-bit, así que un `int64_t` con `1779107383000` (típico de
2026) se truncaba a `990801456` (≈ 1970-01-12). Fix: usar
`valor_entero_de_i64(mtime_ms)` directamente, que preserva los 64
bits completos. Mismo tratamiento para `tamano`.

### Tests

20+ asserts en `test_bytecode_fs2.c`:

- `archivo_borrar` con archivo existente, con archivo inexistente
  (lanza `ErrorDeIO`).
- `directorio_borrar` con directorio vacío, con directorio no
  vacío (lanza).
- `archivo_info` con archivo (6 bytes), con inexistente (lanza).
- Wrappers `archivos.eliminar`, `archivos.info`.
- Métodos `Ruta.eliminar`, `Ruta.info`, `Ruta.tamano`, `Ruta.mtime_ms`.

### Ejemplo

`examples/88_fs_metadata.cor` con 6 secciones:

1. `info()` básico.
2. Métodos `tamano`, `mtime_ms` en `Ruta`.
3. Listado tipo `ls -l` con tamaños en `stdlib/`.
4. `eliminar()` simple.
5. Errores atrapables (`eliminar` inexistente, `info` inexistente,
   `eliminar_directorio` no vacío).
6. Patrón "encontrar archivos grandes": filtrar `> 5 KiB` en
   `stdlib/` usando `entrada.tamano()`.

### Pendiente futuro

- `rm -rf` recursivo (`eliminar_arbol(ruta)`).
- `mkdir -p` (crear con padres).
- Modificar mtime/permisos.
- Symlinks (lstat, readlink).
- Watch (notificaciones de cambio en FS).

### Archivos

- `src/nativos.c` — 3 nativas nuevas (~250 líneas C portátiles).
- `stdlib/archivos.cor` — 3 wrappers nuevos.
- `stdlib/ruta.cor` — 5 métodos nuevos en clase `Ruta`.
- `tests/unit/test_bytecode_fs2.c` — 8 bloques, 20+ asserts.
- `examples/88_fs_metadata.cor` — 6 secciones demo.
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

267 tests verde, lint+fmt limpios.

---

## [1.98.0] — 2026-05-18 — `cornamusa nuevo <nombre>`: scaffold de proyecto

Nuevo subcomando `cornamusa nuevo <nombre>` que crea un esqueleto
completo de proyecto Cornamusa: `main.cor`, `tests/test_main.cor`
(con suite usando stdlib `pruebas` de v1.96), `README.md` y
`.gitignore`. Cierra el ciclo de las dos releases anteriores:
ahora un usuario empieza un proyecto con un comando y tiene tests
funcionales pasando desde el primer commit.

```bash
$ cornamusa nuevo mi_proyecto
Proyecto creado: mi_proyecto/

Estructura:
  mi_proyecto/main.cor              programa principal
  mi_proyecto/tests/test_main.cor   tests con stdlib pruebas
  mi_proyecto/README.md             instrucciones
  mi_proyecto/.gitignore            exclusiones para git

Siguientes pasos:
  cd mi_proyecto
  cornamusa --bytecode main.cor
  cornamusa --bytecode tests/test_main.cor

$ cd mi_proyecto
$ cornamusa --bytecode tests/test_main.cor
=== Suite: main ===
  [OK]    saluda a mundo
  [OK]    saluda a Ana
---
Total: 2 | Pasados: 2 | Fallados: 0
```

### Archivos generados

- **`main.cor`**: programa "Hola, mundo" con función `saludar(quien)`
  para que haya algo testeable desde la primera línea.
- **`tests/test_main.cor`**: 2 casos con `pruebas.Suite`, exit code 1
  si algún test falla — listo para CI.
- **`README.md`**: instrucciones para ejecutar el programa y los
  tests.
- **`.gitignore`**: excluye `build/`, `*.tmp`, `*.log`,
  `.cornamusa_historial`, `.vscode/`, `.idea/`, `*.swp`.

### Errores manejados

- `cornamusa nuevo` sin nombre → exit 64 + mensaje "se requiere un
  nombre de proyecto".
- `cornamusa nuevo X` cuando `X` ya existe → exit 1 + "ya existe,
  abortando" (NO sobreescribe).
- `cornamusa nuevo --ayuda` → texto de uso, exit 0.
- Opciones desconocidas (`cornamusa nuevo -foo bar`) → exit 64.

### Decisiones de diseño

- **Función `saludar` replicada en el test**: para que el test sea
  ejecutable sin sintaxis de `importar main` (que requeriría que el
  test esté ejecutado en un layout específico). El comentario
  inline explica que cuando el usuario organice en módulos, podrá
  reemplazar la duplicación por `importar main`.
- **Exit code 1 cuando algún test falla**: convención Unix para
  integración con CI. El test generado usa
  `si r["fallados"] > 0: salir(1) fin si`.
- **Solo un directorio**: `nuevo` crea solo el directorio raíz del
  proyecto + `tests/`. No es `mkdir -p` — si quieres
  `proyectos/nuevos/foo`, debes crear las carpetas padres primero.

### Tests

Tres tests en `tests/CMakeLists.txt`:

1. `nuevo_ayuda`: verifica que `cornamusa nuevo --ayuda` imprime
   "Crea un nuevo proyecto" (PASS_REGULAR_EXPRESSION).
2. `nuevo_sin_args_falla`: verifica que `cornamusa nuevo` (sin
   argumentos) falla con exit code no-cero (WILL_FAIL).
3. `nuevo_end_to_end` (script CMake `tests/scripts/test_nuevo.cmake`):
   - Invoca `cornamusa nuevo _test_nuevo_e2e` desde la raíz del
     repo.
   - Verifica que se generan los 4 archivos esperados.
   - Ejecuta el test generado y verifica `Pasados: 2`.
   - Confirma que un segundo `nuevo` con el mismo nombre falla.
   - Limpia el directorio temporal al final.

### Archivos

- `src/main.c` — `subcomando_nuevo` + dispatch + entrada en
  `imprimir_uso`. ~200 líneas C con cuatro plantillas inline.
- `tests/CMakeLists.txt` — 3 tests del subcomando.
- `tests/scripts/test_nuevo.cmake` — script CMake para el test
  end-to-end con cleanup automático.
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

265 tests verde, lint+fmt limpios.

### Lo que queda pendiente

- Plantilla extendida con varios módulos (`--plantilla=biblioteca`,
  `--plantilla=cli`, ...).
- Integración con git (`git init` automático). De momento se deja
  al usuario.
- Plantilla configurable vía archivo (~/.config/cornamusa/plantilla).

---

## [1.97.0] — 2026-05-18 — Filesystem: directorios, listado, cwd, crear

Cuatro nativas C nuevas para operar sobre el sistema de archivos
que faltaban hasta v1.96, junto con wrappers en `stdlib/archivos`
y métodos nuevos en `stdlib/ruta`. Resuelve la limitación
documentada de `ruta.existe()` en v1.94 (que solo veía archivos,
no directorios) y habilita scripts que recorren árboles de archivos.

```cornamusa
importar ruta
importar archivos

# Built-ins directos
imprimir(obtener_cwd())                     # "C:/Users/david/Desktop/Cornamusa"
imprimir(archivo_es_directorio("examples")) # verdadero
ents = directorio_listar("examples")        # lista de cadenas

# Wrappers en archivos
archivos.es_directorio("stdlib")
archivos.listar("stdlib")
archivos.directorio_actual()
archivos.crear_directorio("nuevo_dir")  # lanza ErrorDeIO si falla

# Metodos de Ruta (encadenamiento)
r = ruta.Ruta("examples")
r.existe()           # verdadero (ahora cubre dirs, v1.94 solo archivos)
r.es_directorio()    # verdadero
r.es_archivo()       # falso

# Filtrar archivos .cor en un directorio
para entrada en ruta.cwd().listar_rutas():
    si entrada.es_archivo() y entrada.extension() == ".cor":
        imprimir(entrada.nombre())
    fin si
fin para
```

### Nativas C nuevas (`src/nativos.c`)

| Nativa | Retorna | Errores |
|---|---|---|
| `archivo_es_directorio(ruta)` | booleano | falso si no existe |
| `directorio_listar(ruta)` | lista de cadenas (sin `.`/`..`) | `ErrorDeIO` si la ruta no es directorio |
| `obtener_cwd()` | cadena (con `\` normalizados a `/`) | `ErrorDeIO` improbable |
| `directorio_crear(ruta)` | `nulo` | `ErrorDeIO` si ya existe o el padre no |

Portabilidad: usan `sys/stat.h` (universal) y separan
`<dirent.h>`/`<unistd.h>` POSIX de `<windows.h>` Windows con
`#ifdef _WIN32`. `_mkdir` / `mkdir`, `_getcwd` / `getcwd`,
`FindFirstFileA`/`FindNextFileA` / `opendir`/`readdir`.

`obtener_cwd()` normaliza `\` a `/` antes de retornar — coherente
con la convención de `stdlib/ruta` (separador canónico `/`).

### `stdlib/archivos.cor` (wrappers)

Sección nueva al final del módulo:

- `archivos.es_directorio(ruta)`
- `archivos.listar(ruta)`
- `archivos.directorio_actual()`
- `archivos.crear_directorio(ruta)`

Documentación inline: `existe()` (v1.8) sigue siendo solo para
archivos; para distinguir, usar `es_directorio()`. `listar` no es
recursivo. `crear_directorio` no es `mkdir -p` — crea solo un nivel.

### `stdlib/ruta.cor` (métodos nuevos)

- `r.es_archivo()` — verdadero si es archivo regular existente.
- `r.es_directorio()` — verdadero si es directorio existente.
- `r.existe()` — **ampliado**: cubre ambos casos (v1.94 solo archivos).
- `r.listar()` → lista de cadenas con nombres de las entradas
  inmediatas (sin `.`/`..`).
- `r.listar_rutas()` → lista de `Ruta`s (ya unidas con `r`), útil
  para encadenamiento: `entry.extension()`, `entry.es_archivo()`.
- `ruta.cwd()` (función de módulo) → `Ruta` del directorio actual.

### Tests

13 asserts en `test_bytecode_fs.c`:

- `archivo_es_directorio` con dir, archivo, inexistente.
- `directorio_listar` con `examples/` (>10 entradas, contiene
  `01_hola_mundo.cor`).
- `directorio_listar` lanza `ErrorDeIO` atrapable si la ruta no es
  directorio.
- `obtener_cwd` retorna cadena no vacía.
- `directorio_crear` crea, comprueba con `archivos.es_directorio`,
  intenta crear de nuevo (falla con `ErrorDeIO`), limpia con
  `rmdir` desde el test C.
- Wrappers de `archivos` y métodos de `Ruta`.
- `ruta.cwd()` retorna `Ruta` (instancia) con cadena no vacía.

### Ejemplo

`examples/87_filesystem.cor` con 7 secciones:

1. Built-ins directos (`obtener_cwd`, `archivo_es_directorio`).
2. Wrappers en `stdlib/archivos`.
3. Clase `Ruta` con métodos FS (`existe`, `es_archivo`,
   `es_directorio`).
4. Listar contenido de un directorio (`listar_rutas`).
5. Filtrar por extensión (contar archivos `.cor` en `examples/`).
6. Recorrer subdirectorios del repo (listar carpetas de top-level).
7. Crear directorio con cleanup atrapando `ErrorDeIO`.

### Lo que queda pendiente

- `directorio_borrar(ruta)` — borrar directorios. No incluido en
  esta release (no se necesita aún; cuando se añada, vendrá con un
  `archivo_borrar` también).
- `mkdir -p` (crear con padres) — actualmente solo un nivel.
- Información de mtime/size — para implementar `ls -la` completo.
- Recursión: `glob`/`encontrar` para árboles enteros.

### Archivos

- `src/nativos.c` — 4 nativas nuevas (~200 líneas C con #ifdef
  POSIX/Windows) + registro en tabla `NATIVAS`.
- `stdlib/archivos.cor` — sección filesystem con 4 wrappers.
- `stdlib/ruta.cor` — 5 métodos nuevos en `Ruta` + función
  `ruta.cwd()` de módulo.
- `tests/unit/test_bytecode_fs.c` — 8 bloques, 13 asserts.
- `examples/87_filesystem.cor` — 7 secciones demo.
- `README.md`, `docs/introduccion.md`, `docs/referencia.md`:
  documentación actualizada.

### Estado

262 tests verde, lint+fmt limpios. Limitación v1.94 de
`Ruta.existe()` (solo archivos) — **cerrada**.

---

## [1.96.0] — 2026-05-18 — Stdlib `pruebas`: framework de testing minimalista (23º módulo)

Nuevo módulo `stdlib/pruebas.cor` con un framework de testing
pure-Cornamusa: asserts standalone para comprobaciones lineales y
una clase `Suite` para tests organizados con acumulación de
resultados. Cubre un hueco real — hasta v1.95 los tests de
Cornamusa se escribían en C contra la VM. Ahora un usuario puede
escribir tests para su propio código en cornamusa puro.

```cornamusa
importar pruebas
importar matematicas

# Modo 1: asserts standalone (cualquier fallo lanza ErrorDeValor)
pruebas.aseverar(2 + 2 == 4, "matematica basica")
pruebas.aseverar_igual(longitud([1, 2, 3]), 3)
pruebas.aseverar_aproximado(0.1 + 0.2, 0.3, 1e-9)

# Modo 2: Suite con casos nombrados
funcion test_factorial_5():
    pruebas.aseverar_igual(matematicas.factorial(5), 120)
fin funcion

s = pruebas.Suite("matematicas")
s.caso("factorial(5) == 120", test_factorial_5)
r = s.ejecutar()
# === Suite: matematicas ===
#   [OK]    factorial(5) == 120
# ---
# Total: 1 | Pasados: 1 | Fallados: 0
```

### Asserts standalone

Todos lanzan `ErrorDeValor` con mensaje claro en fallo. Pasan
silenciosamente si la condición se cumple.

| Assert | Lanza si |
|---|---|
| `aseverar(cond, msg)` | `cond` no es verdadero |
| `aseverar_igual(actual, esperado)` | `actual != esperado` |
| `aseverar_distinto(a, b)` | `a == b` |
| `aseverar_verdadero(c)` / `aseverar_falso(c)` | `c` distinto al valor esperado |
| `aseverar_nulo(v)` / `aseverar_no_nulo(v)` | nulidad opuesta |
| `aseverar_aproximado(a, b, tol)` | `|a-b| > tol` (default `1e-9` si `tol` es `nulo`) |
| `aseverar_contiene(c, x)` / `aseverar_no_contiene(c, x)` | pertenencia opuesta |
| `aseverar_lanza(callable, nombre)` | `callable()` no lanza o lanza tipo distinto |

`aseverar_lanza` acepta `nombre_excepcion` como cadena (p.ej.
`"ErrorAritmetico"`) — exige que aparezca en `repr` de la
excepción capturada. Si es `nulo`, basta con que `callable()`
lance cualquier excepción.

### Clase `Suite`

Acumula casos nombrados, los ejecuta capturando excepciones, y
emite resumen estilo `[OK]` / `[FAIL]`. Retorna dict con
`{total, pasados, fallados, fallos}` (donde `fallos` es lista de
`[etiqueta, repr_excepcion]`).

```cornamusa
s = pruebas.Suite("nombre")
s.caso("etiqueta", fn_de_test)   # fn no toma argumentos
r = s.ejecutar()
```

### Wrapper funcional

`pruebas.ejecutar_casos([[etiqueta, fn], ...])` instancia una
`Suite` anónima y la ejecuta. Útil cuando no se necesita la suite
para reutilizar.

### Por qué pure-Cornamusa

El módulo es ~180 líneas sin nativas nuevas. Compone sobre:

- `lanzar ErrorDeValor(msg)` para fallos
- `intentar/atrapar Excepcion como e` para capturar
- `repr(v)` para mensajes legibles
- `matematicas.absoluto` para tolerancia en `aseverar_aproximado`

Demuestra (otra vez, como `argumentos` v1.93) que la stdlib puede
crecer en cornamusa puro cuando no hay necesidad de tocar el
runtime.

### Tests

24 asserts en `test_bytecode_pruebas.c`:

- Asserts standalone pasando y fallando con mensaje claro.
- `aseverar_aproximado` pasa con tolerancia, falla si demasiado
  estricta.
- `aseverar_lanza`: con nombre correcto, mismatch, `nulo` acepta
  cualquiera, falla si no lanza.
- `Suite`: pasa todos, falla algunos, imprime `[OK]`/`[FAIL]`.
- `ejecutar_casos` wrapper funcional.

### Ejemplo

`examples/86_pruebas.cor` con 5 secciones:

1. Asserts standalone con 5 verificaciones.
2. Suite de tests de `matematicas` (factorial, mcd, signo).
3. Suite con caso fallando intencionalmente para mostrar el output.
4. `aseverar_lanza` con división por cero y ErrorDeValor.
5. Tests para el módulo `ruta` (v1.94) usando `ejecutar_casos`.

### Archivos

- `stdlib/pruebas.cor` — ~180 líneas pure-Cornamusa, 12 asserts +
  clase Suite + wrapper funcional.
- `tests/unit/test_bytecode_pruebas.c` — 11 bloques, 24 asserts.
- `examples/86_pruebas.cor` — 5 secciones demo.
- `docs/referencia.md` §16: nuevo módulo añadido.
- `README.md`, `FAQ.md`, `docs/introduccion.md`, `docs/tutorial.md`:
  stdlib pasa de veintidós a **veintitrés módulos**.

### Estado

260 tests verde, lint+fmt limpios. Stdlib alcanza **23 módulos**.

---

## [1.95.0] — 2026-05-18 — Fix compilador: pre-declarar locales nuevos antes de `si`

Arregla un bug del compilador VM detectado en v1.94 al implementar
`stdlib/ruta.cor`. Cuando una variable se declaraba por primera vez
**dentro** de una rama de un `si/sino`, el slot del stack podía
quedar desalineado si esa rama no se ejecutaba — manifestándose como
valores corrompidos (típicamente la siguiente constante del pool) o
una variable que retornaba `nulo`.

### Síntoma del bug

```cornamusa
clase X:
    funcion m(yo, otro):
        si tipo(otro) == "instancia":
            v = otro.s
        sino:
            v = otro
        fin si
        imprimir("v:", v)    # imprimía "v: v:" en vez de "v: hello"
        retornar v           # retornaba nulo en vez de "hello"
    fin funcion
fin clase
```

Llamando `X().m("hello")`, la rama "sino" se ejecuta porque "hello"
es cadena (no instancia). La salida correcta sería `v: hello` /
retorno `"hello"`. La salida buggy era `v: v:` / retorno `nulo`.

### Causa raíz

En `compilar_asignar`, una asignación a un identificador nuevo
emite `OP_NULO + agregar_local + OP_ASIGNAR_LOCAL` para reservar
y luego asignar el slot. Si esa secuencia está dentro de una rama
del `si` que no se ejecuta, el `OP_NULO` nunca corre — el slot del
stack queda sin reservar. La otra rama, al asignar al mismo nombre,
encuentra el local ya registrado en la tabla del compilador y emite
`OP_ASIGNAR_LOCAL slot` directo, pisando memoria equivocada.

### Solución (compilador.c)

En `compilar_si`, un pre-pass recolecta todos los identificadores
que se asignan **por primera vez** en cualquier rama (recursivamente,
bajando por sub-`si`s y `SENT_BLOQUE` anidados; no entra en
funciones/clases/bucles que tienen su propio scope). Antes de
emitir cualquier código de las ramas, declara cada identificador
único con `OP_NULO + agregar_local`. Las asignaciones dentro de las
ramas encuentran el local ya registrado y emiten solo
`OP_ASIGNAR_LOCAL slot`, sin crear un nuevo slot.

Solo aplica dentro de funciones (`c->actual->es_funcion`) — en
top-level las asignaciones van a globales, no a slots de stack.

### Comportamiento conservado

- Variables declaradas **solo** en una rama, leídas después del
  `si`: si esa rama no se ejecuta, la variable es `nulo`
  (comportamiento documentado de Python). El pre-pass las
  pre-declara como `nulo`, lo cual coincide con la semántica
  esperada.
- Variables ya existentes antes del `si`: el pre-pass las salta
  (`buscar_local` >= 0).
- `global X` declarados: también se saltan.

### Tests de regresión

Nuevo `tests/unit/test_bytecode_locales_si.c` con 7 bloques:

1. Si/sino con asignación + acceso a atributo en una rama (caso
   original del bug).
2. Tomar la rama `si` (con acceso a atributo) — funciona.
3. Método de clase: motivó el descubrimiento del bug en v1.94.
4. Cadena `sino si` con asignación en cada rama.
5. Múltiples variables nuevas en distintas ramas.
6. `si` anidado: variable declarada en sub-`si`.
7. Variable declarada solo en una rama (queda `nulo` si esa rama no
   se ejecuta — comportamiento esperado).

### Refactor de `stdlib/ruta.cor`

El workaround `_a_cadena()` que se introdujo en v1.94 ya no es
necesario. El método `Ruta.unir(yo, otro)` vuelve al patrón
natural:

```cornamusa
funcion unir(yo, otro):
    si tipo(otro) == "instancia":
        otro_s = otro.s
    sino:
        otro_s = otro
    fin si
    retornar Ruta(unir_partes([yo.s, otro_s]))
fin funcion
```

### Estado

258 tests verde (los 257 anteriores + el nuevo test_bytecode_locales_si).
Bug v1.94 declarado en CHANGELOG como pitfall — ahora **cerrado**.

### Archivos

- `src/compilador.c` — pre-pass `_recolectar_locales_nuevos_sent` +
  fix en `compilar_si`.
- `tests/unit/test_bytecode_locales_si.c` — 7 bloques de regresión.
- `stdlib/ruta.cor` — workaround `_a_cadena` eliminado.

---

## [1.94.0] — 2026-05-18 — Stdlib `ruta`: manipulación lexicográfica de rutas (22º módulo)

Nuevo módulo `stdlib/ruta.cor` con una clase `Ruta` al estilo
`pathlib.PurePath` de Python plus una API funcional sin instanciar.
Manipulación **lexicográfica** de rutas — no toca el sistema de
archivos (excepto `existe()`, que delega a `archivos.existe`).
Separador canónico `/`, acepta también `\` en entrada y lo
normaliza. Detección de rutas absolutas Windows (`C:/...`) además
de POSIX.

```cornamusa
importar ruta

r = ruta.Ruta("/home/david/docs/notas.txt")
r.nombre()                       # "notas.txt"
r.tronco()                       # "notas"
r.extension()                    # ".txt"
r.padre().cadena()               # "/home/david/docs"
r.partes()                       # ["/", "home", "david", "docs", "notas.txt"]
r.absoluta()                     # verdadero

# Composicion
sub = ruta.Ruta("/etc").unir("nginx").unir("conf.d")
sub.cadena()                     # "/etc/nginx/conf.d"

# Transformaciones
r.con_extension(".md").cadena()  # "/home/david/docs/notas.md"
r.con_nombre("readme").cadena()  # "/home/david/docs/readme"
```

### API funcional de módulo

| Función | Qué hace |
|---|---|
| `nombre(s)` | último componente |
| `tronco(s)` | nombre sin extensión |
| `extension(s)` | sufijo desde el último `.` (incluye el punto), `""` si no hay |
| `padre(s)` | ruta sin el último componente |
| `partes(s)` | lista de componentes (primer elemento `"/"` si absoluta) |
| `es_absoluta(s)` | `/`, `\` o letra de unidad Windows (`C:`) |
| `unir_partes(lista)` | concatena con `/`; absoluta intermedia reinicia |
| `normalizar(s)` | resuelve `.` y `..` lexicográficamente; vacío → `"."` |

### Clase `Ruta`

Envoltorio OO con todos los métodos correspondientes (`r.nombre()`,
`r.tronco()`, `r.extension()`, `r.padre()` (devuelve `Ruta` nueva),
`r.partes()`, `r.absoluta()`, `r.vacia()`, `r.unir(otro)` (acepta
cadena o `Ruta`), `r.con_nombre(nuevo)`, `r.con_extension(nueva)`,
`r.normalizada()`, `r.cadena()`, `r.existe()`).

- **`__cadena__`** integrado: `imprimir(r)` muestra la ruta como
  cadena, no `"<instancia de Ruta>"`.
- **`__igual__`** por valor: `Ruta("/x") == Ruta("/x")` es
  verdadero.

### Casos prácticos (ejemplo 85)

1. API funcional: `nombre`, `extension`, etc. sobre cadenas.
2. Clase Ruta con encadenamiento (`.unir()`).
3. Transformaciones (`.con_extension`, `.con_nombre`).
4. Clasificación de archivos por extensión.
5. Normalización de rutas con `.` y `..` (`src/./compilador/../vm.c` → `src/vm.c`).
6. Igualdad por valor entre Rutas.
7. Comprobación de existencia (delegada a filesystem).

### Tests

24 asserts en `test_bytecode_ruta.c`:

- `nombre` / `tronco` / `extension` con casos típicos y sin extensión.
- `padre` con ruta normal, raíz, vacía.
- `es_absoluta` con `/`, `C:`, vacía, relativa.
- `unir_partes` con relativos, absoluta inicial, absoluta intermedia (reset).
- `normalizar` con `..`, `.` y vacía.
- Clase `Ruta`: getters, padre devuelve `Ruta`, absoluta, partes.
- `unir` encadenado y con Ruta como argumento.
- `con_nombre` / `con_extension` (incluyendo `""` para quitar).
- Igualdad por valor.
- Normalización de separadores Windows (`C:\Users\david` → `C:/Users/david`).

### Bug encontrado y workaround

Durante el desarrollo se encontró un caso edge del compilador VM
con el patrón:

```cornamusa
clase X:
    funcion m(yo, otro):
        si tipo(otro) == "instancia":
            v = otro.s          # acceso a atributo
        sino:
            v = otro
        fin si
        imprimir("...", v)      # uso posterior con imprimir
        retornar v
    fin funcion
fin clase
```

Cuando este patrón aparece en una clase definida dentro de un
módulo importado (no en el programa principal), la variable `v`
queda con un valor incorrecto tras el `si/sino`. La solución fue
refactorizar a una función helper `_a_cadena(x)` y llamarla desde
el método:

```cornamusa
funcion _a_cadena(x):
    si tipo(x) == "instancia":
        retornar x.s
    fin si
    retornar x
fin funcion

clase Ruta:
    funcion unir(yo, otro):
        otro_s = _a_cadena(otro)
        retornar Ruta(unir_partes([yo.s, otro_s]))
    fin funcion
fin clase
```

El bug subyacente queda pendiente de investigar en una release
futura. Por ahora documentado como pitfall conocido.

### Limitación documentada

`Ruta.existe()` delega a `archivos.existe`, que solo retorna
`verdadero` para archivos regulares — no para directorios. Esto se
mejorará cuando se añada `archivo_es_directorio` nativo (release
futura).

### Archivos

- `stdlib/ruta.cor` — Ruta + API funcional (~370 líneas pure-Cornamusa).
- `tests/unit/test_bytecode_ruta.c` — 13 bloques, 24 asserts.
- `examples/85_ruta.cor` — 7 secciones demo.
- `docs/referencia.md` §16: nuevo módulo añadido.
- `README.md`, `FAQ.md`, `docs/introduccion.md`, `docs/tutorial.md`:
  stdlib pasa de veintiún a **veintidós módulos**.

### Estado

257 tests verde, lint+fmt limpios. Stdlib alcanza **22 módulos**.

---

## [1.93.0] — 2026-05-18 — Stdlib `argumentos`: parser CLI estilo argparse (21º módulo)

Nuevo módulo `stdlib/argumentos.cor` con un `Parser` pure-Cornamusa
para construir scripts CLI con argumentos posicionales, opciones
con valor (`--max 100` / `-m 100`) y banderas booleanas
(`--verboso` / `-v`). Inyecta `--ayuda` / `-h` automáticamente,
genera texto de ayuda y lanza `ErrorDeValor` atrapable en
condiciones de error.

```cornamusa
importar argumentos
importar sistema

p = argumentos.Parser("procesar-csv", "Procesa un archivo CSV")
p.posicional("entrada", "Archivo CSV de entrada", nulo, nulo)
p.posicional("salida", "Archivo CSV de salida", "cadena", "out.csv")
p.opcion("--max-filas", "-m", "Limite de filas", "entero", 1000)
p.bandera("--verboso", "-v", "Imprimir progreso")

args = p.parsear(sistema.argv)

si args["--verboso"]:
    imprimir("Procesando", args["entrada"], "→", args["salida"])
fin si
```

### API del `Parser`

| Método | Qué hace |
|---|---|
| `Parser(nombre, descripcion)` | crea el parser |
| `posicional(nombre, ayuda, tipo, defecto)` | argumento posicional. Si `defecto == nulo`, es obligatorio; si `tipo == nulo`, se asume `"cadena"` |
| `opcion(largo, corto, ayuda, tipo, defecto)` | opción con valor (`--flag valor` / `-c valor`). `corto` puede ser `nulo` |
| `bandera(largo, corto, ayuda)` | flag booleana sin valor; defecto siempre `falso` |
| `parsear(args)` | parsea `args` (lista, normalmente `sistema.argv`) y devuelve un dict |
| `ayuda()` | devuelve el texto de ayuda generado |

### Tipos soportados

- `"cadena"` — sin conversión.
- `"entero"` — convertido con `entero(...)`. Si falla → `ErrorDeValor` con mensaje claro.
- `"decimal"` — convertido con `decimal(...)`.
- `"booleano"` — acepta `verdadero/true/1/si` (case-insensitive ASCII) y `falso/false/0/no`.

### Inyección automática de `--ayuda` / `-h`

Si `--ayuda` o `-h` aparecen en cualquier posición de los argumentos
parseados, el parser imprime `p.ayuda()` y llama a `salir(0)`. No
es necesario declararlas — siempre están disponibles.

### Errores atrapables

Todos los errores de parseo son `ErrorDeValor` con mensaje prefijado
por `"argumentos:"`:

- `opcion desconocida: --xyz`
- `opcion --n requiere un valor`
- `valor invalido para --n: 'abc' no es un entero`
- `argumento posicional obligatorio ausente: archivo`
- `argumento posicional inesperado: extra`

Se atrapan con `atrapar ErrorDeValor como e:` igual que cualquier
otra excepción del lenguaje. El programa puede mostrar mensaje
propio + `salir(2)` siguiendo la convención POSIX.

### Pure-Cornamusa

Sin nativas nuevas — el módulo es ~200 líneas de Cornamusa puro
que componen sobre lo que ya existe: `sistema.argv` (v1.10),
`cadenas.unir`/`minusculas_ascii`, `agregar`/`longitud`, dicts y
listas mutables, `intentar/atrapar`, `lanzar ErrorDeValor`.
Demuestra que la stdlib puede crecer en pure-Cornamusa cuando no
hay necesidad de tocar el runtime.

### Composable con `validacion` (v1.92)

Patrón natural: parsear args + validar:

```cornamusa
importar argumentos
importar validacion

p = argumentos.Parser("crear-usuario", "")
p.opcion("--email", "-e", "Correo", "cadena", nulo)
p.opcion("--edad", "-a", "Edad", "entero", nulo)

args = p.parsear(sistema.argv)

v = validacion.Validador()
v.verificar("email", validacion.es_email(args["--email"]), "email no valido")
v.verificar("edad",  validacion.en_rango(args["--edad"], 18, 120), "edad fuera de rango")

si no v.valido():
    imprimir(v.resumen())
    salir(1)
fin si
```

### Tests

20+ asserts en `test_bytecode_argumentos.c`:

- Parseo básico con posicional + opción + bandera.
- Defaults cuando no se pasan opciones.
- Forma corta (`-m`) equivalente a la larga (`--max`).
- Tipos: entero, decimal, booleano (`verdadero`/`true`/`1`/`si` y `falso`/`false`/`0`/`no`).
- Errores: opción desconocida, posicional obligatorio ausente,
  tipo inválido, opción sin valor.
- Posicional con defecto (no obligatorio).
- Ayuda incluye nombre, descripción, posicionales, opciones
  larga+corta, banderas, mención de `--ayuda`.

### Ejemplo

`examples/84_argumentos.cor` con 4 secciones:

1. Parser típico para un comando "procesar-csv".
2. Texto de ayuda generado automáticamente.
3. Manejo de errores con `atrapar`.
4. Combinación con `validacion.Validador` para validar args.

### Archivos

- `stdlib/argumentos.cor` — Parser + helper `_convertir` (~250 líneas).
- `tests/unit/test_bytecode_argumentos.c` — 11 bloques, 20+ asserts.
- `examples/84_argumentos.cor` — 4 patterns.
- `docs/referencia.md` §16: nuevo módulo añadido.
- `README.md`, `FAQ.md`, `docs/introduccion.md`, `docs/tutorial.md`:
  stdlib pasa de veinte a **veintiún módulos**.

### Estado

255 tests verde, lint+fmt limpios. Stdlib alcanza **21 módulos**.

---

## [1.92.0] — 2026-05-18 — Stdlib `validacion`: predicados de datos + clase `Validador` (20º módulo)

Nuevo módulo `stdlib/validacion.cor` con predicados de validación
sobre datos de entrada (email, URL, fecha ISO, teléfono, rangos
numéricos, longitudes, conjunto cerrado de valores) + clase
`Validador` para acumular errores de múltiples campos en un solo
objeto. Cierra la línea pedagógica "stdlib en castellano para
casos del día a día" abierta por v1.91 (`inspeccion`) y completa el
**20º módulo** de stdlib.

```cornamusa
importar validacion

# Predicados sueltos
validacion.es_email("ana@empresa.es")          # verdadero
validacion.es_url("https://cornamusa.dev")     # verdadero
validacion.es_fecha_iso("2026-05-18")          # verdadero
validacion.en_rango(25, 18, 65)                # verdadero
validacion.longitud_en_rango("hola", 3, 10)    # verdadero
validacion.en_conjunto("rojo", ["rojo", "azul", "verde"])  # verdadero

# Validador acumulando errores
v = validacion.Validador()
v.verificar("email", validacion.es_email(form.email), "email inválido")
v.verificar("edad",  validacion.en_rango(form.edad, 18, 120), "edad fuera de rango")
v.verificar("nombre", validacion.no_vacia(form.nombre), "nombre obligatorio")

si v.valido():
    procesar(form)
sino:
    para campo, msg en v.errores:
        imprimir(campo, "→", msg)
    fin para
fin si
```

### Once predicados pure-Cornamusa

| Predicado | Verifica |
|---|---|
| `es_email(s)` | regex `^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$` (sin `{n,m}`) |
| `es_url(s)` | empieza con `http://` o `https://` y tiene host |
| `es_fecha_iso(s)` | formato `YYYY-MM-DD`, con rangos válidos de mes/día |
| `es_telefono(s)` | dígitos, espacios, guiones, paréntesis y `+` opcional al inicio |
| `en_rango(n, lo, hi)` | `lo <= n <= hi` (cerrado por ambos lados) |
| `en_rango_abierto(n, lo, hi)` | `lo < n < hi` |
| `longitud_en_rango(s, lo, hi)` | longitud de `s` entre `lo` y `hi` (cerrado) |
| `no_vacia(s)` | longitud > 0 y no solo espacios |
| `coincide(s, patron)` | `regex.coincide(patron, s)` (alias breve) |
| `en_conjunto(x, valores)` | `x` está en la lista/conjunto `valores` |

Todos hacen `tipo(s) != "cadena"` → `falso` defensivamente, así que
se pueden encadenar sin pre-checks.

### Clase `Validador`

Acumula errores por campo en un dict interno `errores`. Métodos:

- `verificar(campo, condicion, mensaje)`: si `condicion` es falsa,
  añade `{campo: mensaje}`. Si ya había un error para ese campo, se
  preserva el primero (no se sobrescribe).
- `tiene_errores()`: `longitud(errores) > 0`.
- `valido()`: `longitud(errores) == 0`.
- `resumen()`: cadena formateada `"campo: mensaje\ncampo: mensaje..."`.

Patrón intencionalmente similar a `validate` de Laravel o
`pydantic` en versiones bajas — un punto de entrada para casos
simples sin meter una dependencia de schema completa.

### Tests

20 asserts en `test_bytecode_validacion.c`:

- Cada predicado con caso verdadero, caso falso, no-cadena.
- `es_fecha_iso` con día 32 (falso), mes 13 (falso), febrero 30
  (no detectado — limitación documentada, valida sintaxis no
  calendario).
- `Validador` con 0, 1, varios errores; `tiene_errores`/`valido`;
  `resumen` con formato exacto.

### Ejemplo

`examples/83_validacion.cor` con 4 patterns:

1. Validar formulario de registro de usuario.
2. Validar lista de productos importados de CSV.
3. Validación con mensajes en castellano natural.
4. Combinar predicados sueltos sin Validador (validación funcional).

### Archivos

- `stdlib/validacion.cor` — 11 funciones + clase `Validador`
  (~150 líneas, pure-Cornamusa).
- `tests/unit/test_bytecode_validacion.c` — 20 asserts.
- `examples/83_validacion.cor` — 4 patterns.
- `docs/referencia.md` §16, `README.md`, `FAQ.md`,
  `docs/introduccion.md`, `docs/tutorial.md` — stdlib pasa de
  diecinueve a **veinte módulos**.

### Estado

252 tests verde, lint+fmt limpios. Stdlib alcanza **20 módulos**,
cerrando una fase de expansión que empezó en v1.58 (`csv`) y
acumula: `csv`, `base64`, `hashing`, `jwt`, `tiempo`, `coleccion`,
`inspeccion`, `validacion` como las ocho añadidas en esta serie.

---

## [1.91.0] — 2026-05-17 — Stdlib `inspeccion`: introspección y reflexión (19º módulo)

Nuevo módulo `stdlib/inspeccion.cor` con utilidades de introspección
para instancias, clases y módulos. Complementa los
`tiene_atributo`/`obtener_atributo`/`asignar_atributo` de v1.86 con
info estructural de alto nivel.

```cornamusa
importar inspeccion

clase Perro extiende Animal:
    funcion ladrar(yo): retornar "guau" fin funcion
fin clase

rex = Perro("Rex", 5)

inspeccion.obtener_clase(rex)        # <clase Perro>
inspeccion.obtener_nombre(rex)       # "Perro" (NO "instancia")
inspeccion.listar_metodos(rex)       # ["__iniciar__", "describir", "ladrar"]
inspeccion.listar_atributos(rex)     # ["nombre", "edad"]
inspeccion.describir(rex)            # dict completo con todo lo anterior
```

### Resuelve un problema común

Hasta v1.90, `tipo(instancia)` siempre devolvía `"instancia"` —
sin distinguir entre clases. Ahora `nombre_clase(rex)` devuelve
`"Perro"` exactamente.

### Cuatro nativas C

| Nativa | Tipos aceptados | Devuelve |
|---|---|---|
| `clase_de(inst)` | instancia | `VAL_CLASE` o `nulo` |
| `nombre_clase(x)` | instancia o clase | cadena con nombre |
| `metodos_de(x)` | instancia o clase | lista de cadenas |
| `atributos_de(inst)` | instancia | lista de cadenas |

`metodos_de` incluye métodos heredados (que `OP_HEREDAR` copia a
`clase.metodos`). `atributos_de` solo lista atributos propios, no
métodos.

### Helpers de `stdlib/inspeccion.cor`

| Helper | Comportamiento |
|---|---|
| `obtener_clase(x)` | wrapper de `clase_de` |
| `obtener_nombre(x)` | wrapper de `nombre_clase` |
| `listar_metodos(x)` | wrapper de `metodos_de` |
| `listar_atributos(x)` | wrapper de `atributos_de` |
| `es_callable(x)` | `tipo(x) en {"funcion", "clase"}` |
| `es_clase(x)` | `tipo(x) == "clase"` |
| `es_instancia(x)` | `tipo(x) == "instancia"` |
| `es_modulo(x)` | `tipo(x) == "modulo"` |
| `describir(x)` | dict completo con `tipo`, `clase`/`nombre`, `metodos`, `atributos`, `repr` |

Nota: los wrappers usan `obtener_*`/`listar_*` porque `clase` es
keyword del lenguaje, y `metodos`/`atributos` son nombres más
naturales para listas que los del nativo singular.

### Casos de uso típicos (ejemplo 82)

1. **Inspección de instancia**: `inspeccion.describir(obj)` da un
   resumen completo en una llamada.
2. **Serializador genérico** a JSON: itera `listar_atributos(obj)` y
   construye un dict con `obtener_atributo(obj, attr)`. Funciona
   para cualquier instancia sin necesidad de definir `__a_json__`.
3. **REPL helper `inspeccionar(x)`** que muestra tipo, clase,
   métodos disponibles y atributos con valores actuales — útil para
   debugging interactivo.

### Tests

16 asserts en `test_bytecode_inspeccion.c`:

- `clase_de` con instancia y con tipos primitivos.
- `nombre_clase` con instancia, con clase, rechazo de otros tipos.
- `metodos_de` con clase y con instancia (mismo resultado).
- `atributos_de` con instancia, rechazo de no-instancia.
- `es_clase`/`es_instancia`/`es_callable` con varios tipos.
- `describir` retorna dict con `tipo`/`clase`/`atributos` correctos.

### Archivos

- `src/nativos.c` — 4 nativas nuevas + helper `claves_a_lista`.
- `stdlib/inspeccion.cor` — 9 funciones de alto nivel.
- `tests/unit/test_bytecode_inspeccion.c` — 16 asserts.
- `examples/82_inspeccion.cor` — 5 casos: inspección básica, tests
  booleanos, `describir`, serializador genérico a JSON,
  REPL-style `inspeccionar` helper.
- `docs/referencia.md` §16: nuevo módulo añadido.

### Estado

250 tests verde, lint+fmt limpios. Stdlib pasa de 18 a **19 módulos**.

---

## [1.90.0] — 2026-05-17 — Cookbook ampliado: 5 recetas más (15 totales)

Cinco recetas nuevas validadas contra el intérprete, llevando el
total del cookbook a 15. Continúa la línea pedagógica abierta por
v1.79 (tutorial expandido con 35 ejercicios) y v1.82 (cookbook
inicial con 10 recetas).

### Recetas añadidas

| # | Receta | Módulos involucrados |
|---|---|---|
| 11 | Validar email con regex | `regex` |
| 12 | Merge de configuración con defaults | dicts + iteración |
| 13 | Logger con niveles DEBUG/INFO/WARN/ERROR | `tiempo`, clases |
| 14 | CSV con headers a lista de dicts | `csv`, `funcionales.combinar` |
| 15 | Ordenar lista de dicts por campo | listas mutables, sort manual |

### Caso destacado: receta 14 usa `funcionales.combinar`

```cornamusa
importar csv
importar funcionales

funcion csv_a_dicts(texto):
    filas = csv.parsear(texto)
    si longitud(filas) == 0: retornar [] fin si
    cabecera = filas[0]
    resultado = []
    para fila en filas[1:]:
        d = {}
        para par en funcionales.combinar(cabecera, fila):
            d[par[0]] = par[1]
        fin para
        agregar(resultado, d)
    fin para
    retornar resultado
fin funcion
```

Este patrón une csv (v1.58), funcionales.combinar (v1.87), y
diccionarios — un ejemplo de cómo features añadidas a lo largo de
varias releases componen bien.

### Caso destacado: receta 11 — validación de email

Documentamos honestamente que el motor regex de Cornamusa no soporta
`{n,m}`, así que usamos `[a-zA-Z][a-zA-Z]+` (TLD de 2+ letras) en
lugar de `[a-zA-Z]{2,}`. La receta da el patrón funcional y explica
la limitación.

### Validación

Las 5 recetas se ejecutaron en lotes durante la redacción. Outputs
mostrados en el cookbook son los reales del intérprete.

### Sin cambios de código

249 tests verde sin cambios desde v1.89. Bump por convención de release.

### Archivos

- `docs/cookbook.md` — 5 secciones nuevas + índice actualizado.
  Pasó de ~250 líneas a ~450.

---

## [1.89.0] — 2026-05-17 — Linter: `bool-coerce-conditional` + `for-rango-longitud` (16 categorías)

Dos checks nuevos al linter para patrones clásicos de código nuevo.
Total: 16 categorías.

### `bool-coerce-conditional`

Detecta el patrón "if/else con dos retornos booleanos":

```cornamusa
funcion es_mayor(x):
    si x > 18:
        retornar verdadero       # ← warning
    sino:
        retornar falso
    fin si
fin funcion
```

Simplificable a una sola línea:

```cornamusa
funcion es_mayor(x):
    retornar booleano(x > 18)    # o simplemente `retornar x > 18`
fin funcion
```

Para el caso invertido (`falso/verdadero`), la sugerencia es `retornar no cond`.

**Detalles del check**:
- Solo dispara si la estructura es exactamente 2 ramas (`si` + `sino`),
  cada una con UN único `retornar`, y los valores retornados son
  **literales booleanos distintos** (true/false o false/true).
- Si ambas ramas retornan el mismo valor → no dispara (es otro
  problema: `if-else-equal`, no implementado todavía).
- Si retornan no-booleanos → no dispara.
- Tres o más ramas → no dispara.

### `for-rango-longitud`

Detecta `para i en rango(longitud(X)):`, patrón no idiomático
heredado de C/Java/Pascal:

```cornamusa
xs = [1, 2, 3, 4, 5]
para i en rango(longitud(xs)):     # ← warning
    imprimir(xs[i])
fin para
```

Sugiere:
- Si NO necesitas el índice: `para x en xs:`.
- Si SÍ lo necesitas: `para par en funcionales.enumerar(xs): ...` (v1.11) o `combinar(...)` para iteración paralela (v1.87).

**Detalles del check**: solo el patrón exacto `rango(longitud(X))`
dispara. Variantes como `rango(longitud(xs) - 1)`, `rango(1, longitud(xs))`,
o `rango(longitud(xs) * 2)` no — son legítimas (ventana, salto, etc).

### Auditoría del propio repo

Los nuevos checks pillaron 3 verdaderos positivos del segundo
(`for-rango-longitud`), ninguno del primero:

- `examples/45_dict_ordenado.cor:48` — iteración paralela claves/valores.
  Refactorizado a `funcionales.combinar(claves(config), valores(config))`.
- `examples/74_depurador.cor:35` — iteración paralela precios/descuentos.
  Refactorizado a `funcionales.combinar(precios, descuentos)`.
- `examples/16_lista_busqueda.cor:10` — función `encontrar` que necesita
  el índice. Caso legítimo, pero el ejemplo se ejecuta también en
  tree-walking (sin `importar`), así que `# noqa: for-rango-longitud`
  + comentario explicando por qué.

Tras esto: 0/97 ficheros con warnings.

### Tests

9 asserts nuevos en `test_linter.c` (total: **93 asserts** cubriendo
las 16 categorías):

- `bool-coerce-conditional` con `verdadero/falso`, `falso/verdadero`,
  ramas iguales (no dispara), no-booleanos (no dispara), tres ramas
  (no dispara).
- `for-rango-longitud` con patrón básico, `rango(N)` solo (no dispara),
  `rango(longitud(xs) - 1)` (arg complejo, no dispara), `# noqa`.

### Archivos

- `src/linter.{c,h}` — 2 nuevos `LINT_*` + logica de detección en
  `case SENT_SI` y `case SENT_PARA`.
- `tests/unit/test_linter.c` — 9 asserts nuevos.
- `examples/16_lista_busqueda.cor`, `45_dict_ordenado.cor`,
  `74_depurador.cor` — fixes/noqa de hallazgos en auditoría.
- `docs/referencia.md`, `FAQ.md`: lista actualizada a 16 categorías.

### Estado

249 tests verde, repo limpio (0 warnings).

---

## [1.88.0] — 2026-05-17 — Stdlib `coleccion`: Pila, Cola, ColaDoble (18º módulo)

Tres clases de estructuras de datos clásicas en un nuevo módulo
`stdlib/coleccion.cor`. Pure-Cornamusa sobre listas mutables, API
idiomática que se lee mejor que manipular las listas directamente.

```cornamusa
importar coleccion

# Pila (LIFO):
p = coleccion.Pila()
p.poner(1); p.poner(2); p.poner(3)
p.sacar()     # 3
p.vista()     # 2 (sin sacar)

# Cola (FIFO):
c = coleccion.Cola()
c.poner("ana"); c.poner("luis")
c.sacar()     # "ana"

# ColaDoble (deque):
d = coleccion.ColaDoble()
d.poner_final("medio")
d.poner_frente("inicio")
d.poner_final("fin")
d.sacar_frente()    # "inicio"
d.sacar_final()     # "fin"
```

### API completa

| Clase | Métodos |
|---|---|
| `Pila` | `poner(x)`, `sacar()`, `vista()`, `vacia()`, `__longitud__` |
| `Cola` | `poner(x)`, `sacar()` (frente), `vista()` (frente), `vacia()`, `__longitud__` |
| `ColaDoble` | `poner_frente(x)`, `poner_final(x)`, `sacar_frente()`, `sacar_final()`, `vista_frente()`, `vista_final()`, `vacia()`, `__longitud__` |

Todas lanzan `ErrorDeValor("X vacia")` cuando se intenta sacar de
una colección vacía.

### Casos de uso típicos

- **Pila**: parser de expresiones (RPN), undo/redo simple, balanceo
  de paréntesis, evaluación postfija.
- **Cola**: BFS sobre grafos, queue de tareas FIFO, procesamiento en
  orden de llegada.
- **ColaDoble**: undo/redo con histórico de ambos lados, sliding
  windows (e.g. máximo en ventana), simulación de turnos.

### Rendimiento

Implementación pure-Cornamusa: `Pila.sacar()` y
`ColaDoble.sacar_final()` son O(1). `Cola.sacar()`,
`ColaDoble.sacar_frente()` y `poner_frente` son O(n) por el
desplazamiento de la lista interna.

Para colas con miles de operaciones podría merecer la pena una
versión nativa C con buffer circular — la API está preparada para
ese reemplazo sin cambio visible al usuario. Para uso pedagógico y
scripting típico, O(n) está bien.

### Tests

10 asserts en `test_bytecode_coleccion.c`:

- Pila: LIFO básico, `vista` no remueve, sacar vacía lanza, `vacia()`
  refleja estado.
- Cola: FIFO básico, vista del frente.
- ColaDoble: insertar y sacar por ambos extremos, error en vacía.
- **Uso real cubierto en tests**: balanceo de paréntesis con Pila +
  BFS sobre grafo con Cola.

### Archivos

- `stdlib/coleccion.cor` — 3 clases (~120 líneas).
- `tests/unit/test_bytecode_coleccion.c` — 10 asserts.
- `examples/81_coleccion.cor` — 4 patrones reales: balanceo de
  paréntesis, BFS, undo/redo en `Documento`, procesar tareas.
- `docs/referencia.md` §16: nuevo módulo añadido.

### Estado

248 tests verde, lint y fmt limpios. Stdlib pasa de 17 a **18 módulos**.

---

## [1.87.0] — 2026-05-17 — `funcionales` extendido: agrupar, tomar, saltar, combinar, aplanar, unicos

Seis helpers nuevos en `stdlib/funcionales.cor` para patrones muy
comunes de procesamiento de colecciones. Todos en pure-Cornamusa,
todos iteran sobre cualquier iterable (incluyendo generadores).

| Función | Comportamiento |
|---|---|
| `agrupar_por(xs, f)` | dict `{clave: lista}` donde `clave = f(x)` |
| `tomar(n, xs)` | primeros `n` (o menos si se agota); funciona con generadores infinitos |
| `saltar(n, xs)` | todos menos los primeros `n` |
| `combinar(xs, ys)` | lista de pares; se para con el iterable más corto |
| `aplanar(xss)` | `[[1,2],[3,4]]` → `[1,2,3,4]` (un nivel) |
| `unicos(xs)` | deduplica preservando orden de primera aparición |

### Ejemplos

```cornamusa
importar funcionales

# Clasificar por departamento:
empleados = [{"n": "Ana", "d": "Ventas"}, {"n": "Bea", "d": "RRHH"}, ...]
por_dept = funcionales.agrupar_por(empleados, lambda e: e["d"])

# Generador infinito + tomar (idiom clásico):
funcion naturales():
    n = 0
    mientras verdadero:
        producir n
        n = n + 1
    fin mientras
fin funcion
primeros_10 = funcionales.tomar(10, naturales())

# Paginación trivial:
pagina = funcionales.tomar(5, funcionales.saltar(pag * 5, items))

# Zip clásico:
funcionales.combinar(["Ana", "Bea"], [30, 25])
# → [("Ana", 30), ("Bea", 25)]

# Flatten un nivel:
funcionales.aplanar([[1, 2], [3, 4], [5]])   # [1, 2, 3, 4, 5]

# Dedup preservando orden:
funcionales.unicos([3, 1, 4, 1, 5, 9, 2, 6, 5, 3])
# → [3, 1, 4, 5, 9, 2, 6]
```

### Implementación

Pure-Cornamusa, sin nativas C nuevas. Cada función itera con `para`
y construye la salida con `agregar`/`{...}`. Para `combinar` se
materializa una `lista(xs)` y `lista(ys)` previa para poder indexar
(sin esto haría falta paralelizar iteradores, no soportado por la
sintaxis `para` actual).

### Tests

18 asserts en `test_bytecode_funcionales_v87.c`:

- `agrupar_por` con pares/impares y con clave-string (primera letra).
- `tomar` normal, más que disponibles, `n=0`, `n<0` (ambos → `[]`),
  con generador (`rango(1000)` solo lee 5).
- `saltar` normal, más que disponibles (→ `[]`), `n=0` (todo).
- `combinar` con longitudes distintas (corta con el corto), con
  iterable vacío.
- `aplanar` con sublistas, `[]`, `[[]]`.
- `unicos` preserva orden, todos iguales (→ `[1]`).
- Composición: `agrupar_por` + iterar grupos.

### Archivos

- `stdlib/funcionales.cor` — 6 funciones nuevas (~80 líneas).
- `tests/unit/test_bytecode_funcionales_v87.c` — 18 asserts.
- `examples/80_funcionales_extendido.cor` — 7 patrones (clasificar
  empleados, generador infinito + tomar, paginación, zip, aplanar
  matriz, dedup historico, histograma con barras).
- `docs/referencia.md` §16: lista de `funcionales` actualizada.

### Estado

246 tests verde, lint y fmt limpios.

---

## [1.86.0] — 2026-05-17 — Atributos dinámicos: `tiene_atributo`/`obtener_atributo`/`asignar_atributo`

Tres built-ins análogos a `hasattr` / `getattr` / `setattr` de Python.
Útil para programación dinámica: serializadores genéricos, frameworks
de validación, REPL helpers, procesar datos cuya estructura no se
conoce en tiempo de compilación.

```cornamusa
clase Producto:
    funcion __iniciar__(yo, nombre, precio):
        yo.nombre = nombre
        yo.precio = precio
    fin funcion
fin clase

p = Producto("libro", 25)

# tiene_atributo: chequeo silencioso
tiene_atributo(p, "nombre")     # verdadero
tiene_atributo(p, "stock")      # falso (sin lanzar error)

# obtener_atributo: lookup con valor por defecto
obtener_atributo(p, "nombre")            # "libro"
obtener_atributo(p, "stock")             # nulo
obtener_atributo(p, "stock", 0)          # 0

# asignar_atributo: añadir atributos al vuelo
asignar_atributo(p, "stock", 100)
p.stock                                    # 100
```

### API

| Built-in | Firma | Comportamiento |
|---|---|---|
| `tiene_atributo(obj, nombre)` | → bool | `verdadero` si el atributo existe; nunca lanza |
| `obtener_atributo(obj, nombre, defecto=nulo)` | → valor | El valor si existe, el defecto si no |
| `asignar_atributo(obj, nombre, valor)` | → nulo | Muta `obj.nombre = valor` (solo instancias) |

`nombre` debe ser una cadena (caracteres del identificador). Si no
es cadena → `ErrorDeTipo`.

### Tipos soportados

| Tipo | tiene / obtener | asignar |
|---|---|---|
| `VAL_INSTANCIA` | atributos propios + métodos heredados de la clase | sí, muta `instancia.atributos` |
| `VAL_CLASE` | métodos de la clase | **no** (`ErrorDeTipo`) |
| `VAL_MODULO` | atributos exportados | **no** (`ErrorDeTipo`) |
| Otros (entero, cadena, lista, ...) | `falso` / `defecto` (silencioso) | `ErrorDeTipo` |

### Detalle importante: métodos via `obtener_atributo`

Cuando `obtener_atributo(instancia, "metodo")` encuentra el atributo
en `clase.metodos` y es una closure normal, devuelve un `MetodoLigado`
(no la closure desnuda). Así la invocación `obtener_atributo(p, "saludar")()`
inyecta `yo` automáticamente, igual que `p.saludar()`.

Para `@estaticometodo`/`@clasemetodo` los wrappers actuales no se
desempaquetan en `obtener_atributo` — el caller recibe el `VAL_METODO_ESTATICO`/`VAL_METODO_DE_CLASE`
raw. Limitación menor: el uso típico es `obj.metodo()`, no
`obtener_atributo(obj, "metodo")()`.

### Tests

14 asserts en `test_bytecode_atributo_dinamico.c`:

- `tiene_atributo` sobre instancia (propio + heredado), clase, módulo.
- `obtener_atributo` con valor, sin defecto (devuelve `nulo`), con
  defecto.
- `asignar_atributo` muta + falla sobre no-instancia.
- `tiene_atributo` sobre tipos sin atributos (entero, cadena, lista)
  retorna `falso` silencioso, no lanza.
- Nombre no-cadena lanza `ErrorDeTipo`.
- Patrón genérico: iterar lista de nombres, leer dinámicamente los
  que existen.

### Archivos

- `src/nativos.c` — 3 nuevas nativas + helper `valor_tiene_atributo`.
- `tests/unit/test_bytecode_atributo_dinamico.c` — 14 asserts.
- `examples/79_atributo_dinamico.cor` — 6 patrones de uso: inspección
  dinámica, defecto, cargar datos externos, serializar instancia
  genérica, invocar método via lookup, atributos vs clase.

### Estado

245 tests verde, lint y fmt limpios.

---

## [1.85.0] — 2026-05-17 — `@clasemetodo` (cierra el trío OOP de decoradores)

Tras `@propiedad` (v1.78), `@estaticometodo` (v1.84), llega
`@clasemetodo`: el método recibe la clase como primer argumento.
Habilita **constructores alternativos polimórficos**.

```cornamusa
clase Forma:
    funcion __iniciar__(yo, lados):
        yo.lados = lados
    fin funcion

    @clasemetodo
    funcion triangulo(cls):
        # cls() crea instancia de la clase REAL, no de Forma.
        retornar cls(3)
    fin funcion
fin clase

clase FormaColorada extiende Forma:
    funcion __iniciar__(yo, lados):
        yo.lados = lados
        yo.color = "rojo"
    fin funcion
fin clase

t = Forma.triangulo()           # cls = Forma
tc = FormaColorada.triangulo()  # cls = FormaColorada (polimorfismo)
# tc tiene yo.color = "rojo" — el constructor de FormaColorada se invocó.
```

### Diseño

Reutiliza la infraestructura de v1.84 con un giro mínimo:

- **Nuevo `TipoValor`** `VAL_METODO_DE_CLASE` con tag GC
  `GC_TIPO_METODO_DE_CLASE`. Envuelve un `Closure *closure`.
- **Nativa `clasemetodo(callable)`** crea el wrapper.
- **`OP_OBTENER_ATRIBUTO` extendido** (en las dos ramas `VAL_CLASE`
  e `VAL_INSTANCIA`): cuando encuentra un `VAL_METODO_DE_CLASE` en
  `clase.metodos`, **crea un `MetodoLigado`** con:
  - `metodo` = closure interna.
  - `receptor` = `valor_clase(la_clase)`. Para acceso vía clase, es
    la clase directamente. Para acceso vía instancia, es
    `obj.como.instancia->clase` — clave para polimorfismo: si la
    instancia es de una subclase, el receptor es la subclase.

`MetodoLigado` ya existía y aceptaba cualquier `Valor` como receptor.
La VM inyecta el receptor en slot 0 al llamar — patrón uniforme con
métodos normales. Cero opcode nuevo.

### Polimorfismo verificado

Test 3 de la suite construye exactamente este escenario:

```cornamusa
clase Base:
    @clasemetodo
    funcion crear(cls, n):
        retornar cls(n * 10)
    fin funcion
    funcion __iniciar__(yo, v): yo.v = v fin funcion
fin clase

clase Hijo extiende Base:
    funcion __iniciar__(yo, v):
        yo.v = v + 1000   # marker para detectar quién construyó
    fin funcion
fin clase

assert(Base.crear(5).v == 50)     # cls = Base → 5*10 = 50
assert(Hijo.crear(7).v == 1070)   # cls = Hijo → 7*10 + 1000 (constructor del hijo)
```

### Trío de decoradores OOP completo

| Decorador | Recibe | Uso típico |
|---|---|---|
| (ninguno) | `yo` (instancia) | Comportamiento de la instancia |
| `@propiedad` (v1.78) | `yo` + getter sin paréntesis | Atributos computados |
| `@estaticometodo` (v1.84) | nada | Utilities, namespace bajo clase |
| `@clasemetodo` (v1.85) | `cls` (la clase) | Constructores alternativos polimórficos |

Lo único que queda en el bloque OOP de decoradores es `@x.setter`
para `@propiedad` — sigue pendiente (requiere scope dentro del
cuerpo de clase).

### Tests

11 asserts nuevos en `test_bytecode_clasemetodo.c`:

- `Clase.cm(args)` directo (cls = la clase).
- `instancia.cm(args)` (cls = clase de la instancia).
- Polimorfismo con herencia: `Hijo.cm()` recibe Hijo, no Base.
- 0 args (solo cls).
- `clasemetodo(no_callable)` lanza `ErrorDeTipo`.
- `@clasemetodo` puede usar `@estaticometodo` de la misma clase via
  `cls.helper(...)` — clase es la clase, tiene los métodos.

### Archivos

- `src/valor.{c,h}`, `src/memoria.{c,h}` — `VAL_METODO_DE_CLASE` +
  `struct MetodoDeClase` + GC integration. Patrón idéntico a v1.84.
- `src/nativos.c` — nativa `clasemetodo()` registrada.
- `src/vm.c` — `OP_OBTENER_ATRIBUTO` añade dos ramas (VAL_CLASE y
  VAL_INSTANCIA) que crean `MetodoLigado` con receptor = clase.
- `tests/unit/test_bytecode_clasemetodo.c` — 11 asserts.
- `examples/78_clasemetodo.cor` — `Forma` + `FormaColorada` con
  constructor alternativo polimórfico + `Calculadora.crear_seguro`
  combinando `@clasemetodo` y `@estaticometodo`.

### Estado

242 tests verde, repo limpio.

---

## [1.84.0] — 2026-05-17 — `@estaticometodo` + acceso `Clase.metodo`

Cierra otra reserva del bloque OOP: métodos que NO reciben `yo`
automáticamente. Combinado con el soporte nuevo para acceder atributos
de la clase (no solo de la instancia), permite el patrón "constructor
alternativo":

```cornamusa
clase Punto:
    funcion __iniciar__(yo, x, b):
        yo.x = x
        yo.b = b
    fin funcion

    @estaticometodo
    funcion origen():
        retornar Punto(0, 0)
    fin funcion

    @estaticometodo
    funcion distancia(p1, p2):
        dx = p1.x - p2.x
        dy = p1.b - p2.b
        retornar (dx * dx + dy * dy) ** 0.5
    fin funcion
fin clase

p1 = Punto.origen()        # constructor alternativo
p2 = Punto(3, 4)
d  = Punto.distancia(p1, p2)   # utility relacionada
```

### Diseño

Análogo a `@propiedad` (v1.78):

- **Nuevo `TipoValor`** `VAL_METODO_ESTATICO` con tag GC `GC_TIPO_METODO_ESTATICO`.
  Envuelve un `Closure *closure` con refcount propio.
- **Nativa `estaticometodo(callable)`** crea el wrapper. El decorador
  `@estaticometodo funcion duplicar(n):` desugara a
  `duplicar = estaticometodo(duplicar)`.
- **`OP_OBTENER_ATRIBUTO` extendido**:
  - Si la instancia accede a un método que es `VAL_METODO_ESTATICO`,
    devuelve la closure desnuda (NO crea `MetodoLigado` con `yo`).
  - **NUEVO**: si `obj` es `VAL_CLASE` (no solo `VAL_INSTANCIA`), el
    opcode ahora soporta `Clase.metodo`:
    - Método estático → devuelve closure desnuda.
    - Método normal → devuelve closure no ligada (caller pasa `yo`
      manualmente, semántica Python 3).
    - Propiedad → error claro ("propiedad de instancia, no accesible
      desde la clase").
    - Inexistente → `ErrorDeAtributo`.

### Cambio de comportamiento declarado

Hasta v1.83, `Clase.X` lanzaba `ErrorDeTipo: 'clase' no tiene atributos
accesibles`. Ahora resuelve contra el dict de métodos. **No es
breaking** porque nada legítimo dependía del error — y ahora habilita
casos antes imposibles.

### Patrones que desbloquea

```cornamusa
# 1. Constructor alternativo:
Punto.origen()
Color.desde_hex("#FF8800")

# 2. Namespace de utilities:
clase MathExtra:
    @estaticometodo
    funcion clamp(v, lo, hi):
        si v < lo: retornar lo fin si
        si v > hi: retornar hi fin si
        retornar v
    fin funcion
fin clase
MathExtra.clamp(150, 0, 100)   # 100

# 3. Acceso a método normal via clase (advanced):
fn = Persona.saludar     # closure no ligada
fn(persona, "hola")       # pasamos receptor explícito
```

### Limitaciones declaradas (siguen pendientes)

- **`@clasemetodo`** (Python `@classmethod`, recibe la clase como
  primer arg) — no implementado. Requeriría un VAL_METODO_DE_CLASE
  análogo + inyección de la clase en la llamada.
- **`@x.setter`** para `@propiedad` — sigue pendiente. Requiere scope
  dentro del cuerpo de clase para que `@area.setter` funcione, o
  sintaxis alternativa.

### Tests

14 asserts nuevos en `test_bytecode_estaticometodo.c`:

- `Clase.metodo_estatico(args)` directamente.
- `instancia.metodo_estatico(args)` sin inyectar `yo`.
- Args se pasan normalmente (no shifted).
- `estaticometodo(no_callable)` lanza `ErrorDeTipo`.
- Acceso a método no estático via `Clase.X` devuelve closure no ligada.
- Atributo inexistente de clase lanza `ErrorDeAtributo`.
- Patrón constructor alternativo (`Punto.origen()`).

### Archivos

- `src/valor.{c,h}`, `src/memoria.{c,h}` — `VAL_METODO_ESTATICO` +
  `struct MetodoEstatico` + GC integration.
- `src/nativos.c` — nativa `estaticometodo()` registrada.
- `src/vm.c` — `OP_OBTENER_ATRIBUTO` extendido con rama VAL_CLASE +
  detección de VAL_METODO_ESTATICO en lookup de instancia.
- `tests/unit/test_bytecode_estaticometodo.c` — 14 asserts.
- `examples/77_estaticometodo.cor` — `Punto` con `origen`,
  `desde_tupla`, `distancia` (utility) + `MathExtra` (namespace de
  utilities con `cuadrado` y `clamp`).

### Estado

240 tests verde, repo limpio (0/97 warnings en lint, fmt sin drift).

---

## [1.83.0] — 2026-05-17 — ESPEC.md alineado a v1.82 (deuda mayor cerrada)

ESPEC.md llevaba 30+ versiones de atraso desde la auditoría de v1.74,
declarado como deuda mayor. Esta release hace una pasada de
mantenimiento que actualiza lo crítico sin reescribir el documento
entero (sigue siendo 1098 líneas; los cambios son focalizados).

### Cambios en ESPEC.md

**Header** (líneas 1-9):
- Versión: `1.46.0` → `1.82.0`.
- Última revisión: 2026-05-16 → 2026-05-17.
- Resumen del cambio: "pasada de mantenimiento integrando v1.47-v1.82
  (stdlib expandido a 17 módulos, decoradores sobre funciones y
  métodos, `@propiedad`, suite completa de tooling con depurador
  interactivo)".

**§4.2 — Identificadores reservados sin implementar**:
- Quitada referencia a `siguiente()` built-in global (sigue sin
  existir, pero el dunder `__siguiente__` SÍ está implementado desde
  v1.43 — confundía).
- Aclarado que `__siguiente__` permite iteradores lazy stateful y la
  señal de fin es `ErrorDeIteracion`.

**§4.4 — Biblioteca estándar**:
- "Doce módulos" → "Diecisiete módulos (v1.82)".
- Tabla con columna nueva `Desde` indicando la versión de introducción.
- Añadidos: `csv` (v1.58), `tiempo` (v1.73), `base64` (v1.59),
  `hashing` (v1.60), `jwt` (v1.67). Conservados los 12 originales.
- `cadenas` perdió la entrada `caracter` (eliminada en v1.80 — uso
  `s[i]` built-in).

**§9 — Decisiones cerradas y trabajo futuro**:

Reescrito completamente. Lo que era stale:

- "Stdlib amplia — doce módulos" → "diecisiete módulos (v1.8-v1.73)".
- `borrar`/`global` listados como reservas pendientes → movidos a
  "Implementado desde v1.1" con versiones v1.56/v1.57.
- "Decoradores (`@deco`)" — "Soporte parcial en el parser" → completo
  desde v1.72 (funciones), v1.77 (métodos), v1.78 (`@propiedad`).

Sección nueva **Tooling (Fase 5, v1.47-v1.76)** con tabla de los 7
subcomandos integrados (`fmt`/`lint`/`docs`/`lsp`/`prof`/`cov`/`depurar`)
y versión de introducción de cada uno. REPL con line editing mencionado.

Lista de reservas pendientes **reducida y honesta**:

1. Async/await (`asincrono`/`esperar`) — v2.x.
2. Prefijos de cadena `r"..."` / `b"..."`.
3. Tipos numéricos exactos (`Fraccion`, `Decimal`) — futuro stdlib.
4. Anotaciones de tipo `: tipo` — el parser las acepta, runtime las ignora.
5. Decoradores sobre clases o `@x.setter`/`@estaticometodo`/`@clasemetodo`.

Sección nueva **Trabajo de runtime pendiente** añade:

- **TLS/HTTPS** en `red` — el cliente HTTP/1.1 actual es plano.
  Requiere decisión sobre dep externa (OpenSSL/schannel).
- **Setter para `@propiedad`** — requiere scope dentro del cuerpo de
  clase para resolver `@area.setter` Python-style, o sintaxis
  alternativa.

### Lo que NO se tocó

- **Gramática (§5 PEG)**: no se modificó. La sintaxis core no cambió
  sustancialmente entre v1.46 y v1.82 — los cambios fueron en
  semántica/stdlib/tooling. Una pasada formal a la gramática merece
  release dedicada.
- **§6 Semántica clave**: revisado por encima, sigue describiendo
  correctamente lo que la VM hace.
- **§7 Programa de ejemplo**: el ejemplo sigue siendo válido y
  representativo.

### Estado

239 tests verde sin cambios. ESPEC.md crece de 1098 a ~1115 líneas
(tabla de tooling + reservas más completas; -10 líneas por reservas
quitadas que ya están implementadas).

---

## [1.82.0] — 2026-05-17 — Cookbook con 10 recetas validadas

Nueva pieza de documentación: `docs/cookbook.md`. Complementa el
tutorial (que enseña el lenguaje secuencialmente) y la referencia
(que lista APIs) convirtiendo features dispersas en patrones útiles
listos para copy-paste.

### Recetas incluidas

| # | Receta | Módulos involucrados |
|---|---|---|
| 1 | Procesar CSV y agregar | `csv` |
| 2 | JWT con expiración | `jwt`, `tiempo` |
| 3 | Hash SHA-256 de un valor | `hashing` |
| 4 | Backoff exponencial para reintentos | `tiempo`, `intentar/atrapar` |
| 5 | Memoización con decorador | decoradores (v1.72) |
| 6 | Cronometrar bloques de código | `tiempo.monotonic`/`cronometro` |
| 7 | Atributos computados con `@propiedad` | clases, `@propiedad` (v1.78) |
| 8 | Parser básico de argumentos del programa | `sistema.argv` |
| 9 | Contar frecuencias (counter dict-style) | `cadenas`, diccionarios |
| 10 | JSON pretty-print para configuración | `json.serializar_indentado` |

Cada receta:

1. Enuncia el **problema** concreto en una frase.
2. Da el **código completo** ejecutable (no fragmentos sueltos).
3. Muestra el **output esperado** verificado contra el intérprete.
4. Cuando aplica, lista **variantes** comunes ("si el separador es
   `;`...", "si la firma es por archivo...").

### Validación

Las 10 recetas se ejecutaron en dos lotes con `cornamusa --bytecode`
durante la redacción; los outputs mostrados en el cookbook son los
reales (con la excepción de las marcas de tiempo de `cronómetro` y
similares, que varían entre ejecuciones).

### Por qué un cookbook separado del tutorial

- El **tutorial** enseña progresivamente: si quieres aprender el
  lenguaje, vas en orden.
- La **referencia** es densa: si quieres lookup rápido de sintaxis o
  APIs.
- El **cookbook** asume que ya conoces lo básico y resuelves un
  problema concreto. Llegas con "¿cómo firmo un JWT?", encuentras la
  receta, copia-pega, ajustas a tu caso.

Es el tercer eje pedagógico que en v1.79 había marcado como movimiento
de alto valor para el nicho de Cornamusa (enseñar a programar en
castellano). Tras tutorial + cookbook, queda la documentación
**referencia** y **especificación formal** como ejes técnicos.

### Enlaces actualizados

- README §Documentación: añadido enlace a Cookbook.
- `docs/introduccion.md`: nueva sección "Si quieres copy-paste para
  una tarea común" entre "Referencia" y "Especificación".

### Sin cambios de código

239 tests verde sin cambios desde v1.81. Bump de versión por convención.

---

## [1.81.0] — 2026-05-17 — Linter: `redundant-bool-compare` + `useless-return` (14 categorías)

Dos checks nuevos al linter, ambos pequeños pero útiles. Total: 14
categorías.

### `redundant-bool-compare`

Detecta comparaciones redundantes con literales booleanos:

```cornamusa
si activo == verdadero:       # ← warning
    ...
fin si
# Mejor: si activo:

si flag == falso:             # ← warning
    ...
fin si
# Mejor: si no flag:

si x != verdadero:            # ← warning
    ...
fin si
# Mejor: si no x:

si x != falso:                # ← warning (poco común pero válido)
    ...
fin si
# Mejor: si x:
```

**Caso deliberado**: a veces SÍ quieres `== verdadero` para verificar
estrictamente el tipo (e.g., tras un round-trip JSON donde un valor
podría haber llegado como `"verdadero"` cadena vs `verdadero` bool).
Suprime con `# noqa: redundant-bool-compare`.

**Excepción del check**: `verdadero == falso` (ambos literales) NO
dispara — es comparación constante, otro tipo de problema.

### `useless-return`

Detecta `retornar` o `retornar nulo` al final del cuerpo de una
función:

```cornamusa
funcion log(msg):
    imprimir(msg)
    retornar nulo        # ← warning: Cornamusa retorna nulo por defecto
fin funcion

funcion side_effect():
    imprimir("hola")
    retornar             # ← warning: idem
fin funcion
```

**Refinamiento importante**: el patrón "find-returns-nil" tras un
bucle o condicional NO dispara, porque comunica intención
(`buscar(xs, k)` que devuelve `nulo` si no encuentra):

```cornamusa
funcion buscar(xs, k):
    para x en xs:
        si x == k:
            retornar x
        fin si
    fin para
    retornar nulo        # ← NO dispara: tras `para`, comunica intención
fin funcion
```

El check excluye casos donde la penúltima sentencia es: `para`,
`mientras`, `si`, `intentar` o `coincidir`. Tras una sentencia
"lineal" (asignación, expr, imprimir), sí dispara.

### Auditoría del propio repo

Los nuevos checks pillaron 3 verdaderos positivos:

- `examples/32_json_archivos.cor:45-46`: 2× `== verdadero/falso` en
  verificación deliberada de round-trip JSON. Suprimidos con `# noqa`
  + comentario explicando por qué se quedan.
- `examples/25_biblioteca_oop.cor:108`: 1× `retornar nulo` tras un
  `para`. Era el caso find-returns-nil que motivó el refinamiento
  del check — sin él, el ejemplo limpio habría disparado falso
  positivo. Tras el refinamiento, no dispara.

Tras esto, **0/97 ficheros con warnings** en lint + 0 drift en fmt.

### Tests

11 asserts nuevos en `test_linter.c` (total: 84 asserts cubriendo
las 14 categorías):

- `redundant-bool-compare`: `== verdadero`, `== falso`, `!= verdadero`,
  `verdadero == falso` (no dispara), comparación con cadena (no
  dispara), `# noqa` lo suprime.
- `useless-return`: `retornar nulo`, `retornar` sin valor, con valor
  no nulo (no dispara), patrón find-returns-nil tras `para` (no
  dispara), tras `si` (no dispara).

### Archivos

- `src/linter.{c,h}` — 2 nuevos `LINT_*`, helpers `categoria_a_bit`
  + `linter_tipo_nombre`, lógica de detección en `visitar_expr` (para
  bool-compare) y `case SENT_FUNCION` (para useless-return).
- `tests/unit/test_linter.c` — 11 asserts.
- `examples/32_json_archivos.cor` — 2 `# noqa` con comentario.

### Estado

239 tests verde, repo limpio.

---

## [1.80.0] — 2026-05-17 — Limpieza de stdlib (dead code + params renombrados)

Release de mantenimiento tras 4 releases consecutivas de features
(v1.76-v1.79). Cierra la deuda detectada en la auditoría de v1.74 y
no abordada hasta ahora.

### Dead code eliminado

| Símbolo | Donde | Razón |
|---|---|---|
| `funcionales.enumerar_desde(xs, inicio)` | `stdlib/funcionales.cor` | Deprecada desde v1.17 (56 versiones), cero usos en repo. Usa `enumerar(xs, inicio)`. |
| `funcionales.suma_desde(xs, inicial)` | `stdlib/funcionales.cor` | Deprecada desde v1.17, cero usos. Usa `suma(xs, inicial)`. |
| `cadenas.caracter(s, i)` | `stdlib/cadenas.cor` | Wrapper trivial de `s[i]` (built-in syntax). Sin usos en stdlib. Una sola referencia en `examples/22_modulos_avanzado.cor` actualizada para usar `s[i]` directamente. |
| `formato._repetir`, `_unir`, `_indice_de` | `stdlib/formato.cor` | Helpers privados que eran workaround de un bug de re-import en v1.18, ya resuelto en v1.18.1. Reemplazados por llamadas directas a `cadenas.repetir`/`cadenas.unir`/`cadenas.indice_de`. |

Total: ~80 líneas menos en stdlib.

### Param `cadena` renombrado a `s`

En `base64.cor` y `hashing.cor` el parámetro de las funciones se
llamaba `cadena`, que sombrea el conversor built-in `cadena(x)`
dentro del cuerpo:

```cornamusa
# Antes (peligroso si dentro queremos convertir algo):
funcion sha256(cadena):
    retornar hash_sha256(cadena)
fin funcion

# Ahora:
funcion sha256(s):
    retornar hash_sha256(s)
fin funcion
```

Cambio cosmético, pero la auditoría lo marcó como foot-gun real
(8 funciones afectadas entre los dos módulos). Sin breaking change
visible — los argumentos siguen siendo posicionales.

### `cadenas.contar` PRESERVADO

La auditoría lo marcó como dead code (cero usos en repo), pero
`examples/22_modulos_avanzado.cor` lo importa selectivamente. Más
importante: NO tiene reemplazo nativo directo, así que quitarlo
sería regresión real para cualquier usuario externo que cuente
ocurrencias de substring. Se mantiene.

### Comentarios obsoletos limpiados

- Mensaje de error `como_hex requiere n >= 0 en v1.18` → quitado el
  `"en v1.18"` (la limitación sigue, pero ya no está pinned a una
  versión específica).
- `como_binario` igual.
- Nota técnica v0.9.x sobre bug del compilador en `cadenas.contar`
  → quitada (el workaround sigue funcionando pero la causa original
  está resuelta).
- Comentario sobre `formato._*` aliases — desaparece junto con los
  helpers.

### `docs/referencia.md` actualizado

§16 (Biblioteca estándar):
- `cadenas`: quitado `caracter(s,i)` de la lista; añadida nota
  "Para `s[i]` usa la indexación built-in".
- `funcionales`: quitada mención a `enumerar_desde`/`suma_desde`.

### Riesgo de breaking change

**Bajo**. `enumerar_desde`/`suma_desde` estaban marcados deprecados
desde v1.17 (16 meses si una versión fuera un mes). `cadenas.caracter`
era wrapper trivial. Los helpers `_*` de `formato` eran privados.
Pero **sí** rompo compat con cualquier código externo que dependa
de estas funciones — declarado.

### Tests

239 tests verde sin cambios.

---

## [1.79.0] — 2026-05-17 — Tutorial expandido a material didáctico completo

Cierra el tercer eje del plan de mejora propuesto tras la autoevaluación
(3/10 compite global, 9/10 pedagógico-en-castellano): si el nicho real
de Cornamusa es enseñar a programar en castellano, la palanca es la
documentación didáctica.

Esta release añade **~35 ejercicios** distribuidos por las 15
secciones del tutorial, más un **anexo de soluciones validadas**
contra el intérprete. Cada solución se probó corriéndola; los outputs
mostrados coinciden con los reales.

### Ejercicios añadidos

| Sección | # ej. | Cubre |
|---|---|---|
| §1 Tu primer programa | 3 | `imprimir`, argumentos múltiples |
| §2 Variables y tipos | 3 | tipado dinámico, bignum |
| §3 Operadores | 3 | aritmética, ternaria, rangos |
| §4 Control de flujo | 4 | **FizzBuzz**, tabla del 7, palíndromos |
| §5 Destructuring | 2 | swap idiomático, en bucles |
| §6 Funciones | 4 | factorial, `es_primo`, HOF |
| §7 Estructuras de datos | 3 | diccionarios, counters |
| §8 Comprehensions | 3 | con filtros, ternaria dentro |
| §9 Cadenas | 3 | iniciales, contar vocales |
| §10 Clases | 4 | `Libro`, `Cuenta`, `Fraccion` con `@propiedad` |
| §14 Errores | 3 | `intentar`, lanzar tipos correctos |
| §15 Módulos | 3 | matemáticas, archivos, azar |

Cada bloque sigue el mismo patrón: 2-4 ejercicios progresivos (el
primero directo, el último algo más desafiante), enlace al anexo,
sin solución inline para no romper la práctica.

### Anexo de soluciones

Sección nueva entre §16 y §17 con código ejecutable para todos los
ejercicios. Validación hecha en bloque corriendo dos scripts de prueba
contra `cornamusa --bytecode`; outputs cuajan con los esperados en
el anexo.

Una nota importante en la introducción del anexo: "hay varias maneras
correctas de resolver cada ejercicio — la solución mostrada es UNA
forma idiomática, no necesariamente la única". Esto importa para
estudiantes que llegan con soluciones distintas y válidas.

### Tutorial creció de 1038 a 1502 líneas

Aproximadamente +45%, todo en valor pedagógico (no padding). La
estructura de las 17 secciones originales se preserva intacta — los
ejercicios y el anexo son aditivos.

### Estado

No hay cambios de código (la versión bumpea por convención de release).
239 tests verde se mantienen sin cambios desde v1.78.

### Cierre del plan "qué hacer para mejorar"

Esta release cierra los tres movimientos propuestos en la
autoevaluación 3/10:

1. ✅ v1.76: **Debugger interactivo** (`cornamusa depurar`).
2. ✅ v1.77 + v1.78: **Decoradores sobre métodos + `@propiedad`**.
3. ✅ v1.79: **Material didáctico expandido**.

Tras estas tres releases, el subscore de tooling pasa de 7 a 8 (suite
completa: fmt/lint/docs/lsp/prof/cov/depurar), el de lenguaje técnico
de 6 a 6.5 (`@propiedad` + decoradores sobre métodos), y el de
pedagógico-en-castellano de 9 a 9.5 (curso con ejercicios validados).

---

## [1.78.0] — 2026-05-17 — `@propiedad`: getters automáticos

Convierte un método en getter automático que se invoca al acceder al
atributo (sin paréntesis). Útil para atributos computados y para
encapsulación ligera:

```cornamusa
clase Rectangulo:
    funcion __iniciar__(yo, ancho, alto):
        yo.ancho = ancho
        yo.alto = alto
    fin funcion

    @propiedad
    funcion area(yo):
        retornar yo.ancho * yo.alto
    fin funcion
fin clase

r = Rectangulo(3, 4)
imprimir(r.area)    # → 12 (sin parentesis: el getter se invoca solo)
```

### Diseño

- **Nuevo `TipoValor`** `VAL_PROPIEDAD` con tag GC `GC_TIPO_PROPIEDAD`.
  Envuelve un `Closure *getter` con refcount propio.
- **Nativa `propiedad(callable)`** crea el wrapper. El decorador `@propiedad`
  desugara a `area = propiedad(area)` igual que cualquier decorador de
  función — la única diferencia es que `propiedad()` devuelve un
  `VAL_PROPIEDAD` en lugar de otra closure.
- **`OP_METODO` sin cambios**: acepta cualquier valor como entrada del
  dict `clase.metodos`. Las propiedades se guardan ahí junto a los
  métodos normales.
- **`OP_OBTENER_ATRIBUTO` distingue**: si la entrada del dict es
  `VAL_PROPIEDAD`, despacha el getter con `yo` como argumento usando
  el mismo helper que los dunders unarios (`ejecutar_dunder_unario`).
  El frame del getter deja su retorno en el TOS, exactamente donde el
  opcode original habría dejado el valor cacheado.
- **Cache fast-path NO promueve** propiedades: el `OP_OBTENER_ATRIBUTO_INSTANCIA_CACHE`
  cachea valores de `instancia.atributos`, no de `clase.metodos`, así
  que las propiedades naturalmente toman el slow path cada vez (que
  invoca el getter, comportamiento correcto).

### Ejemplo de uso

Atributo computado típico:

```cornamusa
clase Temperatura:
    funcion __iniciar__(yo, celsius):
        yo._celsius = celsius
    fin funcion

    @propiedad
    funcion fahrenheit(yo):
        retornar yo._celsius * 9 / 5 + 32
    fin funcion

    @propiedad
    funcion kelvin(yo):
        retornar yo._celsius + 273.15
    fin funcion
fin clase

t = Temperatura(25)
imprimir(t.fahrenheit)   # 77.0
imprimir(t.kelvin)       # 298.15
```

### Limitaciones declaradas

- **Solo getter**. No hay setter (`@x.setter`). Asignación a atributo
  marcado con `@propiedad` no funciona como Python — actualmente
  sobrescribe la propiedad con el valor crudo en `inst.atributos`
  (donde gana sobre la propiedad en lookups futuros). Workaround: el
  setter es siempre un método explícito (`obj.set_x(valor)`).
- **`@estaticometodo`/`@clasemetodo`** quedan pendientes. Requerirían
  más TipoValor o un esquema de descriptor más general.
- **Propiedad no se ve en `dir()` ni reflexión**. Aceptable para esta
  primera versión.

### Tests

13 asserts nuevos en `test_bytecode_propiedad.c`:

- Propiedad básica (`r.area` → 12).
- Múltiples propiedades en la misma clase (área y perímetro
  independientes).
- Propiedad usa atributos de la instancia (distintas instancias dan
  distintos resultados).
- Propiedad puede lanzar; la excepción se propaga normalmente y se
  puede atrapar.
- `propiedad()` con argumento no-callable lanza `ErrorDeTipo`.
- Acceso repetido al atributo invoca el getter cada vez (no es cache).

### Archivos

- `src/valor.{c,h}` — `VAL_PROPIEDAD` + `struct Propiedad` con
  refcount + constructor/retener/liberar.
- `src/memoria.{c,h}` — `GC_TIPO_PROPIEDAD` marcado/barrido.
- `src/nativos.c` — nativa `propiedad()` registrada.
- `src/vm.c` — `OP_OBTENER_ATRIBUTO` detecta `VAL_PROPIEDAD` y
  despacha el getter.
- `tests/unit/test_bytecode_propiedad.c` — 13 asserts.
- `examples/76_propiedad.cor` — `Rectangulo` con `area/perimetro/es_cuadrado`
  y `Temperatura` con `fahrenheit/kelvin`.

### Estado

238 tests verde. Cierra el grueso del bloque OOP/decoradores junto
con v1.72 y v1.77.

---

## [1.77.0] — 2026-05-17 — Decoradores `@x` sobre métodos de clase

Cierra la limitación declarada desde v1.72: hasta v1.76 los decoradores
sobre métodos lanzaban error explícito en compile-time. v1.77 los
implementa propiamente.

```cornamusa
funcion contar_llamadas(f):
    cuenta = [0]
    funcion w(yo, monto):
        cuenta[0] = cuenta[0] + 1
        imprimir(f"  (llamada #{cuenta[0]})")
        retornar f(yo, monto)
    fin funcion
    retornar w
fin funcion

clase Banco:
    @contar_llamadas
    funcion depositar(yo, monto):
        yo.saldo = yo.saldo + monto
    fin funcion
fin clase
```

Stacking y factories (`@retry(3)`) también soportados en métodos.

### Nuevo opcode: `OP_INTERCAMBIAR`

Intercambia los dos elementos del tope del stack. Necesario porque
durante la compilación de un método decorado, el stack debe quedar:

```
[..., clase, closure]           tras OP_CLOSURE
compilar(decorador)             -> [..., clase, closure, decorador]
OP_INTERCAMBIAR                 -> [..., clase, decorador, closure]
OP_LLAMAR 1                     -> [..., clase, decorador(closure)]
```

La clase se preserva debajo del stack durante toda la cadena de
decoradores; el resultado final pasa a `OP_METODO` que la guarda como
método de la clase.

Implementación trivial: 2 stores. Sin clonado de valores — solo
intercambia las structs `Valor` (la propiedad se transfiere intacta).

### `@propiedad`, `@estaticometodo`, `@clasemetodo`: NO en esta release

Quedan declarados como pendientes para v1.78. Requieren:

- Nuevo `TipoValor` `VAL_PROPIEDAD` o equivalente.
- Modificaciones a `OP_OBTENER_ATRIBUTO_INSTANCIA` (slow + cache) para
  detectar el caso y despachar el getter.
- Tabla aparte en `Clase` o discriminación por tipo en `metodos`.

Alcance que merece release dedicada. v1.77 cubre el 95% del uso real
(decoradores de logging/cache/retry/auth sobre métodos).

### Tests

Test actualizado en `test_bytecode_decoradores.c`: el test que antes
verificaba `decorador_metodo_es_error` ahora verifica
`decorador_metodo_ejecuta` con resultado correcto (10 + 5 + 1000 =
1015, donde +1000 lo añade el decorador).

### Archivos

- `src/chunk.h`, `src/chunk.c`, `src/debug.c` — nuevo opcode
  `OP_INTERCAMBIAR`.
- `src/vm.c` — handler del opcode + revertido check de error en
  `compilar_clase`, ahora emite el desugar.
- `src/compilador.c` — `compilar_clase` aplica decoradores por
  método.
- `examples/75_decoradores_metodos.cor` — demo con `Banco` (contador
  de llamadas) y `Saludador` (stacking).

### Estado

236 tests verde. Repo limpio.

---

## [1.76.0] — 2026-05-17 — Debugger interactivo (`cornamusa depurar`)

Nuevo subcomando que ejecuta un script bajo un debugger interactivo
con prompt `(dep)`. Cierra el último hueco del tooling de Fase 5
tras `fmt`/`lint`/`docs`/`lsp`/`prof`/`cov`.

```
$ cornamusa depurar examples/74_depurador.cor
[examples/74_depurador.cor]
  >    1  precios = [10, 25, 7, 42, 18]
       2  descuentos = [0.0, 0.1, 0.0, 0.2, 0.05]
       3
(dep) b 31
  breakpoint en linea 31
(dep) c
  ...
(dep) p total
  total = 90.2
(dep) pila
  Pila (mas reciente arriba):
    #0  <top-level>  (linea 31)
(dep) c
```

### Comandos

| Atajo | Largo        | Acción |
|-------|--------------|--------|
| `c`   | `continuar`  | sigue hasta próximo breakpoint o fin |
| `s`   | `paso`       | step into (pausa en próxima línea, cualquier frame) |
| `n`   | `siguiente`  | step over (pausa solo en mismo frame o ancestral) |
| `r`   | `retornar`   | step out (continúa hasta volver del frame actual) |
| `b N` | `break N`    | breakpoint en línea N |
| `bd N`| `borrar N`   | borra breakpoint en línea N |
| `bs`  | `breaks`     | lista breakpoints activos |
| `l`   | `lista`      | muestra código alrededor del IP |
| `p X` | `imprimir X` | muestra valor de la global X |
| `pila`| `stack`      | backtrace de frames |
| `q`   | `salir`      | aborta el programa |
| `?`   | `ayuda`      | help |

### Diseño

- **Hook único** en el dispatch loop (mismo punto que profiler/cov).
  Detecta cambio de línea o de frame y consulta `dep_debe_pausar` para
  decidir si entrar al loop interactivo.
- **Modos de step**: estado interno con `frame_objetivo` para distinguir
  `siguiente` (pausa solo si `n_frames <= objetivo`) y `retornar` (pausa
  si `n_frames < objetivo`).
- **Listing**: el debugger guarda una copia de la fuente con offsets
  de inicio de cada línea precomputados — listing en O(1).
- **Pausa inicial**: al activar `depurador_activar` entra en modo
  `DEP_PASO`, así el usuario puede poner breakpoints antes de empezar
  el programa.

### Limitaciones declaradas

- **Inspección solo de globales** (`p NOMBRE` busca en `vm->globales`).
  Locales de función no son accesibles por nombre — el chunk no guarda
  el mapping nombre→slot. Si quieres ver una variable dentro de
  función, hazla `global` o pásala como argumento que se asigna a
  global. Para una v1.x futura se puede añadir tabla de debug en el
  chunk.
- **`siguiente`/`retornar` operan a nivel de frame**, no de línea
  física: una expresión multi-línea cuenta como una sola "siguiente
  línea". Casos extremos son raros porque Cornamusa requiere bloques
  de varias líneas.
- **Stdin/stdout del programa** comparten consola con el debugger —
  los outputs del programa aparecen entremezclados con los prompts
  `(dep)`. Aceptable para uso CLI pero hace ruido en sesiones largas.

### Tests

25 asserts nuevos en `test_depurador.c` que alimentan comandos por
stdin redirigido y verifican stdout capturado:

- `continuar` inmediato ejecuta el programa entero.
- `paso` muestra prompt en cada línea, `p` imprime globales.
- Breakpoint en línea con bucle pausa en cada iteración.
- `p` con nombre no definido reporta claramente.
- `q` aborta antes de ejecutar resto.
- `?` imprime ayuda completa.
- `pila` muestra `<top-level>`.
- `b 2` + `bs` + `bd 2` + `bs` flujo completo.
- Comando desconocido reporta y sigue sin abortar.

### Archivos

- `src/depurador.{c,h}` — módulo nuevo (header + indexado de líneas).
- `src/vm.{c,h}` — `Depurador` embebido en `VM`, loop interactivo
  vive en `vm.c` para acceso a frames/globales.
- `src/main.c` — subcomando `depurar` (alias `debug`).
- `tests/unit/test_depurador.c` — 25 asserts.
- `examples/74_depurador.cor` — script con cálculo de IVA preparado
  para demo del debugger.

### Estado

235 tests verde. Cierra Fase 5 (tooling) completamente: con `depurar`
Cornamusa tiene formato + linter (12 categorías) + generador de docs
+ LSP + profiler + coverage + debugger.

---

## [1.75.0] — 2026-05-17 — Coverage tracker (`cornamusa cov`)

Nuevo subcomando que ejecuta un script con un tracker de líneas
ejecutadas y reporta el porcentaje cubierto. Reusa la misma
arquitectura del profiler (hook al inicio del dispatch loop, cero
coste cuando inactivo) — registra líneas tocadas en lugar de tiempos.

```bash
cornamusa cov script.cor
# (stderr)
# script.cor: 77.8% (7/9 lineas top-level)
#   NOTA v1.75: solo cubre el codigo top-level del archivo principal.

cornamusa cov --uncovered script.cor
# (stderr)
# script.cor: 77.8% (7/9 lineas top-level)
#   lineas no cubiertas: 5, 16
```

### Diseño

- **Hook único** al inicio del dispatch loop: extrae la línea del
  opcode actual y la marca en un bitset si pertenece al chunk
  objetivo (el del archivo principal). Cuando inactivo es un branch
  sobre la bandera `cov.activo` (cero coste real).
- **Fast path**: el hook tiene short-circuit "si la línea es la
  misma que la anterior, skip" — la mayoría de iteraciones del
  dispatch caen aquí (varios opcodes por línea de fuente).
- **Denominador (líneas ejecutables)**: se calcula recorriendo
  `chunk->lineas[]` al final, recogiendo las líneas únicas que
  tienen al menos un byte de bytecode asociado.
- **Bitset por línea** crece exponencialmente con `realloc`. Soporta
  archivos arbitrariamente grandes con uso de memoria moderado.

### CLI

```
cornamusa cov [--uncovered] <archivo.cor> [args...]
  --uncovered   lista las lineas no cubiertas tras el porcentaje
```

Los `args...` se pasan al programa via `sistema.argv`.

### Limitaciones declaradas

- **Solo el chunk principal**: cuerpos de funciones/closures compilan
  en chunks propios (un `FuncionBC.chunk` por función), no en el chunk
  top-level. Coverage v1.75 **no rastrea esos chunks**. Limitación
  significativa que afecta a la métrica: un programa entero dentro
  de `funcion main(): ... fin funcion` reportaría cobertura solo de la
  línea `main()`. Workaround: dejar el código relevante en top-level
  o esperar a una versión futura que rastree todos los chunks
  alcanzables.
- **No produce report HTML/XML**: salida texto a stderr; integradores
  pueden parsear el `archivo: N% (X/Y)` línea. Formato JSON queda
  para si surge demanda.
- **No distingue "ejecutó parcialmente"**: una línea con varios
  branches inline (`x si cond sino y`) se marca tocada con que
  cualquier opcode de esa línea ejecute. Para branch-coverage
  haría falta un modelo más fino.

### Tests

15 asserts nuevos en `test_coverage.c`:

- Inactivo no genera reporte (`n_ejecutables == 0`).
- Script lineal: 100% cubierto.
- Rama no tomada del `si` queda como uncovered (verifica línea
  exacta en la lista).
- Bucle: 100% cubierto cuando todas las iteraciones tocan las
  mismas líneas.
- Bucle con condicional dentro que nunca es true: rama uncovered.
- Código tras `romper` en bucle (linter ya marca `unreachable`).

### Archivos

- `src/coverage.{c,h}` — módulo nuevo.
- `src/vm.{c,h}` — `CovTracker` embebido en `VM`, hook en dispatch.
- `src/main.c` — subcomando `cov` con `--uncovered`.
- `tests/unit/test_coverage.c` — 15 asserts.
- `examples/73_coverage.cor` — demo con coverage parcial intencional.

### Estado

233 tests verde. Repo limpio (lint y fmt sin diferencias). Cierra
junto con `prof` el grupo de herramientas de runtime introspection.

---

## [1.74.0] — 2026-05-17 — Auditoría: tests-gap cerrados + fix de `ErrorDeClave` atrapable

Release de auditoría tras revisión completa del proyecto. No añade
features nuevas — cierra los huecos de cobertura detectados y arregla
un bug latente descubierto en el camino.

### Fix: `d[clave_ausente]` ahora es atrapable

`OP_INDICE` sobre diccionario con clave ausente hacía
`return VM_ERROR_RUNTIME` directo en lugar de `RAISE_OR_DIE()`. El
resultado: `atrapar ErrorDeClave` (y `atrapar Excepcion`) no
capturaban el error — el programa terminaba con traza fatal aunque
hubiera un handler activo.

```cornamusa
d = {"a": 1}
intentar:
    x = d["xyz"]      # antes: fatal. Ahora: atrapable.
atrapar Excepcion como e:
    imprimir("ok:", e)
fin intentar
```

Olvido de v1.10 cuando `RAISE_OR_DIE` se introdujo para hacer
atrapables `ErrorDeTipo`/`ErrorDeIndice`/etc. — este sitio se quedó
sin migrar. Los demás sitios con `ErrorDeClave` (en `borrar d[k]`,
`borrar conj.x`) ya estaban bien.

### Tests añadidos: seguridad JWT (mitigaciones declaradas)

`test_bytecode_jwt.c` ahora verifica explícitamente las mitigaciones
de seguridad anunciadas en docs:

- **`alg=none` rechazado** (RFC 7519 §6.1 attack): atacante construye
  token con `{"alg":"none"}` y firma vacía. Cornamusa siempre intenta
  HMAC-SHA-256, así que `"" ≠ HMAC(clave, mensaje)` y rechaza.
- **`alg=RS256` rechazado** sin clave: atacante no puede forjar firma
  HMAC válida, el chequeo falla primero.
- **alg confusion (HS256 ↔ HS512)**: insider con la clave firma con
  HS256 pero pone `alg=HS512` en el header. La firma cuadra pero
  Cornamusa rechaza por `alg ≠ HS256`.
- **Sin claim `alg`**: header malformado lanza `ErrorDeClave` ahora
  atrapable (ver fix anterior).

Estas mitigaciones se mencionaban repetidamente en docs (CHANGELOG,
FAQ, stdlib/jwt.cor) pero NO había tests que las verificaran —
regresión silenciosa hubiera sido posible.

### Tests añadidos: `csv` (v1.58)

Nuevo `test_bytecode_csv_stdlib.c` con 17 asserts cubriendo el RFC
4180-like del parser:

- Parseo básico, separadores alternativos (`;`, `\t`).
- Campos quoted con `,` y `\n` internos.
- Escape `""` para comilla literal.
- Cadena vacía → lista vacía.
- Trailing `\n` no produce fila espuria.
- `\r\n` como separador de línea.
- Round-trip `parsear` → `serializar` → `parsear`.
- `serializar` quotea automáticamente campos con `,` o `"`.

Antes solo había cobertura indirecta vía `bc_run_66_csv` (regex de
output, no asserts estructurales).

### Tests añadidos: nativas de cadenas (v1.61-v1.62)

Nuevo `test_bytecode_cadenas_nativas.c` con 17 asserts cubriendo las
6 nativas perf-optimizadas:

- `cadena_unir`: vacía, un elemento, separador vacío, UTF-8 multibyte.
- `cadena_indice_de`: inicio, final, no presente, sub vacía.
- `cadena_empieza_con` / `cadena_termina_con`: prefijo/sufijo
  vacíos, prefijo más largo que cadena.
- `cadena_minusculas_ascii` / `cadena_mayusculas_ascii`: ASCII se
  convierte, non-ASCII (`é`, `ñ`) queda intacto, round-trip
  de ASCII puro.

### Bonus: alineación de documentación con realidad

(Cambios ya pusheados en commit anterior `5514800`.)

- README/introducción/tutorial: "doce módulos" → "diecisiete".
- README/referencia/tutorial: "57 ejemplos" → "72".
- `docs/referencia.md` §16 (stdlib) reescrita con los 5 módulos
  faltantes (csv, base64, hashing, jwt, tiempo) y sus APIs.
- `docs/referencia.md` §19: claims falsos sobre `borrar`/`global`/
  decoradores eliminados (sí están implementados; movidos a tabla
  "Implementados").
- FAQ: tooling pendiente actualizado; `8 → 9` tests diferenciales;
  roadmap stale ampliado con v1.41-v1.74.

### También en commit `5514800` (auditoría):

- **Decoradores `@nombre` en tree-walking** ahora lanzan error claro
  pidiendo `--bytecode` (antes: silent-ignore).
- **Decoradores sobre métodos de clase** ahora lanzan error de
  compilación claro (antes: silent-ignore; limitación declarada en
  v1.72 pero no enforced).
- **`tiempo.dormir(NaN/inf)`** lanza `ErrorDeValor` (antes: UB).
- **`nanosleep` loop infinito** si `errno ≠ EINTR` arreglado.
- **`test_profiler.c`** ya no asume Profiler inicializado.

### Estado

232 tests verde (de 230 antes de v1.74). 41 asserts nuevos.

### ESPEC.md sigue atrasado

ESPEC.md está desactualizado 28 versiones (sigue mencionando v1.46).
Es un documento formal grande, requiere una release dedicada de
actualización mayor — sin recursos para esta release.

---

## [1.73.0] — 2026-05-17 — Stdlib `tiempo` (reloj, sleep, cronómetro)

Nuevo módulo (17º de la stdlib) que complementa `fechas`:

```cornamusa
importar tiempo

# Reloj absoluto (Unix epoch):
ts_s  = tiempo.epoch_segundos()   # entero
ts_ms = tiempo.epoch_ms()         # entero (precisión ms)

# Reloj monotónico (para medir duraciones):
inicio = tiempo.monotonic()
trabajo_pesado()
imprimir(f"transcurrido: {tiempo.monotonic() - inicio:.3f}s")

# Sleep cooperativo (acepta decimal, no solo entero):
tiempo.dormir(0.5)   # bloquea 500ms

# Cronómetro (encapsula el inicio):
c = tiempo.cronometro()
trabajo()
imprimir(c.leer())   # segundos transcurridos
c.reiniciar()        # vuelve a 0
```

### Por qué un módulo separado de `fechas`

`fechas` ya cubre componer/descomponer/formatear timestamps absolutos
(operaciones de calendario). `tiempo` cubre el caso **dinámico**:
medir duraciones, dormir, cronómetros. Son responsabilidades distintas
y mantenerlos separados evita confusión.

### Por qué `monotonic` para duraciones

`epoch_segundos()` puede saltar hacia atrás si NTP corrige el reloj
o si el usuario cambia la zona horaria. Para medir cuánto tardó algo,
**siempre** usa `monotonic()` — su origen es arbitrario pero estable
durante la vida del proceso.

### Implementación

Tres nuevas nativas C cross-platform:

| Nativa            | Windows                         | POSIX                       |
|-------------------|---------------------------------|-----------------------------|
| `tiempo_epoch_ms` | `GetSystemTimeAsFileTime`       | `clock_gettime(REALTIME)`   |
| `tiempo_monotonic`| `QueryPerformanceCounter` (vía profiler) | `clock_gettime(MONOTONIC)` |
| `tiempo_dormir`   | `Sleep(ms)`                     | `nanosleep` (reintenta EINTR) |

`tiempo_monotonic` reusa el helper del profiler (`profiler_tiempo_ns`)
— ambos quieren el reloj monotónico de mayor resolución disponible,
así que vale la pena compartir el código.

### Cierre de gap doc

Las docs de `jwt` (v1.70) mencionaban `tiempo.epoch_segundos()` como
ejemplo de uso, pero el módulo no existía hasta hoy — se usaba
`tiempo_actual` (built-in de v1.19) o se calculaba manualmente. Esta
release alinea docs y realidad.

### Tests

9 asserts nuevos en `test_bytecode_tiempo_stdlib.c` (separado del
`test_bytecode_tiempo.c` original que cubre v1.19):

- `epoch_segundos` retorna ts post-2020.
- `epoch_ms` es ~1000× `epoch_segundos` (margen 2s).
- `monotonic` no decreciente.
- `dormir(0)` retorna inmediato.
- `dormir(0.05)` bloquea ≥30ms.
- `cronometro` acumula tras dormir.
- `cronometro.reiniciar()` resetea.
- Cronómetros independientes.

### Archivos

- `src/nativos.c` — 3 nuevas nativas + registro.
- `stdlib/tiempo.cor` — wrappers + clase `Cronometro` (12 funciones).
- `tests/unit/test_bytecode_tiempo_stdlib.c` — 9 asserts.
- `examples/72_tiempo.cor` — relojes, medición, cronómetro, backoff
  exponencial.

### Estado

230 tests verde. Repo limpio (0 warnings en lint).

---

## [1.72.0] — 2026-05-17 — Decoradores `@nombre`

Azúcar sintáctica idiomática para envolver funciones sin renombrarlas
manualmente. `@cache` + `funcion f` desugar a `f = cache(f)`. Stacking
y factories con argumentos soportados.

```cornamusa
funcion memoizar(f):
    cache = {}
    funcion envoltura(n):
        si n en cache:
            retornar cache[n]
        fin si
        r = f(n)
        cache[n] = r
        retornar r
    fin funcion
    retornar envoltura
fin funcion

@memoizar
funcion fib(n):
    si n < 2:
        retornar n
    fin si
    retornar fib(n - 1) + fib(n - 2)
fin funcion

imprimir(fib(35))   # instantáneo
```

### Reglas

- **Stacking**: `@a` + `@b` + `funcion f` produce `f = a(b(f))` — el
  decorador más cercano a la función se aplica primero (igual que
  Python).
- **Factories**: `@retry(3)` expresión arbitraria; el resultado de
  evaluarla es el decorador real. Equivalente a:
  `tmp = retry(3); f = tmp(f)`.
- **Funciones anidadas**: decoradores también funcionan dentro de
  otra función (la asignación va a un slot local en lugar de a una
  global).

### Implementación

- **Token `TT_AT`**: ya existía en el lexer desde hace tiempo, sin
  uso. Reutilizado.
- **AST**: `SENT_FUNCION` gana `Expr **decoradores` y `int n_decoradores`.
- **Parser**: al inicio de `parser_parsear_sentencia`, si el token
  actual es `@`, colecciona expresiones `@e` consecutivas y delega
  a `parsear_funcion`; luego adjunta los decoradores al `SENT_FUNCION`.
- **Compilador**: desugar puro tras `emitir_closure_de_funcion` + la
  asignación. Para cada decorador en orden inverso al fuente: compila
  decorador → empuja → `OP_OBTENER_*` de `f` → `OP_LLAMAR 1` →
  `OP_ASIGNAR_*` de `f`. Sin opcodes nuevos.

### Limitaciones (declaradas)

- **Solo funciones**, no clases (Python permite `@dataclass` sobre
  clases; sin demanda concreta, lo dejamos fuera).
- **No decoración de métodos** dentro de un cuerpo `clase:` (sería el
  caso `@property` / `@staticmethod`; queda para una posible v1.x si
  surge un caso real).

### Tests

10 asserts nuevos en `test_bytecode_decoradores.c`:

- Decorador simple aplica una vez (`ident(5) → 105` con `+100`).
- Stacking `@a @b` produce el orden correcto (`(*2 ∘ +1)(5) = 12`).
- Orden inverso `@b @a` también (`(+1 ∘ *2)(5) = 11`).
- Factory `@rep(3)` repite la salida 3 veces.
- `@x` sin `funcion` después → ErrorDeSintaxis.
- Decorador en función anidada funciona igual.

### Archivos

- `src/ast.h`, `src/ast.c` — campos `decoradores`/`n_decoradores`.
- `src/parser.c` — manejo de `TT_AT` en `parser_parsear_sentencia`.
- `src/compilador.c` — desugar tras `compilar_funcion`.
- `tests/unit/test_bytecode_decoradores.c` — 10 asserts.
- `examples/71_decoradores.cor` — 4 escenarios (log, repetir, stacking, memoize).

### Estado

228 tests verde. Repo limpio (lint y fmt sin diferencias).

---

## [1.71.0] — 2026-05-17 — Profiler determinista (`cornamusa prof`)

Nuevo subcomando que ejecuta un script bajo un profiler determinista
y emite una tabla ordenada por **self time** (tiempo del frame
descontando los frames hijos). Complementa `fmt`/`lint`/`docs`/`lsp`
cerrando la fase de tooling.

```bash
cornamusa prof examples/70_profiler.cor
# (stderr)
#   llamadas       total        self    per-call  funcion
#   --------     -------     -------    --------  -------
#      92712   168.669ms    13.104ms       141ns  fib
#          1    13.176ms    42.000us    42.000us  <top-level>
#          1    13.132ms    27.800us    27.800us  calcular_serie
#          1     2.000us     2.000us     2.000us  sumar_lista
```

### Diseño

- **Hook único** en el dispatch loop de la VM: cuando profiler activo,
  sincroniza `profiler.n_stack` con `vm->n_frames`. Push si subió,
  pop si bajó. Cuando inactivo, es solo un branch sobre la bandera
  (cero coste real para programas que no usan `prof`).
- **Reloj monotónico**: `QueryPerformanceCounter` en Windows,
  `clock_gettime(CLOCK_MONOTONIC)` en POSIX. Resolución submicrosegundo.
- **`total` vs `self`**:
  - `total_ns`: tiempo desde push hasta pop (incluye hijos).
  - `self_ns`: `total - sum(total_de_hijos)`. La métrica útil para
    encontrar hotspots. Funciones recursivas tienen `total >> self`
    porque cada nivel acumula al padre.
- **Identidad por función**: clave = puntero a `FuncionBC` (estable).
  Diferentes closures de la misma función fuente agregan al mismo
  bucket. Frames top-level y de módulos se identifican aparte.
- **Salida a stderr**: stdout queda libre para el output del programa,
  redirigible a pipe sin contaminar la tabla.

### CLI

```
cornamusa prof [--top=N] <archivo.cor> [args...]
  --top=N   muestra solo las N funciones con mas self time (default 20, 0 = todas)
```

Los `args...` se pasan al programa via `sistema.argv`.

### Verificación de cuentas

`tests/unit/test_profiler.c` con 18 asserts incluye un caso conocido:
`fib(5)` recursivo debe registrar exactamente `T(5) = 2*fib(6)-1 = 15`
llamadas. Verificado.

### Limitaciones (declaradas)

- **No mide nativas** que no crean CallFrame. Por ejemplo, una llamada
  a `cadena_unir(lst, sep)` se contabiliza dentro del `self` del
  caller — no como entrada propia.
- **No produce caller/callee tree** (perfile plano). Para call graphs
  habría que registrar el padre en cada exit; queda para v1.x si surge
  un caso concreto.
- **Overhead activo**: cada push/pop registra timestamp + lookup +
  update. Programas muy hot bajan ~3-5×. No usar en producción.
- **`alg=none`** y otros frames excepcionales: si una excepción
  desenrosca varios frames, cada pop se contabiliza al sincronizar.

### Archivos

- `src/profiler.{c,h}` — modulo nuevo.
- `src/vm.{c,h}` — hook único en dispatch loop, `Profiler` embebido en `VM`.
- `src/main.c` — subcomando `prof`, refactor de `ejecutar_archivo_bytecode`.
- `tests/unit/test_profiler.c` — 18 asserts.
- `examples/70_profiler.cor` — fib recursivo demostrando hotspot.

### Estado

226 tests verde. Repo limpio (lint y fmt sin diferencias). Cierra
la suite de tooling de Fase 5 junto con `fmt`/`lint`/`docs`/`lsp`.

---

## [1.70.0] — 2026-05-17 — `jwt`: validación de claims temporales

Pulido del módulo `jwt` (introducido en v1.67). `decodificar()` valida
la firma — pero hasta hoy comprobar si el token había caducado era
responsabilidad manual del caller. Esta release añade dos helpers que
cierran el ciclo:

```cornamusa
importar jwt

# Helper aislado: ¿caducó este payload?
si jwt.expirado(payload, tiempo.epoch_segundos()):
    # ... rechazar petición
fin si

# Atajo "todo en uno": firma + exp + nbf en una sola llamada.
intentar:
    payload = jwt.decodificar_y_validar(token, CLAVE, ahora)
atrapar ErrorDeValor como e:
    # firma inválida, token expirado o aún no activo
fin intentar
```

### Decisión: `ahora` se pasa explícito

`decodificar_y_validar(token, clave, ahora)` recibe el timestamp del
caller en lugar de leerlo internamente de `tiempo.epoch_segundos()`.
La función queda pura y testeable; además el caller puede usar
tiempo simulado en tests de expiración sin monkeypatching.

### Cobertura

- `jwt.expirado(payload, ahora)` → `booleano`. `verdadero` si y solo si
  el payload tiene claim `exp` y `exp <= ahora`. Sin claim `exp`, el
  token nunca expira (devuelve `falso`).
- `jwt.decodificar_y_validar(token, clave, ahora)` → `diccionario` o
  lanza `ErrorDeValor` por (a) firma inválida, (b) `exp <= ahora`, o
  (c) `nbf > ahora`. La validación de firma se hace primero (es la
  única garantía criptográfica — sin firma válida no nos podemos
  fiar de los timestamps).

### Limitaciones (declaradas)

- No se valida `iat` (issued-at) automáticamente — es informativo, no
  bloquea el token. El caller puede compararlo si quiere.
- No se validan `iss` (issuer), `aud` (audience), `sub` (subject) — son
  específicos del flujo de autenticación; pasar lista de aceptados
  como argumento sería sobre-ingeniería sin un caso concreto.
- `alg = "none"` (RFC 7519 §6.1) sigue rechazado (heredado de v1.67).

### Tests

8 asserts nuevos en `test_bytecode_jwt.c` (total: 15 asserts):

- `expirado()` con `exp` futuro / pasado / igual a `ahora`.
- `expirado()` sin claim `exp` devuelve `falso`.
- `decodificar_y_validar()` OK con `exp` futuro.
- `decodificar_y_validar()` lanza con `exp` pasado.
- `decodificar_y_validar()` lanza con `nbf` futuro.
- `decodificar_y_validar()` OK con `nbf <= ahora`.
- `decodificar_y_validar()` rechaza firma inválida (no salta el check).

### Archivos

- `stdlib/jwt.cor` — añade `expirado()` y `decodificar_y_validar()`.
- `tests/unit/test_bytecode_jwt.c` — 7 nuevos asserts.
- `examples/69_jwt.cor` — sección 7 con caso "1h tras emisión" vs "48h".

### Estado

224 tests verde. Repo sigue limpio (0/97 warnings tras audit).

---

## [1.69.0] — 2026-05-17 — Linter: `empty-except` (12ª categoría)

Detecta el anti-patrón clásico de "silenciar errores": una cláusula
`atrapar X:` con cuerpo vacío o solo `pasar`. Es un error que
dificulta debugging — el programa sigue ejecutando como si nada
pero el error real se perdió.

### Ejemplos

```cornamusa
# Mal: error silenciado.
intentar:
    n = entero(entrada)
atrapar Excepcion:
    pasar                  # ← warning [empty-except]
fin intentar

# Mal aunque haya `como e`: sigue sin tratarse.
intentar:
    n = entero(entrada)
atrapar Excepcion como e:
    pasar                  # ← también warning
fin intentar

# Bien: cuerpo con código real.
intentar:
    n = entero(entrada)
atrapar Excepcion:
    n = 0                  # fallback explícito
fin intentar

# Deliberado: suprime con # noqa.
intentar:
    cleanup_best_effort()
atrapar Excepcion:         # noqa: empty-except
    pasar
fin intentar
```

### Cobertura

Detecta dos formas problemáticas:

| Cuerpo del `atrapar` | Estado |
|---|---|
| Vacío (0 sentencias) | warning |
| Una sola `pasar` | warning |
| Cualquier otra cosa | OK |

No distingue si hay `como X` o no — ambas variantes son sospechosas
si el cuerpo no hace nada. Si quieres capturar el error para
re-loguear más tarde, al menos haz `e_global = e` o algún registro.

### Aplicación al repo

0 verdaderos positivos, 0 falsos positivos. **Los ejemplos y stdlib
del proyecto nunca cayeron en este anti-patrón** — buena disciplina
mantenida orgánicamente, ahora protegida automáticamente contra
introducción futura.

### Total del linter: 12 categorías

Con `empty-except` el linter cubre:

- 4 AST-shape: `unreachable`, `redundant-pasar`, `eq-nulo`, `unused-import`.
- 4 scope analysis: `unused-local`, `unused-param`, `shadow`, `unused-loop-var`.
- 2 perf patterns: `mutable-default`, `concat-in-loop`.
- 2 bug catchers: `same-comparison`, **`empty-except`**.

### Implementación

- `LINT_EMPTY_EXCEPT` en `linter.h`.
- Detección dentro del case `SENT_INTENTAR` del visitor: para cada
  `ClausulaAtrapar`, comprueba si el cuerpo (siempre `SENT_BLOQUE`)
  tiene 0 sentencias o 1 sentencia `SENT_PASAR`.
- ~25 líneas C añadidas.
- Suppresión con `# noqa: empty-except` funciona (sale del helper
  común `noqa_silencia`).

### Verificación

- **224/224 tests verde**. `test_linter` extendido a **73 asserts**
  (vs 68): cuerpo `pasar`, con `como e`, cuerpo con código (skip),
  múltiples atrapadores donde uno warna y otro no, `# noqa`.
- Repo entero pasa `cornamusa lint` sin warnings (0 introducidos).

## [1.68.0] — 2026-05-17 — Linter: `same-comparison` (11ª categoría)

Nuevo check del linter que detecta `x == x`, `x < x`, `x != x` y
similares — comparaciones entre el mismo identificador, casi siempre
typos del programador queriendo comparar contra OTRA variable.

### Ejemplo del bug típico

```cornamusa
funcion solapamiento(inicio_a, fin_a, inicio_b, fin_b):
    si inicio_a < fin_a:           # OK
        si inicio_b < inicio_b:    # ← typo: queria `fin_b`
            retornar verdadero
        fin si
    fin si
    retornar falso
fin funcion
```

El warning sale en tiempo de lint, antes de que el bug llegue a runtime.

### Cobertura

Detecta 6 operadores de comparación:

| Patrón | Siempre... |
|---|---|
| `x == x` | verdadero |
| `x != x` | falso |
| `x < x` | falso |
| `x <= x` | verdadero |
| `x > x` | falso |
| `x >= x` | verdadero |

### Heurística (skip rules)

Solo dispara cuando **ambos lados son `EXPR_IDENT` con el mismo
nombre**. Otros casos no warnean:

- `g() == g()` — calls pueden tener efectos secundarios.
- `obj.x == obj.x` — atributos pueden ser propiedades con side-effects.
- `x == 0` — literal en RHS, no es typo.
- `x == y` — idents distintos, comparación legítima.

Para casos donde el patrón es intencional (demo del dunder
`__igual__`, NaN check `decimal != decimal`), usa `# noqa: same-comparison`.

### Aplicación al repo

1 caso encontrado en `examples/28_dunders_jugable.cor:59` — `v == v`
para demostrar que el dunder `__igual__` devuelve `verdadero` al
comparar un Vector2D consigo mismo. Caso didáctico legítimo,
silenciado con `# noqa`.

Bonus: detectado también un `unused-import: hashing` en
`stdlib/jwt.cor` (delegaba directamente al native `hash_hmac_sha256_bytes`
en vez de via `hashing.X`). Arreglado añadiendo el wrapper
`hashing.hmac_sha256_bytes` y haciendo el import productivo.

### Implementación

- `LINT_SAME_COMPARISON` en `linter.h`.
- Helper reutilizado: `es_mismo_ident(izq, der)` de v1.55.
- Detección dentro del case `EXPR_BINARIO` del visitor, junto al
  check de `eq-nulo` (ambos sobre operadores de comparación).
- Mensaje claro: `'fecha_inicio == fecha_inicio' siempre es verdadero — probable typo`.
- ~30 líneas C añadidas.

### Total del linter: 11 categorías

| Categoría | Desde | Detección |
|---|---|---|
| `unreachable` | v1.49 | Código tras retornar/romper |
| `redundant-pasar` | v1.49 | `pasar` en bloque no vacío |
| `eq-nulo` | v1.49 | `== nulo` → `es nulo` |
| `unused-import` | v1.49 | Módulo importado pero no usado |
| `unused-local` | v1.50 | Variable local nunca leída |
| `unused-param` | v1.50 | Parámetro nunca usado |
| `shadow` | v1.55 | Local sombrea outer |
| `unused-loop-var` | v1.55 | `para X` con X no usado |
| `mutable-default` | v1.55 | Default `=[]`/`={}` literal |
| `concat-in-loop` | v1.63 | `x = x + cadena` en loop |
| **`same-comparison`** | **v1.68** | **`x OP x` (typo)** |

### Verificación

- **224/224 tests verde**. `test_linter` extendido a **68 asserts**
  (vs 61): los 6 operadores, literal en RHS skip, calls skip,
  idents distintos skip, suppresión con `# noqa`.
- Repo entero pasa `cornamusa lint` sin warnings tras los 2 fixes.

## [1.67.0] — 2026-05-17 — Stdlib `jwt` (RFC 7519 HS256) — la suite coherente

Cierra el arco de stdlib criptográfica iniciado en v1.58 (csv) →
v1.59 (base64) → v1.60 (hashing) → v1.65 (HMAC) → v1.66 (base64 url-safe).
Todo apuntaba a esto. **Stdlib pasa de 15 a 16 módulos.**

### Lo nuevo

```cornamusa
importar jwt

# Firmar payload (dict) con clave (cadena):
token = jwt.codificar({"sub": "42", "exp": 1735689600}, "mi-secreto")

# Verificar y obtener payload (lanza ErrorDeValor si invalido):
payload = jwt.decodificar(token, "mi-secreto")

# Atajo booleano sin try/except:
si jwt.verificar(token, "mi-secreto"):
    # ...
fin si
```

### Implementación

**~80 líneas Cornamusa puro** sobre las stdlib previas. Sin nuevo
algoritmo, sin nueva C:

- `json.serializar` codifica header y payload.
- `base64.codificar_url` produce las 3 partes URL-safe sin padding.
- `hashing.hmac_sha256_bytes` (nuevo, v1.67) genera 32 bytes raw
  para la firma — único añadido en C (~20 líneas extra como
  refactor de `_hex`).

Formato resultante: `header_b64.payload_b64.signature_b64`. Conforme
con la spec RFC 7519 — válido para cualquier verificador JWT
estándar (jwt.io, etc.).

### Garantías de seguridad

- **`alg=none` rechazado**: mitigación estándar contra ataques de
  algorithm confusion. El header es fijo `{"alg":"HS256","typ":"JWT"}`
  al codificar, y al decodificar `decodificar()` rechaza headers con
  `alg` distinto a `HS256`.
- **Firma se verifica ANTES de parsear JSON** del header o payload —
  si signature inválida, no confiamos en el contenido.
- **Comparación de firma es byte-a-byte**: no constant-time. Para
  v1.67 acceptable (Cornamusa no se usa en contextos donde un timing
  attack es realista — sería bug para clientes server-side de alto
  volumen).

### Lo que NO valida automáticamente

`decodificar()` valida solo la **firma**. NO chequea:
- `exp` (expiración).
- `nbf` (not-before).
- `iat` (issued-at).
- `iss`, `aud` (issuer/audience).

Responsabilidad del código cliente. Ejemplo:

```cornamusa
payload = jwt.decodificar(token, clave)
si payload["exp"] < tiempo_actual():
    lanzar ErrorDeValor("token expirado")
fin si
```

### Cambio en infra de tests

Para que los tests unitarios que usan `importar jwt` funcionen,
todos los tests ahora se ejecutan con `WORKING_DIRECTORY =
${CMAKE_SOURCE_DIR}` (la raíz del repo), donde `stdlib/` es
relativo. Cambio inocuo para los tests que no importan stdlib.

### Verificación

- **224/224 tests verde**. Nuevo `test_bytecode_jwt` con **8 asserts**:
  round-trip, prefijo `eyJ`, `verificar` con clave correcta/mala,
  token mal formado, decode con clave mala, payload anidado.
- Nuevo `examples/69_jwt.cor`: ciclo completo login → verificar.
- Nuevo `bc_run_69_jwt` integration test verifica que un token
  alterado se rechaza.
- `cornamusa lint stdlib/jwt.cor` → 0 warnings.
- `cornamusa fmt --check stdlib/jwt.cor` → canónico.

### Lo que NO incluye (scope para v1.68+)

- **Algoritmos asimétricos**: RS256 (RSA), ES256 (ECDSA). Requieren
  criptografía de clave pública — biblioteca extra (libtomcrypt o
  rolling-our-own). Fuera de scope inmediato.
- **Validación automática de `exp`/`nbf`**: dejado al usuario para
  no asumir política. Podría añadirse helper `jwt.decodificar_y_validar(token, clave, ahora)`.
- **`alg=none` opt-in**: ningún caso de uso real lo justifica.
- **Constant-time signature comparison**: para mitigar timing attacks
  en contextos servidor de alto volumen. Acceptable como deuda
  documentada por ahora.

## [1.66.0] — 2026-05-16 — base64 URL-safe (RFC 4648 §5)

Antes de v1.66, el módulo `base64` solo soportaba el alfabeto
estándar (`+/=`). v1.66 añade la **variante URL-safe** (RFC 4648 §5)
que reemplaza `+/` por `-_` y omite el padding `=`. Esta variante es
**estándar en JWTs, OAuth, cookies y parámetros HTTP** — caracteres
seguros en URLs sin escape.

### API nueva

```cornamusa
importar base64

# Codificar URL-safe (sin padding):
base64.codificar_url("?>?")       # "Pz4_"   (vs "Pz4/" estandar)
base64.codificar_url("Hola")      # "SG9sYQ" (vs "SG9sYQ==" estandar)

# Decodificar: el decoder es TOLERANTE a ambas variantes
# (acepta `-_` igual que `+/`, con o sin padding).
base64.decodificar_url("SG9sYQ")        # "Hola"
base64.decodificar_url("SG9sYQ==")      # "Hola" (también funciona)
base64.decodificar("dXNlYXJpbzo0Mg")    # mismo decoder, alias para legibilidad
```

### Comportamiento del decoder

Para reducir fricción, el decoder estándar **acepta cualquier
variante**:

- `+` y `-` ambos → carácter 62.
- `/` y `_` ambos → carácter 63.
- Padding `=` opcional. Si hay, el total debe ser múltiplo de 4. Si
  no hay, el resto del último bloque puede ser 0, 2 o 3 chars (un
  solo char sobrante es inválido — codifica 6 bits sueltos).

`base64.decodificar_url(s)` es un alias de `base64.decodificar(s)`
— mismo decoder, nombre que clarifica intención.

### Implementación

- `nativos.c`: refactor mínimo del codec en un helper
  `base64_codificar_impl(alfabeto, con_padding)` parametrizado.
  - Standard: alfabeto clásico + padding.
  - URL-safe: alfabeto con `-_` sin padding.
- Nuevo constante `B64_ALFABETO_URL[]` con `-_` en posiciones 62, 63.
- `nativa_base64_codificar_url` expone la variante.
- Decoder: extendido `b64_decode_char` para aceptar `-_` además de
  `+/`. Relajado check de longitud para entrada sin padding (rest
  `% 4` puede ser 0, 2, o 3).
- ~40 líneas de cambio sobre la implementación de v1.59.

### Verificación

- **221/221 tests verde**. `test_base64` extendido a **30 asserts**
  (vs 18 antes):
  - URL-safe sin padding: `"any carnal pleasure."` → 27 chars (no 28).
  - URL chars: `"?>?"` produce `Pz4_` no `Pz4/`.
  - Decoder tolerante: `Pz4_` decodifica igual que `Pz4/`.
  - Decoder sin padding: `SG9sYQ` decodifica a `Hola`.
  - Round-trip URL-safe para 7 tamaños (0..11 bytes).
- `examples/67_base64.cor` extendido con sección §6 "URL-safe".

### Prerequisito para v1.67

JWT (JSON Web Token, RFC 7519) usa exactamente esta variante en sus
3 partes (header, payload, signature). v1.67 añadirá `stdlib/jwt.cor`
combinando `base64.codificar_url` + `json.serializar` +
`hashing.hmac_sha256`.

## [1.65.0] — 2026-05-16 — HMAC en `hashing` (RFC 2104 / RFC 4231)

Antes de v1.65, el módulo `hashing` cubría SHA-256 y MD5 puros.
Faltaba **HMAC** — el esquema estándar para autenticación de mensajes
con clave secreta. Se construye sobre el hash subyacente y es la
base de:

- **JWT signing** (HMAC-SHA-256, alg = "HS256" en el header).
- **Webhooks** de GitHub, Stripe, etc. (verificar payloads).
- **Cookies de sesión firmadas**.
- **API authentication** con shared secret.

### API nueva

```cornamusa
importar hashing

# Firmar un mensaje:
firma = hashing.hmac_sha256(clave_secreta, mensaje)
# → cadena de 64 chars hex

firma_md5 = hashing.hmac_md5(clave_secreta, mensaje)
# → cadena de 32 chars hex

# Verificar: re-computar con la misma clave da el mismo digest.
firma_recibida = "..."
firma_calculada = hashing.hmac_sha256(clave_secreta, mensaje)
es_valida = (firma_calculada == firma_recibida)
```

### Detalle algorítmico

Implementa RFC 2104:

```
HMAC(K, m) = H((K' XOR opad) || H((K' XOR ipad) || m))
```

donde:
- `K'` = K si |K| ≤ B; H(K) si |K| > B; padded con zeros a B.
- `ipad` = 0x36 byte × B.
- `opad` = 0x5C byte × B.
- B (block size) = 64 para SHA-256 y MD5.

### Test vectors validados

**RFC 4231 HMAC-SHA-256** (los 5 vectors del documento):
- Caso 1: Key=0x0b×20, Data="Hi There" → `b0344c61...32cff7`.
- Caso 2: Key="Jefe", Data="what do ya want for nothing?" → `5bdcc146...c3843`.
- Caso 3: Key=0xaa×20, Data=0xdd×50 → `773ea91e...65fe`.
- Caso 4: Key=0x01..0x19, Data=0xcd×50 → `82558a38...665b`.
- Caso 6: Key de 131 bytes (>B, requiere hash-then-pad) → `60e43159...7f54`.

**RFC 2104 HMAC-MD5** (los 3 vectors):
- Caso 1: Key=0x0b×16, Data="Hi There" → `9294727a...fc9d`.
- Caso 2: Key="Jefe", Data="what do ya want for nothing?" → `750c783e...b738`.
- Caso 3: Key=0xaa×16, Data=0xdd×50 → `56be3452...b3f6`.

### Notas de seguridad

- **HMAC-MD5 sigue siendo SEGURO como MAC** pese a que MD5 está
  criptográficamente roto para hashing simple. El esquema HMAC
  protege contra ataques de colisión-en-función-base.
- **HMAC-SHA-256** es el estándar actual de la industria. Considerado
  seguro indefinidamente.
- **NO usar HMAC para hashing de passwords**. Para eso, scrypt/argon2
  (no provistos por Cornamusa).

### Implementación

- Refactor mínimo en `src/hashing.c`: extraídos `sha256_raw` y
  `md5_raw` (devuelven 32/16 bytes raw, no hex) compartidos por la
  versión `_hex` original y por HMAC.
- 2 funciones públicas nuevas: `hashing_hmac_sha256_hex` y
  `hashing_hmac_md5_hex`.
- Helper privado `hmac_preparar_clave` para el ajuste K'.
- 2 nativas nuevas en `src/nativos.c`: `hash_hmac_sha256`,
  `hash_hmac_md5`.
- 2 wrappers en `stdlib/hashing.cor`.
- Total: ~150 líneas C añadidas.

### Verificación

- **221/221 tests verde**. Nuevo `test_hmac` con **10 asserts**
  cubriendo:
  - 5 vectors de RFC 4231 (incluyendo clave > B).
  - 3 vectors de RFC 2104 HMAC-MD5.
  - 2 edge cases: clave vacía, mensaje vacío (no crashea).
- Ejemplo `examples/68_hashing.cor` extendido con sección HMAC
  (firma + verificación + caso de intruso con clave mala).

### Lo que NO incluye (scope para v1.66+)

- **HMAC-SHA-1**: SHA-1 obsoleto, no añadido a propósito.
- **HMAC-SHA-512**: extensión natural cuando se añada SHA-512.

## [1.64.0] — 2026-05-16 — Linter: directiva `# noqa: <categoria>` para supresión selectiva

Cierra una limitación práctica del linter. Hasta v1.63, cualquier
warning del linter dispara siempre. Pero algunos casos legítimos
incluyen anti-patrones intencionalmente (código didáctico, código
generado, trade-offs deliberados). Por ejemplo, `examples/42_defaults.cor`
**demuestra** el footgun `mutable-default` de Python con comentario
explicativo — el warning ahí es correcto técnicamente pero ruido en
contexto.

### Sintaxis

```cornamusa
# Silencia una categoría específica:
importar fechas      # noqa: unused-import
funcion f(x=[]):     # noqa: mutable-default
    retornar x
fin funcion

# Múltiples categorías separadas por coma:
funcion g(a, b):     # noqa: unused-param, shadow
    retornar 1
fin funcion

# Bare noqa silencia TODOS los warnings en esa línea:
codigo_legado()      # noqa
```

### Reglas

- La directiva aplica **a la línea donde aparece**, no a líneas
  posteriores ni anteriores.
- `# noqa: cat` requiere el nombre exacto de la categoría (como lo
  reporta `linter_tipo_nombre`): `unreachable`, `redundant-pasar`,
  `eq-nulo`, `unused-import`, `unused-local`, `unused-param`,
  `shadow`, `unused-loop-var`, `mutable-default`, `concat-in-loop`.
- Categorías desconocidas se ignoran silenciosamente (forward-compat
  con releases que añadan checks nuevos).
- Whitespace alrededor de `noqa` y entre `:` y las categorías es
  flexible: `# noqa: cat1,cat2` o `# noqa:cat1, cat2` ambos válidos.

### Implementación

- `parsear_noqa(fuente, ctx)` pre-escanea el texto línea a línea,
  detecta `#` no-en-string, busca `noqa` después, parsea categorías.
- Tabla `ctx->noqa_mask[linea]` con bitmap por categoría.
  `NOQA_SILENCE_ALL` (bit 31) marca bare-noqa.
- `noqa_silencia(ctx, linea, tipo)` consulta antes de añadir warning
  a la lista de resultados.
- API: `linter_analizar(sents, n, fuente)` ahora acepta `fuente`
  (NULL = sin noqa, retrocompatible para tests que no necesitan).

### Aplicación al repo

- `examples/42_defaults.cor`: añadido `# noqa: mutable-default` al
  parámetro `log=[]`. El ejemplo sigue demostrando el footgun, pero
  ahora pasa lint limpio.
- **Repo entero: 0 warnings en lint** ahora (vs 1 en v1.63).

### Verificación

- **220/220 tests verde**. `test_linter` extendido a **61 asserts**
  (vs 56 antes): noqa con categoría específica, bare, múltiples
  categorías, "solo aplica a su línea", categoría desconocida
  ignorada.

### Limitaciones

- **No soporta noqa multi-línea** (`#` en línea anterior aplica solo
  a la línea anterior, no a la siguiente). Suficiente para los casos
  típicos.
- **No reconoce triple-quoted strings**: un `#` dentro de
  `"""..."""` multi-línea podría falsa-positivo como noqa. Caso edge
  raro.
- **Categoría inexistente no warna**: si tipeas `# noqa: unsed-local`
  (typo), no se silencia el warning Y no hay aviso del typo. Trade-off
  por forward-compat.

## [1.63.0] — 2026-05-16 — Linter `concat-in-loop`: detección automática del patrón cazado en v1.61-62

Cierra el loop de aprendizaje. Tras dos releases consecutivas (v1.61,
v1.62) cazando manualmente el patrón `x = x + cadena` dentro de
bucles —que es O(n²) para acumular cadenas y motivó las 6 nuevas
nativas en C—, ahora el **linter lo detecta automáticamente**.
10ª categoría.

### Cómo funciona

Detecta dos formas del patrón:

```cornamusa
# Forma A: x = x + RHS
para i en rango(n):
    s = s + "x"        # warning [concat-in-loop]
fin para

# Forma B: x += RHS
mientras cond:
    s += f"valor_{i}"  # warning [concat-in-loop]
fin mientras
```

### Heurística refinada para 0 falsos positivos

El check podría warnear MUCHOS sitios (cualquier `total = total + i`
sería sospechoso sin más info). Para evitarlo, el linter usa una
heurística conservadora: **solo emite warning si el RHS es claramente
string-like** — un literal `"..."`, una f-cadena `f"..."`, o una
subexpresión `+` binaria que contiene alguno de los anteriores.

Resultado:

- `total = total + i` ✗ no warna (i podría ser cualquier cosa).
- `total += 1` ✗ no warna (literal numérico).
- `s = s + "x"` ✓ warna (literal cadena).
- `s += f"{x}"` ✓ warna (f-cadena).
- `s = s + ident_otra` ✗ no warna (otra var, podría ser numérica).

Trade-off: pierde algunos casos reales donde la concatenación es de
strings pero el RHS no es un literal. Lo aceptable: el linter es una
herramienta de ayuda, no un type checker.

### Aplicación al repo

Tras añadir el check, el repo entero (15 stdlib + 65 examples) tenía
**2 verdaderos positivos** en `formato.cor`:

- `como_hex(n)`: hacía `s = digitos[valor % 16] + s` en loop. Aunque
  `digitos[...]` es indexación (no literal), el check NO disparó
  porque la heurística no inspecciona indexación. Pero las siguientes
  dos líneas SÍ dispararon.
- `como_binario(n)`: hacía `s = "0" + s` o `s = "1" + s` en loop —
  detectado.

Ambos reescritos para usar `agregar(partes, char); invertir(partes);
cadena_unir(partes, "")`. Para enteros grandes (cientos de dígitos
bignum) el speedup es significativo aunque no benchmarkeado
explícitamente.

### Total de checks del linter

10 categorías ya:

| Categoría | Desde | Detección |
|---|---|---|
| `unreachable` | v1.49 | Código tras retornar/romper/continuar/lanzar |
| `redundant-pasar` | v1.49 | `pasar` en bloque con otras sentencias |
| `eq-nulo` | v1.49 | `== nulo` / `!= nulo` |
| `unused-import` | v1.49 | Módulo importado pero no usado |
| `unused-local` | v1.50 | Variable local nunca leída |
| `unused-param` | v1.50 | Parámetro nunca usado |
| `shadow` | v1.55 | Local sombrea outer |
| `unused-loop-var` | v1.55 | `para X` con X no usado |
| `mutable-default` | v1.55 | Default `=[]`/`={}` literal |
| **`concat-in-loop`** | **v1.63** | **`x = x + cadena` dentro de bucle** |

### Implementación

- `LINT_CONCAT_IN_LOOP` en `linter.h`.
- Contador `profundidad_loop` en `Ctx`: incrementa al entrar
  `SENT_MIENTRAS`/`SENT_PARA` cuerpo, decrementa al salir. Funciones
  anidadas guardan/restauran el contador (no heredan profundidad,
  porque la función puede llamarse fuera del loop).
- Helpers: `es_mismo_ident(a, b)` y `rhs_es_string_like(e)`
  (recursivo dentro de `EXPR_BINARIO +` y `EXPR_GRUPO`).
- Detección en `SENT_ASIGNAR` (caso `x = x + ...`) y
  `SENT_ASIGNAR_AUG` con op `+=`.

### Verificación

- **220/220 tests verde**. `test_linter` extendido a **56 asserts**
  (vs 48 antes): patrón clásico, f-cadena, fuera de loop, mientras,
  función anidada (no hereda profundidad), aug con literal y con
  cadena, contador numérico (no falso positivo), `n += 1` skip.
- Repo entero pasa el linter limpiamente tras los fixes en `formato`.
- `formato.como_hex(255)` / `como_binario(10)` ejecutan correctamente.

### Lección del meta-loop

v1.61 cazó 1 caso (`csv.parsear`). v1.62 audit manual cazó 5 más. v1.63
automatiza la detección — futuro código no introducirá nuevo `O(n²)`
de este tipo sin warning. **Si encuentras un patrón problemático tres
veces, escribe el linter para él.**

## [1.62.0] — 2026-05-16 — Perf round 2 (cont.): 5 nativas más en `cadenas` tras audit

Continuación natural de v1.61. Tras encontrar el O(n²) en
`csv.parsear`, grep sistemático de los patrones culpables
(`texto[i]` con índice en loop + `resultado += x` acumulando
strings) reveló **5 funciones más** en `cadenas.cor` con el mismo
defecto:

- `empieza_con(s, prefijo)` — O(prefijo²) por `s[i] != prefijo[i]`.
- `termina_con(s, sufijo)` — análogo.
- `indice_de(s, sub)` — O(s² · sub) por nested loop con indexing.
- `minusculas_ascii(s)` — O(s²) + lookup table de 26 por carácter.
- `mayusculas_ascii(s)` — análogo.

### Solución

**5 nativas C nuevas** (~250 líneas total en `nativos.c`):

- `cadena_empieza_con(s, prefijo)` — `memcmp` en bytes. O(|prefijo|).
- `cadena_termina_con(s, sufijo)` — `memcmp` desde el final.
- `cadena_indice_de(s, sub)` — naive substring byte-search + conversión a
  char index con `utf8proc_iterate` solo al match. ASCII puro =
  O(s · sub); UTF-8 con match = +O(byte_pos_match) extra.
- `cadena_minusculas_ascii(s)` — byte scan aplicando `c + 32` si está en
  `A..Z`. Conserva todo lo demás (incluso bytes UTF-8 multi-byte).
- `cadena_mayusculas_ascii(s)` — análogo con `c - 32`.

Los wrappers en `stdlib/cadenas.cor` quedan como **thin delegates**
(una línea cada uno). Las ~80 líneas de pure-Cornamusa que
implementaban las 5 funciones se eliminan, incluyendo el helper
`_TABLA_MIN/_TABLA_MAY` con su lookup lineal por carácter.

### Resultados

| Benchmark              | v1.61    | v1.62    | Notas |
|------------------------|----------|----------|-------|
| `csv_parse_1000`       | ~31 ms   | ~42 ms   | sin cambio (ruido) |
| `csv_serialize_1000`   | (nuevo)  | **~22 ms** | 20K llamadas `indice_de` |
| `cadena_caso_50k`      | (nuevo)  | **~10 ms** | minusculas sobre 50 KiB |
| `base64_round_trip`    | ~110 ms  | **~25 ms** | beneficia del `repetir` ya lineal |
| `fibonacci_recursivo`  | ~210 ms  | ~245 ms  | sin cambio (ruido alto) |
| Resto                  | similar  | similar  | |

`csv_serialize_1000` y `cadena_caso_50k` son nuevos en v1.62, sin
baseline directa anterior. Estimación honesta: con las versiones
pre-v1.62, ambos hubieran sido **órdenes de magnitud más lentos**
(centenares de ms para `csv_serialize`, varios segundos para
`cadena_caso_50k`).

`base64_round_trip` cae 130→25ms automáticamente porque el
benchmark usa `cadenas.repetir(s, 11378)` para construir un input
grande, y `repetir` ahora es lineal (v1.61 ya delegaba a
`cadena_unir`, pero la combinación con `cadena_caso` mejora la
historia general).

### Sin regresiones

- 220/220 tests verde.
- Benchmarks existentes sin pérdida (variación dentro del ruido
  habitual de Windows + AV).
- `cornamusa lint stdlib/cadenas.cor` y `fmt --check` limpios tras
  los cambios.

### Lección, refinada

v1.61 cazó **un** O(n²) (`csv_parse`). El audit sistemático en v1.62
encontró **5 más** del mismo patrón. La moraleja: cuando un patrón
problemático aparece una vez en una codebase Cornamusa donde las
cadenas son inmutables y UTF-8, casi seguro aparece en otros sitios.
Sería bueno tener un linter check específico — `concat-in-loop` —
para detectar `resultado = resultado + x` dentro de bucles. Scope
candidato para v1.63 si la racha de perf continúa.

## [1.61.0] — 2026-05-16 — Perf round 2: `cadena_unir` nativo + `csv` con iterator

**Hallazgo cazado midiendo, no asumiendo.** Tras 19 releases sin
tocar rendimiento, esta release introduce 3 benchmarks nuevos para
las stdlib recientes (`csv_parse_1000`, `sha256_1mb`,
`base64_round_trip`) y descubre dos O(n²) escondidos en pure-Cornamusa.
**`csv_parse_1000` cae de 1036ms → ~31ms (33× speedup).**

### Lo que se midió

Suite ampliada en `benchmarks/`:

- `csv_parse_1000.cor` — parsear CSV de 1000 filas × 5 columnas.
- `sha256_1mb.cor` — SHA-256 de 1 MiB de input.
- `base64_round_trip.cor` — codec round-trip sobre 100 KiB.

### Bottlenecks encontrados

#### 1. `cadenas.unir` era O(n²)

El módulo hacía `resultado = resultado + sep + parte` en un loop —
cuadrático por la copia obligada en cada concatenación.

**Fix**: nueva built-in C `cadena_unir(lista, sep)` que pre-calcula
longitud total, alloca una sola vez y usa `memcpy`. Lineal en bytes
de salida. `stdlib/cadenas.cor::unir` ahora es un wrapper de una
línea que delega a la nativa.

#### 2. `texto[i]` para cadenas UTF-8 es O(i)

Cornamusa preserva la semántica char-Unicode: `s[i]` walk-ea desde
el inicio del buffer para encontrar el carácter `i`-ésimo, porque
los chars pueden ocupar 1-4 bytes (utf8proc_iterate). El parser CSV
hacía `texto[i]` en un while-loop, resultando en **O(n²)** total.

**Fix**: usar `para c en texto` que usa el iterator interno
(O(1) amortizado por carácter, O(n) total). Cambio idiomático en
`csv.parsear`.

#### 3. `cadenas.repetir` mismo problema que `unir`

Era `resultado += s` en loop. Reescrito con buffer de lista +
`cadena_unir`. No medido en benchmark separado, pero cualquier
código que usaba `cadenas.repetir(s, N)` con N grande se beneficia.

### Resultados

| Benchmark              | Antes     | Después   | Δ          |
|------------------------|-----------|-----------|------------|
| **`csv_parse_1000`**   | **1036 ms** | **~31 ms** | **−97% (33×)** |
| `bignum_factorial`     | 0.008 s   | 0.007 s   | ~ –       |
| `dicc_intensivo`       | 0.036 s   | 0.035 s   | ~ –       |
| `fibonacci_recursivo`  | 0.218 s   | 0.205 s   | ~ –       |
| `globales_lookup`      | 0.188 s   | 0.190 s   | ~ –       |
| `oo_dunder_aritmetico` | 0.069 s   | 0.072 s   | ~ –       |
| `oo_dunder_indice`     | 0.031 s   | 0.034 s   | ~ –       |
| `oo_intensivo`         | 0.016 s   | 0.014 s   | ~ –       |
| `sha256_1mb`           | 165 ms    | 160 ms    | ~ –       |
| `base64_round_trip`    | 125 ms    | 110 ms    | ~ –       |

Sin regresiones en los demás benchmarks. El speedup es totalmente
atribuible al fix de los dos O(n²) en el path caliente del parser
CSV.

### Implementación

- Nueva nativa `nativa_cadena_unir` en `src/nativos.c` (~50 líneas C):
  valida tipos en una pasada, suma longitud total, alloca una vez,
  copia con memcpy. Maneja sep="" (default) y sep custom.
- `stdlib/cadenas.cor::unir` y `::repetir` reescritas para delegar
  a `cadena_unir`.
- `stdlib/csv.cor::parsear` cambia `while i < n: c = texto[i]; i++`
  a `para c en texto`. Y `campo_actual += c` cambia a buffer de
  lista + `cadena_unir` al final del campo.

### Lo que NO se optimizó

- **fibonacci/OO/globales_lookup**: ya están a un nivel saludable
  tras IC + small-int (v0.10-v0.11) y LTO+O3 (v1.40). Mediciones
  consistentes muestran ~210ms para fib(30), ~180ms para 4M global
  reads. Cualquier ganancia adicional probablemente requeriría
  JIT/tracing (post-v2.0).
- **Designated initializer en `ejecutar_llamar_bc`**: probado y
  medido, **sin efecto en perf** (LTO+O3 ya genera código
  equivalente). Cambio mantenido por concisión (1 designated init
  vs 11 stores explícitos).

### Lección

Cinco releases recientes (`csv`, `base64`, `hashing`, scope analysis,
otros) pasaban todos los tests verdes con inputs de 5-10 elementos.
Solo midiendo carga realista (1000 filas) apareció el O(n²). El
principio "datos antes que feeling" (de la práctica del proyecto)
aplicó: antes de optimizar fibonacci (donde no hay wins obvios), se
midió lo nuevo y encontró el agujero.

### Verificación

- **220/220 tests verde**. Sin regresiones.
- 3 nuevos benchmarks añadidos.
- `benchmarks/RESULTS.md` actualizado con sección v1.61 detallada.

## [1.60.0] — 2026-05-16 — Stdlib `hashing` (SHA-256 + MD5 nativos)

Tercera release consecutiva expandiendo la stdlib (`csv` en v1.58,
`base64` en v1.59, `hashing` ahora). **Stdlib pasa de 14 a 15 módulos.**

Cierra la trilogía de "infraestructura de scripting":
- **json** (intercambio universal de datos estructurados, v1.9).
- **csv** (datos tabulares, v1.58).
- **base64** (text ↔ binary representation, v1.59).
- **hashing** (integrity / fingerprinting / signatures, v1.60).

### API

```cornamusa
importar hashing

hashing.sha256(cadena)   → cadena hex de 64 chars
hashing.md5(cadena)      → cadena hex de 32 chars
```

Implementación nativa en C (`src/hashing.{c,h}` siguiendo el patrón
de `regex` y `red`). Wrappers `nativa_sha256` y `nativa_md5` en
`nativos.c` exponen `hash_sha256` y `hash_md5` como builtins; el
módulo `stdlib/hashing.cor` los re-exporta con nombres limpios.

### Vectors validados

**SHA-256** (FIPS 180-4 §B / RFC 6234):
- `""` → `e3b0c442...b855`.
- `"abc"` → `ba7816bf...15ad`.
- `"abcdbcdecdefdef...nopq"` (56 bytes, ejercita 2-block padding) → `248d6a61...06c1`.
- `"The quick brown fox jumps over the lazy dog"` → `d7a8fbb3...e592`.
- `"...lazy cog"` (1 char distinto: avalanche) → `e4c4d8f3...81be`.
- **1 millón de "a"** (FIPS B.3, streaming multi-bloque) → `cdc76e5c...12cd0`.

**MD5** (RFC 1321 §A.5):
- `""` → `d41d8cd9...427e`.
- `"a"` → `0cc175b9...2661`.
- `"abc"` → `90015098...7f72`.
- `"message digest"` → `f96b697d...61d0`.
- `"abcdefghijklmnopqrstuvwxyz"` → `c3fcd3d7...e13b`.
- `"ABC...XYZabc...xyz0...9"` → `d174ab98...9d9f`.
- `"1234...80digits"` → `57edf4a2...b67a`.
- 56-byte vector compartido con SHA-256 → `8215ef07...664a`.

### Implementación

`src/hashing.c` (~200 líneas C, sin dependencias externas):

- **SHA-256**: estado de 8 × `uint32_t`, procesa bloques de 64 bytes,
  64 rondas con la tabla constante de cube-roots-of-primes (K[64]),
  funciones `rotr32` + `Σ0/Σ1/Maj/Ch` clásicas, padding `0x80` +
  zeros + 64-bit BE length.

- **MD5**: estado de 4 × `uint32_t`, procesa bloques de 64 bytes con
  las 4 funciones de ronda (F/G/H/I), 64 rondas con tabla de
  `floor(2^32 × abs(sin(i+1)))` (K[64]) y shifts per-round (S[64]),
  padding mismo esquema pero longitud en LE (no BE).

Helpers compartidos: `escribir_hex`, `leer_be32`/`escribir_be32`,
`leer_le32`/`escribir_le32`, `rotr32`/`rotl32`.

### Verificación

- **220/220 tests verde**.
- Nuevo `test_hashing` con **14 asserts** contra test vectors RFC.
- Nuevo `bc_run_68_hashing` integration test.
- Nuevo `examples/68_hashing.cor` demuestra: test vectors, avalanche
  effect, checksum de archivo, cache key con MD5, aviso sobre uso
  seguro.
- `cornamusa lint stdlib/hashing.cor` → 0 warnings.
- `cornamusa fmt --check stdlib/hashing.cor` → ya canónico.

### Notas de seguridad

Documentado explícitamente en `stdlib/hashing.cor`:

- **MD5 está criptográficamente roto desde 2004** (colisiones
  prácticas). Sigue siendo útil para:
  - Integridad casual (detectar corrupción accidental).
  - Cache keys (probabilidad de colisión accidental ~0).
  - Compatibilidad con sistemas legacy.
  - **NO** usar para firmas, hashes de passwords, ni cualquier cosa
    que requiera resistencia a colisiones.

- **SHA-256** sigue considerado seguro para:
  - Integridad de archivos.
  - Construcción de HMAC (manualmente: 2 llamadas + XORs).
  - Como parte de protocolos (TLS, Bitcoin, JWT...).
  - **Para passwords** usa scrypt/argon2 — no provistos por
    Cornamusa.

### Lo que NO incluye (scope para v1.61+)

- **SHA-1**: obsoleto, no añadido a propósito.
- **SHA-384, SHA-512**: posibles en v1.61 si surge demanda (mismo
  algoritmo que SHA-256 con words de 64 bits y constantes
  distintas).
- **SHA-3 / Keccak**: algoritmo distinto (sponge), scope futuro.
- **HMAC**: construible sobre `sha256` manualmente; un wrapper en
  stdlib quedará para v1.61 si surge demanda.
- **Hashing incremental** (procesar archivo en chunks de megabytes):
  el input es toda la cadena de una vez. Para archivos pequeños
  (<10 MB) sigue siendo rápido.

## [1.59.0] — 2026-05-16 — Stdlib `base64` (RFC 4648 codec nativo)

Continúa la expansión de stdlib tras `csv` en v1.58. Nuevo módulo
`base64` con implementación nativa en C — rápido incluso para inputs
grandes. Casos de uso típicos: HTTP Basic Auth, Data URIs, JSON Web
Tokens, embeber binarios en archivos de texto.

**Stdlib pasa de 13 a 14 módulos.**

### API

```cornamusa
importar base64

base64.codificar(cadena)    → cadena base64
base64.decodificar(cadena)  → cadena original
```

Implementación nativa en C expone `base64_codificar` y
`base64_decodificar` como built-ins; `stdlib/base64.cor` es un
wrapper delgado (≈4 líneas).

### Cobertura RFC 4648

- Alfabeto estándar `A-Z a-z 0-9 + /` con padding `=`.
- Validados los 7 test vectors de §10 (`""`, `"f"`, `"fo"`, `"foo"`,
  `"foob"`, `"fooba"`, `"foobar"`).
- Decodificador tolerante a whitespace (espacios, `\n`, `\r`, `\t`)
  para soportar entrada MIME-style con line-wrap.
- Errores atrapables (`ErrorDeValor`):
  - Carácter fuera del alfabeto.
  - Padding `=` en mitad de la cadena.
  - Longitud no múltiplo de 4 (tras filtrar whitespace y padding).
  - Resto de 1 carácter (no es codificación válida).

### Casos de uso demostrados

```cornamusa
# HTTP Basic Auth (RFC 7617):
creds = "usuario:contraseña"
header = "Authorization: Basic " + base64.codificar(creds)

# Data URI:
data_uri = "data:image/png;base64," + base64.codificar(bytes_png)

# Round-trip identidad:
base64.decodificar(base64.codificar(s)) == s
```

### Implementación

Nuevas natives en `src/nativos.c`:
- `base64_codificar(s)` — ~50 líneas C, procesa 3 bytes → 4 chars con
  table lookup en alfabeto constante.
- `base64_decodificar(s)` — ~80 líneas C, filtra whitespace, valida
  padding, lookup inverso vía función.

Trabaja sobre los bytes UTF-8 subyacentes de las cadenas Cornamusa,
así que cualquier dato representable como cadena (incluyendo binario
si el caller puede construirlo) funciona.

Nuevo `stdlib/base64.cor`: wrapper con docs (~4 líneas funcionales).

### Lo que NO incluye (scope para v1.60+)

- **Variante URL-safe** (RFC 4648 §5): usa `-` y `_` en lugar de `+`
  y `/`. Sin padding por convención. Útil para JWTs.

### Verificación

- **217/217 tests verde**.
- Nuevo `test_base64` con **18 asserts**:
  - 7 test vectors de RFC 4648 §10 (codificación).
  - 7 vectores inversos (decodificación).
  - Tolerancia a whitespace (2 casos).
  - Round-trip HTTP Basic Auth (2 casos).
- Nuevo `bc_run_67_base64` integration test.
- Nuevo `examples/67_base64.cor` cubre 6 casos de uso.
- `cornamusa lint stdlib/base64.cor` → 0 warnings.
- `cornamusa fmt --check stdlib/base64.cor` → ya canónico.

## [1.58.0] — 2026-05-16 — Stdlib `csv` (parser/writer RFC 4180-like)

Tras cerrar el lenguaje core en v1.57, vuelta a ampliar la stdlib.
Nuevo módulo `csv` — el formato más universal para intercambio de
datos tabulares, lectura de exports de Excel/Google Sheets, logs
estructurados, y prácticamente cualquier tarea de scripting con
datos.

**Stdlib pasa de 12 a 13 módulos.**

### API

```cornamusa
importar csv

# Parser
filas = csv.parsear(texto)                   # [["a","b"], ["1","2"]]
filas = csv.parsear(texto, sep=";")           # separador configurable
fila  = csv.parsear_linea(linea)              # una sola fila

# Writer
texto = csv.serializar(filas)
linea = csv.serializar_linea(campos)

# Helpers de archivo
filas = csv.leer("datos.csv")
csv.escribir("salida.csv", filas)
```

### Cobertura del estándar

Sigue RFC 4180 con tolerancia razonable:

- **Campos sin quotes**: leídos literal hasta el separador o newline.
- **Campos quoted (`"..."`)**: pueden contener separadores, newlines
  y comillas escapadas (`""` → `"`).
- **Separador configurable**: `,` por defecto, soporta cualquier
  single-char (`;`, `\t`, `|`, etc.).
- **Line endings**: parser acepta tanto `\n` como `\r\n`; writer
  siempre emite `\n`.
- **Escape automático al serializar**: si un campo contiene el
  separador, `"`, `\n`, o `\r`, se wrappea en quotes y los `"` se
  duplican.

### Limitaciones reconocidas

- **Sin inferencia de tipos**: todos los campos vienen como cadena.
  Conversión manual (`entero(...)`, `decimal(...)`).
- **Sin headers automáticos**: la primera fila se trata como cualquier
  otra. Si quieres dict-style access, conviértelo tú:
  `cabeceras = filas[0]; resto = filas[1:]`.
- **Separador debe ser single-char** (consistente con `csv.reader` de
  Python — multi-char es muy raro en CSVs reales).
- **Performance**: pure-Cornamusa, no bindings nativos. Para CSVs
  grandes (decenas de MB) puede ser lento. Los tests sobre archivos
  típicos (cientos de KB) son rápidos.

### Implementación

Nuevo `stdlib/csv.cor` (~140 líneas Cornamusa). Parser state-machine
con tres estados (campo normal, dentro de quoted, tras quote cerrado).
Writer con `_necesita_quote` + `_escapar` helpers.

### Verificación

- **214/214 tests verde**. Nuevo `bc_run_66_csv` ejecuta el ejemplo
  con `PASS_REGULAR_EXPRESSION` verificando que el round-trip
  reproduce campos con comas internas.
- Nuevo `examples/66_csv.cor` cubre los 6 casos de uso principales:
  parsing básico, quoted con comas, escapes de comillas, separador
  alternativo, round-trip semánticamente idéntico, ida/vuelta a
  archivo.
- `cornamusa lint stdlib/csv.cor` → 0 warnings.
- `cornamusa fmt --check stdlib/csv.cor` → ya en formato canónico.

### Test interactivo de round-trip

```cornamusa
importar csv

datos = [
    ["producto", "precio", "notas"],
    ["queso, manchego", "8.50", "lo dice \"el experto\""],
    ["aceite", "5.00", ""],
]

texto = csv.serializar(datos)
imprimir(texto)
# producto,precio,notas
# "queso, manchego",8.50,"lo dice ""el experto"""
# aceite,5.00,

reparseado = csv.parsear(texto)
# reparseado es semánticamente == datos
```

## [1.57.0] — 2026-05-16 — `global X` implementado en bytecode VM

Cierra el último gap real del lenguaje core. Con esto, **el cuarteto
de scoping queda completo**: locales (default), `nolocal` (v1.4),
`global` (v1.57), y closures (upvalues automáticos).

### Lo nuevo

Dentro de una función, `global X` declara que las asignaciones (y
aug-assigns) sobre X van al scope del módulo en lugar de crear una
local nueva. Sin la declaración, el comportamiento por defecto en
Cornamusa es: la asignación dentro de función crea local — el global
permanece intacto.

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

Diferencia con el patrón pre-v1.57 (workaround vía dict mutable):

```cornamusa
# v1.56 y antes: workaround con dict.
estado = {"contador": 0}
funcion inc():
    estado["contador"] += 1   # muta el dict — no reasigna

# v1.57: idiom natural con global.
contador = 0
funcion inc():
    global contador
    contador += 1
```

`global` también puede **crear** la variable a nivel módulo si no
existía antes (semántica Python):

```cornamusa
funcion lazy_init():
    global cache
    cache = {"ready": verdadero}
fin funcion

lazy_init()
imprimir(cache)   # {"ready": verdadero}
```

### Validaciones del compilador

- `global` fuera de cualquier función → `ErrorDeSintaxis`.
- `global X` cuando X ya es local del scope actual → `ErrorDeSintaxis`
  (contradictorio: no puede ser local y global a la vez).
- `global X` cuando X ya está marcado `nolocal` en el mismo scope →
  `ErrorDeSintaxis` (solo uno o el otro).
- Múltiples nombres en una declaración: `global a, b, c` — válido.
- Declaraciones duplicadas (`global x; global x`) — idempotente, no
  error.

### Implementación

Cambios en `ScopeCompilador` (`src/compilador.h`):
- Nuevo array `globales[COMPILADOR_NOLOCALES_MAX]` paralelo a
  `nolocales[]`. Cada entrada guarda (nombre, longitud_nombre).
- Contador `n_globales`.

Cambios en `src/compilador.c`:
- Helper `es_global_declarado(scope, name, len)`.
- Caso `SENT_GLOBAL`: valida no-conflict con local/nolocal, registra
  cada nombre en el array.
- En `EXPR_IDENT` (lectura): si `es_global_declarado`, salta los
  intentos de local/upvalue y emite `OP_OBTENER_GLOBAL`.
- En `compilar_asignar` con destino `EXPR_IDENT`: si declarado
  global, emite `OP_DEFINIR_GLOBAL` en lugar de `OP_ASIGNAR_LOCAL`.
- En `compilar_asignar_aug` con destino `EXPR_IDENT`: mismo
  bypass — fuerza el path global tanto en el load como en el store.

Sin cambios en VM o bytecode — reusa `OP_OBTENER_GLOBAL` /
`OP_DEFINIR_GLOBAL` existentes.

### Verificación

- **211/211 tests verde**.
- Nuevo `test_bytecode_global` con **13 asserts**:
  - `global X` modifica el global existente.
  - `global X` con aug-assign (`+=`).
  - `global X` crea nuevo si no existía.
  - Sin `global`: crea local, no modifica global.
  - Errores: `global` top-level / conflict con local / conflict con `nolocal`.
  - Múltiples nombres en declaración.
- Nuevo `examples/65_global.cor` ejecuta correctamente.
- 0 regresiones en suite existente.

### Estado del lenguaje core

Con `global` cerrado, el modelo de scoping de Cornamusa está
completo:

| Mecanismo | Significado |
|---|---|
| Default | Asignación crea local en función |
| Read implícito | Lookup: local → upvalue (closure) → global → builtin |
| `nolocal X` (v1.4) | X refiere a local del scope enclosing |
| `global X` (v1.57) | X refiere al scope de módulo |
| Closures | Capturan locales automáticamente (lectura) |

No hay otros gaps de scoping/aliasing conocidos.

## [1.56.0] — 2026-05-16 — Lenguaje core: `borrar` y aug-assign sobre atributos

Vuelta al lenguaje tras el arco de tooling de Fase 5. Cierra dos gaps
del lenguaje que se hicieron evidentes en el housekeeping de v1.55.1
(donde tuve que reescribir ejemplos que usaban estas features).

### Lo nuevo

#### 1. `borrar` para colecciones e instancias

Hasta v1.55, `borrar` era keyword reservada por el lexer pero el
parser la rechazaba con "se esperaba una expresión". Ahora funciona:

```cornamusa
# Diccionario: quita la entrada con esa clave.
d = {"a": 1, "b": 2}
borrar d["a"]
imprimir(d)  # {"b": 2}

# Lista: quita el elemento del indice, desplaza el resto.
lst = [10, 20, 30, 40]
borrar lst[1]
imprimir(lst)  # [10, 30, 40]

# Conjunto: quita el elemento.
s = {1, 2, 3}
borrar s[2]
imprimir(s)  # {1, 3}

# Atributo de instancia: quita del dicc interno de atributos.
obj.cache = "data"
borrar obj.cache
# obj.cache  → ErrorDeAtributo
```

**Errores atrapables**:
- `ErrorDeClave`: clave no presente en dict/conjunto.
- `ErrorDeIndice`: indice fuera de rango en lista.
- `ErrorDeAtributo`: atributo no presente en instancia.
- `ErrorDeTipo`: el objeto no soporta `borrar` (p. ej. cadenas).

#### 2. Aug-assign sobre atributos: `obj.x op= valor`

Hasta v1.55, `obj.x += 1` daba `ErrorDeSintaxis: destino de asignacion
aumentada no soportado en bytecode v0.6`. Había que escribir
`obj.x = obj.x + 1`. Ahora funciona todo el set:

```cornamusa
clase Contador:
    funcion __iniciar__(yo, inicial=0):
        yo.valor = inicial
    fin funcion
fin clase

cnt = Contador(10)
cnt.valor += 5      # 15
cnt.valor -= 2      # 13
cnt.valor *= 3      # 39
cnt.valor //= 4     # 9
cnt.valor %= 5      # 4
cnt.valor **= 3     # 64
```

El compilador desazucara `obj.x op= v` a:
```
compile obj                ; [obj]
OP_DUP                     ; [obj, obj]
OP_OBTENER_ATRIBUTO        ; [obj, obj.x]
compile v                  ; [obj, obj.x, v]
OP_op                      ; [obj, resultado]
OP_ASIGNAR_ATRIBUTO        ; pop ambos, set obj.x = resultado
OP_DESCARTAR
```

Hace una sola evaluación del objeto (correcta cuando hay efectos
secundarios) y reusa el OP_OBTENER_ATRIBUTO cacheable.

### Implementación

#### AST + parser
- `SENT_BORRAR { Expr *destino }` añadido al AST.
- Parser: handler para `TT_BORRAR` que parsea la expresión siguiente
  y construye SENT_BORRAR.
- Pretty-printer en `ast.c`: `(borrar destino)`.

#### Compilador
- SENT_BORRAR: si destino es EXPR_INDICE emite `OP_BORRAR_INDICE`;
  si es EXPR_ATRIBUTO emite `OP_BORRAR_ATRIBUTO` con índice de
  nombre. Otros destinos: ErrorDeSintaxis claro.
- SENT_ASIGNAR_AUG con destino EXPR_ATRIBUTO: nueva rama que emite
  la secuencia descrita arriba.

#### Bytecode (chunk.h)
- Nuevos opcodes:
  - `OP_BORRAR_INDICE` (1 byte): stack `[obj, clave]` → quita.
  - `OP_BORRAR_ATRIBUTO` (2 bytes: opcode + name_idx): stack `[obj]` → quita.

#### VM (vm.c)
- Handler de `OP_BORRAR_INDICE`: dispatch por tipo (dict/list/conjunto)
  → llama a `dicc_quitar`/desplazamiento manual/`conj_quitar`.
- Handler de `OP_BORRAR_ATRIBUTO`: requiere VAL_INSTANCIA, llama a
  `dicc_quitar` sobre el dicc de atributos.

#### Linter (linter.c)
- Visita SENT_BORRAR como referencia (no asignación). El `obj` de
  `borrar obj.attr` y el `obj` + `key` de `borrar obj[k]` se marcan
  como usados en su scope.

### Verificación

- **209/209 tests verde**. Nuevo `test_bytecode_borrar_augattr` con
  14 asserts:
  - `borrar d[k]` (dict).
  - `borrar lst[i]` (lista, desplaza).
  - `borrar obj.attr` (instancia).
  - `ErrorDeClave` atrapable.
  - `obj.x += / -= / **=` (3 variantes).
- Nuevo `examples/64_borrar_y_aug_attr.cor` ejecuta correctamente
  con `--bytecode`.
- 0 regresiones en los tests existentes.

### Pendiente (scope para v1.57+)

- `global X` declarado en función no transfiere asignaciones al
  scope de módulo (la keyword existe, el linter la trata, pero el
  compilador la rechaza con "no implementada en bytecode v0.9").
- `borrar variable_local` (sin índice ni atributo) — no soportado;
  para limpiar referencias en Cornamusa usa `var = nulo`.

## [1.55.1] — 2026-05-16 — Housekeeping: dogfooding del toolkit sobre el propio repo

Release de _housekeeping_ (sin features nuevas). Aplica el toolkit
construido en releases anteriores al propio repo. Cierre simbólico
del arco de Fase 5: el repo entero pasa sus propios checks.

### Cambios

- **`== nulo` → `es nulo` y `!= nulo` → `no es nulo`** en 4 archivos
  donde el linter reportaba `eq-nulo` (10 sitios en total):
  - `examples/32_json_archivos.cor:47`
  - `examples/35_iteradores.cor:115,121,130,134`
  - `examples/58_repr_booleano.cor:94,97`
  - `stdlib/regex.cor:58,64`

- **`para i en rango(n)` → `para _ en rango(n)`** en `stdlib/cadenas.cor:19`
  (el contador no se usa — convención `_` para descartes).

- **Bugs preexistentes en `examples/06_diccionarios.cor` arreglados**:
  - Usaba `texto.dividir(" ")` (método inexistente) → cambiado a
    `desde cadenas importar separar` + `separar(texto, " ")`.
  - Usaba destructuring en `para` (`para palabra, conteo en
    frecuencias.elementos():`) — no soportado. Cambiado al idiom
    canónico: `para palabra en frecuencias: ... frecuencias[palabra]`.

- **Bug preexistente en `examples/11_iterador.cor` arreglado**:
  - Usaba `lanzar FinDeIteración()` (nombre incorrecto con tilde) →
    `lanzar ErrorDeIteracion()` (nombre real desde v1.43).
  - Usaba `yo.actual -= 1` (augmented assign sobre atributo, no
    soportado en bytecode) → `yo.actual = yo.actual - 1`.
  - One-liner `si cond: lanzar ...` → forma multi-línea con `fin si`.

- **`cornamusa fmt` aplicado a 12 archivos** que necesitaban reformat
  (mayoritariamente: colapsar 2 líneas en blanco a 1, normalizar
  trailing newlines).

### Estado tras housekeeping

- **0 archivos** con parse-error en `examples/` y `stdlib/`.
- **0 archivos** necesitan reformat (`cornamusa fmt --check` limpio
  sobre todos).
- **1 archivo** con un único warning del linter:
  `examples/42_defaults.cor:35` — `mutable-default`. **Intencional**:
  el ejemplo demuestra el footgun, con comentario "CUIDADO: el mismo
  objeto se reutiliza entre llamadas" justo arriba.
- **208/208 tests** verde.
- Ejemplos arreglados verificados ejecutando con `--bytecode`.

### Significado

Esto cierra el arco que empezó en v1.47. El proyecto ahora puede
afirmar: "todo el código del repo pasa los propios checks del
toolkit, salvo casos didácticos explícitamente documentados". Es la
señal de salud que un proyecto maduro debería poder dar.

## [1.55.0] — 2026-05-16 — Linter: shadow + loop-var + mutable-default

Novena release de la **Fase 5 — Tooling**. Tres checks nuevos en el
linter, todos clásicos en suites como pyflakes/pylint y de alta
utilidad práctica.

### Lo nuevo

#### 1. `shadow`

Detecta cuando un local en una función anidada introduce un nombre
que **ya existe en algún scope exterior**. Útil para evitar bugs
sutiles donde el lector cree que un nombre se refiere al outer.

```cornamusa
funcion outer():
    x = 1
    funcion inner():
        x = 2          # ← shadow: 'x' sombrea variable del scope exterior
        retornar x
    fin funcion
    retornar inner() + x
fin funcion
```

**Skip rules**:
- `nolocal X` / `global X` **no** generan shadow (son intencionales).
- Nombre `_` (descarte) **no** genera shadow.
- Nombre `yo` (self implícito) **no** genera shadow.

Se chequea tanto en asignaciones como en parámetros de función
declarados en scopes anidados.

#### 2. `unused-loop-var`

Detecta `para X en ...:` donde X no se referencia ni en el cuerpo
del bucle ni después de él. Idiom: usar `_` para descartes.

```cornamusa
para i en rango(10):       # ← unused-loop-var: 'i' no se usa
    imprimir("ping")
fin para
```

vs.

```cornamusa
para _ en rango(10):       # ← OK, _ es convención de descarte
    imprimir("ping")
fin para
```

**Tracking**: la variable de bucle se registra en el scope de la
función con tipo `DECL_LOOP_VAR`. Si se referencia desde dentro del
cuerpo o desde código posterior (Cornamusa preserva la variable tras
el bucle, igual que Python), se marca como usada.

#### 3. `mutable-default`

Detecta parámetros con **default mutable**: `=[]`, `={}`, `={1, 2}`.
Estos literales se evalúan una sola vez al definir la función y se
comparten entre llamadas — bug clásico de Python:

```cornamusa
funcion agregar(item, log=[]):       # ← mutable-default
    log.anadir(item)
    retornar log
fin funcion

agregar("a")      # → ["a"]
agregar("b")      # → ["a", "b"]   sorpresa!
```

Detecta literales `EXPR_LISTA`, `EXPR_DICCIONARIO`, `EXPR_CONJUNTO`.
**No** intenta detectar fábricas como `lista()` o `dict()`, que
también producen objetos mutables — esos requieren resolución de
referencia y quedan para v1.56+.

Aplica a `funcion` y `lambda`.

### Total de categorías

El linter ahora cubre **9 checks**:

| Categoría | Desde | Qué detecta |
|---|---|---|
| `unreachable` | v1.49 | Código tras `retornar`/`romper`/`continuar`/`lanzar` |
| `redundant-pasar` | v1.49 | `pasar` en bloque con otras sentencias |
| `eq-nulo` | v1.49 | `== nulo` / `!= nulo` → sugiere `es nulo` |
| `unused-import` | v1.49 | Módulo importado pero no usado |
| `unused-local` | v1.50 | Variable local nunca leída |
| `unused-param` | v1.50 | Parámetro nunca usado (skip `yo`, `_`, varargs) |
| `shadow` | **v1.55** | Local sombrea variable del scope exterior |
| `unused-loop-var` | **v1.55** | `para X` con X no usado |
| `mutable-default` | **v1.55** | Default `=[]`/`={}` literal |

### Implementación

- `src/linter.{c,h}`: añadidas constantes `LINT_SHADOW`,
  `LINT_UNUSED_LOOP_VAR`, `LINT_MUTABLE_DEFAULT` y `DECL_LOOP_VAR`.
- Nueva función `verificar_shadow` que walk hacia padres buscando
  matches no-`es_extern`.
- Nueva función `verificar_mutable_default` que clasifica defaults
  por tipo de Expr literal.
- Nueva función `declarar_objetivo_para` para target de `SENT_PARA`.
- `scope_declarar` ahora devuelve `bool` (true si fue inserción
  nueva) — gatilla shadow check solo en nuevos locales.
- `emitir_warnings_scope` distingue `DECL_LOOP_VAR` para emitir
  `unused-loop-var` con mensaje específico.

### Verificación

- **208/208 tests verde**. `test_linter` extendido a **48 asserts**
  (vs 36 antes) cubriendo:
  - Shadow: local, param, nolocal-no-warn, underscore-no-warn.
  - Loop-var: no usado, usado en body, `_` skip, usado tras loop.
  - Mutable: lista, dict, defaults inmutables OK, lambda con
    mutable default.

### Hallazgos en el repo

Tras el merge, el barrido de los 63 ejemplos + 12 módulos stdlib
encuentra exactamente:

- `examples/42_defaults.cor:35` → `mutable-default` (legítimo: el
  ejemplo **demuestra intencionadamente** el footgun, con comentario
  "CUIDADO: el mismo objeto se reutiliza").
- `stdlib/cadenas.cor:19` → `unused-loop-var` (legítimo: `para i en
  rango(n)` donde `i` no se usa, podría ser `_`).

**0 falsos positivos**. Ambos hallazgos son valiosos para limpieza
opcional en una release minor.

## [1.54.0] — 2026-05-16 — LSP: goto-definition + formatting

Octava release de la **Fase 5 — Tooling**. El LSP gana dos capacidades
estándar: navegación (goto-definition) y formato del archivo entero
delegado al formateador interno. Con esto cubre las 4 acciones
"básicas" que un usuario espera de un editor con soporte de lenguaje:
diagnostics, hover, goto-def, formatting.

### Lo nuevo

#### `textDocument/definition` (goto-def)

Capability nueva: `definitionProvider: true`.

Al hacer **Ctrl-click** o **F12** sobre el nombre de una función o
clase top-level, el editor salta a la declaración. La respuesta es
una `Location` con `uri` + `range` apuntando exactamente al nombre
en su declarador:

```json
{
  "uri": "file:///t.cor",
  "range": {
    "start": {"line": 0, "character": 8},
    "end":   {"line": 0, "character": 17}
  }
}
```

El rango cubre el nombre exacto (no toda la línea) para que el
editor pueda resaltar el símbolo.

**Cómo funciona**:

1. Recibe `(line, character)` del cliente.
2. Extrae la palabra bajo el cursor.
3. Re-parsea el documento.
4. Busca SENT_FUNCION o SENT_CLASE top-level con ese nombre.
5. Calcula offset del nombre en el texto fuente
   (`s->como.funcion.nombre - texto`).
6. Convierte offset → `(line_0, col_0)`.
7. Emite Location con rango = `[col_0, col_0 + len(nombre))`.

Si el cursor no está sobre un identificador o el símbolo no existe
como top-level → `result: null`.

#### `textDocument/formatting`

Capability nueva: `documentFormattingProvider: true`.

"Format Document" del editor invoca al formateador interno
(`cornamusa fmt`) sobre el documento abierto y devuelve un solo
`TextEdit` que reemplaza el contenido entero:

```json
[
  {
    "range": {
      "start": {"line": 0, "character": 0},
      "end":   {"line": 9999, "character": 0}
    },
    "newText": "...código formateado..."
  }
]
```

El rango `end` con línea muy alta se clampea automáticamente al
final real del documento por el editor (comportamiento estándar LSP).

**Opciones ignoradas**: `options.tabSize` y `options.insertSpaces` se
ignoran — Cornamusa siempre usa 4 espacios (decisión B1).
Documentado como limitación.

### Capabilities anunciadas

```json
{
  "textDocumentSync": 1,
  "hoverProvider": true,
  "definitionProvider": true,
  "documentFormattingProvider": true
}
```

### Verificación

- **208/208 tests verde**. Sin tests unitarios nuevos —
  goto-def/formatting requieren state del LSP (document store) y se
  prueban por integración.
- **Script Python end-to-end** verifica:
  - Goto-def sobre referencia a función → location correcta (línea/col
    del nombre).
  - Goto-def sobre nombre de clase → location correcta.
  - Goto-def sobre carácter no-identificador → `null`.
  - Formatting sobre documento mal indentado → TextEdit con el doc
    reformateado correctamente (mismo output que `cornamusa fmt`).

### Lo que NO incluye (scope para v1.55+)

- **Goto-def para parámetros, locales, atributos** (necesita scope
  analysis runtime).
- **Goto-def para símbolos importados** (`importar mat; mat.PI`).
- **`textDocument/completion`** (sugerencias al teclear) — bigger
  lift, requiere análisis de scope completo + lista de built-ins.
- **`workspace/symbol`** (índice de todos los símbolos del workspace).
- **Incremental document sync**.
- **Range formatting** (`textDocument/rangeFormatting`) — formato
  parcial de una selección.

### Notas

Con esto el LSP cubre lo que un usuario espera de un servidor "básico
pero útil": real-time diagnostics, hover docs, goto-def, formatting.
Lo que falta (completion, refactoring) es propio de servidores
avanzados.

## [1.53.0] — 2026-05-16 — LSP polish: parse errors estructurados + hover

Séptima release de la **Fase 5 — Tooling**. Pulido del LSP MVP de
v1.52 con dos mejoras visibles para el usuario.

### Lo nuevo

#### 1. Parse errors estructurados

El parser ahora puede acumular sus errores como **datos estructurados**
en lugar de imprimirlos directamente a stderr. El LSP lo aprovecha
para emitir diagnostics con línea, columna y mensaje **reales** del
parser (antes era un placeholder genérico "Error de sintaxis").

**Refactor del parser**: añadidos campos `capturar_errores` (bool)
y `errores_capturados` (`ErroresParser *`) a la struct `Parser`. Si
el cliente los configura, `error_en` añade a la lista en vez de
imprimir. Comportamiento por defecto NO cambia — todos los binarios
existentes (`cornamusa`, `cornamusa --check`, etc.) siguen imprimiendo
a stderr exactamente igual.

```c
ErroresParser e = {0};
parser.capturar_errores = true;
parser.errores_capturados = &e;
parser_parsear_programa(&parser, &n);
/* e.items[0..e.n-1] son los errores. */
parser_errores_liberar(&e);
```

#### 2. `textDocument/hover` para top-level symbols

El LSP ahora soporta hover: pasar el cursor sobre el nombre de una
función o clase top-level dispara un popup con:

- **Firma** sintetizada del AST (parámetros con `*args`, `**kwargs`,
  defaults como `=...`).
- **Comentarios `#` precedentes** como documentación (estilo Go,
  igual que `cornamusa docs`).
- Para clases: lista de **métodos** con sus firmas.

Ejemplo de respuesta para hover sobre `factorial`:

```markdown
```cornamusa
funcion factorial(n)
```

Calcula el factorial recursivamente.
```

Ejemplo para hover sobre clase `Punto`:

```markdown
```cornamusa
clase Punto
```

Una clase ejemplo.

**Metodos:**
- `__iniciar__(yo, a, b)`
```

**Cómo funciona**:
1. Recibe `(line, character)` del cliente.
2. Convierte a offset en el texto del documento.
3. Extrae la palabra bajo el cursor por scanning de caracteres
   identificadores (`[a-zA-Z_]` + bytes UTF-8 continuos).
4. Re-parsea el documento (con captura de errores).
5. Si parsea OK, busca SENT_FUNCION o SENT_CLASE top-level con ese
   nombre.
6. Si lo encuentra, formatea markdown; si no, responde `null`.

Capabilities anunciadas: `hoverProvider: true`.

### Lo que NO incluye (scope para v1.54+)

- **Hover para parámetros, variables locales y atributos** (necesita
  mapeo posición → AST node).
- **Hover para métodos en uso** (`obj.metodo()` — necesita
  resolución de tipos, no trivial en lenguaje dinámico).
- **`textDocument/definition`** (goto-def) — naturalmente extensible
  desde el código de hover, pero ya scope para v1.54.
- **`textDocument/completion`** — bigger lift (analizar scope con
  imports/built-ins).
- **Incremental document sync**.

### Implementación

- `src/parser.{h,c}`: añadidos campos `capturar_errores` +
  `errores_capturados` y función `parser_errores_liberar`. Cambio
  conservador — el path por defecto (stderr) sigue igual.
- `src/lsp.c`: `diagnose_doc` ahora usa captura para emitir parse
  errors detallados. Nueva función `responder_hover` (~120 líneas)
  con helpers de extracción de palabra (`extraer_palabra`),
  resolución de offset (`pos_a_offset`), generación de markdown
  para funciones y clases.

### Verificación

- **208/208 tests verde**. Nuevo `test_parser_errores` con
  **14 asserts**: programa válido sin errores, error simple
  capturado con línea/mensaje, múltiples errores, liberación
  correcta, default sin captura.
- **Script Python de integración**: verifica hover sobre función
  (devuelve markdown con firma + doc), sobre clase (devuelve markdown
  con firma + doc + métodos), sobre carácter no-identificador
  (devuelve `result: null`).
- **Round-trip stderr**: el path clásico de errores a stderr sigue
  funcionando igual; tests de `--check` y `--ast` no se rompen.

## [1.52.0] — 2026-05-16 — Language Server Protocol MVP (`cornamusa lsp`)

Sexta y última release planeada de la **Fase 5 — Tooling**. Cornamusa
ahora habla **LSP** (Language Server Protocol): el binario incluye un
servidor que cualquier editor con cliente LSP (VS Code, Neovim, Emacs,
Helix, etc.) puede conectar para recibir diagnostics del linter en
tiempo real.

### Lo nuevo

- **Subcomando `cornamusa lsp`**: arranca el servidor por stdio con
  framing JSON-RPC (`Content-Length: N\r\n\r\n` + body).
- **Mini parser/builder JSON** en C: nuevo módulo `src/json_min.{c,h}`
  (~450 líneas). No depende de librerías externas. Cubre lo necesario:
  objetos, arrays, strings (con escapes básicos), numbers, bool, null.
- **Servidor LSP** en `src/lsp.{c,h}` (~300 líneas).

### Métodos LSP implementados

- `initialize` → responde con capabilities `{ textDocumentSync: 1 }`
  (sincronización full-document) y `serverInfo`.
- `initialized` → notification, ack interno.
- `shutdown` → responde `null`.
- `exit` → process exit limpio.
- `textDocument/didOpen` → guarda el documento, dispara linter,
  emite `publishDiagnostics`.
- `textDocument/didChange` → actualiza el documento, dispara linter,
  emite `publishDiagnostics`.
- `textDocument/didClose` → limpia diagnostics (envía array vacío),
  elimina el documento del store.

### Diagnostics

Los avisos del linter se traducen a `Diagnostic` LSP:

- `range`: línea/columna 1-indexed (linter) → 0-indexed (LSP). Single
  character range (start == end + 1 char).
- `severity`: 2 (Warning) para warnings del linter; 1 (Error) para
  parse errors.
- `source`: `"cornamusa"`.
- `code`: la categoría (`unused-local`, `eq-nulo`, etc.).
- `message`: el texto del aviso.

Parse errors: emiten un solo diagnostic genérico en línea 1 con
severidad Error. Los detalles específicos no se exponen porque el
parser actual imprime errores a stderr; surfacing estructurado queda
para v1.53.

### Detalles de implementación

- **Windows binary mode**: en el subcomando `lsp` se llama a
  `_setmode(_fileno(stdin/stdout), _O_BINARY)` para evitar que el
  modo texto convierta `\r\n` → `\n` y rompa el conteo de
  Content-Length.
- **Document store**: array simple URI → texto, capacidad 64
  documentos abiertos (suficiente para casi cualquier sesión).
- **stderr suprimido durante parse**: el parser imprime errores a
  stderr — para que esos errores no escapen al stdout LSP, durante el
  parse redirigimos `stderr` a `nul`/`devnull` y lo restauramos.
- **Sincronización completa**: cada `didChange` envía el documento
  entero (más sencillo y robusto para v1). Incremental sync es trabajo
  futuro.

### Verificación

- **207/207 tests verde**. Nuevo `test_json_min` con **24 asserts**
  cubre parse + build + escape + anidación + round-trip.
- **3 tests de integración manuales** vía script Python que envía
  mensajes JSON-RPC al binario y verifica las respuestas:
  - Initialize + didOpen + diagnostic → OK.
  - didOpen + didChange + didClose + cleanup → OK.
  - Parse error → diagnostic Error en línea 1 → OK.

### Cómo conectarlo a tu editor

**VS Code** (extensión genérica LSP): configurar `serverOptions`
para ejecutar `cornamusa lsp`, `documentSelector` para `*.cor`.

**Neovim** (built-in LSP):
```lua
vim.lsp.start({
  name = 'cornamusa',
  cmd = { 'cornamusa', 'lsp' },
  root_dir = vim.fn.getcwd(),
})
```

**Helix** (`languages.toml`):
```toml
[[language]]
name = "cornamusa"
file-types = ["cor"]
language-servers = ["cornamusa"]

[language-server.cornamusa]
command = "cornamusa"
args = ["lsp"]
```

### Lo que NO incluye (scope para v1.53+)

- `textDocument/hover` — mostrar firma + docstring al pasar el cursor
  sobre un símbolo. Requiere mapear posición (línea/col) → nodo AST,
  + reusar el `docs_generar` para extraer el doc del símbolo.
- `textDocument/definition` (goto-def) — mismo problema de mapeo.
- `textDocument/completion` — bigger lift, requiere análisis de
  scope con resolución de imports/built-ins.
- `textDocument/formatting` — invocar el formateador desde LSP.
- Surfacing detallado de errores de parser (refactor del parser para
  acumular errores en lugar de imprimir a stderr).
- Incremental document sync.

## [1.51.0] — 2026-05-16 — Generador de docs `cornamusa docs`

Quinta release de la **Fase 5 — Tooling**. Cornamusa ahora trae un
generador de documentación que produce Markdown desde el AST + los
comentarios `#` del archivo.

### Lo nuevo

- **Subcomando `cornamusa docs <archivo.cor>`**: imprime Markdown
  con la API documentada del módulo.
- **`-o salida.md`**: escribe a archivo en vez de stdout.

### Convención de docstrings

Cornamusa adopta el estilo **Go**: la documentación de una función,
clase o método son los **comentarios `#` consecutivos inmediatamente
anteriores a la declaración**, sin línea en blanco intermedia.

```cornamusa
# Multiplica dos numeros.
# Devuelve un entero o decimal segun los operandos.
funcion mul(a, b):
    retornar a * b
fin funcion
```

Una **línea en blanco corta la asociación** — útil para distinguir
comentarios de grupo de docstrings per-item:

```cornamusa
# Operaciones aritmeticas         # ← comentario de GRUPO

# Multiplica dos numeros.
funcion mul(a, b):
    ...
fin funcion
```

### Estructura del Markdown generado

- **H1**: título del módulo (derivado del basename del archivo, sin
  extensión `.cor`).
- Bloque de doc del módulo: comentarios `#` al inicio del archivo,
  hasta la primera línea no-comentario.
- **H2 `funcion(a, b)`**: cada `funcion` top-level con su firma
  sintetizada del AST.
- **H2 clase `Nombre`**: cada `clase` top-level. Si extiende, se
  indica con `extiende ...`.
- **H3 `metodo(yo, ...)`**: cada método dentro del cuerpo de una
  clase.

### Firmas sintetizadas

El AST no preserva la expresión de los defaults, así que se muestran
como `=...`. `*args` y `**kwargs` se preservan:

```
## `f(a, b=..., *rest, **kw)`
```

### Lo que NO hace (scope para v1.52+)

- **Docstrings estilo Python** (primer string literal del cuerpo).
- **Doc de constantes top-level** (asignaciones a nivel módulo).
- **Doc de imports / re-exports**.
- **Reconstrucción de expresiones de defaults** (saldría feo en muchos
  casos sin un pretty-printer de expresiones).
- **Enlaces cruzados** entre funciones (`[`foo`](#foo)`).
- **Salida HTML directa** — por ahora solo Markdown, conversor externo.

### Implementación

Nuevo módulo `src/docs.{c,h}` (~300 líneas). El AST aporta firmas,
nombres y números de línea; el texto fuente aporta los comentarios.
Construyo un `IndiceLineas` que mapea cada línea (1-indexed) a su
offset en el buffer fuente — permite saltar a la línea N y extraer
contenido sin re-scannear. Para cada item, walk-back recolecta
comentarios contiguos.

### Verificación

- **206/206 tests verde**. Nuevo `test_docs` con **18 asserts**:
  H1 con nombre, doc del módulo, firmas con defaults y `*args`/`**kw`,
  asociación de comentarios, línea en blanco corta, clases con
  métodos, orden preservado, módulo vacío.
- **12/12 módulos de stdlib generan docs sin error**. Resultado
  visualmente correcto para `matematicas`, `formato`, `cadenas`,
  `fechas`, `azar`, `archivos`, `json`, `regex`, `proceso`, `red`,
  `sistema`, `coleccion`.

### Limitación conocida

Si el último comentario antes de una función es un comentario de
**grupo** (sin blank line intermedia), se asocia como doc per-item.
Ejemplo en `matematicas.cor`:
```
# Operaciones simples
funcion cuadrado(n): ...
```
genera `### cuadrado(n)` con doc "Operaciones simples". Solución
recomendada: insertar línea en blanco entre comentario de grupo y la
primera función. Esto se podría auto-arreglar en una limpieza pasa
de stdlib en una release minor posterior.

## [1.50.0] — 2026-05-16 — Scope analysis en el linter (`unused-local`, `unused-param`)

Cuarta release de la **Fase 5 — Tooling**. El linter ahora hace
análisis de scope a nivel de función: detecta variables locales y
parámetros que se declaran pero nunca se leen.

### Lo nuevo

- **`unused-local`**: variable local de función asignada en el cuerpo
  pero nunca leída posteriormente.

- **`unused-param`**: parámetro de función nunca referenciado dentro
  del cuerpo.

  **Skip rules** (no warnean):
  - Nombre empieza con `_` (convención para descartes intencionales).
  - Nombre es `yo` (self implícito en métodos).
  - Parámetro `*args` o `**kwargs` (contenedores, a menudo solo
    reenviados).

### Cómo trata casos delicados

- **Closures con `nolocal` / `global`**: si una función anidada captura
  una variable del cuerpo enclosing con `nolocal`, la outer se marca
  como usada por la inner. El `nolocal X` registra X como
  `es_extern=true` en el scope inner — al resolver una referencia, el
  algoritmo se salta entradas `es_extern` y sigue hacia padres hasta
  encontrar el declarador real. Así `n = 0; funcion inc(): nolocal n;
  n = n + 1` no genera falsos positivos.

- **Destructuring**: `a, b = par` registra `a` y `b` por separado.
  Cada uno se analiza independientemente.

- **Reasignación**: `x = 1; x = x + 1; retornar x` no warnea porque
  `x` es leído en `x + 1` y en `retornar x`.

- **Lambdas**: se tratan como funciones. Parámetros no usados generan
  `unused-param`.

- **Module-level**: las variables top-level NO se warnean — podrían
  ser API pública y la información de uso no es local.

### Lo que NO chequea (queda para v1.51+)

- **Loop var no usado** (`para X en ...:` cuerpo sin usar X). Idiom
  recomendado: `para _ en ...`. La detección automática se aplaza
  porque a veces el loop var sí se usa indirectamente (índice de
  iteración).
- **Atrapar / con / pattern bind no usados**: misma razón.
- **Shadowing**: locales que sombrean variables del scope enclosing.
- **Funciones anidadas no usadas con sintaxis `funcion`**: declarar
  `inner` via `funcion inner(): ...` no se registra como local, así
  que no se warnea si no se referencia. Inconsistente con
  `inner = lambda: ...` que sí se warnearía. Decisión conservadora
  para evitar falsos positivos con métodos.

### Implementación

Nuevo concepto en `src/linter.{c,h}`: stack de `ScopeFunc` con
declaraciones (`DeclLocal[]`). Cada función abierta empuja un scope;
los parámetros se registran como `DECL_PARAM`, las asignaciones a
nombres simples como `DECL_VAR`. Una referencia (`EXPR_IDENT`) sube
por la cadena de padres marcando el primer match no-`es_extern`.

Tamaño total del módulo `linter`: ~550 líneas (vs ~400 antes).

### Verificación

- **205/205 tests verde**. `test_linter` ahora tiene **36 asserts**
  (vs 22 antes): casos de scope, closures, `nolocal`/`global`,
  destructuring, lambdas, skip rules.
- **63 ejemplos + 12 módulos de stdlib**: 0 falsos positivos. Tras
  arreglar el bug de `es_extern` (closures que escriben a outer via
  `nolocal` debían marcar la outer como usada), el barrido completo
  del repo no genera ningún `unused-local` o `unused-param` espurios.

### Limitaciones reconocidas

- Análisis solo a nivel de función. Variables top-level y de clase
  no se analizan.
- Lambda con `*args` / `**kwargs` no warnea — consistente con
  funciones.

## [1.49.0] — 2026-05-16 — Linter integrado `cornamusa lint`

Tercera release de la **Fase 5 — Tooling**. Cornamusa ahora detecta
automáticamente errores comunes de estilo y bugs latentes a través de
un linter integrado al binario.

### Lo nuevo

- **Subcomando `cornamusa lint archivo.cor`**:
  - Parsea el archivo (si hay errores de sintaxis, el linter no
    ejecuta — el parser ya los reporta).
  - Recorre el AST aplicando 4 checks ortogonales.
  - Imprime avisos al stdout con formato
    `archivo:linea:col: warning [tipo]: mensaje`.
  - Exit 0 si no hay avisos, 1 si los hay — apto para `pre-commit`
    y CI.

### Checks aplicados

- **`unreachable`**: detecta sentencias tras un terminator de flujo
  (`retornar`, `romper`, `continuar`, `lanzar`) en el mismo bloque.
  Solo se reporta la primera tras el terminator — las posteriores se
  asumen consecuencia del mismo bug.

- **`redundant-pasar`**: `pasar` dentro de un bloque con otras
  sentencias. `pasar` solo tiene sentido como marcador en cuerpos
  vacíos (`clase X: pasar fin clase`).

- **`eq-nulo`**: comparación con `nulo` usando `==` o `!=`. Sugiere
  el operador idiomático `es nulo` / `no es nulo`. Detecta tanto
  `x == nulo` como `nulo == x`.

- **`unused-import`**: módulo importado pero nunca referenciado en el
  programa. Maneja las tres formas:
  - `importar mat` → registra `mat`.
  - `importar mat como m` → registra `m`.
  - `desde mat importar PI, factorial` → registra `PI` y `factorial`
    por separado.
  - `desde mat importar *` → no se warnea (no se sabe qué se trajo).

### Lo que NO chequea (queda para v1.50+)

- **Variables locales no usadas**: requiere análisis de scope completo
  (función, comprehension, clausula `cuando` con bind, etc.) — trabajo
  más grande que el resto.
- **Sombras entre scopes**.
- **Aridades incorrectas en llamadas**: el runtime ya las detecta con
  buen mensaje, y el linter no tiene la info de aridad de built-ins.
- **Tipos**: Cornamusa es dinámico; tipado opcional es trabajo lejano.

### Implementación

Nuevo módulo `src/linter.{c,h}` (~400 líneas). Una sola pasada
recursiva por el AST emite los 4 tipos de aviso. La detección de
imports no usados usa una mini-tabla (`MAX_IMPORTS=256`) que registra
nombres importados durante la travesía y los marca cuando aparece un
`EXPR_IDENT` con el mismo nombre. Tras la travesía, los no marcados
se reportan.

Los avisos se ordenan por (línea, columna) para output determinista.

### Verificación

- **205/205 tests verde**, incluyendo nuevo `test_linter` con 22
  asserts cubriendo: cada categoría aisladamente, combinaciones,
  código limpio sin falsos positivos, las tres formas de import.
- **63 ejemplos + 12 módulos de stdlib**: solo 3 ejemplos
  (`32_json_archivos`, `35_iteradores`, `58_repr_booleano`) y
  1 módulo (`stdlib/regex`) generan avisos — todos son
  `eq-nulo` legítimos en código real (deuda pequeña aparte que
  podría limpiarse en una release minor).

### Limitaciones reconocidas

- **No analiza scope**: un import "no usado" se mide a nivel
  programa, no por scope. Si un import se usa solo dentro de una
  función, sigue contando como usado — correcto.
- **No detecta `pasar` redundante dentro de `coincidir`**: cada
  rama de `cuando` es su propio bloque; si una tiene solo `pasar`
  es válida y no warneada.
- **`unused-import` no atrapa shadowing**: si después de
  `importar mat` alguien hace `mat = 1`, el ident `mat` cuenta
  como uso. Aceptable para v1.

## [1.48.0] — 2026-05-16 — Formateador integrado `cornamusa fmt`

Segundo release de la **Fase 5 — Tooling**. Cornamusa ahora trae un
formateador integrado al binario, con reglas conservadoras pensadas
para no romper código existente y ser ejecutables en pipelines de CI.

### Lo nuevo

- **Subcomando `cornamusa fmt`** con modos:
  - `cornamusa fmt archivo.cor` — reescribe el archivo si difiere.
  - `cornamusa fmt --check archivo.cor` — exit 0 si ya está formateado,
    1 si no. Pensado para `pre-commit` y CI.
  - `cornamusa fmt --stdout archivo.cor` — imprime resultado a stdout
    sin tocar el archivo.
  - `cornamusa fmt -` — lee stdin → escribe stdout.

### Reglas aplicadas

- **Reindentación a 4 espacios** derivada mecánicamente de la
  profundidad de bloques. Bloques se abren con línea que termina en
  `:` y se cierran con `fin <etiqueta>`. Mid-block markers (`sino`,
  `atrapar`, `finalmente`) se dedentan al nivel del abridor.
- **`cuando` consecutivo dentro de `coincidir`** se trata correctamente
  via mini-pila: el segundo `cuando` cierra el case anterior antes de
  abrir el nuevo. `fin coincidir` auto-cierra el `cuando` colgante.
- **Trailing whitespace** eliminado en todas las líneas.
- **Líneas en blanco** en runs de ≥2 se colapsan a 1.
- **Trailing newline** normalizado: exactamente uno si el archivo no
  está vacío.
- **Líneas de continuación** (dentro de `()`, `[]`, `{}` o
  triple-quoted strings) **preservan el leading whitespace original**
  — el formateador no realinea contenido entre brackets, solo trimea
  trailing.
- **Comentarios `# ...` preservados** verbatim (incluyendo comentarios
  alineados en mitad de línea de código).

### Lo que NO hace (scope para v1.49+)

- No toca espaciado de operadores (`x+1` no se convierte a `x + 1`).
- No rompe líneas largas.
- No reordena imports.
- No añade ni elimina paréntesis ni `pasar` implícitos.

Decisión deliberada: este release prioriza **conservadurismo e
idempotencia garantizada** sobre cobertura. Un pulido más agresivo del
estilo es trabajo de releases posteriores.

### Implementación

Nuevo módulo `src/formateador.{c,h}` (~250 líneas, sin dependencias
del lexer/parser — caminata textual con tracking de cadenas y
brackets). El estado por archivo es:
- Profundidad de bloque (entero).
- Profundidad de paréntesis/corchetes/llaves (entero).
- `en_triple` + delimitador (para que triple-quoted strings con `\n`
  internos se traten como continuación).
- Pila ligera de tipos de bloque, usada solo para resolver `cuando`
  consecutivo y `fin <X>` que auto-cierra cases colgantes.

### Verificación

- **Suite completa: 204/204 tests verde**, incluyendo nuevo
  `test_formateador` con 32 asserts: idempotencia, mid-block dedent
  (`sino`/`atrapar`/`finalmente`/`cuando`), continuaciones,
  comentarios, strings con `#`, archivo vacío, flag `cambiada`.
- **63 ejemplos + 12 módulos de stdlib**: el formateador no rompe
  ninguno que parseaba antes (verificado tras `--check`). 2 ejemplos
  (`06_diccionarios.cor`, `11_iterador.cor`) ya tenían errores de
  sintaxis preexistentes (destructuring en `para` no soportado) que
  el formateador no introduce — son arreglo aparte.
- **Idempotencia**: `fmt(fmt(x)) == fmt(x)` verificada sobre todos
  los archivos del repo.

### Limitaciones reconocidas

- **No alinea continuaciones**: si la indentación dentro de un bracket
  multilínea es inconsistente, el formateador la deja como está.
- **No detecta one-liners malformados**: `funcion f(): retornar 1`
  (sin `fin funcion`) ya no se separa en líneas. Es un parse error
  aguas abajo de todos modos.
- **No introduce 2 líneas en blanco entre funciones top-level** (estilo
  PEP-8). El proyecto prefiere 1 línea consistentemente.

## [1.47.0] — 2026-05-16 — REPL con line editing e historial (abre Fase 5: tooling)

Primer release de la **Fase 5 — Tooling**. La prioridad nº1 declarada
de David (tooling) finalmente arranca. El REPL pasa de `fgets` plano
a un editor de línea interactivo con historial persistente — el QoL
más visible del día a día.

### Lo nuevo

- **Edición de línea**: cursores ←/→, Home/End (también Ctrl-A/E),
  Backspace, Delete. Insertar/borrar a mitad de línea funciona.
- **Historial navegable** con ↑/↓. La línea-en-progreso se preserva
  cuando navegas hacia atrás y vuelves al final.
- **Persistencia entre sesiones**: el historial se guarda en
  `~/.cornamusa_historial` (`%USERPROFILE%\.cornamusa_historial` en
  Windows) al salir, y se carga al arrancar. Tope: 1000 entradas más
  recientes.
- **Ctrl-C** cancela la línea actual sin salir del REPL.
- **Ctrl-D** (POSIX) / **Ctrl-Z** (Windows) con buffer vacío termina
  el REPL; con buffer no vacío borra a la derecha del cursor.

### Implementación

Nuevo módulo `src/repl_line.{c,h}` con detección de plataforma:
- **POSIX**: `termios` raw mode + parseo de secuencias escape ANSI
  (`\x1b[A` para ↑, etc.).
- **Windows**: `_getch()` para teclas char-por-char +
  `SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING)` para que ANSI
  funcione en el output. Codepage temporal a UTF-8 para que tildes y
  ñ se rendericen correctamente.

Sin dependencias externas (no se vendoró ni linenoise ni readline) —
solo 400 líneas de C cross-platform. El `main.c` quedó casi idéntico:
sustituye el `fgets`+`stdin` por una llamada a `repl_leer_linea` y
maneja el resultado como antes.

**Fallback automático**: si `stdin` no es un TTY (script piped,
archivo, CI), `repl_leer_linea` cae a `fgets` y omite el line editor.
Las suites de tests por pipe siguen funcionando sin cambios.

### Tests

- **203/203 tests verde** (sin cambios — el line editor requiere TTY
  para probarse interactivamente, y el path no-TTY ya estaba cubierto
  por las suites existentes).
- Smoke test manual: aritmética simple, bloques multilínea, history
  con ↑↓, edición a mitad de línea — todo OK en consola de Windows 11.

### Limitaciones aceptadas

- **Sin tab completion**: requeriría introspección de scope. Puede
  llegar en v1.47.1 si surge demanda.
- **Sin reverse search (Ctrl-R)**: power user nice-to-have, aplazado.
- **Sin syntax highlighting** en el line editor.
- **Edición en una sola línea**: si el comando es más largo que el
  ancho del terminal, el repintado puede saltar líneas — el line
  editor no rastrea wrap. En la práctica los comandos del REPL son
  cortos; las funciones multi-línea van por el modo continuación
  (prompt `... `) donde cada línea se edita independientemente.

## [1.46.0] — 2026-05-16 — Multi-recurso `con` + combinar `*args`/`**kwargs` (cierre Fase 4)

Último release de la Fase 4 (sintaxis idiomática post-modelo de
datos). Cierra dos limitaciones documentadas explícitamente en el
intérprete.

### Multi-recurso `con A, B:`

Varios context managers separados por coma se anidan automáticamente.
Entran en orden A→B→C; salen en orden inverso C→B→A (LIFO), incluso
si el cuerpo lanza.

```cornamusa
con abrir_conexion("db") como conn, abrir_archivo("log") como log:
    conn.consultar("...")
    log.escribir("ok")
fin con
```

Equivale exactamente a `con A: con B: con C: ... fin con fin con fin con`
anidados — el parser desazucara así. No hay opcode nuevo; el bytecode
es el mismo que tres `con` consecutivos. Cada nivel tiene su propio
nombre interno único (`__cm_<linea>_<col>_<sufijo>`).

Hasta 16 recursos por bloque (límite arbitrario; documentado).

### Combinar `*args` y `**kwargs` en la misma llamada

Antes el compilador rechazaba explícitamente `f(*args, **kw)` con
"no se puede combinar `*args` con keyword args / **dict en la misma
llamada". v1.46 lo levanta. Habilita el **patrón clásico del wrapper
genérico**:

```cornamusa
funcion log_y_llamar(f, *args, **kw):
    imprimir("llamando con", args, kw)
    retornar f(*args, **kw)
fin funcion
```

Las cuatro formas (posicional simple, `*spread`, kwarg explícito,
`**dspread`) se mezclan libremente:

```cornamusa
destino(*[1], 2, c=3, **{"d": 4})    # válido
```

**Implementación**: nuevo opcode `OP_LLAMAR_SPREAD_KW_DICT`. El
compilador, cuando detecta la mezcla, construye:
1. Una **lista** runtime con todos los posicionales (incluido `*spread`),
   igual que `OP_LLAMAR_SPREAD`.
2. Un **dict** runtime con todos los kwargs (incluido `**dspread`),
   igual que `OP_LLAMAR_KW_DICT`.
3. Emite `OP_LLAMAR_SPREAD_KW_DICT` que toma `[callee, lista, dict]`,
   expande la lista a `n_pos` posicionales (dinámico en runtime),
   convierte el dict en pares (clave, valor), y delega a
   `ejecutar_llamar_kw` — el mismo helper que usan OP_LLAMAR_KW y
   OP_LLAMAR_KW_DICT.

Sin opcodes auxiliares ni nuevo path en `ejecutar_llamar_kw`: solo
una manera distinta de ALIMENTAR ese helper.

### Tests

- **203/203 tests verde** (9 nuevos en `test_bytecode_con_multi_spread`
  + `lex_63` + `bc_run_63_con_multi_spread`).
- Ejemplo `examples/63_con_multi_spread.cor`: tres recursos, liberación
  ante excepción, wrapper genérico, configuración compuesta vía dos
  `**dict` mergeados.

### Estado

**Fase 4 completa.** El plan original cubría:
- v1.41 `__repr__` + `__booleano__`
- v1.42 `__hash__` + `__igual__`
- v1.43 `__siguiente__`
- v1.44 ternaria + slicing assignment
- v1.45 f-string format specifiers
- v1.46 multi-recurso `con` + combinar spread

Todos cumplidos. El modelo de datos y la sintaxis idiomática quedan
al nivel de Python 3.10+ (módulo de las features explícitamente
aplazadas a v2.x: async/await, threading).

## [1.45.0] — 2026-05-16 — F-string format specifiers

Segundo release de sintaxis idiomática (post-modelo de datos). Cierra
el gap más visible del uso diario de f-cadenas.

### Sintaxis

Tras `:` dentro de `{...}`, un especificador de formato estilo Python:

```
{expr:[relleno][alineación][ancho][.precisión][tipo]}
```

- **alineación**: `<` (izquierda), `>` (derecha), `^` (centrado).
- **relleno**: cualquier carácter, requiere alineación explícita.
- **ancho**: dígitos; rellena hasta ese ancho si el cuerpo es más corto.
- **.precisión**: dígitos. Decimales para `f`/`e`; truncado para `s`.
- **tipo**: `d` entero, `f` decimal, `e` científica, `x`/`X` hex
  (minúsculas/mayúsculas), `b` binario, `s` cadena explícita.

Defaults Python-compatibles: alineación implícita es `>` (derecha) para
tipos numéricos y `<` (izquierda) para cadenas. Prefijo `0` antes del
ancho implica zero-padding alineado a derecha.

```cornamusa
f"{42:5d}"            # "   42"
f"{42:05d}"           # "00042"
f"{3.14159:.2f}"      # "3.14"
f"{3.14159:>10.2f}"   # "      3.14"
f"{1234.5:.2e}"       # "1.23e+03"
f"{255:08X}"          # "000000FF"
f"{5:b}"              # "101"
f"{'hi':-^10}"        # "----hi----"
```

Habilita tablas alineadas con solo f-strings — el caso que justifica
esta release. El ejemplo `examples/62_fstring_formato.cor` muestra una
tabla de productos con cantidad/precio/total alineados.

### Implementación

- **Parser**: la búsqueda del `:` que separa la expresión del spec
  rastrea profundidad de `()`/`[]`/`{}` y cadenas internas. El `:` de
  slicing (`xs[1:3]`) o de un dict literal (`{1: 2}`) NO se confunde
  con el inicio de un spec — solo cuenta cuando aparece en el nivel
  superior de la interpolación.
- **AST**: `ParteFCadena` añade campos `spec`/`spec_longitud`.
- **Compilador**: nuevo opcode `OP_FORMATO_F_SPEC [u8 spec_idx]` que
  recibe el spec como constante cadena. Cuando una interpolación tiene
  spec, se emite este opcode en lugar del par habitual
  `OP_FORMATO_F + OP_ASEGURAR_CADENA`.
- **Runtime**: `valor_formatear_con_spec(valor, spec)` en `valor.c`
  parsea el spec y formatea. Tipos numéricos usan `mp_radix_size` /
  `mp_to_radix` para bignum o `snprintf` para decimales. Padding,
  alineación y zero-fill aplicados al final.

### Tests

- **200/200 tests verde** (16 casos nuevos en `test_bytecode_fstring_spec`
  + `lex_62` + `bc_run_62_fstring_formato`).
- Ejemplo `examples/62_fstring_formato.cor` con tabla alineada de
  productos, bignum como `d`, slicing y dict no confundidos con spec,
  errores atrapables.

### Limitaciones aceptadas

- El spec **no** invoca `__cadena__`. Es decir, `f"{obj:<10}"` no
  llama al dunder — usa stringificación canónica. Workaround: el
  usuario hace `f"{cadena(obj):<10}"` explícito.
- Sin `__format__` dunder (Python lo tiene). Si surge demanda real,
  v1.x.1 podría añadirlo — pero el caso de uso principal (alineación
  y precisión numérica) ya está cubierto.

## [1.44.0] — 2026-05-16 — Expresión ternaria + slicing assignment

Primer release de **sintaxis idiomática pendiente** (post-modelo de
datos). Dos huecos pequeños que se notan a diario.

### Expresión ternaria `A si C sino B`

Condicional inline al estilo Python `a if c else b`. Vive en una sola
línea, precedencia más baja que cualquier operador, asociativa
derecha.

```cornamusa
signo = "pos" si n > 0 sino ("cero" si n == 0 sino "neg")
absolutos = [v si v >= 0 sino -v para v en vals]   # dentro de comprehensions
imprimir(mas(1 si verdadero sino 99, 2))           # en args de llamada
```

**Implementación**: nuevo nivel `PREC_TERNARIA` por debajo de `PREC_O`
en el Pratt parser. `parsear_expresion` ahora parsea con piso
`PREC_TERNARIA` para incluir la ternaria como infijo de `si`. La
heurística "fin de sentencia" se extiende a `TT_SI`: un `si` que
abre línea distinta NO se consume como infijo — sigue siendo el inicio
de una sentencia `si`. La cabeza de iter en comprehensions
(`[expr para v en iter si guarda]`) parsea iter con `PREC_O`
directamente para que el `si` de la guarda no se mal-parsee como
ternaria sobre iter.

Nuevo `EXPR_TERNARIA` en el AST; el compilador desugara a
`OP_SALTAR_SI_FALSO` + `OP_DESCARTAR` + rama_si + `OP_SALTAR` +
`OP_DESCARTAR` + rama_no — mismo esqueleto que la sentencia `si`.

### Slicing assignment `xs[i:j:k] = nuevo`

Sustituir un rango de una lista con nuevos elementos. Con `paso == 1`
la lista crece o encoge para acomodar el nuevo tamaño. Con `paso != 1`,
los tamaños deben coincidir exactamente.

```cornamusa
xs = [1, 2, 3, 4, 5]
xs[1:4] = [99]              # [1, 99, 5]    (encoge)
xs[1:1] = [10, 20, 30]      # inserta sin borrar
xs[::2] = [10, 30]          # sustitución 1-a-1 con paso=2
xs[2:4] = []                # borrar slice
```

**Implementación**: nuevo opcode `OP_ASIGNAR_REBANADA`. El compilador
detecta `EXPR_REBANADA` como destino de asignación, emite la pila
`[lista, inicio, fin, paso, iterable]` y `OP_ASIGNAR_REBANADA`. El
runtime resuelve `nulo` como default para los tres índices (semántica
Python: `[:]`, `[:n]`, `[n:]`), aplica el clamp silencioso, hace
`memmove` para el desplazamiento si el tamaño cambia, y libera/copia
elementos según haga falta.

Limitaciones:
- Solo listas. Cadenas son inmutables — usar concatenación.
- El iterable de la asignación debe ser lista o tupla. Generadores
  no soportados (habría que materializar; queda como patch si surge
  demanda).

### Tests

- **197/197 tests verde** (16 casos nuevos dentro de
  `test_bytecode_ternaria_slicing` + `lex_61` + `bc_run_61_ternaria_slicing`).
- Ejemplo `examples/61_ternaria_slicing.cor` con ternaria simple,
  anidada (asociativa derecha), en comprehensions, en args de
  llamada; slicing assignment con tamaño igual, crecer, encoger,
  borrar, paso != 1, paso != 1 con error, desde tupla, sobre cadena
  (error).

## [1.43.0] — 2026-05-15 — Iteradores lazy con `__siguiente__`

Tercer release de la **Fase 4 — modelo de datos**. Cierra el protocolo
de iteración: una clase puede ahora ser un iterador stateful sin tener
que materializar sus valores en una lista. Aprovecha directamente la
infraestructura de sub-VM síncrono construida en v1.42.

### Lo nuevo

- **`__siguiente__(yo)` → cualquier valor**. La VM lo invoca en cada
  paso de `para x en obj`. Al agotarse, el dunder debe lanzar
  `ErrorDeIteracion()` (nueva excepción built-in) — el `para` la
  atrapa internamente y termina el bucle.

- **`__iterar__(yo)`** puede ahora devolver una **instancia** con
  `__siguiente__` (no solo un iterable nativo). Patrón Python
  `__iter__(self): return self` soportado: si la instancia define
  ambos dunders, `__siguiente__` tiene prioridad — la instancia
  funciona como su propio iterador.

- **Instancias con solo `__siguiente__`** son iterables directamente
  (sin `__iterar__`): `para v en MiIter():`.

```cornamusa
clase Contador:
    funcion __iniciar__(yo, ini, tope):
        yo.i = ini; yo.tope = tope
    fin funcion
    funcion __iterar__(yo): retornar yo; fin funcion
    funcion __siguiente__(yo):
        si yo.i >= yo.tope:
            lanzar ErrorDeIteracion()
        fin si
        v = yo.i
        yo.i = yo.i + 1
        retornar v
    fin funcion
fin clase

para n en Contador(0, 5):
    imprimir(n)   # 0 1 2 3 4
fin para
```

### Implementación

- **`OP_ITER_INICIAR`**: la instancia con `__siguiente__` tiene
  prioridad y se conserva en el slot del iterador tal cual; sin
  `__siguiente__` se busca `__iterar__` (comportamiento previo).
- **`OP_ITER_SIGUIENTE`**: si el slot es VAL_INSTANCIA, despacha
  `__siguiente__` vía `vm_ejecutar_dunder_sync` (la misma rutina que
  v1.42 usa para `__hash__`/`__igual__`). Si el dunder lanza
  `ErrorDeIteracion`, se detecta por prefijo del mensaje en
  `vm->error.mensaje`, se limpia el error y se salta al fin del bucle.
  Cualquier otra excepción se propaga al `atrapar` del caller.
- **Constructor `ErrorDeIteracion(...)`** añadido a la lista de
  excepciones built-in. Por conveniencia (es una señal sin contexto),
  los constructores de **todas** las excepciones built-in ahora aceptan
  0 argumentos (mensaje vacío) además de los 1-2 anteriores.

### Bug fix tangencial: cleanup del sub-VM en excepción no atrapada

`vm_ejecutar_dunder_sync` no limpiaba la pila ni los frames cuando el
dunder lanzaba una excepción que escapaba del sub-VM (caso normal en
`__siguiente__`). Resultado: corrupción de estado que solo se notaba
tras varias operaciones siguientes — la VM ejecutaba bytecode basura
y reportaba errores fantasma como `'nulo' no es invocable` o
`operando '-' entre 'nulo' y 'cadena'`. Fix: capturar
`n_frames`/`tope` ANTES de los pushes y restaurarlos en el path de
error.

### Tests

- **199/199 tests verde** (7 nuevos en `test_bytecode_iter_lazy.c` +
  `lex_60` + `bc_run_60_iteradores_lazy`).
- Ejemplo `examples/60_iteradores_lazy.cor`: Contador, iterador
  stateful sin `__iterar__`, colección con iterador separado, filtro
  perezoso encadenado, `romper`, `ErrorDeIteracion` manual.

### Estado del data model

Con v1.43 **todos los dunders fundamentales están implementados** —
no quedan dunders reservados sin invocar en `vm.c`. El plan original
de la Fase 4 cubre la coerción (v1.41), el hashing (v1.42) y la
iteración lazy (v1.43). Lo que sigue es sintaxis idiomática
(ternaria, slicing assignment, format specifiers, multi-recurso `con`).

## [1.42.0] — 2026-05-15 — Instancias hashables: `__hash__` + `__igual__`

Segundo release de la **Fase 4 — completar el modelo de datos**. Las
instancias de clase ahora pueden usarse como claves de dict/conjunto
**por valor**, no solo por identidad — el patrón record-clase, esencial
para programación con datos.

### Lo nuevo

- **`__hash__(yo)` → entero**. Invocado por `hash_valor` cuando la
  instancia se usa como clave (`d[obj]`, `obj en s`, `{obj: v}`, …).
  El runtime cachea el resultado por instancia tras el primer despacho:
  llamadas siguientes son O(1) sin re-ejecutar el dunder. Python:
  `__hash__` debe ser estable durante la vida del objeto.

- **`__igual__(yo, otro)`** ya existía para `==`, pero ahora la VM
  también lo despacha desde **`valor_iguales`** — la función interna
  que usan `dicc_buscar_slot` y `conj_buscar_slot` para resolver
  colisiones de hash. Sin este despacho, dos instancias con mismo
  `__hash__` pero objetos distintos quedaban como entradas separadas;
  ahora se mergean correctamente.

```cornamusa
clase Punto:
    funcion __iniciar__(yo, x, y): yo.x = x; yo.y = y; fin funcion
    funcion __hash__(yo): retornar yo.x * 31 + yo.y; fin funcion
    funcion __igual__(yo, otro): retornar yo.x == otro.x y yo.y == otro.y; fin funcion
fin clase

distancias = {Punto(3, 4): 5, Punto(3, 4): 5, Punto(6, 8): 10}
longitud(distancias)         # 2 — Punto(3, 4) deduplica
distancias[Punto(3, 4)]      # 5 — encuentra por valor
Punto(3, 4) en {Punto(3, 4)} # verdadero
```

- Las instancias **sin** `__hash__`/`__igual__` siguen siendo
  hashables — por identidad de puntero, como en Python con un objeto
  sin `__hash__` redefinido. Cambio respecto a versiones previas:
  antes `valor_es_hashable` rechazaba VAL_INSTANCIA; ahora la acepta.

### Implementación

Reto principal: `hash_valor` y `valor_iguales` viven en `valor.c`, que
**no incluye `vm.h`**. Despachar un dunder requiere acceso a la VM
(empujar un frame, correr el dispatch). Solución:

- **Hooks** declarados en `valor.h` (`ValorHashDunderHook` y
  `ValorIgualesDunderHook`) con tri-estado de retorno (`OK` /
  `NO_DUNDER` / `ERROR`). `vm_iniciar` registra implementaciones vía
  `valor_set_hooks`.
- **Sub-VM síncrono**: `vm_ejecutar_dunder_sync` empuja un frame,
  activa `vm->modo_sub_call`, fija `vm->frame_techo`, llama a
  `vm_ejecutar_dispatch_impl` con `inicial=false`. El nuevo código de
  retorno `VM_OK_SUB_RETURN` indica "el frame del dunder retornó;
  valor en TOS del caller". Restaurar estado, leer TOS, devolver.
- **`vm->handler_techo`** limita la búsqueda de `OP_LANZAR` a los
  handlers instalados dentro del sub-VM. Sin esto, una excepción
  dentro del dunder se desenroscaría más allá del sub-call y dejaría
  el C-stack inconsistente. Una bandera one-shot
  (`valor_dunder_hubo_error_y_limpiar`) avisa al OP llamante para
  propagar el error vía `RAISE_OR_DIE`.
- **Cache**: dos campos nuevos en `Instancia` — `uint64_t cache_hash`
  y `bool cache_hash_valido`. El cache se inicializa perezosamente en
  el primer uso (sea con dunder o por identidad).

### Cobertura

- **Operadores**: `d[k]` (OP_INDICE), `d[k] = v` (OP_ASIGNAR_INDICE),
  `k en d/s` (OP_EN), `{k1: v1, …}` (OP_BUILD_DICC), `{k1, …}`
  (OP_BUILD_CONJUNTO), comprehensions (OP_DICC_AGREGAR_PAR,
  OP_CONJUNTO_AGREGAR).
- **Built-ins**: `agregar(s, k)`, `quitar(d, k)`, `quitar(s, k)`,
  `conjunto(iter)` con instancias — el error de un dunder viaja por
  el path `vm->error` que ya conecta natives ↔ atrapar (sin código
  extra; OP_LLAMAR_NATIVA simplemente limpia la bandera one-shot).

### Tests

- **189/189 tests verde** (12 nuevos en `test_bytecode_hashable.c` +
  `lex_59` + `bc_run_59_hashable`).
- Ejemplo `examples/59_hashable.cor`: Punto 2D como clave, deduplicación
  por valor, cache verificado con contador, errores atrapables.

### Limitaciones aceptadas

- Para que la dedupe-por-valor funcione, **ambos** `__hash__` y
  `__igual__` deben estar definidos coherentes (iguales ⇒ mismo hash).
  Con solo `__hash__` y sin `__igual__`, dos instancias con mismo hash
  quedan separadas porque la igualdad cae a identidad de puntero. Es
  responsabilidad del usuario (Python: misma convención).
- `__hash__` que retorna otra instancia con `__hash__` (caso degenerado)
  se corta vía el límite de frames del VM. Recomendación: `__hash__`
  debe ser puro y retornar un entero.

## [1.41.0] — 2026-05-15 — Dunders de coerción: `__repr__` y `__booleano__`

Primer release de la **Fase 4 — completar el modelo de datos**. Dos
dunders que estaban reservados desde hacía versiones ahora se invocan
de verdad. Mismo patrón de despacho que `__cadena__` (v1.2): cuando el
operando es una instancia y la clase define el dunder, la VM empuja
un frame para ejecutarlo; si no, fallback a la semántica por defecto.

### `__repr__(yo)` → cadena

Invocado por el built-in `repr(obj)`. Devuelve la representación
"inspeccionable" del objeto, útil para depurar — puede ser distinta
de `__cadena__` (lo que ve el usuario final).

```cornamusa
clase Punto:
    funcion __iniciar__(yo, a, b):
        yo.a = a
        yo.b = b
    fin funcion
    funcion __cadena__(yo):
        retornar f"({yo.a}, {yo.b})"        # → "(3, 4)"
    fin funcion
    funcion __repr__(yo):
        retornar f"Punto(a={yo.a}, b={yo.b})" # → "Punto(a=3, b=4)"
    fin funcion
fin clase

p = Punto(3, 4)
imprimir(cadena(p))   # (3, 4)
imprimir(repr(p))     # Punto(a=3, b=4)
```

Sin `__repr__`, `repr(obj)` mantiene la representación canónica
anterior (fallback a `valor_a_repr`).

**Implementación**: nuevo `OP_REPR`. El compilador detecta el call
`repr(arg)` con un solo argumento y emite `OP_REPR + OP_ASEGURAR_CADENA`
— mismo atajo que `cadena(arg)` para `__cadena__`. La nativa `repr`
queda como fallback indirecto (vía `f = repr; f(x)` no pasa por el
atajo).

### `__booleano__(yo)` → booleano

Invocado en cualquier contexto que evalúe la **verdadez** de una
instancia: `si obj:`, `mientras obj:`, operandos de `y`/`o`,
`no obj`. Sin él, una instancia es siempre verdadera (mismo
comportamiento que Python para objetos sin `__bool__`).

```cornamusa
clase Bolsa:
    funcion __iniciar__(yo, items):
        yo.items = items
    fin funcion
    funcion __booleano__(yo):
        retornar longitud(yo.items) > 0
    fin funcion
fin clase

cola = Bolsa(["a", "b", "c"])
mientras cola:
    quitar(cola.items, 0)
fin mientras   # termina cuando __booleano__(cola) devuelve falso
```

**Implementación**: dispatch inline en `OP_NO` y `OP_SALTAR_SI_FALSO`.
Si TOS es VAL_INSTANCIA con `__booleano__` definido, se rebobina IP al
opcode actual y se empuja el frame del dunder; cuando éste retorna,
re-ejecutamos el opcode con el valor de retorno en TOS. No requiere
nuevos opcodes ni inflar el bytecode con coerciones antes de cada
conditional branch.

**Contrato**: `__booleano__` debe retornar un booleano. Si retorna
otra instancia con `__booleano__` se entra en recursión y el límite
de frames la cortará — análogo a cualquier dunder mal definido.

### Otros cambios menores

- Mensaje de `OP_ASEGURAR_CADENA` generalizado: antes era
  "`__cadena__` debe retornar cadena", ahora es "se esperaba cadena,
  no '<tipo>'". Aplica también al validador de `__repr__`. Un test de
  `test_bytecode_dunders.c` se actualizó al nuevo mensaje.

### Tests

- **188/188 tests verde** (185 anteriores + nuevo
  `test_bytecode_dunders_coercion` con 12 tests + `lex_58_repr_booleano`
  + `bc_run_58_repr_booleano`).
- Nuevo ejemplo `examples/58_repr_booleano.cor` ejercitando ambos
  dunders, fallbacks y errores atrapables.

### Limitaciones aceptadas

- `__hash__` sigue **reservado, sin invocar**. Las instancias se
  hashean por identidad (lo que ya hacían). Sería v1.42.
- `booleano(obj)` (built-in) sigue **sin invocar** `__booleano__` — usa
  identidad por defecto. La inconsistencia con `si obj:` se documenta;
  el atajo de compilador equivalente al de `repr` queda como patch
  para .x.1 si surge demanda. Mientras tanto: `si obj: ... sino:` da
  el comportamiento correcto.

## [1.40.0] — 2026-05-14 — Performance: `-O3` + LTO

Primer release de la línea de **performance**. Sube la build Release
a `-O3` y activa **LTO** (Link-Time Optimization). Al hacerlo se
destapó —y se arregló— un bug de UB latente desde v1.4.

### Cambios de build

- `-O2` → **`-O3`** explícito para builds no-Debug (GCC/Clang) y
  `/O2` para MSVC.
- **LTO** vía `CMAKE_INTERPROCEDURAL_OPTIMIZATION` — la API oficial
  de CMake, que propaga correctamente a las librerías estáticas
  vendoradas (`tommath`, `utf8proc`) y selecciona los `gcc-ar`/
  `gcc-ranlib` adecuados. Activable/desactivable con
  `-DCORNAMUSA_LTO=ON|OFF` (ON por defecto).

LTO inlinea los helpers hot del intérprete —`empujar`, `sacar`,
`valor_clonar`, `dicc_obtener`, todos en `valor.c`— dentro del
dispatch loop de `vm.c`. Sin LTO quedaban como llamadas a través de
unidades de traducción.

### Resultados (mejor de 3 corridas, GCC 13.2, Windows)

| Benchmark              | v1.39 | v1.40 | Δ      |
|------------------------|-------|-------|--------|
| `dicc_intensivo`       | 0.069 | 0.053 | −23%   |
| `fibonacci_recursivo`  | 0.263 | 0.222 | −16%   |
| `oo_dunder_aritmetico` | 0.112 | 0.091 | −19%   |
| `oo_dunder_indice`     | 0.084 | 0.054 | −36%   |
| `oo_intensivo`         | 0.038 | 0.032 | −16%   |
| `globales_lookup`      | 0.221 | 0.212 | −4%    |
| `bignum_factorial`     | 0.027 | 0.028 | ~0%    |

`bignum_factorial` no mejora — está dominado por libtommath, no por
el dispatch. Los benchmarks OO/dicc, que hacen muchas llamadas a
helpers pequeños cross-file, son los que más se benefician.

Detalle en [benchmarks/RESULTS.md](benchmarks/RESULTS.md). Los
runners `benchmarks/run.sh` y `run.ps1` se actualizaron para apuntar
a `./build/` (antes `./build_v2/`).

### Bug fix: UB latente en `nolocal` (desde v1.4)

Activar LTO+O3 destapó un **segfault en closures con `nolocal`**
(`bc_run_30_closures_nolocal`). UBSan lo identificó: el campo
`n_nolocales` de `ScopeCompilador` **nunca se inicializaba** en
`scope_iniciar` ([src/compilador.c](src/compilador.c)).

Con `-O0`/`-O2` el campo solía caer en 0 por suerte del layout de
stack. Con `-O3`+LTO arrancaba en basura: el contador disparaba un
falso "demasiadas declaraciones nolocal", o corrompía el índice del
array `nolocales[]` → segfault.

El bug llevaba latente desde que se añadió `nolocal` en v1.4 — solo
no se manifestaba porque ningún build era lo bastante agresivo. Fix:
`s->n_nolocales = 0;` en `scope_iniciar`. Cubierto por
`bc_run_30_closures_nolocal` con LTO activo.

### Tests

- **185/185 tests verde** con `-O3`+LTO.
- Sin tests nuevos: el bug del `nolocal` ya estaba cubierto por el
  ejemplo `30_closures_nolocal.cor` / `bc_run_30_closures_nolocal` —
  solo que pasaba "por suerte" hasta que LTO lo destapó. Ahora pasa
  de verdad.

## [1.39.0] — 2026-05-14 — Flag `--check` (validar sin ejecutar)

`cornamusa --check archivo.cor` valida **sintaxis y compilación** sin
ejecutar el programa. Pensado para integración continua, hooks de
pre-commit y editores que quieran validar al guardar.

### Uso

```sh
$ cornamusa --check script.cor
script.cor: OK (42 sentencias, sin errores de sintaxis ni compilacion)
$ echo $?
0

$ cornamusa --check con_error.cor
ErrorDeSintaxis en con_error.cor:2:5
    retornar 1
    ^^^^^^^^
se esperaba un nombre de parámetro

con_error.cor: fallo de sintaxis.
$ echo $?
65
```

### Pipeline

`lex → parse → compilar`. Se detiene tras la compilación — **nunca
ejecuta la VM**. Detecta:

- Errores de sintaxis (parser): paréntesis sin cerrar, tokens
  inesperados, bloques mal anidados...
- Errores de compilación: `'producir' fuera de funcion`, `'**kw' debe
  ser el ultimo parametro`, demasiadas constantes...

Lo que NO detecta (son errores de runtime): nombres indefinidos,
errores de tipo, índices fuera de rango. Para eso hay que ejecutar.

### Exit codes

- `0` — sin errores de sintaxis ni compilación.
- `65` — error de sintaxis o compilación (`EX_DATAERR`).
- `74` — no se pudo leer el archivo (`EX_IOERR`).

`--validar` es alias de `--check`.

### Tests añadidos

- `tests/fixtures/check_sintaxis_mala.cor` — fixture con error de
  sintaxis deliberado.
- `check_ejemplo_valido` — `--check` sobre un ejemplo válido pasa
  (exit 0, salida contiene "OK").
- `check_sintaxis_mala` — `--check` sobre el fixture falla
  (`WILL_FAIL TRUE`).
- **185/185 tests verde**.

## [1.38.0] — 2026-05-14 — Traceback multi-frame

Cuando un error de runtime fatal ocurre dentro de funciones
anidadas, Cornamusa ahora imprime la **cadena de llamadas** completa
— igual que el "Traceback" de Python.

### Ejemplo

```cornamusa
funcion nivel_c(x):
    retornar x + indefinido    # ← error aquí
fin funcion
funcion nivel_b(x):
    retornar nivel_c(x * 2)
fin funcion
funcion nivel_a():
    retornar nivel_b(10)
fin funcion
nivel_a()
```

Salida:

```
ErrorDeNombre en script.cor:2:0
    retornar x + indefinido
    ^
nombre 'indefinido' no esta definido
traza (mas reciente al final):
  linea 13, en <programa>
  linea 10, en nivel_a
  linea 6, en nivel_b
  linea 2, en nivel_c
```

Cada línea de la traza muestra **dónde estaba la ejecución** en ese
frame: en el frame que falló, la línea del error; en los padres, la
línea de la llamada que descendió.

### Implementación

`VM` gana un buffer `char traceback[1024]`. Cuando `vm_ejecutar`
recibe `VM_ERROR_RUNTIME` del dispatch, llama `vm_capturar_traceback`
— los `CallFrame` siguen en pila (un `return VM_ERROR_RUNTIME` no los
desenrolla), así que itera `vm->frames[0..n_frames-1]` y formatea
una línea por frame con `linea_actual_frame` + el nombre del closure.

`main.c` imprime el traceback tras el mensaje de error si no está
vacío.

### Cuándo NO hay traceback

- **Error en top-level** (un solo frame): la línea del mensaje basta,
  el traceback sería redundante.
- **Error atrapado** con `intentar/atrapar`: no es fatal, nunca llega
  a `vm_ejecutar` como `VM_ERROR_RUNTIME`.

### Tests añadidos

- `tests/unit/test_bytecode_traceback.c` — 4 tests: traza de tres
  niveles (verifica cada frame presente), traza de un nivel
  (encabezado + función + `<programa>`), error de top-level sin
  traceback, error atrapado sin traceback.
- **183/183 tests verde**.

## [1.37.0] — 2026-05-14 — Errores de runtime con contexto de fuente

Los errores en tiempo de ejecución ahora muestran la **línea de
código fuente** con un caret `^`, igual que los errores de sintaxis
del parser. Antes solo se imprimía la cabecera y el mensaje.

### Antes

```
ErrorDeNombre en script.cor:3:0
nombre 'c' no esta definido
```

### Ahora

```
ErrorDeNombre en script.cor:3:0
    b = c + a
    ^
nombre 'c' no esta definido
```

Funciona para todos los errores de runtime: `ErrorDeNombre`,
`ErrorDeIndice`, `ErrorAritmetico`, `ErrorDeTipo`, `ErrorDeAtributo`...

### Causa

`error_imprimir` solo dibujaba el contexto cuando
`columna_inicio > 0`. Los errores de runtime de la VM rastrean la
**línea** pero no la columna exacta (`columna_inicio == 0`), así que
el snippet nunca aparecía para ellos.

### Fix

[src/errores.c](src/errores.c): la condición pasa de
`linea > 0 && columna_inicio > 0` a solo `linea > 0`. Cuando
`columna_inicio == 0`, el caret apunta al **primer carácter
no-blanco** de la línea — una aproximación visual razonable que da
contexto sin pretender una precisión que la VM no tiene.

Los errores de parser (que sí tienen columna precisa) no cambian:
siguen mostrando el caret en la columna exacta con el span completo.

### Tests añadidos

- `tests/unit/test_errores_contexto.c` — 4 tests que capturan la
  salida de `error_imprimir` vía `tmpfile`: caret con columna
  precisa, caret sin columna (col 0 → primer no-blanco), caret tras
  indentación, sin fuente (solo cabecera + mensaje, sin caret).
- **182/182 tests verde**.

## [1.36.0] — 2026-05-14 — Sugerencias en atributos

Extiende las sugerencias "¿quisiste decir...?" de v1.35 a los
`ErrorDeAtributo`: acceso a atributo de instancia, método de clase, o
símbolo de módulo inexistente.

### Ejemplos

```cornamusa
clase Punto:
    funcion __iniciar__(yo):
        yo.coord_x = 1
        yo.coord_y = 2
    fin funcion
fin clase
p = Punto()
imprimir(p.coord_z)
# ErrorDeAtributo: instancia de 'Punto' no tiene atributo 'coord_z'
#   (¿quisiste decir 'coord_x'?)

# Métodos también:
c.abrr()
# ErrorDeAtributo: instancia de 'Caja' no tiene atributo 'abrr'
#   (¿quisiste decir 'abrir'?)

# Símbolos de módulo:
importar azar
azar.decimial()
# ErrorDeAtributo: el modulo 'azar' no tiene atributo 'decimial'
#   (¿quisiste decir 'decimal'?)
```

### Implementación

El helper de v1.35 se refactorizó:

- `escanear_dicc_cercano(d, ...)` — escanea un diccionario
  actualizando el mejor candidato in-place. Compartido por todas las
  variantes de sugerencia.
- `sugerir_atributo_cercano(d1, d2, ...)` — busca en **hasta dos**
  diccionarios. Para instancias usa `instancia->atributos` +
  `clase->metodos` (un typo puede acercarse a un atributo o a un
  método). Para módulos usa `modulo->atributos` con `d2 = NULL`.

Mismo umbral adaptativo (2 para nombres ≥ 4 chars, 1 para más
cortos) y mismo filtro de nombres internos (`$...`).

Aplicado en los dos sitios de `ErrorDeAtributo` en
[src/vm.c](src/vm.c): `OP_OBTENER_ATRIBUTO` para módulo e instancia.

### `ErrorDeNombre` ahora atrapable

Como parte de este release, `ErrorDeNombre` (nombre global
inexistente) pasó de `return VM_ERROR_RUNTIME` directo a
`RAISE_OR_DIE()` — ahora es **atrapable con `intentar/atrapar`**
como cualquier otro error, coherente con la política de v1.10.

```cornamusa
intentar:
    longutud([1, 2, 3])
atrapar ErrorDeNombre como e:
    imprimir(cadena(e))
    # ErrorDeNombre: nombre 'longutud' no esta definido
    #   (¿quisiste decir 'longitud'?)
fin intentar
```

### Tests añadidos

- `tests/unit/test_bytecode_sugerencias.c` — 3 tests nuevos: typo en
  atributo de instancia, typo en método, atributo sin candidato
  cercano. (El caso de símbolo de módulo se cubre en el ejemplo —
  los unit tests no tienen `stdlib/` en su working directory.)
- `examples/57_sugerencias.cor` + `bc_run_57_sugerencias` — demuestra
  los 6 escenarios atrapando los errores.
- **181/181 tests verde**.

## [1.35.0] — 2026-05-14 — Mensajes de error con sugerencias

Cuando un nombre global no existe, la VM busca el más parecido entre
los globales definidos y lo sugiere — el clásico "¿quisiste decir...?".
Primer paso de la línea de **tooling/usabilidad**.

### Antes

```
ErrorDeNombre: nombre 'longutud' no esta definido
```

### Ahora

```
ErrorDeNombre: nombre 'longutud' no esta definido (¿quisiste decir 'longitud'?)
```

Funciona tanto para built-ins (`longitud`, `imprimir`, `rango`...)
como para variables y funciones del usuario:

```cornamusa
mi_contador = 10
imprimir(mi_contadr)
# ErrorDeNombre: nombre 'mi_contadr' no esta definido
#   (¿quisiste decir 'mi_contador'?)
```

### Implementación

[src/vm.c](src/vm.c) añade dos helpers:

- `distancia_levenshtein(a, b)` — distancia de edición clásica con
  DP de dos filas. Tope de longitud 64 (nombres más largos no se
  sugieren).
- `sugerir_nombre_cercano(dicc, objetivo, ...)` — itera el diccionario
  de globales buscando la clave con menor distancia. **Umbral
  adaptativo**: 2 para nombres ≥ 4 caracteres, 1 para más cortos
  (evita sugerencias absurdas con nombres de 1-2 letras). Salta
  nombres internos (`$iter`, `$comp_acc`, ...).

Se invoca en los dos sitios donde se produce `ErrorDeNombre`:
`OP_OBTENER_GLOBAL` y `OP_ASIGNAR_GLOBAL`. Si no hay candidato dentro
del umbral, el mensaje queda como antes (sin sugerencia espuria).

### Limitaciones

- Solo para **globales** (built-ins, funciones y variables
  top-level). Los locales se resuelven en compile-time y rara vez
  generan este error en runtime.
- Una sola sugerencia (la más cercana), no una lista.

### Tests añadidos

- `tests/unit/test_bytecode_sugerencias.c` — 7 tests: typo en
  built-in, typo en `imprimir`, typo en variable de usuario, typo en
  nombre de función, sin sugerencia cuando nada se parece, error base
  presente igual, no sugiere nombres internos (`$...`).
- **179/179 tests verde**.

## [1.34.0] — 2026-05-14 — Generator expressions inline

`(expr para v en iter [si guarda])` — el equivalente lazy de una
list comprehension. En lugar de materializar una lista, produce un
**generador** que se evalúa elemento a elemento.

### Sintaxis

```cornamusa
cuadrados = (n * n para n en rango(1, 7))
imprimir(tipo(cuadrados))   # generador

# Pasada directamente a un `para` — sin lista intermedia
para v en (n para n en rango(20) si n % 2 == 0):
    imprimir(v)
fin para

# Como argumento a otra función
tomar((x * 3 para x en rango(100)), 5)   # [0, 3, 6, 9, 12]
```

### Captura de variables externas

La genex captura las variables del scope que la rodea como
**upvalues** — igual que un closure:

```cornamusa
funcion escalador(factor):
    retornar (x * factor para x en [1, 2, 3, 4])
fin funcion

g = escalador(10)
# g produce: 10, 20, 30, 40 — `factor` capturado del scope de escalador
```

### Implementación

`EXPR_COMPREHENSION` gana un `tipo_destino = 3` (genex). El parser lo
construye cuando ve `para` tras la primera expresión dentro de `(...)`.

El compilador desugar la genex a una **FuncionBC sintética
generadora** de un parámetro:

```
funcion $genex($gx_param):
    para v en $gx_param:
        si guarda:
            producir expr
        fin si
    fin para
fin funcion
```

- Se compila en un scope hijo → `expr`/`guarda` resuelven variables
  externas como upvalues automáticamente.
- `fn->es_generador = true` → llamarla devuelve un `VAL_GENERADOR`.
- En el scope padre: `OP_CLOSURE` + upvalues, luego se compila el
  iterable real y `OP_LLAMAR 1`.

Reusa toda la maquinaria de generadores de v1.31 — cada
`iter_siguiente` reanuda el frame suspendido. **Lazy de verdad**:
`(x para x en rango(1000000))` no aloca un millón de elementos.

### Tests añadidos

- `test_bytecode_generadores.c`: 6 tests — genex básica, es
  generador, con guarda, captura upvalue, top-level, inline en `para`.
- `examples/56_generadores.cor`: caso 7 con genex lazy, guarda y
  captura de upvalue vía función `escalador`.
- **178/178 tests verde**.

### Cierre de la familia de iteración perezosa

Con v1.34, Cornamusa tiene el conjunto completo Python-paritario:
comprehensions (v1.30/v1.32), generadores con `producir` (v1.31),
`producir desde` (v1.33) y generator expressions (v1.34).

## [1.33.0] — 2026-05-14 — `producir desde` (delegación de generadores)

`producir desde EXPR` delega a un sub-generador (o cualquier iterable):
todos sus valores se reproducen uno a uno como si fueran del generador
externo. Equivalente al `yield from` de Python.

### Sintaxis

```cornamusa
funcion hojas():
    producir 1
    producir 2
fin funcion

funcion arbol():
    producir 0
    producir desde hojas()      # delega al sub-generador
    producir desde [3, 4, 5]    # también funciona con iterables
    producir 6
fin funcion

para v en arbol():
    imprimir(v)   # 0, 1, 2, 3, 4, 5, 6
fin para
```

Se compone recursivamente — `producir desde` de un generador que a su
vez tiene `producir desde` funciona a cualquier profundidad.

### Implementación

Desugar puro en el parser, cero cambios en VM/compilador:

```
producir desde EXPR
```

se reescribe como

```
para $yf_N en EXPR:
    producir $yf_N
fin para
```

El parser construye el AST del bucle directamente: un `SENT_PARA` con
objetivo `$yf_N` (nombre único por contador para evitar colisión con
variables del usuario), iterable `EXPR`, y cuerpo `producir $yf_N`.
Reusa toda la maquinaria de iteración y generadores ya existente
(`OP_ITER_INICIAR`/`OP_ITER_SIGUIENTE` ya despachan sobre
`VAL_GENERADOR` desde v1.31).

### Tests añadidos

- `test_bytecode_generadores.c`: `test_producir_desde_subgenerador`,
  `test_producir_desde_lista`, `test_producir_desde_anidado` (dos
  niveles de delegación).
- `examples/56_generadores.cor`: caso 6 nuevo demostrando delegación
  combinando sub-generadores e iterables literales.
- **178/178 tests verde**.

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

