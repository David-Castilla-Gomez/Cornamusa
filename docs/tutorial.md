# Tutorial de Cornamusa

> Aprende Cornamusa desde cero. Cada sección incluye código ejecutable que puedes copiar al intérprete.

Cornamusa es un lenguaje de programación dinámico, interpretado y **enteramente en castellano**. Si has programado antes en Python, Java o JavaScript, te resultará familiar; si nunca has programado, este tutorial te lleva paso a paso.

Para ejecutar los ejemplos:

```bash
./build/cornamusa --bytecode programa.cor
```

O abre el REPL interactivo:

```bash
./build/cornamusa
```

---

## 1. Tu primer programa

Crea un archivo `hola.cor` con:

```cornamusa
imprimir("¡Hola, mundo!")
```

Y ejecútalo:

```
$ ./build/cornamusa --bytecode hola.cor
¡Hola, mundo!
```

`imprimir` es una **función** que escribe en pantalla. El paréntesis `()` invoca la función con un **argumento** — la cadena `"¡Hola, mundo!"`.

`imprimir` acepta varios argumentos separados por coma. Los une con un espacio:

```cornamusa
imprimir("Hola,", "mundo")
imprimir("La respuesta es:", 42)
```

```
Hola, mundo
La respuesta es: 42
```

---

## 2. Variables y tipos

Una **variable** es un nombre que apunta a un valor. Se asigna con `=`:

```cornamusa
nombre = "Ana"
edad = 30
altura = 1.65
mayor_de_edad = verdadero

imprimir(nombre, "tiene", edad, "años")
```

Cornamusa es de **tipado dinámico**: las variables no tienen tipo, los valores sí. Puedes reasignar a un tipo distinto:

```cornamusa
x = 42
imprimir(tipo(x))     # entero
x = "ahora soy una cadena"
imprimir(tipo(x))     # cadena
```

### Tipos básicos

| Tipo | Ejemplos |
|---|---|
| `entero` | `0`, `42`, `-7`, `1_000_000`, `0xff`, `0b1010` |
| `decimal` | `3.14`, `-0.5`, `1.5e-3` |
| `booleano` | `verdadero`, `falso` |
| `nulo` | `nulo` (ausencia de valor) |
| `cadena` | `"texto"`, `'también'`, `"""multilínea"""` |
| `lista` | `[1, 2, 3]` |
| `tupla` | `(1, 2, 3)` |
| `diccionario` | `{"clave": "valor"}` |
| `conjunto` | `{1, 2, 3}` |

Los enteros son de **precisión arbitraria** — no hay overflow:

```cornamusa
gigante = 2 ** 100
imprimir(gigante)    # 1267650600228229401496703205376
```

---

## 3. Operadores y expresiones

### Aritméticos

```cornamusa
imprimir(7 + 3)      # 10  (suma)
imprimir(7 - 3)      # 4   (resta)
imprimir(7 * 3)      # 21  (multiplicación)
imprimir(7 / 3)      # 2.333...  (división — siempre decimal)
imprimir(7 // 3)     # 2   (división entera)
imprimir(7 % 3)      # 1   (módulo)
imprimir(2 ** 10)    # 1024 (potencia)
```

### Comparación

```cornamusa
imprimir(5 == 5)     # verdadero
imprimir(5 != 4)     # verdadero
imprimir(5 < 10)     # verdadero
imprimir(5 >= 5)     # verdadero
```

### Lógicos (palabras, no símbolos)

```cornamusa
imprimir(verdadero y falso)   # falso  (and)
imprimir(verdadero o falso)   # verdadero (or)
imprimir(no verdadero)         # falso (not)
```

### Asignación compuesta

```cornamusa
contador = 0
contador += 1     # equivale a: contador = contador + 1
contador *= 2
imprimir(contador)   # 2
```

---

## 4. Control de flujo

### `si` / `sino si` / `sino`

Los bloques se abren con `:` y se cierran con `fin <etiqueta>`:

```cornamusa
edad = 18

si edad < 18:
    imprimir("Menor")
sino si edad < 65:
    imprimir("Adulto")
sino:
    imprimir("Mayor")
fin si
```

> **Importante**: la indentación es **estilística**, no semántica. Lo que delimita los bloques es `:` al abrir y `fin <etiqueta>` al cerrar. Recomendamos 4 espacios de indentación por convención.

Si tras `:` viene una sola sentencia en la misma línea, no hace falta `fin`:

```cornamusa
si x > 0: imprimir("positivo")
```

### `mientras`

```cornamusa
contador = 5
mientras contador > 0:
    imprimir(contador)
    contador -= 1
fin mientras
imprimir("¡Despegue!")
```

### `para`

Itera sobre cualquier valor iterable (rango, lista, cadena, diccionario, conjunto, tupla, generador):

```cornamusa
para i en rango(5):
    imprimir(i)        # 0, 1, 2, 3, 4
fin para

frutas = ["manzana", "pera", "uva"]
para fruta en frutas:
    imprimir("Me gusta la", fruta)
fin para
```

`rango` admite 1, 2 o 3 argumentos:

```cornamusa
rango(5)            # 0, 1, 2, 3, 4
rango(2, 8)         # 2, 3, 4, 5, 6, 7
rango(0, 10, 2)     # 0, 2, 4, 6, 8
rango(10, 0, -1)    # 10, 9, 8, ..., 1
```

### `romper` y `continuar`

```cornamusa
para i en rango(20):
    si i % 2 == 0:
        continuar      # salta a la siguiente iteración
    fin si
    si i > 10:
        romper         # sale del bucle
    fin si
    imprimir(i)
fin para
# imprime: 1, 3, 5, 7, 9
```

---

## 5. Destructuring (desempaquetado)

Cuando un valor es una colección, puedes desempaquetarlo en varias variables de una sola vez:

```cornamusa
a, b = (1, 2)
imprimir(a, b)        # 1 2

[a, b, c] = [10, 20, 30]
imprimir(a, b, c)     # 10 20 30
```

> Ojo: `y`, `o`, `no`, `es`, `en`, `si` son palabras reservadas — no pueden ser nombres de variable. Usa `b`, `cy`, `eje_y`... en su lugar.

El idiom más útil: **intercambiar variables sin variable temporal**:

```cornamusa
i = "izquierda"
d = "derecha"
i, d = d, i
imprimir(i, d)        # derecha izquierda
```

Funciona con anidación arbitraria:

```cornamusa
(cabeza, (op, valor)) = ("set", ("+", 42))
imprimir(cabeza, op, valor)   # set + 42
```

Es ideal para funciones que devuelven varios valores:

```cornamusa
funcion dividir(num, den):
    retornar (num // den, num % den)
fin funcion

cociente, resto = dividir(17, 5)
imprimir(cociente, "resto", resto)   # 3 resto 2
```

Y para iterar sobre pares — la variable del `para` es un solo nombre, así que destructuras dentro del cuerpo:

```cornamusa
gente = [("Ana", 30), ("Luis", 25)]
para persona en gente:
    nombre, edad = persona
    imprimir(nombre, "tiene", edad)
fin para
```

---

## 6. Funciones

```cornamusa
funcion saludar(nombre):
    retornar "¡Hola, " + nombre + "!"
fin funcion

mensaje = saludar("Mundo")
imprimir(mensaje)        # ¡Hola, Mundo!
```

Si no hay `retornar`, la función devuelve `nulo`.

### Argumentos por defecto

```cornamusa
funcion saludar(nombre, idioma="es"):
    si idioma == "es":
        retornar "¡Hola, " + nombre + "!"
    sino:
        retornar "Hello, " + nombre + "!"
    fin si
fin funcion

imprimir(saludar("Ana"))            # ¡Hola, Ana!
imprimir(saludar("Bob", "en"))      # Hello, Bob!
```

### Argumentos por nombre (keyword)

Puedes pasar argumentos por su nombre, en cualquier orden. Útil para legibilidad:

```cornamusa
funcion conectar(host, puerto=80, usar_tls=falso):
    esquema = "http"
    si usar_tls:
        esquema = "https"
    fin si
    retornar esquema + "://" + host + ":" + cadena(puerto)
fin funcion

imprimir(conectar("api.dev", usar_tls=verdadero))         # sin recordar el orden
imprimir(conectar("api.dev", puerto=443, usar_tls=verdadero))
```

### `*args` — número variable de argumentos

`*resto` recoge en una **tupla** todos los argumentos posicionales que sobren:

```cornamusa
funcion suma(*nums):
    total = 0
    para n en nums:
        total = total + n
    fin para
    retornar total
fin funcion

imprimir(suma())            # 0
imprimir(suma(1, 2, 3))     # 6
imprimir(suma(10, 20))      # 30
```

Se combina con parámetros fijos: `funcion saludar(nombre, *titulos):`.

### `**kwargs` — argumentos por nombre variables

`**kw` recoge en un **diccionario** los argumentos por nombre que no coincidan con un parámetro fijo:

```cornamusa
funcion api(host, **opciones):
    s = "GET " + host
    si "puerto" en opciones:
        s = s + ":" + cadena(opciones["puerto"])
    fin si
    retornar s
fin funcion

imprimir(api("api.dev"))                    # GET api.dev
imprimir(api("api.dev", puerto=443))        # GET api.dev:443
```

### Spread: expandir colecciones en la llamada

El reverso de `*args`/`**kwargs`: `*` expande un iterable como argumentos posicionales, `**` expande un dict como argumentos por nombre.

```cornamusa
nums = [10, 20, 30]
imprimir(suma(*nums))               # ≡ suma(10, 20, 30)

config = {"puerto": 443}
imprimir(api("api.dev", **config))  # ≡ api("api.dev", puerto=443)
```

Esto hace trivial escribir *wrappers* genéricos (funciones que envuelven a otras):

```cornamusa
funcion con_log(f, *args, **kw):
    imprimir("-> llamando con", longitud(args), "argumentos")
    retornar f(*args, **kw)
fin funcion
```

Desde v1.46 puedes combinar `*args`, kwargs explícitos y `**dict` en la misma llamada — el patrón clásico del wrapper genérico funciona sin restricciones.

### Lambda

Función anónima de una sola expresión:

```cornamusa
cuadrado = lambda x: x * x
imprimir(cuadrado(7))    # 49
```

### Closures y `nolocal`

Una función definida dentro de otra captura las variables del scope que la envuelve. Por defecto puede **leerlas**:

```cornamusa
funcion crear_saludador(saludo):
    funcion saludar(nombre):
        retornar saludo + ", " + nombre
    fin funcion
    retornar saludar
fin funcion

hola = crear_saludador("Hola")
imprimir(hola("Ana"))      # Hola, Ana
```

Para **escribir** en una variable del scope enclosing, decláralа con `nolocal`. Así se construyen contadores y acumuladores con estado:

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

---

## 7. Estructuras de datos

### Listas

Secuencia mutable indexada:

```cornamusa
xs = [10, 20, 30, 40, 50]
imprimir(xs[0])      # 10 (primer elemento)
imprimir(xs[-1])     # 50 (último, índice negativo)
imprimir(longitud(xs))  # 5

xs[0] = 99           # modificación por índice
agregar(xs, 60)      # añade al final
quitar(xs, 0)        # quita por índice, devuelve el valor
insertar(xs, 0, 10)  # inserta en posición 0
invertir(xs)         # invierte in-place
ordenar(xs)          # ordena in-place
```

### Slicing (rebanadas)

```cornamusa
xs = [10, 20, 30, 40, 50]
imprimir(xs[1:4])      # [20, 30, 40]
imprimir(xs[::2])      # [10, 30, 50] — paso 2
imprimir(xs[::-1])     # [50, 40, 30, 20, 10] — invertida
imprimir(xs[:3])       # [10, 20, 30]
imprimir(xs[2:])       # [30, 40, 50]
```

### Diccionarios

Mapa clave→valor mutable. **Preserva el orden de inserción**:

```cornamusa
persona = {"nombre": "Ana", "edad": 30, "ciudad": "Madrid"}

imprimir(persona["nombre"])   # Ana
persona["edad"] = 31          # modificación
persona["email"] = "ana@..."  # añadir clave nueva
quitar(persona, "ciudad")     # borrar una clave

para clave en claves(persona):
    imprimir(clave, "->", persona[clave])
fin para
```

### Conjuntos

Sin elementos repetidos, sin orden:

```cornamusa
s = {1, 2, 3, 2, 1}
imprimir(s)             # {1, 2, 3} — sin duplicados
imprimir(longitud(s))   # 3

vacio = conjunto()      # {} es un diccionario, no conjunto vacío
agregar(vacio, "a")
quitar(vacio, "a")
```

### Tuplas

Como listas pero **inmutables** — útiles para datos que no deben cambiar:

```cornamusa
punto = (3, 4)
imprimir(punto[0])   # 3
# punto[0] = 10  → ✗ ErrorDeTipo: las tuplas no se modifican
```

---

## 8. Comprehensions

Una **comprehension** construye una colección en una sola expresión, en lugar de un bucle con `agregar`. Compáralo:

```cornamusa
# La forma larga:
dobles = []
para n en rango(10):
    agregar(dobles, n * 2)
fin para

# La comprehension equivalente:
dobles = [n * 2 para n en rango(10)]
```

Puedes añadir un filtro con `si`:

```cornamusa
pares = [n para n en rango(20) si n % 2 == 0]
largas = [w para w en palabras si longitud(w) > 4]
```

Y construir diccionarios o conjuntos con la misma sintaxis:

```cornamusa
cuadrados = {n: n * n para n en rango(1, 6)}      # dict
iniciales = {w[0] para w en ["alfa", "beta"]}     # set (deduplica)
```

Entre **paréntesis** la comprehension es una *generator expression*: perezosa, no calcula nada hasta que la recorres (ver §11):

```cornamusa
perezosa = (n * n para n en rango(1, 1000000))    # instantáneo, nada se calcula aún
```

---

## 9. Cadenas

```cornamusa
s = "Cornamusa"
imprimir(longitud(s))    # 9
imprimir(s[0])           # C
imprimir(s[-1])          # a (último)
imprimir(s[0:4])         # Corn (slicing)

saludo = "Hola, " + "mundo"   # concatenación
linea = "=" * 30              # repetición
imprimir("orna" en s)         # verdadero (pertenencia)
```

### F-cadenas con interpolación

Para construir cadenas con valores embebidos sin concatenar manualmente, usa el prefijo `f`:

```cornamusa
nombre = "Ana"
edad = 30
imprimir(f"Hola {nombre}, tienes {edad} años")
# → Hola Ana, tienes 30 años

# Cualquier expresión vale dentro de las llaves
imprimir(f"el doble de {edad} es {edad * 2}")

# Llaves literales con `{{` y `}}`
imprimir(f"{{literal}}")           # → {literal}
```

### Conversores de tipo

```cornamusa
imprimir(cadena(42))            # "42"
imprimir(entero("42"))          # 42
imprimir(entero(3.9))           # 3 (trunca hacia cero)
imprimir(decimal("3.14"))       # 3.14
imprimir(booleano([]))          # falso (lista vacía es falsy)
imprimir(lista("abc"))          # ["a", "b", "c"]
imprimir(tupla(rango(3)))       # (0, 1, 2)
```

### Entrada interactiva con `leer()`

```cornamusa
nombre = leer("Tu nombre: ")
edad = entero(leer("Edad: "))
imprimir(f"Hola {nombre}, en 10 años tendrás {edad + 10}")
```

`leer()` sin argumentos lee una línea silenciosa; con un prompt cadena lo imprime sin newline antes de leer.

### El módulo `cadenas`

Para operaciones más avanzadas, importa el módulo `cadenas`:

```cornamusa
importar cadenas

imprimir(cadenas.mayusculas_ascii("hola"))      # HOLA
imprimir(cadenas.separar("a,b,c", ","))         # ["a", "b", "c"]
imprimir(cadenas.unir(["a", "b", "c"], "-"))    # a-b-c
imprimir(cadenas.recortar("  hola  "))          # "hola"
imprimir(cadenas.reemplazar("foo bar", "o", "0"))  # f00 bar
```

---

## 10. Clases y objetos

```cornamusa
clase Persona:
    funcion __iniciar__(yo, nombre, edad):
        yo.nombre = nombre
        yo.edad = edad
    fin funcion

    funcion saludar(yo):
        retornar "Hola, soy " + yo.nombre
    fin funcion

    funcion cumplir_anos(yo):
        yo.edad = yo.edad + 1
    fin funcion
fin clase

ana = Persona("Ana", 30)
imprimir(ana.saludar())     # Hola, soy Ana
ana.cumplir_anos()
imprimir(ana.edad)          # 31
```

`__iniciar__` es el constructor. El primer parámetro de cada método se llama `yo` por convención (equivale a `self`).

### Herencia

```cornamusa
clase Animal:
    funcion __iniciar__(yo, nombre):
        yo.nombre = nombre
    fin funcion

    funcion hablar(yo):
        retornar yo.nombre + " hace algún sonido"
    fin funcion
fin clase

clase Perro extiende Animal:
    funcion hablar(yo):
        retornar yo.nombre + " ladra: ¡guau!"
    fin funcion
fin clase

mascotas = [Perro("Rex"), Animal("???")]
para m en mascotas:
    imprimir(m.hablar())
fin para
```

### `super`

Llama al método de la superclase:

```cornamusa
clase Empleado extiende Persona:
    funcion __iniciar__(yo, nombre, edad, salario):
        super.__iniciar__(nombre, edad)    # constructor de Persona
        yo.salario = salario
    fin funcion

    funcion saludar(yo):
        retornar super.saludar() + ", empleado"
    fin funcion
fin clase
```

### Métodos mágicos (dunders)

Los métodos con nombre `__...__` se invocan **automáticamente** al usar el operador correspondiente. Esto deja que tus objetos se comporten como tipos nativos:

```cornamusa
clase Vector:
    funcion __iniciar__(yo, px, py):
        yo.px = px
        yo.py = py
    fin funcion

    funcion __sumar__(yo, otro):
        retornar Vector(yo.px + otro.px, yo.py + otro.py)
    fin funcion

    funcion __cadena__(yo):
        retornar f"({yo.px}, {yo.py})"
    fin funcion

    funcion __igual__(yo, otro):
        retornar yo.px == otro.px y yo.py == otro.py
    fin funcion
fin clase

a = Vector(1, 2)
b = Vector(3, 4)
imprimir(a + b)              # (4, 6)  ← usa __sumar__ y __cadena__
imprimir(a == Vector(1, 2)) # verdadero
```

Los principales: `__cadena__` (`cadena`/`imprimir`/f-strings), `__longitud__` (`longitud`), `__iterar__` (`para`), `__indice__` / `__asignar_indice__` (`obj[i]`), `__llamar__` (`obj(...)`), `__entrar__` / `__salir__` (bloque `con`), los aritméticos (`__sumar__`, `__restar__`, ...) y los de comparación (`__igual__`, `__menor__`, ...). La lista completa está en la [referencia](referencia.md#11-clases-y-objetos).

---

## 11. Generadores

Una función que contiene `producir` es un **generador**. Llamarla no ejecuta el cuerpo: devuelve un objeto generador. Cada vez que lo iteras, la función se reanuda donde quedó hasta el siguiente `producir`, conservando todo su estado.

```cornamusa
funcion contar(ini, tope):
    i = ini
    mientras i <= tope:
        producir i
        i = i + 1
    fin mientras
fin funcion

para v en contar(1, 5):
    imprimir(v)        # 1 2 3 4 5
fin para
```

Lo potente: un generador puede ser **infinito**, porque solo calcula valores a medida que se piden:

```cornamusa
funcion fibonacci():
    a = 0
    b = 1
    mientras verdadero:
        producir a
        a, b = b, a + b
    fin mientras
fin funcion

i = 0
para n en fibonacci():
    si i >= 10: romper
    imprimir(n)
    i = i + 1
fin para
```

`producir desde` delega en otro generador o iterable, encadenándolos:

```cornamusa
funcion arbol():
    producir 0
    producir desde [1, 2, 3]
    producir desde otra_secuencia()
fin funcion
```

Y entre paréntesis tienes *generator expressions*, generadores en una línea:

```cornamusa
cuadrados = (n * n para n en rango(1, 1000000))
# nada se calcula hasta que recorres `cuadrados`
```

---

## 12. Pattern matching

`coincidir` compara un valor contra una serie de **patrones** y ejecuta la rama del primero que encaje. Es más expresivo que una cadena de `si`/`sino si`:

```cornamusa
funcion describir(valor):
    coincidir valor:
        cuando 0:
            retornar "cero"
        cuando 1 | 2 | 3:
            retornar "pequeño"
        cuando [a]:
            retornar f"lista de un elemento: {a}"
        cuando [primero, *resto]:
            retornar f"lista que empieza por {primero}"
        cuando (a, b) si a == b:
            retornar "par diagonal"
        cuando n si n > 100:
            retornar f"un número grande: {n}"
        cuando _:
            retornar "ni idea"
    fin coincidir
fin funcion

imprimir(describir(0))            # cero
imprimir(describir(2))            # pequeño
imprimir(describir([1, 2, 3]))    # lista que empieza por 1
imprimir(describir((5, 5)))       # par diagonal
imprimir(describir(500))          # un número grande: 500
```

Los patrones disponibles:

- **Literales**: `cuando 0`, `cuando "hola"`.
- **Nombre**: `cuando n` enlaza el valor a `n`.
- **Comodín**: `cuando _` encaja con todo.
- **Listas y tuplas**: `cuando [a, b]`, `cuando (a, b)`, con `*resto` en cualquier posición (`[primero, *medio, ultimo]`). Anidables.
- **OR**: `cuando 1 | 2 | 3` (separador `|`).
- **Type-match con binding**: `cuando Perro() como p` encaja con cualquier instancia de la clase `Perro` y la enlaza a `p`.
- **Guarda**: `cuando patron si condicion` añade una condición booleana.

---

## 13. Context managers (`con`)

El bloque `con` garantiza que un recurso se **libere siempre**, haya o no una excepción. La clase del recurso define `__entrar__` (se ejecuta al entrar) y `__salir__` (se ejecuta al salir, pase lo que pase):

```cornamusa
clase Conexion:
    funcion __iniciar__(yo, host):
        yo.host = host
    fin funcion

    funcion __entrar__(yo):
        imprimir("abriendo conexión a", yo.host)
        retornar yo
    fin funcion

    funcion __salir__(yo):
        imprimir("cerrando conexión")
    fin funcion
fin clase

con Conexion("api.dev") como c:
    imprimir("trabajando con", c.host)
fin con
# Salida:
#   abriendo conexión a api.dev
#   trabajando con api.dev
#   cerrando conexión
```

Aunque el cuerpo lance una excepción, `__salir__` se ejecuta igualmente.

---

## 14. Manejo de errores

Cornamusa lanza **excepciones** cuando algo va mal: división por cero, clave inexistente, tipo incorrecto, etc.

### `intentar` / `atrapar`

```cornamusa
intentar:
    x = 10 / 0
atrapar ErrorAritmetico como e:
    imprimir("Error:", e)
fin intentar
```

### Múltiples tipos y `finalmente`

`finalmente` se ejecuta **siempre**, haya o no excepción — ideal para limpieza:

```cornamusa
intentar:
    valor = procesar(entrada)
atrapar ErrorDeTipo como e:
    imprimir("Tipo incorrecto:", e)
atrapar ErrorDeValor como e:
    imprimir("Valor inválido:", e)
finalmente:
    imprimir("Limpieza")
fin intentar
```

### Lanzar excepciones

```cornamusa
funcion raiz_cuadrada(n):
    si n < 0:
        lanzar ErrorDeValor("no hay raíz real de un negativo")
    fin si
    retornar n ** 0.5
fin funcion
```

### Tipos de excepción

`Excepcion` (base), `ErrorAritmetico`, `ErrorDeTipo`, `ErrorDeValor`, `ErrorDeIndice`, `ErrorDeClave`, `ErrorDeNombre`, `ErrorDeAtributo`, `ErrorDeSistema`, `ErrorDeIO`.

### Tracebacks y sugerencias

Cuando un error **no se atrapa**, Cornamusa imprime un *traceback* con la cadena de llamadas que llevó al fallo y la línea de fuente exacta. Además, si escribes mal un nombre o un atributo, te sugiere el más parecido:

```
ErrorDeNombre: nombre 'longutud' no esta definido
    ¿quisiste decir 'longitud'?
```

---

## 15. Módulos y la biblioteca estándar

Un **módulo** es un archivo `.cor` cuyas variables y funciones se pueden importar desde otro:

```cornamusa
# matematicas_propias.cor
PI = 3.14159
funcion cuadrado(x):
    retornar x * x
fin funcion
```

```cornamusa
# uso.cor
importar matematicas_propias
imprimir(matematicas_propias.cuadrado(5))   # 25

# Traer nombres específicos:
desde matematicas_propias importar PI, cuadrado
imprimir(cuadrado(7))

# Con alias:
importar matematicas_propias como mat
imprimir(mat.PI)
```

### La biblioteca estándar

Cornamusa trae **diecisiete módulos** listos para usar. Un vistazo rápido:

```cornamusa
importar matematicas
imprimir(matematicas.factorial(20))         # bignum, sin overflow

importar funcionales
imprimir(funcionales.mapear(lambda x: x*x, [1, 2, 3]))   # [1, 4, 9]
imprimir(funcionales.filtrar(lambda x: x > 2, [1, 2, 3, 4]))  # [3, 4]

importar cadenas
imprimir(cadenas.separar("uno,dos,tres", ","))

importar archivos
archivos.escribir("salida.txt", "hola")
imprimir(archivos.leer("salida.txt"))

importar json
texto = json.serializar({"nombre": "Ana", "edad": 30})
imprimir(json.parsear(texto))

importar fechas
imprimir(fechas.legible(fechas.ahora()))

importar azar
imprimir(azar.entero(1, 6))                 # tirada de dado
imprimir(azar.elegir(["cara", "cruz"]))

importar regex
imprimir(regex.todos("\\d+", "tengo 3 gatos y 2 perros"))   # ["3", "2"]

importar formato
imprimir(formato.con_decimales(3.14159, 2)) # 3.14

importar proceso
imprimir(proceso.capturar("echo", "hola"))

importar red
respuesta = red.obtener("http://example.com")
imprimir(respuesta["codigo"])
```

La lista completa de funciones de cada módulo está en la [referencia](referencia.md#16-biblioteca-estándar-stdlib).

---

## 16. Un programa completo

Pongámoslo todo junto. **Análisis de un registro de ventas**:

```cornamusa
# ventas.cor — usa destructuring, comprehensions, clases y stdlib.

importar funcionales

clase Venta:
    funcion __iniciar__(yo, producto, unidades, precio):
        yo.producto = producto
        yo.unidades = unidades
        yo.precio = precio
    fin funcion

    funcion total(yo):
        retornar yo.unidades * yo.precio
    fin funcion

    funcion __cadena__(yo):
        retornar f"{yo.producto}: {yo.unidades} x {yo.precio} = {yo.total()}"
    fin funcion
fin clase

funcion main():
    datos = [
        ("gaita", 3, 200),
        ("tambor", 5, 80),
        ("flauta", 12, 25),
    ]

    # La variable de una comprehension es un solo nombre; accedemos a
    # los campos de cada tupla por índice.
    ventas = [Venta(f[0], f[1], f[2]) para f en datos]

    para v en ventas:
        imprimir(" -", v)
    fin para

    totales = funcionales.mapear(lambda v: v.total(), ventas)
    imprimir("Ingreso total:", funcionales.suma(totales))

    # La venta más grande, con pattern matching sobre el resultado.
    mayor = ventas[0]
    para v en ventas:
        si v.total() > mayor.total():
            mayor = v
        fin si
    fin para
    imprimir("Mayor venta:", mayor.producto)
fin funcion

main()
```

```
$ ./build/cornamusa --bytecode ventas.cor
 - gaita: 3 x 200 = 600
 - tambor: 5 x 80 = 400
 - flauta: 12 x 25 = 300
Ingreso total: 1300
Mayor venta: gaita
```

---

## 17. Próximos pasos

- Consulta la **[referencia rápida](referencia.md)** para tener toda la sintaxis y la stdlib a mano.
- Explora los **[72 ejemplos](https://github.com/David-Castilla-Gomez/Cornamusa/tree/main/examples)** del repositorio: hay uno por cada característica del lenguaje.
- Lee la **[especificación formal](https://github.com/David-Castilla-Gomez/Cornamusa/blob/main/ESPEC.md)** si quieres el detalle preciso de la gramática y la semántica.
- Si encuentras un bug o algo confuso, [abre un issue en GitHub](https://github.com/David-Castilla-Gomez/Cornamusa/issues).

¡Buen camino!
