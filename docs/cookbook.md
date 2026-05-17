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
11. [Validar email con regex](#11-validar-email-con-regex)
12. [Merge de configuración con defaults](#12-merge-de-configuración-con-defaults)
13. [Logger con niveles](#13-logger-con-niveles)
14. [CSV con headers a lista de dicts](#14-csv-con-headers-a-lista-de-dicts)
15. [Ordenar lista de dicts por campo](#15-ordenar-lista-de-dicts-por-campo)

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

## 11. Validar email con regex

**Problema**: aceptar o rechazar una cadena como email plausible. No queremos RFC 5322 completo, solo una primera barrera.

```cornamusa
importar regex

funcion email_valido(s):
    retornar regex.coincide(
        "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+[.][a-zA-Z][a-zA-Z]+$", s)
fin funcion

para s en ["ana@ejemplo.com", "no-email", "x@y.io", "user@x", "a.b+c@dom.io"]:
    imprimir(f"  {s} -> {email_valido(s)}")
fin para
```

```
  ana@ejemplo.com -> verdadero
  no-email -> falso
  x@y.io -> verdadero
  user@x -> falso
  a.b+c@dom.io -> verdadero
```

El motor regex de Cornamusa no soporta `{n,m}`, así que usamos `[a-zA-Z][a-zA-Z]+` (TLD de 2+ letras) en lugar de `[a-zA-Z]{2,}`. Para validación más estricta combina varias regex o usa una librería específica.

---

## 12. Merge de configuración con defaults

**Problema**: tienes un dict de defaults y otro con overrides del usuario. Quieres el resultado combinado.

```cornamusa
funcion fusionar(defaults, override):
    resultado = {}
    para k en claves(defaults):
        resultado[k] = defaults[k]
    fin para
    para k en claves(override):
        resultado[k] = override[k]
    fin para
    retornar resultado
fin funcion

DEFAULTS = {
    "host": "localhost",
    "puerto": 8080,
    "timeout": 30,
    "debug": falso,
}

usuario = {"host": "api.ejemplo.com", "debug": verdadero}
config = fusionar(DEFAULTS, usuario)
imprimir(config)
```

```
{"host": "api.ejemplo.com", "puerto": 8080, "timeout": 30, "debug": verdadero}
```

Las claves del override **sustituyen** las del defaults; las que no están en override conservan el default. Útil para cargar configuración desde JSON encima de constantes del código.

---

## 13. Logger con niveles

**Problema**: registrar mensajes con timestamp y nivel (DEBUG/INFO/WARN/ERROR), filtrando los que están por debajo del nivel mínimo.

```cornamusa
importar tiempo

DEBUG = 0
INFO = 1
WARN = 2
ERROR = 3

NOMBRES = ["DEBUG", "INFO", "WARN", "ERROR"]

clase Logger:
    funcion __iniciar__(yo, nivel_min=INFO):
        yo.nivel_min = nivel_min
    fin funcion

    funcion _registrar(yo, nivel, mensaje):
        si nivel < yo.nivel_min:
            retornar
        fin si
        ts = tiempo.epoch_segundos()
        imprimir(f"[{ts}] {NOMBRES[nivel]}: {mensaje}")
    fin funcion

    funcion debug(yo, m): yo._registrar(DEBUG, m) fin funcion
    funcion info(yo, m):  yo._registrar(INFO, m)  fin funcion
    funcion warn(yo, m):  yo._registrar(WARN, m)  fin funcion
    funcion error(yo, m): yo._registrar(ERROR, m) fin funcion
fin clase

log = Logger(INFO)
log.debug("este no se imprime")     # filtrado: DEBUG < INFO
log.info("servicio iniciado")
log.warn("disco al 85%")
log.error("conexion rechazada")
```

```
[1779039672] INFO: servicio iniciado
[1779039672] WARN: disco al 85%
[1779039672] ERROR: conexion rechazada
```

Para escribir a archivo en lugar de stdout, reemplaza `imprimir` por `archivos.agregar("app.log", linea + "\n")`. Para producción real considera incluir fecha legible (`fechas.formato(ts, "%Y-%m-%d %H:%M:%S")`) en lugar de epoch.

---

## 14. CSV con headers a lista de dicts

**Problema**: parsear CSV con primera fila de cabecera y obtener una lista de dicts (uno por fila), accesible por nombre de columna.

```cornamusa
importar csv
importar funcionales

funcion csv_a_dicts(texto):
    filas = csv.parsear(texto)
    si longitud(filas) == 0:
        retornar []
    fin si
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

datos = "nombre,edad,ciudad\nAna,30,Madrid\nLuis,25,Sevilla\nEva,40,Bilbao"
registros = csv_a_dicts(datos)
para r en registros:
    imprimir(f"  {r['nombre']} ({r['edad']}, {r['ciudad']})")
fin para
```

```
  Ana (30, Madrid)
  Luis (25, Sevilla)
  Eva (40, Bilbao)
```

Los valores son cadenas (CSV no infiere tipos). Convierte con `entero()`/`decimal()` cuando lo necesites.

---

## 15. Ordenar lista de dicts por campo

**Problema**: ordenar una colección de registros por un campo arbitrario.

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

productos = [
    {"nombre": "libro", "precio": 25},
    {"nombre": "lapiz", "precio": 3},
    {"nombre": "pluma", "precio": 15},
]

por_precio = ordenar_por(productos, "precio")
para p en por_precio:
    imprimir(f"  {p['precio']}€ — {p['nombre']}")
fin para
```

```
  3€ — lapiz
  15€ — pluma
  25€ — libro
```

Bubble sort, O(n²). Para listas grandes considera el `ordenar()` nativo con un comparator personalizado si Cornamusa lo soporta, o usa Schwartzian transform: `funcionales.mapear(lambda p: (p["precio"], p), lst)`, ordenar, extraer.

---

## Siguientes pasos

- **[Tutorial paso a paso](tutorial.md)**: si todavía no has aprendido el lenguaje, empieza aquí.
- **[Referencia rápida](referencia.md)**: sintaxis y stdlib completa.
- **[Ejemplos](https://github.com/David-Castilla-Gomez/Cornamusa/tree/main/examples)**: 82 programas, uno por feature.
- **[Issues en GitHub](https://github.com/David-Castilla-Gomez/Cornamusa/issues)**: ¿falta una receta? Pídela.
