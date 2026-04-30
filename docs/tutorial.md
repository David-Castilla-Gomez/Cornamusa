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
imprimir("Mide", altura, "metros")
imprimir("¿Es mayor?", mayor_de_edad)
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

Itera sobre cualquier valor iterable (rango, lista, cadena, diccionario, conjunto, tupla):

```cornamusa
para i en rango(5):
    imprimir(i)        # 0, 1, 2, 3, 4
fin para

frutas = ["manzana", "pera", "uva"]
para fruta en frutas:
    imprimir("Me gusta la", fruta)
fin para

para letra en "Hola":
    imprimir(letra)    # H, o, l, a
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

## 5. Funciones

```cornamusa
funcion saludar(nombre):
    retornar "¡Hola, " + nombre + "!"
fin funcion

mensaje = saludar("Mundo")
imprimir(mensaje)        # ¡Hola, Mundo!
```

### Argumentos por defecto

```cornamusa
funcion saludar(nombre, idioma="es"):
    si idioma == "es":
        retornar "¡Hola, " + nombre + "!"
    sino:
        retornar "Hello, " + nombre + "!"
    fin si
fin funcion

imprimir(saludar("Ana"))               # ¡Hola, Ana!
imprimir(saludar("Bob", "en"))         # Hello, Bob!
```

### Funciones que no devuelven valor

Si no hay `retornar`, la función devuelve `nulo`:

```cornamusa
funcion saludar(nombre):
    imprimir("Hola,", nombre)
fin funcion

resultado = saludar("Ana")
imprimir(resultado)     # nulo
```

### Closures (funciones dentro de funciones)

Una función definida dentro de otra captura variables del scope enclosing como **upvalues** (lectura):

```cornamusa
funcion crear_saludador(saludo):
    funcion saludar(nombre):
        retornar saludo + ", " + nombre
    fin funcion
    retornar saludar
fin funcion

hola = crear_saludador("Hola")
imprimir(hola("Ana"))      # Hola, Ana
imprimir(hola("Bob"))      # Hola, Bob
```

> **Nota v0.11.4**: la **escritura** a upvalues (con `nolocal` o equivalente) está reservada para v1.x. En v0.11.4 los upvalues son de solo lectura — útiles para closures sobre constantes y configuraciones, pero no para "contadores mutables tipo Python".

### Lambda

Función anónima de una sola expresión:

```cornamusa
cuadrado = lambda x: x * x
imprimir(cuadrado(7))    # 49
```

---

## 6. Estructuras de datos

### Listas

Secuencia mutable indexada:

```cornamusa
xs = [10, 20, 30, 40, 50]
imprimir(xs[0])      # 10 (primer elemento)
imprimir(xs[-1])     # 50 (último, índice negativo)
imprimir(longitud(xs))  # 5

# Modificación
xs[0] = 99
imprimir(xs)         # [99, 20, 30, 40, 50]

# Built-ins de mutación
agregar(xs, 60)      # añade al final
imprimir(xs)         # [99, 20, 30, 40, 50, 60]

quitar(xs, 0)        # quita por índice
imprimir(xs)         # [20, 30, 40, 50, 60]

insertar(xs, 0, 10)  # inserta en posición 0
imprimir(xs)         # [10, 20, 30, 40, 50, 60]

invertir(xs)
imprimir(xs)         # [60, 50, 40, 30, 20, 10]

ordenar(xs)
imprimir(xs)         # [10, 20, 30, 40, 50, 60]
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

Mapa hash mutable:

```cornamusa
persona = {"nombre": "Ana", "edad": 30, "ciudad": "Madrid"}

imprimir(persona["nombre"])   # Ana
persona["edad"] = 31          # modificación
persona["email"] = "ana@..."  # añadir clave nueva

# Iteración
para clave en claves(persona):
    imprimir(clave, "->", persona[clave])
fin para
```

> En v0.11.5 no hay built-in para borrar una clave del dict (planeado v1.x). Mientras tanto puedes reescribir el dict completo con las claves que quieres conservar.

### Conjuntos

Sin claves repetidas, sin orden:

```cornamusa
s = {1, 2, 3, 2, 1}
imprimir(s)          # {1, 2, 3} — sin duplicados
imprimir(longitud(s))   # 3

vacio = conjunto()   # {} es un diccionario, no conjunto vacío
agregar(vacio, "a")
imprimir(vacio)      # {a}
```

### Tuplas

Como listas pero **inmutables**:

```cornamusa
punto = (3, 4)
imprimir(punto[0])   # 3

# punto[0] = 10  → error: las tuplas no se pueden modificar
```

---

## 7. Cadenas

```cornamusa
s = "Cornamusa"
imprimir(longitud(s))    # 9
imprimir(s[0])           # C
imprimir(s[-1])          # a (último)

# Concatenación
saludo = "Hola, " + "mundo"

# Repetición
linea = "=" * 30
imprimir(linea)

# Pertenencia
imprimir("orna" en s)    # verdadero
```

### Módulo `cadenas`

Para operaciones más avanzadas (introducidas en v0.9.1):

```cornamusa
importar cadenas

imprimir(cadenas.repetir("-", 20))           # --------------------
imprimir(cadenas.empieza_con("hola.cor", "hola"))   # verdadero
imprimir(cadenas.termina_con("foo.txt", ".txt"))    # verdadero
imprimir(cadenas.contar("aaabaa", "aa"))     # 2
imprimir(cadenas.caracter("Cornamusa", 4))   # a
```

---

## 8. Clases y objetos

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

`__iniciar__` es el constructor (equivalente a `__init__` de Python). El primer parámetro de cada método se llama `yo` por convención (puedes usar otro nombre, pero `yo` es estándar).

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

clase Gato extiende Animal:
    funcion hablar(yo):
        retornar yo.nombre + " maúlla: ¡miau!"
    fin funcion
fin clase

mascotas = [Perro("Rex"), Gato("Misi"), Animal("???")]
para m en mascotas:
    imprimir(m.hablar())
fin para
```

### `super`

Llama al método de la superclase:

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

clase Empleado extiende Persona:
    funcion __iniciar__(yo, nombre, edad, salario):
        super.__iniciar__(nombre, edad)    # llama al __iniciar__ de Persona
        yo.salario = salario
    fin funcion

    funcion saludar(yo):
        base = super.saludar()              # método saludar de Persona
        retornar base + ", empleado"
    fin funcion
fin clase

ana = Empleado("Ana", 30, 50000)
imprimir(ana.saludar())   # Soy Ana, empleado
imprimir(ana.salario)     # 50000
```

> Nota: en v0.11.4 los dunders aritméticos (`__sumar__`, etc.) son métodos ordinarios — no se invocan automáticamente al hacer `a + b`. Solo `__iniciar__` es especial. Esto cambia en v1.x.

---

## 9. Módulos

Un **módulo** es un archivo `.cor` cuyas variables y funciones se pueden importar desde otro:

```cornamusa
# matematicas.cor
PI = 3.14159

funcion cuadrado(x):
    retornar x * x
fin funcion
```

```cornamusa
# uso.cor
importar matematicas

imprimir(matematicas.PI)
imprimir(matematicas.cuadrado(5))   # 25
```

### `desde X importar Y`

Trae nombres específicos al scope local:

```cornamusa
desde matematicas importar PI, cuadrado

imprimir(PI)
imprimir(cuadrado(7))
```

### Alias

```cornamusa
importar matematicas como mat
imprimir(mat.PI)

desde matematicas importar cuadrado como cuad
imprimir(cuad(8))
```

### Stdlib disponible (v0.11.4)

- **`matematicas`**: `PI`, `E`, `cuadrado`, `cubo`, `absoluto`, `maximo`, `minimo`, `signo`, `factorial`, `suma_rango`, `es_par`, `es_impar`, `mcd`.
- **`cadenas`**: `repetir`, `empieza_con`, `termina_con`, `contar`, `caracter`.
- **`sistema`**: `argv` (lista de argumentos del programa).

---

## 10. Manejo de errores

Cornamusa lanza **excepciones** cuando algo va mal: división por cero, acceso a clave inexistente, tipo incorrecto, etc.

### `intentar` / `atrapar`

```cornamusa
intentar:
    x = 10 / 0
atrapar ErrorAritmetico como e:
    imprimir("Error:", e)
fin intentar
```

### Múltiples tipos

```cornamusa
intentar:
    valor = procesar(entrada)
atrapar ErrorDeTipo como e:
    imprimir("Tipo incorrecto:", e)
atrapar ErrorDeValor como e:
    imprimir("Valor inválido:", e)
fin intentar
```

### `finalmente`

Bloque que **siempre** se ejecuta (haya o no excepción):

```cornamusa
intentar:
    arriesgado()
atrapar Excepcion como e:
    imprimir("Falló:", e)
finalmente:
    imprimir("Limpieza")
fin intentar
```

### Lanzar excepciones

```cornamusa
funcion dividir(a, b):
    si b == 0:
        lanzar ErrorAritmetico("división por cero")
    fin si
    retornar a / b
fin funcion

intentar:
    dividir(10, 0)
atrapar ErrorAritmetico como e:
    imprimir("Capturado:", e)
fin intentar
```

### Tipos de excepción built-in

- `Excepcion` — base.
- `ErrorAritmetico` — división por cero, etc.
- `ErrorDeTipo` — operación con tipo incorrecto.
- `ErrorDeValor` — valor del tipo correcto pero inválido.
- `ErrorDeIndice` — índice fuera de rango.
- `ErrorDeClave` — clave no presente en diccionario.
- `ErrorDeNombre` — identificador no definido.

---

## 11. Programa completo

Pongamos todo junto en un programa real. **Conversor de temperatura interactivo**:

```cornamusa
# conversor.cor

clase Conversor:
    funcion __iniciar__(yo, escala):
        yo.escala = escala
    fin funcion

    funcion a_celsius(yo, valor):
        si yo.escala == "F":
            retornar (valor - 32) * 5 / 9
        sino si yo.escala == "K":
            retornar valor - 273.15
        sino:
            retornar valor   # ya está en Celsius
        fin si
    fin funcion
fin clase

importar matematicas

funcion main():
    valores_f = [32, 68, 98.6, 212]
    c = Conversor("F")
    para v en valores_f:
        celsius = c.a_celsius(v)
        imprimir(v, "°F =", matematicas.absoluto(celsius), "°C")
    fin para
fin funcion

main()
```

```
$ ./build/cornamusa --bytecode conversor.cor
32 °F = 0 °C
68 °F = 20 °C
98.6 °F = 37 °C
212 °F = 100 °C
```

---

## Próximos pasos

- Lee la **[referencia rápida](referencia.md)** para tener todo a mano.
- Mira los **[ejemplos](../examples/)** del repositorio: 23+ programas demostrando cada feature.
- Si encuentras un bug o una característica confusa, [abre un issue en GitHub](https://github.com/David-Castilla-Gomez/Cornamusa/issues).
- Cornamusa está en evolución activa. La hoja de ruta vive en [README.md](../README.md) y en `decisiones/`.

¡Buen camino!
