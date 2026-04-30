# Referencia rápida de Cornamusa

> Cheatsheet de sintaxis + tablas de built-ins, stdlib y errores. Para una explicación pedagógica usa el [tutorial](tutorial.md). Para la especificación formal del lenguaje, [ESPEC.md](../ESPEC.md).

**Versión:** 0.11.5

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
```

One-liner: si tras `:` viene una sola sentencia en la misma línea, no requiere `fin`:

```cornamusa
si x > 0: imprimir(x)
```

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
+7          # identidad
```

### Comparación

```cornamusa
a == b      # igualdad estructural
a != b      # desigualdad
a < b       # menor
a <= b      # menor o igual
a > b       # mayor
a >= b      # mayor o igual
a es b      # identidad (mismo objeto)
a es no b   # identidad negada
a en lista  # pertenencia
a no en l   # pertenencia negada
```

### Lógicos (palabras, no símbolos)

```cornamusa
verdadero y falso     # falso
verdadero o falso     # verdadero
no verdadero          # falso
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
| `diccionario` | `{"clave": "valor"}` | sí |
| `conjunto` | `{1, 2, 3}` (no vacío) o `conjunto()` (vacío) | sí |

> Los **enteros son de precisión arbitraria**: `2 ** 1000` es válido. Internamente se distingue SMALL (≤ 63 bits, inline) y BIG (mp_int), invisible al programa.

### Verdadez (truthy/falsy)

**Falsos**: `falso`, `nulo`, `0`, `0.0`, `""`, `[]`, `()`, `{}` (dicc vacío), `conjunto()`.
**Verdaderos**: todo lo demás.

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

`iterable` puede ser: lista, tupla, conjunto, diccionario (itera claves), cadena (itera caracteres), rango.

### `romper` y `continuar`

```cornamusa
para i en rango(100):
    si i % 2 == 0: continuar
    si i > 20: romper
    imprimir(i)
fin para
```

### `pasar`

Sentencia vacía:

```cornamusa
funcion no_implementado():
    pasar
fin funcion
```

---

## 5. Funciones

### Sintaxis

```cornamusa
funcion nombre(p1, p2, p3=valor_por_defecto):
    ...
    retornar resultado
fin funcion
```

### Lambda

```cornamusa
cuadrado = lambda x: x * x
suma = lambda a, b: a + b
```

### Closures

Lectura de variables del scope enclosing:

```cornamusa
funcion crear_saludador(saludo):
    funcion saludar(nombre):
        retornar saludo + ", " + nombre
    fin funcion
    retornar saludar
fin funcion
```

> Escritura a upvalues (`nolocal`) reservada para v1.x.

---

## 6. Estructuras de datos

### Listas

```cornamusa
xs = [1, 2, 3]
xs[0]              # 1
xs[-1]             # 3 (último)
xs[1:3]            # [2, 3] (slice)
xs[::-1]           # [3, 2, 1] (invertida)
longitud(xs)       # 3
xs[0] = 10         # mutación
agregar(xs, 99)    # añadir
quitar(xs, 0)      # quitar por índice
insertar(xs, 0, 7) # insertar en posición
ordenar(xs)        # in-place
invertir(xs)       # in-place
```

### Diccionarios

```cornamusa
d = {"a": 1, "b": 2}
d["a"]             # 1
d["c"] = 3         # añadir
"a" en d           # verdadero/falso
claves(d)          # ["a", "b", "c"]
valores(d)         # [1, 2, 3]
longitud(d)        # cantidad de pares
para clave en claves(d):
    imprimir(clave, "->", d[clave])
fin para
```

> En v0.11.5 no hay built-in para borrar una clave del dict. Reservado para v1.x (probablemente vía `borrar d[clave]`).

### Conjuntos

```cornamusa
s = {1, 2, 3}
agregar(s, 4)
"x" en s
longitud(s)
conjunto()         # vacío (no {} que es dict vacío)
```

> `quitar` solo opera sobre listas en v0.11.5. Quitar de conjuntos también queda para v1.x.

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

---

## 7. Cadenas

### Operaciones básicas

```cornamusa
"hola" + " " + "mundo"     # concatenación
"=" * 30                   # repetición
"orna" en "Cornamusa"      # pertenencia
longitud("Cornamusa")      # 9
"Cornamusa"[0]             # "C"
"Cornamusa"[-1]            # "a"
```

### Escape

```
\n   nueva línea
\t   tabulador
\r   retorno de carro
\\   barra invertida
\'   comilla simple
\"   comilla doble
\0   null
\xHH       byte hex (00-FF)
\uHHHH     codepoint Unicode (4 dígitos)
\u{HHHHHH} codepoint Unicode (cualquier ancho)
```

### Prefijos

```cornamusa
f"hola {nombre}"     # interpolación (parser lo acepta; ejecución v1.x)
r"C:\ruta\a"         # raw, sin escapes
b"\x00\xff"          # bytes (reservado v1.x)
```

---

## 8. Clases

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

> En v0.11.5 solo `__iniciar__` se invoca automáticamente. Los demás dunders (`__sumar__`, `__cadena__`, etc.) son métodos ordinarios, no se ejecutan al usar operadores. Ver §13.

---

## 9. Excepciones

### Lanzar

```cornamusa
lanzar ErrorDeValor("mensaje")
```

### Atrapar

```cornamusa
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
| `Excepcion` | Base de jerarquía |
| `ErrorAritmetico` | División por cero, overflow lógico |
| `ErrorDeTipo` | Operación sobre tipo incorrecto |
| `ErrorDeValor` | Valor del tipo correcto pero inválido |
| `ErrorDeIndice` | Índice fuera de rango |
| `ErrorDeClave` | Clave no presente en dict |
| `ErrorDeNombre` | Identificador no definido |

---

## 10. Módulos

```cornamusa
importar matematicas                       # global `matematicas`
importar matematicas como mat              # alias
desde matematicas importar PI, factorial   # selectivo
desde matematicas importar factorial como fact
importar paquete.submodulo                 # subsegmentos
```

> `desde X importar *` no soportado.

---

## 11. Built-ins (v0.11.5)

22 funciones y constructores registrados como globales.

### E/S y conversión

| Firma | Descripción |
|---|---|
| `imprimir(*args)` | Imprime args separados por espacio + `\n` |
| `tipo(x)` | Cadena con el nombre del tipo |

### Tamaño e iteración

| Firma | Descripción |
|---|---|
| `longitud(x)` | Tamaño de cadena/lista/dicc/conjunto/tupla/rango |
| `rango(fin)` / `rango(inicio, fin)` / `rango(inicio, fin, paso)` | Iterador entero perezoso |

### Mutación de colecciones

| Firma | Descripción |
|---|---|
| `agregar(coleccion, x)` | Añade `x` al final (lista) o al conjunto |
| `quitar(lista, indice?)` | Quita por índice (lista). Sin índice: quita el último. Devuelve el valor quitado. **Solo listas en v0.11.5**. |
| `insertar(lista, i, x)` | Inserta `x` en posición `i`, desplaza |
| `invertir(lista)` | In-place |
| `ordenar(lista, invertido=falso)` | In-place; `invertido=verdadero` para descendente |
| `claves(dicc)` | Lista de claves |
| `valores(dicc)` | Lista de valores |
| `conjunto(iter?)` | `conjunto()` vacío, o desde iterable |

### Excepciones (constructores)

| Constructor | Argumento |
|---|---|
| `Excepcion(msg)` | Excepción base |
| `ErrorAritmetico(msg)` | |
| `ErrorDeTipo(msg)` | |
| `ErrorDeValor(msg)` | |
| `ErrorDeIndice(msg)` | |
| `ErrorDeClave(msg)` | |
| `ErrorDeNombre(msg)` | |

### Sistema y memoria

| Firma | Descripción |
|---|---|
| `recolectar()` | Fuerza pasada de GC |
| `obtener_argv()` | Lista de cadenas con args del programa (también `sistema.argv`) |
| `salir(codigo=0)` | Termina el proceso. No retorna |

### No registrados todavía (v1.x)

`leer`, `enumerar`, `mapear`, `filtrar`, `reducir`, `suma`, `mínimo`/`minimo`, `máximo`/`maximo`, `absoluto`, `redondear`, `cadena`, `entero`, `decimal`, `booleano`, `lista`, `tupla`, `diccionario`, `abrir`, `iterar`, `siguiente`, `instancia_de`, `subclase_de`, `id`, `resumen`, `repr`.

---

## 12. Biblioteca estándar (`stdlib/`)

### `matematicas`

Constantes y funciones puras escritas en Cornamusa.

| Símbolo | Descripción |
|---|---|
| `matematicas.PI` | 3.141592653589793 |
| `matematicas.E` | 2.718281828459045 |
| `matematicas.cuadrado(n)` | `n * n` |
| `matematicas.cubo(n)` | `n * n * n` |
| `matematicas.absoluto(n)` | `\|n\|` |
| `matematicas.maximo(a, b)` | el mayor |
| `matematicas.minimo(a, b)` | el menor |
| `matematicas.signo(n)` | `-1`, `0` o `1` |
| `matematicas.factorial(n)` | recursivo, soporta bignum (`factorial(100)` OK) |
| `matematicas.suma_rango(a, b)` | suma de los enteros en `[a, b)` |
| `matematicas.es_par(n)` | booleano |
| `matematicas.es_impar(n)` | booleano |
| `matematicas.mcd(a, b)` | máximo común divisor (Euclides) |

### `cadenas`

Operaciones sobre texto que requieren indexación UTF-8.

| Función | Descripción |
|---|---|
| `cadenas.repetir(s, n)` | `s` repetido `n` veces |
| `cadenas.empieza_con(s, prefijo)` | Booleano |
| `cadenas.termina_con(s, sufijo)` | Booleano |
| `cadenas.contar(s, sub)` | Ocurrencias no-solapadas |
| `cadenas.caracter(s, i)` | Equivalente a `s[i]` (un carácter) |

### `sistema`

| Símbolo | Descripción |
|---|---|
| `sistema.argv` | Lista de cadenas con argumentos del programa |

`salir(codigo)` también disponible globalmente, sin importar.

---

## 13. Reservas para v1.x

Sintaxis aceptada por el parser pero no implementada (forward-compatibility):

| Característica | Estado |
|---|---|
| F-strings con expresiones (`f"hola {x+1}"`) | parser OK; runtime: ignora interpolación |
| `con` (context managers) | reservada como keyword |
| `coincidir` (pattern matching) | reservada como keyword |
| `borrar` (`del` de Python) | reservada |
| `producir` (generadores) | reservada |
| `asincrono` / `esperar` (async/await) | reservadas; v2.0+ |
| `nolocal` (escritura a upvalue) | reservada |
| Anotaciones de tipo (`p: tipo`) | parser OK; runtime ignora |
| Decoradores (`@deco`) | parser OK; runtime parcial |
| Dunders (`__sumar__`, `__cadena__`, ...) | parser OK; en v0.11.5 solo `__iniciar__` se invoca automáticamente |

---

## 14. CLI

```bash
cornamusa programa.cor                # ejecuta con tree-walking (compatibilidad histórica)
cornamusa --bytecode programa.cor     # ejecuta con VM bytecode (recomendado, 3x más rápido)
cornamusa --tokens programa.cor       # vuelca tokens del lexer (debug)
cornamusa --ast programa.cor          # vuelca AST en S-expression (debug)
cornamusa -v / --version              # versión
cornamusa -h / --ayuda                # ayuda
cornamusa                             # REPL interactivo
cornamusa --bytecode prog.cor a b c   # pasa "a", "b", "c" a sistema.argv del programa
```

---

## 15. Errores comunes

### `ErrorDeNombre: nombre 'x' no esta definido`

Variable usada sin asignar. Verifica typos. Nota: `if`, `def`, `class` etc. son palabras inglesas — Cornamusa usa `si`, `funcion`, `clase`.

### `ErrorDeTipo: '<tipo>' no soporta el operador '+'`

Operación entre tipos incompatibles. Cornamusa **no** auto-convierte cadenas a número:

```cornamusa
"5" + 3       # ✗ ErrorDeTipo
"5" + "3"     # ✓ "53"
5 + 3         # ✓ 8
```

### `ErrorDeIndice: indice X fuera de rango`

Lista/tupla accedida con índice inválido:

```cornamusa
xs = [1, 2, 3]
xs[5]         # ✗ ErrorDeIndice
xs[-4]        # ✗ ErrorDeIndice
```

### `ErrorDeClave: 'X'`

Diccionario sin esa clave:

```cornamusa
d = {"a": 1}
d["b"]        # ✗ ErrorDeClave: "b"
"b" en d      # forma idiomática para chequear primero
```

### `ErrorDeSintaxis: se esperaba ':' tras la cabecera de la clase`

Olvidaste el `:` que abre el bloque o pusiste otra cosa donde Cornamusa esperaba `:`.

### `ErrorDeSintaxis: se esperaba un nombre tras 'funcion'`

Pasos comunes:
- Olvidaste el nombre de la función.
- Usaste un nombre con caracteres reservados (`y`, `o`, `no`, `si`, `en`, `es`).

> ¡Cuidado con `y`! Es la conjunción lógica AND, **no** un identificador válido. Causa errores confusos en parámetros como `def f(x, y):` (en castellano `funcion f(x, y)` no compila — usa `funcion f(x, b)` o similar).

### `desbordamiento de pila de llamadas (>VM_FRAMES_MAX frames)`

Recursión infinita. Por defecto VM_FRAMES_MAX ≈ 1024.

---

## 16. Recursos

- **[Tutorial paso a paso](tutorial.md)** — para aprender desde cero.
- **[Especificación formal](../ESPEC.md)** — sintaxis EBNF y semántica completa.
- **[Decisiones arquitectónicas](../decisiones/)** — `B1` a `B10`, razonamiento detrás de las elecciones.
- **[Ejemplos](../examples/)** — 23+ programas demostrando features.
- **[Issues y discusión](https://github.com/David-Castilla-Gomez/Cornamusa/issues)** — bugs, ideas, preguntas.
