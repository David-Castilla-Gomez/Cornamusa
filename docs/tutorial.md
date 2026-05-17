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

### Ejercicios

1. Imprime tu nombre y tu edad en la misma línea: `Me llamo Ana y tengo 30 años`.
2. Imprime tres líneas: tu nombre, tu ciudad y tu lenguaje favorito (una por línea, una sola llamada a `imprimir` por línea).
3. Imprime el resultado de `7 * 6` directamente como argumento (sin variable intermedia).

> Las soluciones están al final del tutorial, en el [anexo de soluciones](#anexo-soluciones-a-los-ejercicios).

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

### Ejercicios

1. Crea tres variables con tu nombre, año de nacimiento y altura en metros. Imprime una frase que use las tres.
2. Calcula `2 ** 200` y guarda el resultado en una variable. Imprime el resultado y luego imprime `tipo` del resultado para verificar que sigue siendo `entero`.
3. Define `x = 5`, luego reasigna `x = "cinco"`. Imprime `tipo(x)` antes y después para confirmar el cambio.

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

### Ejercicios

1. Calcula y muestra cuántos minutos hay en un año no bisiesto (365 días). Usa solo expresiones, una línea.
2. Dado un número `n = 17`, imprime si es par o impar (sin usar `si` todavía — solo con expresiones booleanas y `imprimir`).
3. Verifica con una sola expresión booleana si el año `2026` está entre `2020` y `2030` *inclusive*. Imprime el resultado.

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

### Ejercicios

1. **FizzBuzz**: imprime los números del 1 al 20. Si es múltiplo de 3 imprime `Fizz`, si es múltiplo de 5 imprime `Buzz`, si es de ambos imprime `FizzBuzz`.
2. Imprime la tabla de multiplicar del `7`: `7 x 1 = 7`, `7 x 2 = 14`, ..., `7 x 10 = 70`.
3. Suma los números pares del 1 al 100. Esperado: `2550`.
4. Imprime los números del 1 al 100 que sean palíndromos en base 10 (se leen igual al revés): `1, 2, ..., 9, 11, 22, 33, 44, 55, 66, 77, 88, 99`. Pista: una manera fácil de invertir un número es convertirlo a cadena.

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

### Ejercicios

1. Dado `coord = (3, 7)`, intercambia las componentes usando destructuring (sin variable temporal) e imprime el resultado.
2. Dada la lista `[("Ana", "Madrid"), ("Luis", "Sevilla"), ("Eva", "Bilbao")]`, itera y para cada par destructura en `nombre, ciudad` dentro del bucle.

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

### Ejercicios

1. Escribe una función `factorial(n)` que devuelva `n!`. Verifica con `factorial(10)` (esperado: `3628800`).
2. Escribe `es_primo(n)` que devuelva `verdadero` o `falso`. Verifica con `[2, 3, 4, 9, 17, 25]`.
3. Escribe `repetir(s, n)` que devuelva una cadena con `s` repetido `n` veces, separado por guiones. Ejemplo: `repetir("ja", 3)` → `"ja-ja-ja"`.
4. Escribe una función `aplicar_varios(f, lista)` que aplique la función `f` a cada elemento y devuelva una lista nueva. Pruébala con `aplicar_varios(funcion(x): retornar x*x fin funcion, [1, 2, 3, 4])`. *Nota: o usa una función definida con `funcion` y pásala por nombre.*

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

### Ejercicios

1. Crea una lista con los números del 1 al 50. Usa un bucle `para` para sumarlos. Verifica el total (esperado: `1275`).
2. Dado `palabras = ["sol", "luna", "estrella", "mar", "viento", "tierra"]`, construye un diccionario `{palabra: longitud}` para todas las palabras.
3. Dada `votos = ["si", "no", "si", "si", "abstencion", "no", "si"]`, calcula cuántas veces aparece cada valor distinto (cuenta de cada uno).

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

### Ejercicios

1. Usa una comprehension para construir la lista de los cubos de los números del 1 al 10: `[1, 8, 27, ..., 1000]`.
2. Usa una comprehension con filtro para extraer las palabras de longitud impar de `["uno", "dos", "tres", "cuatro", "cinco"]`. Esperado: `["uno", "dos", "cinco"]`.
3. Construye un diccionario `{n: "par" si n es par sino "impar" para n en rango(1, 6)}`. *Pista*: la expresión ternaria `valor1 si cond sino valor2` es válida en cualquier sitio donde se espere una expresión.

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

### Ejercicios

1. Dada `frase = "hola mundo desde cornamusa"`, imprime cuántas palabras tiene. Pista: `cadenas.separar(frase, " ")` y `longitud`.
2. Escribe una función `iniciales(nombre_completo)` que devuelva las iniciales de cada palabra en mayúsculas separadas por punto. Ejemplo: `"ana de la torre"` → `"A.D.L.T."`.
3. Cuenta cuántas vocales (`aeiouáéíóú`) hay en una cadena. *Pista*: itera con `para c en s`.

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

### Ejercicios

1. Crea una clase `Libro` con `titulo`, `autor`, `paginas`. Añade `__cadena__` que devuelva `"<Libro: 'titulo' de autor (N pags)>"`. Crea dos libros e imprímelos.
2. Crea una clase `Cuenta` con saldo inicial. Métodos: `depositar(n)`, `retirar(n)` (con error si `n > saldo`), `consultar()`. Verifica con un escenario.
3. Crea una clase `Fraccion` con numerador y denominador. Implementa `__sumar__` (suma `a/b + c/d = (ad+bc)/(bd)`), `__igual__` (con cross-product: `a/b == c/d` si `a*d == b*c`) y `__cadena__` (`"a/b"`). Verifica `Fraccion(1,2) + Fraccion(1,3) == Fraccion(5,6)`.
4. Usa `@propiedad` (v1.78) para añadir a `Fraccion` un atributo `decimal` que devuelva el cociente como decimal: `Fraccion(1,4).decimal` → `0.25`.

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

### Ejercicios

1. Escribe una función `dividir_seguro(a, b)` que devuelva `a / b` normalmente, pero si `b == 0` capture el `ErrorAritmetico` y devuelva `nulo`.
2. Escribe `convertir_a_entero(s)` que devuelva el entero parseado, o `0` si no es válido. Atrapa `ErrorDeValor`.
3. Modifica `dividir_seguro` para que también valide tipos: si `a` o `b` no son numéricos, lanza `ErrorDeTipo` explícitamente con un mensaje claro.

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

### Ejercicios

1. Usa `matematicas.factorial` y un bucle para imprimir `n!` para `n` de 1 a 10.
2. Lee tu propio archivo fuente con `archivos.leer`, cuenta cuántas líneas tiene usando `cadenas.separar(contenido, "\n")` y `longitud`.
3. Genera 10 tiradas aleatorias de un dado de 6 caras con `azar.entero(1, 6)`. Calcula la media (suma / 10).

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

## Anexo: soluciones a los ejercicios

> Todas las soluciones se han validado contra el intérprete v1.78. Hay varias maneras correctas de resolver cada ejercicio — la solución mostrada es **una** forma idiomática, no necesariamente la única ni la más corta.

### §1 — Tu primer programa

```cornamusa
# 1.1
nombre = "Ana"
edad = 30
imprimir(f"Me llamo {nombre} y tengo {edad} años")
# → Me llamo Ana y tengo 30 años

# 1.2
imprimir("Ana")
imprimir("Madrid")
imprimir("Cornamusa")

# 1.3
imprimir(7 * 6)     # → 42
```

### §2 — Variables y tipos

```cornamusa
# 2.1
nombre = "Ana"
anio_nac = 1995
altura = 1.65
imprimir(f"{nombre}, nacida en {anio_nac}, mide {altura}m")

# 2.2
gigante = 2 ** 200
imprimir(gigante)      # 1606938...376
imprimir(tipo(gigante)) # entero (precisión arbitraria)

# 2.3
x = 5
imprimir(tipo(x))      # entero
x = "cinco"
imprimir(tipo(x))      # cadena
```

### §3 — Operadores y expresiones

```cornamusa
# 3.1
imprimir(365 * 24 * 60)              # 525600

# 3.2  (ternaria + módulo)
n = 17
imprimir("par" si n % 2 == 0 sino "impar")

# 3.3
anio = 2026
imprimir(anio >= 2020 y anio <= 2030)   # verdadero
```

### §4 — Control de flujo

```cornamusa
# 4.1 FizzBuzz
para i en rango(1, 21):
    si i % 15 == 0:
        imprimir("FizzBuzz")
    sino si i % 3 == 0:
        imprimir("Fizz")
    sino si i % 5 == 0:
        imprimir("Buzz")
    sino:
        imprimir(i)
    fin si
fin para

# 4.2 tabla del 7
para i en rango(1, 11):
    imprimir(f"7 x {i} = {7 * i}")
fin para

# 4.3 suma pares 1..100
total = 0
para i en rango(1, 101):
    si i % 2 == 0:
        total = total + i
    fin si
fin para
imprimir(total)      # 2550

# 4.4 palíndromos 1..100
palindromos = []
para i en rango(1, 101):
    s = cadena(i)
    invertida = ""
    para c en s:
        invertida = c + invertida
    fin para
    si s == invertida:
        agregar(palindromos, i)
    fin si
fin para
imprimir(palindromos)
# → [1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 22, 33, 44, 55, 66, 77, 88, 99]
```

### §5 — Destructuring

```cornamusa
# 5.1
coord = (3, 7)
a, b = coord
a, b = b, a
imprimir((a, b))         # (7, 3)

# 5.2
gente = [("Ana", "Madrid"), ("Luis", "Sevilla"), ("Eva", "Bilbao")]
para par en gente:
    nombre, ciudad = par
    imprimir(f"{nombre} vive en {ciudad}")
fin para
```

### §6 — Funciones

```cornamusa
# 6.1 factorial
funcion factorial(n):
    si n <= 1:
        retornar 1
    fin si
    retornar n * factorial(n - 1)
fin funcion
imprimir(factorial(10))      # 3628800

# 6.2 es_primo
funcion es_primo(n):
    si n < 2:
        retornar falso
    fin si
    para d en rango(2, n):
        si d * d > n:
            romper
        fin si
        si n % d == 0:
            retornar falso
        fin si
    fin para
    retornar verdadero
fin funcion

# 6.3 repetir
importar cadenas
funcion repetir(s, n):
    partes = []
    para _ en rango(n):
        agregar(partes, s)
    fin para
    retornar cadenas.unir(partes, "-")
fin funcion
imprimir(repetir("ja", 3))   # ja-ja-ja

# 6.4 aplicar_varios (función como argumento)
funcion aplicar_varios(f, lista):
    resultado = []
    para x en lista:
        agregar(resultado, f(x))
    fin para
    retornar resultado
fin funcion
funcion cuadrado(x):
    retornar x * x
fin funcion
imprimir(aplicar_varios(cuadrado, [1, 2, 3, 4]))   # [1, 4, 9, 16]
```

### §7 — Estructuras de datos

```cornamusa
# 7.1
total = 0
para i en rango(1, 51):
    total = total + i
fin para
imprimir(total)              # 1275

# 7.2
palabras = ["sol", "luna", "estrella", "mar", "viento", "tierra"]
d = {}
para p en palabras:
    d[p] = longitud(p)
fin para
imprimir(d)
# → {"sol": 3, "luna": 4, "estrella": 8, "mar": 3, "viento": 6, "tierra": 6}

# 7.3 contar votos
votos = ["si", "no", "si", "si", "abstencion", "no", "si"]
cuenta = {}
para v en votos:
    si v en cuenta:
        cuenta[v] = cuenta[v] + 1
    sino:
        cuenta[v] = 1
    fin si
fin para
imprimir(cuenta)
# → {"si": 4, "no": 2, "abstencion": 1}
```

### §8 — Comprehensions

```cornamusa
# 8.1
cubos = [n * n * n para n en rango(1, 11)]
imprimir(cubos)
# → [1, 8, 27, 64, 125, 216, 343, 512, 729, 1000]

# 8.2
ps = ["uno", "dos", "tres", "cuatro", "cinco"]
impares = [w para w en ps si longitud(w) % 2 == 1]
imprimir(impares)            # ["uno", "dos", "cinco"]

# 8.3
d = {n: ("par" si n % 2 == 0 sino "impar") para n en rango(1, 6)}
imprimir(d)
# → {1: "impar", 2: "par", 3: "impar", 4: "par", 5: "impar"}
```

### §9 — Cadenas

```cornamusa
importar cadenas

# 9.1 contar palabras
frase = "hola mundo desde cornamusa"
imprimir(longitud(cadenas.separar(frase, " ")))   # 4

# 9.2 iniciales
funcion iniciales(nombre):
    partes = cadenas.separar(nombre, " ")
    inits = []
    para p en partes:
        agregar(inits, cadenas.mayusculas_ascii(p[0]))
    fin para
    retornar cadenas.unir(inits, ".") + "."
fin funcion
imprimir(iniciales("ana de la torre"))   # A.D.L.T.

# 9.3 contar vocales
funcion contar_vocales(s):
    cuenta = 0
    para c en s:
        si c en "aeiouáéíóúAEIOUÁÉÍÓÚ":
            cuenta = cuenta + 1
        fin si
    fin para
    retornar cuenta
fin funcion
imprimir(contar_vocales("Cornamusa"))    # 4
imprimir(contar_vocales("murciélago"))   # 5
```

### §10 — Clases

```cornamusa
# 10.1 Libro
clase Libro:
    funcion __iniciar__(yo, titulo, autor, paginas):
        yo.titulo = titulo
        yo.autor = autor
        yo.paginas = paginas
    fin funcion
    funcion __cadena__(yo):
        retornar f"<Libro: '{yo.titulo}' de {yo.autor} ({yo.paginas} pags)>"
    fin funcion
fin clase

# 10.2 Cuenta
clase Cuenta:
    funcion __iniciar__(yo, saldo):
        yo.saldo = saldo
    fin funcion
    funcion depositar(yo, n):
        yo.saldo = yo.saldo + n
    fin funcion
    funcion retirar(yo, n):
        si n > yo.saldo:
            lanzar ErrorDeValor("saldo insuficiente")
        fin si
        yo.saldo = yo.saldo - n
    fin funcion
    funcion consultar(yo):
        retornar yo.saldo
    fin funcion
fin clase

# 10.3 + 10.4 Fraccion con @propiedad
clase Fraccion:
    funcion __iniciar__(yo, num, den):
        yo.num = num
        yo.den = den
    fin funcion
    funcion __sumar__(yo, otro):
        retornar Fraccion(yo.num * otro.den + otro.num * yo.den,
                          yo.den * otro.den)
    fin funcion
    funcion __igual__(yo, otro):
        retornar yo.num * otro.den == otro.num * yo.den
    fin funcion
    funcion __cadena__(yo):
        retornar f"{yo.num}/{yo.den}"
    fin funcion
    @propiedad
    funcion decimal(yo):
        retornar yo.num / yo.den
    fin funcion
fin clase

imprimir(Fraccion(1, 2) + Fraccion(1, 3))          # 5/6
imprimir(Fraccion(1, 4).decimal)                    # 0.25
```

### §14 — Manejo de errores

```cornamusa
# 14.1 dividir_seguro
funcion dividir_seguro(a, b):
    intentar:
        retornar a / b
    atrapar ErrorAritmetico:
        retornar nulo
    fin intentar
fin funcion
imprimir(dividir_seguro(10, 2))    # 5.0
imprimir(dividir_seguro(10, 0))    # nulo

# 14.2 convertir_a_entero
funcion convertir_a_entero(s):
    intentar:
        retornar entero(s)
    atrapar ErrorDeValor:
        retornar 0
    fin intentar
fin funcion

# 14.3 valida tipos
funcion dividir_v2(a, b):
    si tipo(a) != "entero" y tipo(a) != "decimal":
        lanzar ErrorDeTipo("a debe ser numerico")
    fin si
    si tipo(b) != "entero" y tipo(b) != "decimal":
        lanzar ErrorDeTipo("b debe ser numerico")
    fin si
    intentar:
        retornar a / b
    atrapar ErrorAritmetico:
        retornar nulo
    fin intentar
fin funcion
```

### §15 — Módulos y stdlib

```cornamusa
# 15.1 factoriales 1..10
importar matematicas
para n en rango(1, 11):
    imprimir(f"{n}! = {matematicas.factorial(n)}")
fin para

# 15.2 contar líneas de tu propio fuente
importar archivos
importar cadenas
contenido = archivos.leer("mi_programa.cor")
imprimir("lineas:", longitud(cadenas.separar(contenido, "\n")))

# 15.3 dados
importar azar
importar funcionales
azar.semilla(42)   # opcional, para resultados reproducibles
tiradas = []
para _ en rango(10):
    agregar(tiradas, azar.entero(1, 6))
fin para
imprimir("tiradas:", tiradas)
imprimir("media:", funcionales.suma(tiradas) / 10)
```

---

## 17. Próximos pasos

- Consulta la **[referencia rápida](referencia.md)** para tener toda la sintaxis y la stdlib a mano.
- Explora los **[72 ejemplos](https://github.com/David-Castilla-Gomez/Cornamusa/tree/main/examples)** del repositorio: hay uno por cada característica del lenguaje.
- Lee la **[especificación formal](https://github.com/David-Castilla-Gomez/Cornamusa/blob/main/ESPEC.md)** si quieres el detalle preciso de la gramática y la semántica.
- Si encuentras un bug o algo confuso, [abre un issue en GitHub](https://github.com/David-Castilla-Gomez/Cornamusa/issues).

¡Buen camino!
