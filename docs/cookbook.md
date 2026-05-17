# Cookbook de Cornamusa

> Recetas listas para copy-paste cubriendo casos reales. Cada receta está validada contra el intérprete (`cornamusa --bytecode receta.cor`); los outputs mostrados son los reales.

Este recetario complementa el [tutorial](tutorial.md) (que enseña el lenguaje) y la [referencia](referencia.md) (que lista APIs). Aquí ves **patrones de uso** — cómo combinar features para resolver problemas que aparecen.

## Índice

1. [Procesar CSV y agregar](#1-procesar-csv-y-agregar)
2. [JWT con expiración](#2-jwt-con-expiración)
3. [Hash SHA-256 de un valor](#3-hash-sha-256-de-un-valor)
4. [Backoff exponencial para reintentos](#4-backoff-exponencial-para-reintentos)
5. [Memoización con decorador](#5-memoización-con-decorador)
6. [Cronometrar bloques de código](#6-cronometrar-bloques-de-código)
7. [Atributos computados con `@propiedad`](#7-atributos-computados-con-propiedad)
8. [Parser básico de argumentos del programa](#8-parser-básico-de-argumentos-del-programa)
9. [Contar frecuencias (counter dict-style)](#9-contar-frecuencias-counter-dict-style)
10. [JSON pretty-print para configuración](#10-json-pretty-print-para-configuración)

---

## 1. Procesar CSV y agregar

**Problema**: tienes un CSV con columnas `producto,unidades,precio` y quieres calcular el total facturado.

```cornamusa
importar csv

texto_csv = "producto,unidades,precio\ngaita,3,200\ntambor,5,80\nflauta,12,25"
filas = csv.parsear(texto_csv)
encabezado = filas[0]
datos = filas[1:]

total = 0
para fila en datos:
    unidades = entero(fila[1])
    precio = entero(fila[2])
    total = total + unidades * precio
fin para
imprimir(f"Total: {total}€")
```

```
Total: 1300€
```

**Variantes útiles**:
- Si el CSV está en disco: `texto_csv = archivos.leer("ventas.csv")`.
- Si el separador es `;` o `\t`: `csv.parsear(texto, ";")`.
- Si hay campos con `,` o `\n` internos: el parser los maneja correctamente cuando están entre `"..."`.

---

## 2. JWT con expiración

**Problema**: emitir un token para un usuario que caduque en 1 hora, y verificarlo en otro punto del programa.

```cornamusa
importar jwt
importar tiempo

CLAVE = "mi-secreto"
ahora = tiempo.epoch_segundos()
payload = {"sub": "usuario42", "rol": "admin", "exp": ahora + 3600}
token = jwt.codificar(payload, CLAVE)
imprimir(f"Token emitido (longitud: {longitud(token)})")

intentar:
    extraido = jwt.decodificar_y_validar(token, CLAVE, ahora + 30)
    imprimir(f"Validado: usuario={extraido['sub']}, rol={extraido['rol']}")
atrapar ErrorDeValor como e:
    imprimir("Token invalido:", e)
fin intentar
```

```
Token emitido (longitud: 148)
Validado: usuario=usuario42, rol=admin
```

`decodificar_y_validar` rechaza el token si: la firma es inválida (clave incorrecta), `exp <= ahora`, o `nbf > ahora`. El parámetro `ahora` se pasa explícito — útil para tests con tiempo simulado.

---

## 3. Hash SHA-256 de un valor

**Problema**: calcular un identificador estable a partir de un contenido (cache key, fingerprint de archivo).

```cornamusa
importar hashing
firma = hashing.sha256("mensaje importante")
imprimir(f"SHA-256: {firma}")
```

```
SHA-256: bb8f75e69b27cd5d555ca03fc67436983423e4141c515151ae2ea10d889bf925
```

Para un archivo entero: `hashing.sha256(archivos.leer("documento.pdf"))`. Para autenticación con clave compartida usa `hashing.hmac_sha256(clave, mensaje)`.

> Para hashes de contraseñas usa argon2/scrypt en lugar de SHA-256. Cornamusa no incluye esos algoritmos.

---

## 4. Backoff exponencial para reintentos

**Problema**: una operación falla intermitentemente (red, archivo bloqueado). Reintentar con espera creciente entre intentos.

```cornamusa
importar tiempo

funcion intentar_con_backoff(max_intentos):
    para intento en rango(max_intentos):
        si intento == max_intentos - 1:
            imprimir(f"  intento {intento + 1}: OK")
            retornar verdadero
        fin si
        espera = 0.01 * (2 ** intento)
        imprimir(f"  intento {intento + 1}: fallo, espero {espera}s")
        tiempo.dormir(espera)
    fin para
    retornar falso
fin funcion

intentar_con_backoff(4)
```

```
  intento 1: fallo, espero 0.01s
  intento 2: fallo, espero 0.02s
  intento 3: fallo, espero 0.04s
  intento 4: OK
```

En código real reemplaza la condición artificial por la operación que puede fallar dentro de `intentar:/atrapar Excepcion`. Si la última iteración también falla, propaga la excepción.

---

## 5. Memoización con decorador

**Problema**: una función pura es lenta de calcular varias veces con el mismo argumento. Cachear resultados sin tocar el cuerpo de la función.

```cornamusa
funcion memoizar(f):
    cache = {}
    funcion w(n):
        si n en cache:
            retornar cache[n]
        fin si
        r = f(n)
        cache[n] = r
        retornar r
    fin funcion
    retornar w
fin funcion

@memoizar
funcion fib(n):
    si n < 2:
        retornar n
    fin si
    retornar fib(n - 1) + fib(n - 2)
fin funcion

imprimir(f"fib(40) = {fib(40)}")
```

```
fib(40) = 102334155
```

Sin memoización, `fib(40)` haría ~2 mil millones de llamadas recursivas. Con memoización es instantáneo (~40 llamadas). El decorador se aplica `f = memoizar(f)` automáticamente.

---

## 6. Cronometrar bloques de código

**Problema**: medir cuánto tarda una porción concreta del programa.

```cornamusa
importar tiempo

c = tiempo.cronometro()
total = 0
para i en rango(100000):
    total = total + i
fin para
imprimir(f"sumar 100k tardo {c.leer() * 1000:.1f}ms, total = {total}")
```

```
sumar 100k tardo 12.8ms, total = 4999950000
```

Usa `tiempo.monotonic()` (lo que hace `Cronometro` internamente) en lugar de `tiempo.epoch_segundos()` para medir duraciones: el monotónico no se ve afectado por cambios del reloj del sistema, NTP o zonas horarias.

Para profilear el programa entero por función: `cornamusa prof script.cor`.

---

## 7. Atributos computados con `@propiedad`

**Problema**: un atributo del objeto se deriva de otros — no quieres guardarlo redundantemente ni llamar a un método con `()`.

```cornamusa
clase Temperatura:
    funcion __iniciar__(yo, celsius):
        yo._c = celsius
    fin funcion

    @propiedad
    funcion celsius(yo):
        retornar yo._c
    fin funcion

    @propiedad
    funcion fahrenheit(yo):
        retornar yo._c * 9 / 5 + 32
    fin funcion

    funcion __cadena__(yo):
        retornar f"{yo._c}°C ({yo.fahrenheit}°F)"
    fin funcion
fin clase

t = Temperatura(25)
imprimir(t)
```

```
25°C (77.0°F)
```

Acceder `t.fahrenheit` invoca el getter — sin paréntesis. Útil para vistas derivadas (área a partir de ancho × alto, edad a partir de fecha de nacimiento, total con IVA, etc.). `@propiedad` solo provee getter en v1.81; setter explícito como método (`set_celsius(n)`) si lo necesitas.

---

## 8. Parser básico de argumentos del programa

**Problema**: leer argumentos pasados al script desde la línea de comandos.

```cornamusa
importar sistema

args = sistema.argv
si longitud(args) < 2:
    imprimir("uso: programa.cor <nombre> [edad]")
    edad = 0
    nombre = "anonimo"
sino:
    nombre = args[1]
    si longitud(args) > 2:
        edad = entero(args[2])
    sino:
        edad = 0
    fin si
fin si
imprimir(f"hola {nombre}, edad={edad}")
```

```bash
$ cornamusa --bytecode programa.cor Ana 30
hola Ana, edad=30

$ cornamusa --bytecode programa.cor
uso: programa.cor <nombre> [edad]
hola anonimo, edad=0
```

`sistema.argv[0]` es el nombre del archivo `.cor` (igual que `sys.argv[0]` en Python). Para parsing más sofisticado (flags `--xxx`, valores con `=`, etc.) escribe tu propia función — Cornamusa no incluye `argparse` todavía.

---

## 9. Contar frecuencias (counter dict-style)

**Problema**: contar cuántas veces aparece cada elemento de una colección.

```cornamusa
importar cadenas

texto = "el perro corre tras el gato y el gato escapa del perro"
palabras = cadenas.separar(texto, " ")
freq = {}
para p en palabras:
    si p en freq:
        freq[p] = freq[p] + 1
    sino:
        freq[p] = 1
    fin si
fin para
imprimir("frecuencias:", freq)
```

```
frecuencias: {"el": 3, "perro": 2, "corre": 1, "tras": 1, "gato": 2, "y": 1, "escapa": 1, "del": 1}
```

El diccionario preserva el orden de inserción (semántica Python 3.7+). Para encontrar el más frecuente: itera el dict y compara valores.

---

## 10. JSON pretty-print para configuración

**Problema**: serializar una estructura compleja a JSON legible (para guardar en disco o imprimir).

```cornamusa
importar json

config = {
    "version": "1.81.0",
    "modulos": ["jwt", "csv", "tiempo"],
    "activo": verdadero,
}
imprimir(json.serializar_indentado(config, 2))
```

```json
{
  "version": "1.81.0",
  "modulos": [
    "jwt",
    "csv",
    "tiempo"
  ],
  "activo": true
}
```

`json.serializar(...)` produce JSON compacto en una sola línea (útil para network/storage). `json.serializar_indentado(valor, n)` añade saltos de línea y `n` espacios de indentación — útil para humanos.

Para parsear: `json.parsear("...")` devuelve el valor correspondiente (dict, lista, número, etc.).

---

## Siguientes pasos

- **[Tutorial paso a paso](tutorial.md)**: si todavía no has aprendido el lenguaje, empieza aquí.
- **[Referencia rápida](referencia.md)**: sintaxis y stdlib completa.
- **[Ejemplos](https://github.com/David-Castilla-Gomez/Cornamusa/tree/main/examples)**: 72 programas, uno por feature.
- **[Issues en GitHub](https://github.com/David-Castilla-Gomez/Cornamusa/issues)**: ¿falta una receta? Pídela.
