# Referencia rápida de Cornamusa

> Cheatsheet de sintaxis + tablas de built-ins, stdlib y errores. Para una explicación pedagógica usa el [tutorial](tutorial.md). Para la especificación formal del lenguaje, [ESPEC.md](https://github.com/David-Castilla-Gomez/Cornamusa/blob/main/ESPEC.md).

**Versión:** 1.46.0

---

## 1. Sintaxis básica

### Variables y comentarios

```cornamusa
# Comentario de una línea
x = 42
nombre = "Ana"
"""
Comentario de varias líneas
(técnicamente una cadena ignorada).
"""
```

### Bloques

Todo bloque abre con `:` y cierra con `fin <etiqueta>`:

```cornamusa
si cond:
    ...
fin si

mientras cond:
    ...
fin mientras

para x en iter:
    ...
fin para

funcion f(args):
    ...
fin funcion

clase C:
    ...
fin clase

intentar:
    ...
atrapar Tipo como e:
    ...
fin intentar

con recurso como r:
    ...
fin con

coincidir valor:
    cuando patron:
        ...
fin coincidir
```

> La indentación es **estilística**, no semántica: lo que delimita los bloques es `:` al abrir y `fin <etiqueta>` al cerrar. La convención es 4 espacios.

One-liner: si tras `:` viene una sola sentencia en la misma línea, no requiere `fin`:

```cornamusa
si x > 0: imprimir(x)
para i en rango(3): imprimir(i)
```

### Expresión ternaria (v1.44)

Condicional inline al estilo Python `a if c else b`:

```cornamusa
signo = "pos" si n > 0 sino ("cero" si n == 0 sino "neg")
```

Precedencia más baja que cualquier operador. Asociativa derecha. Vive en **una sola línea**: un `si` que abre línea es siempre el inicio de una sentencia `si`, no una ternaria.

---

## 2. Operadores

### Precedencia (de menor a mayor)

```
o                    (lógico OR)
y                    (lógico AND)
no                   (lógico NOT)
== != < <= > >=      (comparación)
es  es no            (identidad)
en  no en            (pertenencia)
| ^ &                (bitwise OR / XOR / AND)
<< >>                (desplazamiento)
+ -                  (suma / resta)
* / // %             (mult / div / div entera / módulo)
+ - ~                (unarios)
**                   (potencia, asociativa derecha)
() [] .              (llamada / índice / atributo)
```

### Asignación

| Operador | Equivalente |
|---|---|
| `=` | asignación |
| `+=` `-=` `*=` `/=` `//=` `%=` `**=` | aritméticos compuestos |
| `&=` `\|=` `^=` `<<=` `>>=` | bitwise compuestos |

El lado izquierdo de `=` puede ser un nombre, un índice (`xs[0] = ...`), un atributo (`obj.campo = ...`) o un patrón de **destructuring** (§5).

### Aritmética

```cornamusa
7 + 3       # 10
7 - 3       # 4
7 * 3       # 21
7 / 3       # 2.3333...    ← siempre devuelve decimal
7 // 3      # 2            ← división entera
7 % 3       # 1            ← módulo
2 ** 10     # 1024         ← potencia
-7          # negación
~7          # NOT bit a bit
```

### Comparación e identidad

```cornamusa
a == b      # igualdad estructural
a != b      # desigualdad
a < b       # menor      (también <=, >, >=)
a es b      # identidad (mismo objeto)
a es no b   # identidad negada
a en lista  # pertenencia
a no en l   # pertenencia negada
```

### Lógicos (palabras, no símbolos)

```cornamusa
imprimir(verdadero y falso)     # falso
imprimir(verdadero o falso)     # verdadero
imprimir(no verdadero)          # falso
```

Cortocircuito como Python: `a y b` no evalúa `b` si `a` es falso.

---

## 3. Tipos primitivos

| Tipo | Sintaxis | Mutable |
|---|---|---|
| `entero` | `42`, `0xff`, `0o755`, `0b1010`, `1_000_000` | no |
| `decimal` | `3.14`, `1.5e-3` | no |
| `booleano` | `verdadero`, `falso` | no |
| `nulo` | `nulo` | no |
| `cadena` | `"hola"`, `'mundo'`, `"""multilínea"""` | no |
| `lista` | `[1, 2, 3]` | sí |
| `tupla` | `(1, 2, 3)` | no |
| `diccionario` | `{"clave": "valor"}` — **preserva orden de inserción** (v1.20) | sí |
| `conjunto` | `{1, 2, 3}` (no vacío) o `conjunto()` (vacío) | sí |
| `funcion` | `funcion`, `lambda`, nativas, métodos ligados | no |
| `clase` / instancia | ver §11 | instancia: sí |
| `generador` | producto de llamar a una función con `producir` (§9) | — |

> Los **enteros son de precisión arbitraria**: `2 ** 1000` es válido. Internamente se distingue SMALL (≤ 63 bits, inline) y BIG (mp_int), invisible al programa.

### Verdadez (truthy/falsy)

**Falsos**: `falso`, `nulo`, `0`, `0.0`, `""`, `[]`, `()`, `{}` (dicc vacío), `conjunto()`.
**Verdaderos**: todo lo demás. Las instancias pueden redefinir su verdadez con el dunder `__booleano__` (v1.41); sin él una instancia es siempre verdadera.

---

## 4. Control de flujo

### `si` / `sino si` / `sino`

```cornamusa
si cond1:
    ...
sino si cond2:
    ...
sino:
    ...
fin si
```

### `mientras`

```cornamusa
mientras cond:
    ...
fin mientras
```

### `para`

```cornamusa
para x en iterable:
    ...
fin para
```

`iterable` puede ser: lista, tupla, conjunto, diccionario (itera claves), cadena (itera caracteres), rango, **generador** y cualquier instancia con `__iterar__`.

La variable del `para` es un único nombre. Para desempaquetar pares, destructura dentro del cuerpo:

```cornamusa
para par en pares:
    clave, valor = par
    imprimir(clave, "->", valor)
fin para
```

### `romper` y `continuar`

```cornamusa
para i en rango(100):
    si i % 2 == 0: continuar
    si i > 20: romper
    imprimir(i)
fin para
```

### `pasar`

Sentencia vacía (placeholder):

```cornamusa
funcion no_implementado():
    pasar
fin funcion
```

---

## 5. Destructuring (desempaquetado)

`a, b = iterable` desempaqueta en una sola línea. Funciona con tuplas, listas y cadenas; admite anidación.

```cornamusa
a, b = (1, 2)             # a=1, b=2
[x, b, z] = [10, 20, 30]  # listas también
i, d = d, i               # swap sin variable temporal
(cab, (op, val)) = ("set", ("+", 42))   # anidado

funcion divmod(n, d):
    retornar (n // d, n % d)
fin funcion
coc, resto = divmod(17, 5)   # coc=3, resto=2
```

Errores atrapables: aridad incorrecta → `ErrorDeValor`; valor no iterable → `ErrorDeTipo`.

---

## 6. Funciones

### Sintaxis

```cornamusa
funcion nombre(p1, p2, p3=valor_por_defecto):
    ...
    retornar resultado
fin funcion
```

Sin `retornar`, la función devuelve `nulo`.

### Argumentos por defecto

```cornamusa
funcion saludar(nombre, idioma="es"):
    ...
fin funcion
saludar("Ana")            # idioma toma "es"
saludar("Bob", "en")
```

### `*args` — variádicos

`*resto` recoge en una **tupla** todos los posicionales extra:

```cornamusa
funcion suma(*nums):
    total = 0
    para n en nums: total = total + n
    retornar total
fin funcion
suma(1, 2, 3)             # 6
```

### `**kwargs`

`**kw` recoge en un **diccionario** los keyword args que no coinciden con un parámetro fijo:

```cornamusa
funcion api(host, **opciones):
    si "puerto" en opciones: ...
fin funcion
api("api.dev", puerto=443, tls=verdadero)
```

`*args` y `**kwargs` se combinan: `funcion f(a, *args, **kw):`.

### Keyword arguments en la llamada

```cornamusa
crear_punto(eje_y=3, eje_x=1)     # por nombre, cualquier orden
area_rect(5, alto=10)             # mezcla posicional + kwarg
```

### Spread en la llamada

`*` expande un iterable como posicionales; `**` expande un dict como kwargs:

```cornamusa
suma(*[10, 20, 30])               # ≡ suma(10, 20, 30)
suma(1, *[10, 20], 99)            # mezcla posicionales y *spread
api("h", **{"puerto": 443})       # ≡ api("h", puerto=443)
api("h", **{"puerto": 443}, tls=verdadero)   # **spread + kwarg explícito
```

> Desde v1.46 se pueden combinar `*args`, kwargs explícitos y `**dict` en la misma llamada: `f(*xs, c=10, **opts)`. Habilita wrappers genéricos `f(*args, **kw)`.

### Lambda

Función anónima de una sola expresión. Admite defaults, `*args` y `**kwargs`:

```cornamusa
cuadrado = lambda x: x * x
contar   = lambda *xs: longitud(xs)
```

### Closures y `nolocal`

Una función anidada captura variables del scope enclosing como **upvalues**. Por defecto son de solo lectura; `nolocal` permite **escribirlas**:

```cornamusa
funcion contador():
    n = 0
    funcion inc():
        nolocal n
        n = n + 1
        retornar n
    fin funcion
    retornar inc
fin funcion
c = contador()
imprimir(c(), c(), c())   # 1 2 3
```

---

## 7. Estructuras de datos

### Listas

```cornamusa
xs = [1, 2, 3]
xs[0]              # 1
xs[-1]             # 3 (último)
xs[1:3]            # [2, 3] (slice)
xs[::-1]           # [3, 2, 1] (invertida)
longitud(xs)       # 3
xs[0] = 10         # mutación
xs[1:2] = [9, 9]   # slicing assignment (v1.44) — crece/encoge
agregar(xs, 99)    # añadir al final
quitar(xs, 0)      # quitar por índice (devuelve el valor)
insertar(xs, 0, 7) # insertar en posición
ordenar(xs)        # in-place
invertir(xs)       # in-place
```

### Diccionarios

Mapa hash que **preserva el orden de inserción** (v1.20):

```cornamusa
d = {"a": 1, "b": 2}
d["a"]             # 1
d["c"] = 3         # añadir
"a" en d           # verdadero/falso
quitar(d, "a")     # quitar por clave (devuelve el valor)
claves(d)          # ["b", "c"]
valores(d)         # [2, 3]
longitud(d)        # cantidad de pares
para clave en claves(d):
    imprimir(clave, "->", d[clave])
fin para
```

### Conjuntos

```cornamusa
s = {1, 2, 3}
agregar(s, 4)
quitar(s, 4)
"x" en s
longitud(s)
conjunto()         # vacío (no {} que es dict vacío)
conjunto([1,1,2])  # desde iterable → {1, 2}
```

### Tuplas

Inmutables:

```cornamusa
t = (1, 2, 3)
t[0]               # 1
longitud(t)
# t[0] = 10        # ✗ ErrorDeTipo: tupla inmutable
```

### Rangos

```cornamusa
rango(5)            # 0, 1, 2, 3, 4
rango(2, 8)         # 2, 3, 4, 5, 6, 7
rango(0, 10, 2)     # 0, 2, 4, 6, 8
rango(10, 0, -1)    # 10, 9, 8, ..., 1
```

### Comprehensions

Construyen listas, dicts y conjuntos en una expresión. Un `para ... en ...` y una guarda `si` opcional:

```cornamusa
[n * 2 para n en rango(10)]                   # list
[n para n en rango(20) si n % 2 == 0]         # con filtro
{n: n * n para n en rango(1, 6)}              # dict
{w[0] para w en palabras}                     # set (deduplica)
(n * n para n en rango(1, 7))                 # generator expression (lazy)
```

---

## 8. Cadenas

### Operaciones básicas

```cornamusa
"hola" + " " + "mundo"     # concatenación
"=" * 30                   # repetición
"orna" en "Cornamusa"      # pertenencia
longitud("Cornamusa")      # 9
"Cornamusa"[0]             # "C"
"Cornamusa"[-1]            # "a"
"Cornamusa"[0:4]           # "Corn" (slicing)
```

### Escape

```
\n   nueva línea          \t   tabulador
\r   retorno de carro     \\   barra invertida
\'   comilla simple       \"   comilla doble
\0   null
\xHH       byte hex (00-FF)
\uHHHH     codepoint Unicode (4 dígitos)
\u{HHHHHH} codepoint Unicode (cualquier ancho)
```

### F-cadenas

Interpolación completa: cada `{expr}` se evalúa y se convierte a cadena.

```cornamusa
f"hola {nombre}"          # interpolación de variable
f"{1 + 2 * 3}"            # expresión arbitraria
f"{{literal}}"            # → "{literal}", llaves dobles escapan
f"capas: {f'in-{x}'}"     # anidación
```

### Format specifiers (v1.45)

Tras `:` dentro de `{...}` puedes pasar un especificador de formato estilo Python:

```
{expr:[relleno][alineación][ancho][.precisión][tipo]}
```

| Parte | Valores | Notas |
|---|---|---|
| relleno | cualquier carácter | requiere alineación explícita |
| alineación | `<` `>` `^` | izquierda, derecha, centrado |
| ancho | dígitos | rellena hasta ese ancho |
| `.precisión` | dígitos | decimales (`f`/`e`); truncado (`s`) |
| tipo | `d f e x X b s` | entero, decimal, científica, hex, hex mayúsculas, binario, cadena explícita |

Defaults Python-compatibles: numéricos alinean a derecha, cadenas a izquierda. Prefijo `0` antes del ancho implica zero-padding alineado a derecha.

```cornamusa
f"{42:5d}"           # "   42"
f"{42:<5d}"          # "42   "
f"{42:05d}"          # "00042"
f"{3.14159:.2f}"     # "3.14"
f"{3.14159:>10.2f}"  # "      3.14"
f"{1234.5:.2e}"      # "1.23e+03"
f"{255:x}"           # "ff"
f"{255:08X}"         # "000000FF"
f"{5:b}"             # "101"
f"{'hi':-^10}"       # "----hi----"
f"{'hola mundo':.4}" # "hola"
```

El `:` de slicing (`xs[1:3]`) y dict (`{k: v}`) **no** se confunden con el inicio de un spec — el parser solo detecta `:` cuando está en el nivel superior de la interpolación, fuera de `[]`/`()`/`{}` anidados.

Limitación: el spec **no** invoca `__cadena__` — la stringificación dentro del spec es siempre canónica. Si necesitas el dunder, computa `cadena(obj)` primero.

---

## 9. Generadores

Una función que contiene `producir` es un **generador**: llamarla no ejecuta el cuerpo, devuelve un objeto generador. Iterarlo con `para` lo reanuda hasta el siguiente `producir`; el estado (locales + posición) se preserva.

```cornamusa
funcion contar(ini, tope):
    i = ini
    mientras i <= tope:
        producir i
        i = i + 1
    fin mientras
fin funcion

para v en contar(1, 5):
    imprimir(v)            # 1 2 3 4 5
fin para
```

`producir desde` delega en un sub-generador o iterable:

```cornamusa
funcion arbol():
    producir 0
    producir desde [1, 2, 3]   # iterables
    producir desde hojas()     # otros generadores
fin funcion
```

Generator expressions inline: `(expr para v en it si guarda)` — lazy, no materializa.

---

## 10. Pattern matching (`coincidir` / `cuando`)

```cornamusa
coincidir valor:
    cuando 0:
        imprimir("cero")
    cuando 1 | 2 | 3:
        imprimir("pequeño")           # OR-pattern (separador `|`)
    cuando [a, b]:
        imprimir("par", a, b)
    cuando [cabeza, *resto]:
        imprimir("lista no vacía", cabeza)
    cuando (a, b) si a == b:
        imprimir("diagonal")          # guarda con `si`
    cuando Perro() como p:
        imprimir("un perro:", p)      # type-match + binding
    cuando _:
        imprimir("cualquier otra cosa")
fin coincidir
```

Patrones disponibles:

- **Literales**: `cuando 0`, `cuando "hola"`.
- **Nombre** (bind): `cuando n` enlaza el valor a `n`. **Comodín**: `cuando _`.
- **Tupla / lista**: `cuando (a, b)`, `cuando [a, b]`, con `*resto` en cualquier posición. Anidables.
- **OR**: `cuando p1 | p2 | p3` (separador `|`).
- **Type-match**: `cuando Clase() como nombre` — encaja con instancias de `Clase` (de usuario) y las enlaza.
- **Guarda**: `cuando patron si condicion`.

> No hay patrones de diccionario.

---

## 11. Clases y objetos

```cornamusa
clase Persona:
    funcion __iniciar__(yo, nombre, edad):
        yo.nombre = nombre
        yo.edad = edad
    fin funcion

    funcion saludar(yo):
        retornar "Soy " + yo.nombre
    fin funcion
fin clase

ana = Persona("Ana", 30)
ana.saludar()
ana.edad = 31
```

### Herencia y `super`

```cornamusa
clase Empleado extiende Persona:
    funcion __iniciar__(yo, nombre, edad, salario):
        super.__iniciar__(nombre, edad)
        yo.salario = salario
    fin funcion

    funcion saludar(yo):
        retornar super.saludar() + ", empleado"
    fin funcion
fin clase
```

### Dunders (métodos mágicos)

Se invocan automáticamente al usar el operador correspondiente:

| Dunder | Disparador |
|---|---|
| `__iniciar__(yo, ...)` | construcción `Clase(...)` |
| `__cadena__(yo)` | `cadena(obj)`, `imprimir(obj)`, f-strings |
| `__repr__(yo)` | `repr(obj)` (v1.41) |
| `__booleano__(yo)` | `si obj:`, `mientras obj:`, `y`/`o`, `no obj` (v1.41) |
| `__hash__(yo)` | clave de dict/conjunto — debe retornar entero (v1.42) |
| `__longitud__(yo)` | `longitud(obj)` |
| `__iterar__(yo)` | `para x en obj` — devuelve un iterable nativo o una instancia con `__siguiente__` |
| `__siguiente__(yo)` | siguiente valor de un iterador lazy; lanza `ErrorDeIteracion` al agotarse (v1.43) |
| `__llamar__(yo, ...)` | `obj(...)` |
| `__indice__(yo, i)` | `obj[i]` |
| `__asignar_indice__(yo, i, v)` | `obj[i] = v` |
| `__entrar__(yo)` / `__salir__(yo)` | bloque `con` |
| `__igual__(yo, otro)` | `==`, **igualdad en dict/conjunto** (v1.42) |
| `__distinto__` | `!=` |
| `__menor__` `__menor_igual__` `__mayor__` `__mayor_igual__` | `<` `<=` `>` `>=` |
| `__sumar__` `__restar__` `__multiplicar__` `__dividir__` | `+` `-` `*` `/` |
| `__dividir_entero__` `__modulo__` `__potencia__` | `//` `%` `**` |
| `__sumar_derecho__`, etc. | operador con la instancia a la derecha (`5 + obj`) |

> Para usar instancias como **claves por valor** define `__hash__` + `__igual__` coherentes (iguales ⇒ mismo hash). El runtime cachea `__hash__` por instancia tras el primer despacho. Sin estos dunders, las instancias siguen siendo hashables — por identidad.

---

## 12. Context managers (`con`)

`con` ejecuta `__entrar__` antes del cuerpo y `__salir__` después, **incluso si el cuerpo lanza una excepción**.

```cornamusa
con mutex como m:
    imprimir("trabajando bajo", m.nombre)
fin con
```

Equivale a `_ctx = expr; nombre = _ctx.__entrar__(); intentar: cuerpo finalmente: _ctx.__salir__()`.

### Multi-recurso (v1.46)

Varios recursos separados por coma. Entra en orden A→B→C; libera en orden inverso C→B→A (LIFO), incluso si el cuerpo lanza.

```cornamusa
con abrir_conexion("db") como conn, abrir_archivo("log") como log:
    conn.consultar("SELECT * FROM ...")
    log.escribir("ok")
fin con
```

Equivale a `con A: con B: con C: ... fin con fin con fin con` anidados.

> Limitación: `__salir__` se invoca sin argumentos (no recibe la excepción).

---

## 13. Excepciones

### Lanzar y atrapar

```cornamusa
lanzar ErrorDeValor("mensaje")

intentar:
    ...
atrapar ErrorDeTipo como e:
    imprimir(e)
atrapar ErrorDeValor como e:
    imprimir(e)
finalmente:
    imprimir("siempre")
fin intentar
```

### Tipos built-in

| Excepción | Cuándo |
|---|---|
| `Excepcion` | Base de la jerarquía |
| `ErrorAritmetico` | División por cero, overflow lógico |
| `ErrorDeTipo` | Operación sobre tipo incorrecto |
| `ErrorDeValor` | Valor del tipo correcto pero inválido |
| `ErrorDeIndice` | Índice fuera de rango |
| `ErrorDeClave` | Clave no presente en dict |
| `ErrorDeNombre` | Identificador no definido |
| `ErrorDeAtributo` | Atributo/método inexistente en instancia o módulo |
| `ErrorDeSistema` | Fallo del sistema operativo |
| `ErrorDeIO` | Fallo de entrada/salida |
| `ErrorDeIteracion` | Señal de fin de un iterador lazy (`__siguiente__`); el `para` la atrapa internamente. v1.43 |

Cuando un error no se atrapa, Cornamusa imprime un **traceback** multi-frame con la cadena de llamadas y la línea de fuente.

---

## 14. Módulos

```cornamusa
importar matematicas                       # global `matematicas`
importar matematicas como mat              # alias
desde matematicas importar PI, factorial   # selectivo
desde matematicas importar factorial como fact
importar paquete.submodulo                 # subsegmentos
```

> `desde X importar *` no está soportado.

---

## 15. Built-ins

Funciones y constructores registrados como globales (no requieren `importar`).

### E/S y conversión

| Firma | Descripción |
|---|---|
| `imprimir(*args)` | Imprime args separados por espacio + `\n` |
| `leer([prompt])` | Lee una línea de stdin. Sin args, silencioso; con cadena, la imprime como prompt |
| `tipo(x)` | Cadena con el nombre del tipo |
| `cadena(x)` | Coerción a cadena (usa `__cadena__` si existe) |
| `entero(x)` | Coerción a entero (entero/decimal/booleano/cadena) |
| `decimal(x)` | Coerción a decimal |
| `booleano(x)` | Coerción a booleano según truthiness |
| `lista(iter)` | Materializa un iterable como lista |
| `tupla(iter)` | Materializa un iterable como tupla |
| `diccionario(pares)` | Construye dict desde iterable de pares o de otro dict |
| `repr(x)` | Representación inspeccionable (cadenas entre comillas, etc.) |

### Tamaño e iteración

| Firma | Descripción |
|---|---|
| `longitud(x)` | Tamaño de cadena/lista/dict/conjunto/tupla/rango (usa `__longitud__`) |
| `rango(fin)` / `rango(ini, fin)` / `rango(ini, fin, paso)` | Iterador entero perezoso |

### Mutación de colecciones

| Firma | Descripción |
|---|---|
| `agregar(coleccion, x)` | Añade al final (lista) o al conjunto |
| `quitar(coleccion, clave?)` | Quita por índice (lista), clave (dict) o valor (conjunto); devuelve lo quitado. Sin arg en lista: quita el último |
| `insertar(lista, i, x)` | Inserta `x` en la posición `i` |
| `invertir(lista)` | In-place |
| `ordenar(lista, invertido=falso)` | In-place |
| `claves(dict)` / `valores(dict)` | Listas con las claves / los valores |
| `conjunto(iter?)` | `conjunto()` vacío, o desde un iterable |

### Numéricos y reflexión

| Firma | Descripción |
|---|---|
| `absoluto(n)` | Valor absoluto |
| `redondear(n, decimales=0)` | Redondeo a `decimales` posiciones |
| `instancia_de(obj, Clase)` | ¿`obj` es instancia de `Clase` (o subclase)? Solo clases de usuario |
| `subclase_de(A, B)` | ¿`A` es subclase de `B`? |
| `id(x)` | Identificador entero del objeto |

### Excepciones (constructores)

`Excepcion`, `ErrorAritmetico`, `ErrorDeTipo`, `ErrorDeValor`, `ErrorDeIndice`, `ErrorDeClave`, `ErrorDeNombre`, `ErrorDeSistema`, `ErrorDeIO` — todos `(msg)`. `ErrorDeAtributo` lo lanza la VM.

### Sistema y memoria

| Firma | Descripción |
|---|---|
| `recolectar()` | Fuerza una pasada del GC |
| `obtener_argv()` | Lista de cadenas con los args del programa (también `sistema.argv`) |
| `salir(codigo=0)` | Termina el proceso |

### Built-ins de bajo nivel (envueltos por la stdlib)

`archivo_leer`, `archivo_escribir`, `archivo_existe`, `archivo_lineas`, `archivo_agregar`, `json_parsear`, `json_serializar`, `tiempo_actual`, `tiempo_descomponer`, `tiempo_componer`, `tiempo_formato`, `azar_decimal`, `azar_entero`, `azar_semilla`, `proceso_ejecutar`, `regex_coincide`, `regex_buscar`, `regex_todos`, `regex_reemplazar`, `red_http_obtener`.

Se pueden usar directamente, pero la práctica recomendada es importar el módulo de stdlib correspondiente (§16), que ofrece nombres legibles y funciones de conveniencia.

---

## 16. Biblioteca estándar (`stdlib/`)

Diecisiete módulos. Se importan con `importar <nombre>`.

### `matematicas`

`PI`, `E`, `cuadrado(n)`, `cubo(n)`, `absoluto(n)`, `maximo(a,b)`, `minimo(a,b)`, `signo(n)`, `factorial(n)`, `suma_rango(a,b)`, `es_par(n)`, `es_impar(n)`, `mcd(a,b)`.

### `cadenas`

`repetir(s,n)`, `es_vacia(s)`, `unir(partes,sep)`, `caracter(s,i)`, `empieza_con(s,pre)`, `termina_con(s,suf)`, `indice_de(s,sub)`, `contiene(s,sub)`, `separar(s,sep)`, `reemplazar(s,viejo,nuevo)`, `minusculas_ascii(s)`, `mayusculas_ascii(s)`, `recortar(s)`, `recortar_izquierda(s)`, `recortar_derecha(s)`, `contar(s,sub)`.

### `funcionales`

`mapear(f,xs)`, `filtrar(p,xs)`, `reducir(f,xs,inicial)`, `enumerar(xs,inicio=0)`, `cualquiera(p,xs)`, `todos(p,xs)`, `suma(xs,inicial=0)`, `minimo(xs)`, `maximo(xs)`. (Existen también `enumerar_desde`/`suma_desde` deprecados — usa los con `inicial=`.)

### `formato`

`rellenar(s,ancho,car=" ")`, `alinear_derecha(...)`, `centrar(...)`, `con_decimales(n,decimales=2)`, `numero_con_separador(n,sep="_")`, `porcentaje(d,decimales=2)`, `como_hex(n,prefijo="0x")`, `como_binario(n,prefijo="0b")`, `linea(car="-",ancho=60)`, `fila(valores,anchos,sep=" | ")`.

### `archivos`

`leer(ruta)`, `escribir(ruta,contenido)`, `lineas(ruta)`, `existe(ruta)`, `agregar(ruta,contenido)`.

### `json`

`parsear(texto)`, `serializar(valor)`, `serializar_indentado(valor,indent)`.

### `csv` (v1.58)

`parsear(texto, sep=",")` → lista de listas, `serializar(filas, sep=",")` → cadena, `leer(ruta, sep=",")` y `escribir(ruta, filas, sep=",")` para archivos. RFC 4180-like: campos entre `"` admiten `,` y `\n` internos; `""` escapa `"`.

### `fechas`

`ahora()`, `componentes(ts)`, `construir(año,mes,dia,hora=0,minuto=0,segundo=0)`, `formato(ts,spec)`, `iso8601(ts)`, `legible(ts)`, `solo_fecha(ts)`, `solo_hora(ts)`, `sumar_dias(ts,n)`, `sumar_horas(ts,n)`, `diferencia_seg(a,b)`, `diferencia_dias(a,b)`, `es_bisiesto(año)`, `dias_en_mes(año,mes)`, `nombre_dia(d)`, `nombre_mes(m)`. Constantes: `SEGUNDO`, `MINUTO`, `HORA`, `DIA`, `SEMANA`.

### `tiempo` (v1.73)

Reloj, sleep, cronómetro (complementa `fechas`):

- `epoch_segundos()` → entero, segundos Unix epoch.
- `epoch_ms()` → entero, milisegundos Unix epoch.
- `monotonic()` → decimal, segundos desde punto arbitrario (correcto para medir duraciones).
- `dormir(s)` → bloquea `s` segundos (acepta decimal, `s <= 0` retorna inmediato, NaN/inf lanzan `ErrorDeValor`).
- `cronometro()` → instancia con `.leer()` (segundos transcurridos) y `.reiniciar()`.

### `azar`

`decimal()` → [0,1), `entero(a,b)` → [a,b], `semilla(n)`, `elegir(seq)`, `barajar(lista)`, `muestra(seq,k)`, `booleano(p=0.5)`, `uniforme(a,b)`.

### `proceso`

`ejecutar(programa, *args)` → dict `{salida, error, codigo}`, `capturar(programa, *args)` → solo stdout, `codigo(programa, *args)` → solo exit code.

### `regex`

`coincide(patron,texto)`, `buscar(patron,texto)`, `todos(patron,texto)`, `reemplazar(patron,texto,rep)`, `contiene(patron,texto)`, `extraer(patron,texto)`. Subset soportado: literales y escapes, `* + ? {n,m}`, clases `. [abc] [^abc] [a-z] \d \w \s` y negadas, anclas `^ $ \b`, grupos `() (?:)`, alternancia `|`.

### `red`

`obtener(url,cabeceras_extra=nulo,timeout=10)` → dict `{codigo, cabeceras, cuerpo}`, `descargar_cuerpo(url)`, `parsear_cabeceras(cab_raw)`. Solo HTTP/1.1 plano (sin TLS).

### `base64` (v1.59 + v1.66 URL-safe)

`codificar(s)` → cadena RFC 4648 estándar (`+/=`), `decodificar(s)` → cadena (tolerante: acepta `-_` y entrada sin padding), `codificar_url(s)` → variante URL-safe (`-_` sin `=`, RFC 4648 §5).

### `hashing` (v1.60 + v1.65 HMAC)

`sha256(s)` / `md5(s)` → digest hex en minúsculas (FIPS 180-4 / RFC 1321). `hmac_sha256(clave, mensaje)` / `hmac_md5(...)` → MAC hex (RFC 2104/4231). `hmac_sha256_bytes(...)` → 32 bytes raw (usado por `jwt`). MD5 está roto criptográficamente; sigue siendo útil en HMAC y para integridad casual.

### `jwt` (v1.67 + v1.70)

JSON Web Tokens HS256 (RFC 7519) sobre `json` + `base64` + `hashing`:

- `codificar(payload, clave)` → cadena `header.payload.firma`.
- `decodificar(token, clave)` → diccionario (valida firma; lanza `ErrorDeValor` si firma inválida, header malformado, o `alg` ≠ `HS256`).
- `verificar(token, clave)` → booleano sin lanzar.
- `expirado(payload, ahora)` → booleano (true si `exp <= ahora`; sin claim `exp` retorna `falso`).
- `decodificar_y_validar(token, clave, ahora)` → diccionario o `ErrorDeValor` por firma, `exp <= ahora`, o `nbf > ahora`.

`alg=none` está rechazado por diseño (mitigación contra algorithm confusion). RS256/ES256 (clave pública) no soportados.

### `sistema`

`sistema.argv` — lista de cadenas con los argumentos del programa.

---

## 17. CLI

```bash
cornamusa programa.cor                # ejecuta con tree-walking (compatibilidad histórica)
cornamusa --bytecode programa.cor     # ejecuta con la VM bytecode (recomendado)
cornamusa --check programa.cor        # valida (lex+parse+compila) sin ejecutar; alias --validar
cornamusa --tokens programa.cor       # vuelca tokens del lexer (debug)
cornamusa --ast programa.cor          # vuelca el AST en S-expression (debug)
cornamusa -v / --version              # versión
cornamusa -h / --ayuda                # ayuda
cornamusa                             # REPL interactivo (history en ~/.cornamusa_historial)
cornamusa --bytecode prog.cor a b c   # pasa "a", "b", "c" a sistema.argv

cornamusa fmt programa.cor            # formatea in-place (v1.48)
cornamusa fmt --check programa.cor    # exit 0 si ya formateado, 1 si no
cornamusa fmt --stdout programa.cor   # formato a stdout, no toca archivo
cornamusa fmt -                       # lee stdin → escribe stdout

cornamusa lint programa.cor           # avisos de estilo (v1.49)
                                       # exit 0 sin avisos, 1 con avisos

cornamusa docs programa.cor           # Markdown a stdout (v1.51)
cornamusa docs programa.cor -o doc.md # Markdown a archivo

cornamusa lsp                         # Language Server Protocol por stdio (v1.52)
                                       # para integracion editor (VS Code, etc.)
```

Categorias del linter: `unreachable`, `redundant-pasar`, `eq-nulo`, `unused-import`, `unused-local`, `unused-param`, `shadow`, `unused-loop-var`, `mutable-default`, `concat-in-loop`, `same-comparison`, `empty-except`.

---

## 18. Errores comunes

### `ErrorDeNombre: nombre 'x' no esta definido`

Variable usada sin asignar. Cornamusa sugiere el nombre más parecido si lo hay (`¿quisiste decir 'longitud'?`). Recuerda: las keywords son castellanas (`si`, `funcion`, `clase`), no inglesas.

### `ErrorDeTipo: '<tipo>' no soporta el operador '+'`

Operación entre tipos incompatibles. Cornamusa **no** auto-convierte:

```cornamusa
"5" + 3       # ✗ ErrorDeTipo
"5" + "3"     # ✓ "53"
"5" + cadena(3)   # ✓ "53"
```

### `ErrorDeIndice: indice X fuera de rango`

```cornamusa
xs = [1, 2, 3]
xs[5]         # ✗ ErrorDeIndice
```

### `ErrorDeClave: 'X'`

```cornamusa
d = {"a": 1}
d["b"]        # ✗ ErrorDeClave
"b" en d      # forma idiomática para chequear primero
```

### `ErrorDeAtributo: instancia de 'C' no tiene atributo 'x'`

Atributo o método mal escrito. Cornamusa sugiere el más parecido.

### `desbordamiento de pila de llamadas`

Recursión infinita. El límite de frames es ≈ 1024.

---

## 19. Reservas para el futuro

Sintaxis aceptada por el parser pero **no implementada todavía** en el runtime:

| Característica | Estado |
|---|---|
| `asincrono` / `esperar` (async/await) | keywords reservadas; v2.x |
| Anotaciones de tipo (`p: tipo`, `funcion f() -> T`) | el parser las acepta; el runtime las ignora (sin type-checker) |
| Prefijos de cadena `r"..."` (raw), `b"..."` (bytes) | reservados |
| Decoradores sobre clases o sobre métodos | el parser rechaza `@x` antes de `clase`, y el compilador lanza error claro si aparecen sobre métodos de clase |

Implementadas en versiones recientes (ya no son "reservas"):

| Característica | Versión |
|---|---|
| `borrar d[k]` / `borrar obj.attr` | v1.56 |
| `global X` ejecutable en bytecode | v1.57 |
| Decoradores `@nombre` sobre funciones (stacking + factories) | v1.72 |
| Dunders `__hash__`, `__repr__`, `__booleano__`, `__siguiente__`, `__igual__` | v1.41-v1.43 |

---

## 20. Recursos

- **[Tutorial paso a paso](tutorial.md)** — para aprender desde cero.
- **[Especificación formal](https://github.com/David-Castilla-Gomez/Cornamusa/blob/main/ESPEC.md)** — sintaxis EBNF y semántica.
- **[Decisiones arquitectónicas](https://github.com/David-Castilla-Gomez/Cornamusa/tree/main/decisiones)** — el razonamiento detrás de las elecciones de diseño.
- **[Ejemplos](https://github.com/David-Castilla-Gomez/Cornamusa/tree/main/examples)** — 72 programas, uno por feature.
- **[Issues y discusión](https://github.com/David-Castilla-Gomez/Cornamusa/issues)** — bugs, ideas, preguntas.
