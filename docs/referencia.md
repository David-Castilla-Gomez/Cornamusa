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

### Walrus operator `:=` (v1.113)

Asignación como expresión (PEP 572 de Python). `nombre := valor` asigna `valor` a `nombre` y deja el valor en la expresión:

```cornamusa
si (n := longitud(xs)) > 5:
    imprimir(f"grande: {n=}")
fin si

mientras (item := siguiente()) != nulo:
    procesar(item)
fin mientras
```

Solo el lado izquierdo IDENT — no se admite `obj.x := v` ni `xs[0] := v`. Se permite paréntesis (`(n := 5)`) en cualquier expresión.

**Semántica**:
- Si la variable ya existe (local, upvalue, global), se actualiza.
- Si no existe, se crea (local nuevo en función, global en top-level).

**Limitación**: dentro de bucles, crear una variable **nueva** con `:=` falla porque el slot del compilador se fija en la primera iteración. La variable debe pre-existir antes del loop si va a usarse con walrus.

### Anotaciones de tipo opcionales (v1.114)

Cornamusa permite anotaciones de tipo en parámetros, retornos y asignaciones de variables (PEP 484-style). **Sin verificación runtime** — son puramente sintácticas, útiles para documentación y futuras herramientas de tipos.

```cornamusa
# Parametros y retorno
funcion sumar(a: entero, b: entero = 0) -> entero:
    retornar a + b
fin funcion

# Variables (top-level y locales)
nombre: cadena = "Ana"
edades: lista = [25, 30, 35]
```

Las **anotaciones pueden ser cualquier expresión**: identificador (`entero`, `cadena`), llamada (`Opcional(entero)`), índice (`lista[entero]`). El parser las consume pero el compilador las descarta.

**Limitaciones**:
- No se admite anotación en atributos (`yo.x: tipo = ...` no parsea).
- No se admite anotación en destructuring (`a: tipo, b: tipo = par`).
- Los identificadores usados como tipos NO se resuelven en runtime — `funcion f(x: TipoQueNoExiste)` parsea pero no falla aunque `TipoQueNoExiste` no esté definido.

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

### Debug format (v1.112)

Sufijo `=` dentro de `{expr=}` emite la expresión tal cual escrita por el usuario + `=` + valor formateado. Equivalente a `f"expr={expr}"`, pero sin tener que duplicar el texto:

```cornamusa
x = 5
imprimir(f"{x=}")           # "x=5"
imprimir(f"{x*2=}")          # "x*2=10"
imprimir(f"{x = }")          # "x = 5"   (espacios preservados)
imprimir(f"{x=:>5}")         # "x=    5" (combinable con spec)
```

El parser **no confunde** `=` debug con operadores `==`, `!=`, `<=`, `>=`: el `=` solo se interpreta como debug si NO está precedido por `=`, `!`, `<` o `>`.

Patrón estándar de print debugging (Python 3.8+). Reduce muchísimo la verborrea típica de `imprimir(f"x = {x}, y = {y}, suma = {x+y}")` → `imprimir(f"{x=}, {y=}, {x+y=}")`.

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

Veinte módulos. Se importan con `importar <nombre>`.

### `matematicas`

`PI`, `E`, `cuadrado(n)`, `cubo(n)`, `absoluto(n)`, `maximo(a,b)`, `minimo(a,b)`, `signo(n)`, `factorial(n)`, `suma_rango(a,b)`, `es_par(n)`, `es_impar(n)`, `mcd(a,b)`.

**v1.110**: constantes y predicados.
- Constantes: `TAU` (2·PI), `INFINITO` (decimal infinito positivo), `NO_NUMERO` (NaN).
- Predicados: `es_infinito(x)`, `es_no_numero(x)`, `es_finito(x)` — necesarios porque IEEE 754 garantiza `NaN != NaN` (comparación directa nunca detecta NaN).

**v1.103**: funciones continuas (libm). Aceptan enteros/decimales/booleanos, devuelven decimal. Ángulos en **radianes**.
- **Raíz y potencia**: `raiz(x)` (rechaza negativo), `potencia(x, expo)`, `hipotenusa(a, b)`.
- **Logaritmos y exponencial**: `ln(x)` (rechaza no-positivo), `log10(x)`, `log(x, base)`, `exp(x)`.
- **Trigonometría**: `seno(x)`, `coseno(x)`, `tangente(x)`.
- **Inversas**: `arco_seno(x)` y `arco_coseno(x)` (dominio `[-1, 1]`), `arco_tangente(x)`, `arco_tangente2(dy, dx)` (rango `[-PI, PI]`, maneja cuadrantes).
- **Conversión**: `grados_a_radianes(g)`, `radianes_a_grados(r)`.
- **Redondeo**: `techo(x)` (ceil), `suelo(x)` (floor), `redondear(x)` (half-away-from-zero).

Errores típicos lanzan `ErrorDeValor` atrapable. Built-ins subyacentes: `mat_raiz`, `mat_ln`, `mat_log10`, `mat_exp`, `mat_seno`/`coseno`/`tangente`, `mat_arco_*`, `mat_techo`/`suelo`/`redondear`, `mat_potencia`.

### `cadenas`

`repetir(s,n)`, `es_vacia(s)`, `unir(partes,sep)`, `empieza_con(s,pre)`, `termina_con(s,suf)`, `indice_de(s,sub)`, `contiene(s,sub)`, `separar(s,sep)`, `reemplazar(s,viejo,nuevo)`, `minusculas_ascii(s)`, `mayusculas_ascii(s)`, `recortar(s)`, `recortar_izquierda(s)`, `recortar_derecha(s)`, `contar(s,sub)`. (Para `s[i]` usa la indexación built-in, no función.)

### `funcionales`

`mapear(f,xs)`, `filtrar(p,xs)`, `reducir(f,xs,inicial)`, `enumerar(xs,inicio=0)`, `cualquiera(p,xs)`, `todos(p,xs)`, `suma(xs,inicial=0)`, `minimo(xs)`, `maximo(xs)`, `agrupar_por(xs,f)`, `tomar(n,xs)`, `saltar(n,xs)`, `combinar(xs,ys)`, `aplanar(xs)`, `unicos(xs)`.

**v1.101**: `ordenar_por(xs, clave)` y `ordenar_por_inverso(xs, clave)` — mergesort **estable** O(n log n) que ordena `xs` por el resultado de aplicar `clave` a cada elemento. `clave` se invoca **una vez** por elemento (no por comparación). Devuelve nueva lista (no muta `xs`). Soluciona la limitación del built-in `ordenar` que solo compara números y cadenas directamente.

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

**v1.103**: `normal(mu, sigma)` — muestra de distribución normal (Gaussiana) con media `mu` y desviación `sigma` usando transformada Box-Muller. Requiere `sigma >= 0`; `sigma == 0` devuelve `mu` exacto. Para reproducibilidad combinar con `azar.semilla(n)`.

**v1.110**: tres distribuciones adicionales.
- `exponencial(tasa)` — tiempos entre eventos en procesos Poisson. `tasa > 0`; media `= 1/tasa`. Implementación: `-ln(U)/tasa`.
- `binomial(n, p)` — número de éxitos en `n` ensayos Bernoulli con probabilidad `p`. `n >= 0`, `0 <= p <= 1`. Implementación: contar éxitos de `n` Bernoulli(p).
- `poisson(media)` — número de eventos por intervalo con media dada. `media >= 0`. Implementación: algoritmo de Knuth (multiplicar uniformes hasta superar `e^-media`).

Todas pure-Cornamusa sobre `azar_decimal` (PRNG xoshiro256\*\*). Verificadas estadísticamente: media empírica converge al valor teórico con ~5000 muestras.

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

**v1.104**: entorno del proceso.
- `sistema.obtener_variable(nombre)` → cadena con el valor, o `nulo` si no está definida.
- `sistema.establecer_variable(nombre, valor)` → asigna. Si `valor == nulo`, borra la variable.
- `sistema.variables()` → dict completo `{nombre: valor}` del entorno actual.
- `sistema.inicio()` → cadena con el directorio HOME (POSIX) o USERPROFILE (Windows). Separadores normalizados a `/`.

Built-ins subyacentes: `obtener_variable_entorno`, `establecer_variable_entorno`, `variables_entorno`, `directorio_inicio`. Portabilidad: `getenv`/`setenv` POSIX, `getenv`/`_putenv_s` Windows. Listado completo via `environ` (POSIX) / `_environ` (Windows).

**v1.108**: identidad del sistema.
- `sistema.usuario()` → nombre del usuario actual. POSIX: `USER`/`LOGNAME`. Windows: `USERNAME`. Lanza `ErrorDeSistema` si no se determina.
- `sistema.host()` → nombre de la máquina. POSIX: `gethostname()`. Windows: `GetComputerNameA()`.
- `sistema.directorio_temp()` → directorio temporal del SO. POSIX: `TMPDIR` o `/tmp`. Windows: `TEMP`/`TMP` o `C:/Windows/Temp`. Separadores normalizados a `/`.

Built-ins subyacentes: `usuario_actual`, `hostname`, `directorio_temporal`.

### `coleccion` (v1.88, ampliado v1.116)

Estructuras de datos clásicas implementadas con clases sobre listas y diccionarios nativos:

- `Pila()` — LIFO. Métodos: `poner(x)`, `sacar()`, `vista()`, `vacia()`, `__longitud__`.
- `Cola()` — FIFO. Métodos: `poner(x)`, `sacar()` (saca del frente), `vista()`, `vacia()`, `__longitud__`.
- `ColaDoble()` — deque. Métodos: `poner_frente(x)`, `poner_final(x)`, `sacar_frente()`, `sacar_final()`, `vista_frente()`, `vista_final()`, `vacia()`, `__longitud__`.
- `Heap(clave=nulo)` (v1.116, clave en v1.120) — min-heap binario. Métodos: `poner(x)` y `sacar()` O(log n), `vista()` O(1), `vacia()`, `__longitud__`. Sin `clave`, los elementos deben ser comparables con `<` nativamente (números, cadenas). Con `clave` (callable como `lambda p: p[0]`), compara `clave(a) < clave(b)` — desbloquea heaps de listas, tuplas, dicts o instancias por campo. Para max-heap, usar `Heap(lambda x: -x)`.
- `Contador(items?)` (v1.116) — multiset estilo Counter. Métodos: `incrementar(x, n=1)`, `decrementar(x, n=1)` (elimina al llegar a 0), `obtener(x)` (defecto 0), `mas_comunes(n=nulo)` (ordenado descendente), `total()`, `items()` (lista de `[k, v]`), `__longitud__` (claves distintas). Acepta lista de items en el constructor para contar frecuencias directamente.

Pila/Cola/ColaDoble lanzan `ErrorDeValor("X vacia")` al sacar de colección vacía. `Heap` lanza `ErrorDeValor("Heap vacio")` en `sacar`/`vista`. `Contador` jamás lanza por clave ausente — usa `obtener` (devuelve 0) o el operador `en`.

### `estadisticas` (v1.117)

Estadística descriptiva e inferencial básica sobre listas de números. Reusa `funcionales`, `matematicas` y `coleccion.Contador` — pure-Cornamusa.

**Centralidad**:
- `media(xs)` — promedio aritmético.
- `mediana(xs)` — central; promedio de los dos centrales si `n` es par.
- `mediana_baja(xs)` / `mediana_alta(xs)` — menor / mayor central en listas pares (en impares coinciden con `mediana`).
- `moda(xs)` — más frecuente; en empate devuelve el primero.
- `multimodal(xs)` — lista de modas con frecuencia máxima.
- `media_armonica(xs)` — `n / Σ(1/xi)`. Lanza si algún valor es ≤ 0.
- `media_geometrica(xs)` — `(Π xi)^(1/n)`, en espacio log.

**Dispersión**:
- `varianza(xs)` (muestral, `n-1`) / `varianza_pob(xs)` (poblacional, `n`).
- `desviacion(xs)` / `desviacion_pob(xs)` — raíces de las anteriores.
- `amplitud(xs)` — `max - min` (no se llama `rango` porque colisiona con el built-in de iteración).

**Posición**:
- `percentil(xs, p)` — interpolación lineal, `p ∈ [0, 100]`.
- `cuartiles(xs)` → `[Q1, Q2, Q3]`.

**Dos series**:
- `covarianza(xs, ys)` (muestral).
- `correlacion(xs, ys)` — Pearson, en `[-1, 1]`.
- `regresion_lineal(xs, ys)` → `{"pendiente": m, "intercepto": b}` por mínimos cuadrados.

**Resumen**:
- `resumen(xs)` → dict con `n`, `min`, `max`, `media`, `mediana`, `Q1`, `Q3`, `desviacion`. Útil para REPL.

Las funciones de varianza y correlación muestrales lanzan `ErrorDeValor` si `n < 2`; las que dividen por desviación o varianza lanzan si la serie tiene varianza cero.

### `iteradores` (v1.118)

Combinatoria y herramientas de iteración inspiradas en `itertools` de Python. Pure-Cornamusa, eager (devuelve listas).

**Combinatoria**:
- `producto(xs, ys)` — producto cartesiano → lista de `[a, b]`.
- `producto3(xs, ys, zs)` — análogo para 3 iterables.
- `producto_repeticion(xs, r)` — `xs^r`, todas las r-tuplas con repetición.
- `permutaciones(xs, r=-1)` — sin repetición, orden lexicográfico. `r=-1` usa `longitud(xs)`.
- `combinaciones(xs, r)` — sin repetición; `r=0 → [[]]`, `r>n → []`.
- `combinaciones_con_repeticion(xs, r)` — multiconjuntos.

**Iteración**:
- `concatenar(xs, ys)` — encadena dos iterables (nombre distinto de `cadena` para no sombrear el built-in de conversión a texto).
- `repetir(valor, n)` — lista con `valor` repetido `n` veces.
- `ventana(xs, n)` — ventanas deslizantes de tamaño `n`. `n > longitud(xs) → []`. `n <= 0` lanza `ErrorDeValor`.
- `pares_consecutivos(xs)` — atajo para `ventana(xs, 2)`.
- `agrupar_consecutivos(xs)` — `[[clave, sub-lista], ...]` por igualdad de adyacentes. Base de run-length encoding.
- `comprimir(xs, selectores)` — filtra `xs[i]` cuando `selectores[i]` es verdadero, hasta agotar la más corta.
- `dividir_en(xs, n)` — particiones consecutivas de tamaño `n`; la última puede ser más corta.

### `grafos` (v1.119)

Grafos dirigidos / no dirigidos con pesos + algoritmos clásicos. Pure-Cornamusa sobre `coleccion.Cola`/`Pila` y `dict` de adyacencia.

**Clase `Grafo(dirigido=verdadero)`** — los nodos pueden ser cualquier valor hashable:
- `agregar_nodo(n)`, `agregar_arista(u, v, peso=1)`, `quitar_arista(u, v)`.
- `nodos()`, `aristas()` (lista de `[u, v, peso]`), `vecinos(n)`, `peso(u, v)` (devuelve `nulo` si no existe), `contiene(n)`, `__longitud__`, `__cadena__`.

En grafos no dirigidos, agregar `u→v` crea automáticamente `v→u` con el mismo peso, y `aristas()` no duplica.

**Algoritmos**:
- `bfs(g, inicio)` → lista en orden de visita (`coleccion.Cola`).
- `dfs(g, inicio)` → lista en preorden iterativo (`coleccion.Pila`).
- `dijkstra(g, inicio)` → `dict` nodo→distancia. Solo nodos alcanzables aparecen. Lanza `ErrorDeValor` con peso negativo. O((V+E) log V) usando `coleccion.Heap(clave=...)` con lazy deletion (desde v1.120).
- `camino_mas_corto(g, inicio, finn)` → lista `[inicio, ..., finn]` o `[]`.
- `componentes(g)` → lista de listas. En dirigidos calcula débilmente conexas.
- `topologico(g)` → orden topológico por Kahn. Lanza si hay ciclo o si el grafo no es dirigido.
- `tiene_ciclo(g)` → booleano.

### `inspeccion` (v1.91)

Introspección y reflexión sobre instancias, clases y módulos. Útil para serializadores genéricos, REPL helpers y debugging.

- `obtener_clase(inst)` → la clase (VAL_CLASE) o `nulo` si no es instancia.
- `obtener_nombre(clase_o_inst)` → cadena con el nombre (e.g. `"Persona"` para instancias).
- `listar_metodos(clase_o_inst)` → lista de nombres de métodos (incluye heredados).
- `listar_atributos(inst)` → lista de atributos propios de la instancia.
- `es_callable(x)` / `es_clase(x)` / `es_instancia(x)` / `es_modulo(x)` → booleanos.
- `describir(x)` → dict con `tipo`, `clase`/`nombre`, `metodos`, `atributos`, `repr`.

Las nativas directas (`clase_de`, `nombre_clase`, `metodos_de`, `atributos_de`) están disponibles globalmente sin `importar`.

### `validacion` (v1.92)

Validadores comunes para formularios y entradas de usuario. Todos devuelven booleano (no lanzan) y aceptan tipos no-cadena como `falso` silencioso.

- **Formatos**: `es_email(s)`, `es_url(s)`, `es_fecha_iso(s)` (YYYY-MM-DD), `es_telefono(s)`.
- **Rangos**: `en_rango(n, lo, hi)` (cerrado), `en_rango_abierto(n, lo, hi)`.
- **Cadenas**: `longitud_en_rango(s, min, max)`, `no_vacia(s)` (true si tras `recortar` queda no-vacía).
- **Genéricos**: `coincide(s, patron_regex)`, `en_conjunto(x, valores)`.
- **`Validador()`**: clase que acumula errores. Métodos: `verificar(campo, cond, msg)`, `valido()`, `tiene_errores()`, `resumen()`.

### `argumentos` (v1.93)

Parser de argumentos CLI estilo `argparse`, pure-Cornamusa sobre `sistema.argv`. Soporta argumentos posicionales, opciones con valor (`--max 10` / `-m 10`) y banderas booleanas (`--verboso` / `-v`). Errores son `ErrorDeValor` atrapables.

```cornamusa
importar argumentos
importar sistema

p = argumentos.Parser("mi-script", "Descripcion")
p.posicional(nombre, ayuda, tipo, defecto)     # tipo/defecto nulo: cadena/obligatorio
p.opcion(largo, corto, ayuda, tipo, defecto)   # ej: "--max", "-m", ...
p.bandera(largo, corto, ayuda)                 # booleana sin valor

args = p.parsear(sistema.argv)
# args es un dict con keys = nombres posicionales + flags largas
```

- **Tipos**: `"cadena"`, `"entero"`, `"decimal"`, `"booleano"` (acepta `verdadero/true/1/si` y `falso/false/0/no`).
- **`--ayuda` / `-h`** se inyectan automáticamente: imprimen el texto de ayuda generado y llaman a `salir(0)`.
- **`p.ayuda()`** devuelve el texto generado (uso, descripción, posicionales, opciones, banderas).
- Errores típicos atrapables: opción desconocida, opción sin valor, tipo inválido, posicional obligatorio ausente.

### `ruta` (v1.94)

Manipulación lexicográfica de rutas al estilo `pathlib.PurePath` de Python. Pure-Cornamusa: no toca el sistema de archivos (excepto `existe()`, que delega a `archivos.existe`). Separador canónico `/`, acepta también `\` en entrada y lo normaliza.

**API funcional (sin instanciar)**:

- `ruta.nombre(s)` → último componente (`"/a/b.txt"` → `"b.txt"`).
- `ruta.tronco(s)` → nombre sin extensión.
- `ruta.extension(s)` → sufijo desde el último `.` (incluye el punto), `""` si no hay.
- `ruta.padre(s)` → la ruta sin el último componente.
- `ruta.partes(s)` → lista de componentes (primer elemento `"/"` si absoluta).
- `ruta.es_absoluta(s)` → `verdadero` si empieza por `/`, `\` o letra de unidad Windows (`C:`).
- `ruta.unir_partes(lista)` → concatena con `/`, una absoluta intermedia reinicia.
- `ruta.normalizar(s)` → resuelve `.` y `..` lexicográficamente; ruta vacía → `"."`.

**Clase `Ruta`**: envoltorio OO con métodos `.nombre()`, `.tronco()`, `.extension()`, `.padre()` (devuelve Ruta), `.partes()`, `.absoluta()`, `.vacia()`, `.unir(otro)` (acepta cadena o Ruta), `.con_nombre(nuevo)`, `.con_extension(nueva)`, `.normalizada()`, `.cadena()`, `.existe()`. Igualdad por valor (`a == b` si tienen la misma cadena interna). `__cadena__` integrado (se imprime como su ruta).

```cornamusa
importar ruta
r = ruta.Ruta("/home/david/notas.txt")
imprimir(r.nombre())                     # "notas.txt"
imprimir(r.con_extension(".md").cadena())  # "/home/david/notas.md"
sub = ruta.Ruta("/etc").unir("nginx").unir("conf.d")
imprimir(sub.cadena())                   # "/etc/nginx/conf.d"
```

**v1.97**: `existe()` ahora cubre archivos Y directorios. Métodos nuevos:
- `r.es_archivo()` / `r.es_directorio()` — distinguen tipo.
- `r.listar()` → lista de cadenas con nombres de las entradas inmediatas.
- `r.listar_rutas()` → lista de `Ruta` (ya unidas con `r`); útil para encadenar (`.extension()`, `.es_directorio()`).
- `ruta.cwd()` (módulo) → `Ruta` del directorio actual.

Wrappers correspondientes en `stdlib/archivos`: `es_directorio(ruta)`, `listar(ruta)`, `directorio_actual()`, `crear_directorio(ruta)`. Built-ins: `archivo_es_directorio`, `directorio_listar`, `obtener_cwd`, `directorio_crear` (todos atrapan errores como `ErrorDeIO`).

**v1.99**: borrado e info.
- `r.eliminar()` → quita archivo. Lanza `ErrorDeIO` si la ruta es un directorio o no existe (usar `borrar` es palabra reservada del lenguaje, de ahí `eliminar`).
- `r.eliminar_directorio()` → quita directorio **vacío**. No es `rm -rf`. Lanza `ErrorDeIO` si no vacío.
- `r.info()` → dict `{tamano, mtime_epoch_ms, es_archivo, es_directorio}`. Lanza `ErrorDeIO` si no existe.
- `r.tamano()` / `r.mtime_ms()` → atajos a las claves de `info`.

Wrappers en `stdlib/archivos`: `eliminar(ruta)`, `eliminar_directorio(ruta)`, `info(ruta)`. Built-ins: `archivo_borrar`, `directorio_borrar`, `archivo_info`. `mtime_epoch_ms` tiene precisión por-segundo en Windows y sub-segundo en POSIX.

**v1.102**: borrado recursivo y mkdir -p.
- `r.eliminar_arbol()` → borra el árbol entero (rm -rf). Si la ruta es un archivo, fallback a `eliminar()`. Lanza `ErrorDeValor` si la ruta es vacía o un separador raíz (`""`, `"/"`, `"\\"`) — guardrail anti-accidente. Lanza `ErrorDeIO` si no existe.
- `r.crear_arbol()` → crea el directorio creando todos los padres (mkdir -p). **Idempotente**: si la ruta ya existe como directorio, no falla. Acepta separadores `/` y `\` mezclados.

Wrappers en `stdlib/archivos`: `eliminar_arbol(ruta)`, `crear_arbol(ruta)`. Pure-Cornamusa sobre las nativas de v1.97/v1.99 — no introducen código C nuevo.

**v1.105**: copia.
- `r.copiar(destino)` → copia un archivo regular. `destino` puede ser cadena o `Ruta`. Devuelve `Ruta(destino)` para encadenar. Lanza `ErrorDeIO` si `r` es un directorio o no existe.
- `r.copiar_arbol(destino)` → copia recursivamente el árbol. Si `r` es un archivo, fallback a `copiar`. Crea el destino (y sus padres) si no existe — mkdir -p implícito.

Wrappers en `stdlib/archivos`: `copiar(origen, destino)` (delega a built-in `archivo_copiar`), `copiar_arbol(origen, destino)` (recursivo pure-Cornamusa). `archivo_copiar` lee/escribe con buffer de 64 KiB; no preserva mtime ni permisos (pendiente futura).

**v1.111**: mover y modificar mtime.
- `r.mover(destino)` → renombra/mueve el archivo. Atómico en mismo FS (`rename` POSIX / `MoveFileExA` Windows). Sobrescribe destino si existe. Devuelve `Ruta(destino)` para encadenar. Acepta cadena o `Ruta`.
- `r.tocar()` → actualiza mtime al instante actual (como `touch` de Unix). NO crea el archivo si no existe.
- `r.set_mtime(mtime_ms)` → establece mtime explícito en milisegundos UNIX epoch. Útil para preservar mtime al copiar o restaurar backups.

Wrappers en `stdlib/archivos`: `mover(origen, destino)`, `tocar(ruta)`, `set_mtime(ruta, ms)`. Built-ins: `archivo_mover` (`rename`/`MoveFileExA` con `REPLACE_EXISTING`), `archivo_set_mtime` (`utimes` POSIX / `SetFileTime` Windows con conversión UNIX → Windows epoch).

**v1.100**: glob recursivo.
- `ruta.recorrer(directorio)` → lista de `Ruta` con todas las entradas alcanzables desde `directorio` recursivamente (DFS, incluye archivos y directorios).
- `ruta.encontrar(directorio, patron)` → como `recorrer` filtrado por glob sobre el **nombre** (no la ruta completa).
- `r.recorrer()` / `r.encontrar(patron)` — métodos equivalentes sobre instancia.
- `r.coincide(patron)` → verdadero si el nombre de `r` matchea `patron` glob.

Matcher glob soporta `*` (cero o más caracteres) y `?` (uno). No soporta `**` recursivo, clases `[abc]`, ni alternancias — para eso usar `stdlib/regex`. Implementación iterativa O(n·m) con backtracking. Si la ruta no es directorio o no es accesible, `recorrer`/`encontrar` retornan lista vacía silenciosamente.

### `pruebas` (v1.96)

Framework de testing minimalista pure-Cornamusa. Útil para scripts de comprobación lineales (asserts standalone) o tests organizados (clase `Suite`).

**Asserts standalone** — todos lanzan `ErrorDeValor` con mensaje claro en caso de fallo:

- `aseverar(cond, msg)` — comprueba que `cond` sea verdadero.
- `aseverar_igual(actual, esperado)` — `actual == esperado`.
- `aseverar_distinto(a, b)` — `a != b`.
- `aseverar_verdadero(c)` / `aseverar_falso(c)`.
- `aseverar_nulo(v)` / `aseverar_no_nulo(v)`.
- `aseverar_aproximado(a, b, tolerancia)` — `|a-b| ≤ tolerancia` (default `1e-9` si `nulo`).
- `aseverar_contiene(coleccion, x)` / `aseverar_no_contiene(coleccion, x)`.
- `aseverar_lanza(callable, nombre_excepcion)` — `callable()` debe lanzar; si `nombre_excepcion` es cadena (p.ej. `"ErrorDeValor"`), exige que aparezca en `repr` de la excepción; si es `nulo`, basta cualquiera.

**Clase `Suite`** — acumula casos nombrados, los ejecuta y reporta:

```cornamusa
funcion test_suma():
    pruebas.aseverar_igual(2 + 2, 4)
fin funcion

s = pruebas.Suite("aritmetica")
s.caso("suma simple", test_suma)
r = s.ejecutar()
# r = {"total": 1, "pasados": 1, "fallados": 0, "fallos": []}
```

Cada caso se imprime con `[OK]` o `[FAIL]` y un resumen final `Total: N | Pasados: P | Fallados: F`.

**Wrapper funcional**: `pruebas.ejecutar_casos([[etiqueta1, fn1], [etiqueta2, fn2], ...])` para evitar instanciar `Suite` cuando no se necesita reutilizar.

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

cornamusa prof [--top=N] prog.cor     # profiler determinista (v1.71)
                                       # tabla por funcion: calls/total/self/per-call

cornamusa cov [--uncovered] prog.cor  # coverage tracker (v1.75)
                                       # reporta % lineas top-level cubiertas

cornamusa depurar prog.cor            # depurador interactivo (v1.76)
                                       # comandos en prompt (dep):
                                       #   c continuar, s paso, n siguiente, r retornar
                                       #   b N break, bd N borrar, bs listar
                                       #   p NOMBRE imprimir global, pila stack
                                       #   l listar, q salir, ? ayuda

cornamusa nuevo <nombre>              # scaffold de proyecto (v1.98)
                                       # crea <nombre>/ con:
                                       #   main.cor (Hola mundo)
                                       #   tests/test_main.cor (stdlib pruebas)
                                       #   README.md + .gitignore
                                       # Falla si <nombre> ya existe.
```

Categorias del linter (16 totales): `unreachable`, `redundant-pasar`, `eq-nulo`, `unused-import`, `unused-local`, `unused-param`, `shadow`, `unused-loop-var`, `mutable-default`, `concat-in-loop`, `same-comparison`, `empty-except`, `redundant-bool-compare`, `useless-return`, `bool-coerce-conditional`, `for-rango-longitud`.

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
