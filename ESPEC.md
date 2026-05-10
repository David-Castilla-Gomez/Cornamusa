# Especificación del lenguaje Cornamusa

**Versión del documento:** 0.11.4
**Estado:** Estable (camino a v1.0).
**Última revisión:** 2026-04-30 — actualizada tras [B9 small-int tagging](decisiones/B9-small-int-tagging.md) y [B10 scope de v1.0](decisiones/B10-scope-de-v1.md).

Este documento define la sintaxis, semántica y vocabulario de Cornamusa, un lenguaje de programación dinámico interpretado con identidad castellana. La especificación es el contrato que une al implementador con el usuario del lenguaje: cualquier cambio aquí debe propagarse a `lexer.c`, `parser.c`, los built-ins de `nativos.c` y la documentación de usuario.

Las características marcadas con **(reservada)** o **(planeada)** describen sintaxis o semántica reservada para una versión futura — el parser puede aceptarla sin que la VM la implemente todavía. Son contratos intencionales con los usuarios para que su código sea forward-compatible.

---

## 1. Filosofía

1. **Castellano natural, no traducción literal.** Las palabras clave deben sonar idiomáticas a un hispanohablante, no como traducciones automáticas del inglés.
2. **Keywords ASCII, identificadores Unicode** (decisión [B4](decisiones/B4-tildes-y-unicode.md)). Las palabras clave del lenguaje son ASCII puro sin tildes ni `ñ` (`funcion`, no `función`); los identificadores definidos por el usuario admiten cualquier letra Unicode (`niño`, `año_actual`, `función_principal` son válidos).
3. **UTF-8 universal con normalización NFC.** El compilador es UTF-8 nativo y normaliza el código fuente a NFC antes de tokenizar (evita que el mismo identificador en NFC y NFD se considere distinto).
4. **Tipado dinámico fuerte.** Los valores tienen tipo en tiempo de ejecución; las variables no.
5. **Bloques delimitados explícitamente** con `:` al abrir y `fin <etiqueta>` al cerrar (decisión [B1](decisiones/B1-modelo-de-bloques.md)). La indentación es estilística, no semántica.
6. **Números con `.` decimal y `_` separador de miles** (decisión [B7](decisiones/B7-formato-numerico.md)). El formato castellano (`3,14`, `1.000.000`) es función de la biblioteca estándar, no de la sintaxis.
7. **Una sola forma evidente** de hacer cada cosa cuando sea posible.

---

## 2. Léxico

### 2.1 Caracteres y codificación

- Codificación de archivos fuente: **UTF-8 estricto** (con o sin BOM).
- Extensión canónica: `.cor`.
- Línea: `\n` (LF) o `\r\n` (CRLF). Se normalizan a `\n` internamente.

### 2.2 Identificadores

```
identificador  ← inicio_id continua_id*
inicio_id      ← Letra | "_" | "$"
continua_id    ← Letra | Dígito | "_" | "$"
```

Donde `Letra` incluye toda letra Unicode (categoría `L`), incluyendo `ñ`, `Ñ`, `á`, `é`, `í`, `ó`, `ú`, `ü`, etc.

**Normalización Unicode (decisión [B4](decisiones/B4-tildes-y-unicode.md)):** el lexer normaliza el archivo fuente a **NFC** (Normalization Form Canonical Composed) antes de tokenizar. Sin esta normalización, `función` escrito en macOS (NFD: `o` + acento combinante) sería un identificador distinto de `función` escrito en Windows (NFC: `ó` precompuesto), produciendo bugs invisibles entre sistemas.

**Sensibilidad a mayúsculas:** los identificadores son **case-sensitive**. `nombre`, `Nombre` y `NOMBRE` son tres identificadores distintos. Las keywords están todas en minúscula; escribir `Si` o `Funcion` es identificador, no keyword.

**Convenciones recomendadas:**
- Variables y funciones: `serpiente_minuscula` (`mi_variable`, `calcular_total`).
- Clases: `MayusculaCamello` (`Persona`, `ListaEnlazada`).
- Constantes: `MAYUSCULAS_CON_GUION` (`PI`, `MAX_INTENTOS`).
- Primer parámetro de métodos de instancia: **`yo`** (decisión [B5](decisiones/B5-B6-yo-y-dunders.md)). Es convención, no keyword: el nombre del parámetro es libre, pero `yo` es la forma canónica de la stdlib y los ejemplos oficiales. Ver §6.6.

### 2.3 Palabras clave

Todas las palabras clave son **ASCII puro, sin tildes ni `ñ`, en minúscula** (decisión [B4](decisiones/B4-tildes-y-unicode.md)). Esta es la **única forma aceptada** del lenguaje; `función` es un identificador, no una keyword.

#### Control de flujo
| Keyword | Significado |
|---|---|
| `si` | condicional |
| `sino` | rama alternativa |
| `sino si` | rama alternativa con condición (compuesta de dos tokens) |
| `mientras` | bucle por condición |
| `para` | bucle por iteración |
| `en` | operador de pertenencia / iteración |
| `romper` | salir del bucle |
| `continuar` | siguiente iteración |
| `retornar` | devolver valor |
| `pasar` | sentencia vacía |

#### Funciones, clases y módulos
| Keyword | Significado |
|---|---|
| `funcion` | declaración de función |
| `lambda` | función anónima (convención matemática internacional) |
| `clase` | declaración de clase |
| `extiende` | herencia (`clase Hija extiende Madre:`) |
| `super` | acceso a la superclase |
| `importar` | importación de módulo |
| `desde` | importación selectiva (`desde X importar Y`) |
| `como` | renombrado en import o except |
| `global` | declarar variable global |
| `nolocal` | declarar variable no-local en closure |

#### Manejo de excepciones
| Keyword | Significado |
|---|---|
| `intentar` | bloque protegido |
| `atrapar` | manejo de excepción |
| `finalmente` | bloque siempre ejecutado |
| `lanzar` | lanzar excepción |
| `fin` | cierre de bloque (siempre seguido de etiqueta: `fin si`, `fin funcion`, etc.) |

#### Operadores lógicos y comparativos (palabras)
| Keyword | Significado |
|---|---|
| `y` | conjunción lógica |
| `o` | disyunción lógica |
| `no` | negación lógica |
| `es` | identidad de objetos |
| `es no` | identidad negada |

#### Literales
| Keyword | Significado |
|---|---|
| `verdadero` | booleano cierto |
| `falso` | booleano falso |
| `nulo` | ausencia de valor |

#### Reservadas para futuro
`producir`, `asincrono`, `esperar`, `con`, `borrar`, `coincidir` (match-case).

### 2.4 Operadores y puntuación

| Símbolo | Nombre |
|---|---|
| `+ - * / % **` | aritméticos (suma, resta, mult., div., módulo, potencia) |
| `//` | división entera |
| `== != < > <= >=` | comparaciones |
| `= += -= *= /= %= **= //=` | asignación y compuestos |
| `( ) [ ] { }` | agrupaciones |
| `, : ; .` | separadores |
| `->` | anotación de retorno (opcional) |
| `@` | decorador |
| `&  | ^ ~ << >>` | bitwise (versión simbólica única, sin keywords) |

### 2.5 Literales

#### Numéricos
```
entero    ← dígito+ ("_" dígito+)*           # 1_000_000
binario   ← "0b" [01]+ ("_" [01]+)*
octal     ← "0o" [0-7]+ ("_" [0-7]+)*
hexa      ← "0x" [0-9a-fA-F]+ ("_" [0-9a-fA-F]+)*
decimal   ← entero "." entero (("e"|"E") ("+"|"-")? entero)?
```

**Formato decimal en código fuente** (decisión [B7](decisiones/B7-formato-numerico.md)): el separador decimal es siempre `.` (universal en programación), independientemente del idioma. El separador de miles es `_` (opcional, posición libre). La convención castellana de coma decimal y punto miles (`3,14`, `1.000.000`) **no se usa en el código** porque la coma colisionaría con el separador de elementos en listas y argumentos.

**Precisión de enteros** (decisión [B3](decisiones/B3-representacion-numerica.md)): los enteros son de **precisión arbitraria**. No hay overflow ni límite por hardware. `2 ** 1000` es un valor entero válido, igual que `factorial(100)`.

```cornamusa
pi = 3.14159
poblacion = 47_500_000
porcentaje_iva = 0.21
distancia = 1.496e11                  # notación científica
gugol = 10 ** 100                     # entero de 101 dígitos, sin overflow
```

**Formato castellano en E/S**: el módulo `formato` de la biblioteca estándar (Fase 9) ofrece:

```cornamusa
desde formato importar formatear, leer_numero

imprimir(formatear(3.14, locale="es"))      # → "3,14"
imprimir(formatear(1000000, locale="es"))   # → "1.000.000"
n = leer_numero("3,14", locale="es")        # → 3.14
```

#### Cadenas
- Comilla simple: `'hola'`
- Comilla doble: `"hola"`
- Triple comilla (multilínea): `"""..."""` o `'''...'''`
- Prefijo `f` (interpolación, **completo desde v1.1**): `f"hola, {nombre}, en {edad+10} anos"`. Cada `{expr}` se evalúa como expresión Cornamusa completa y se concatena con las partes literales. Llaves dobles `{{` y `}}` se preservan como llave literal. Anidación soportada (`f"{f'{x}'}"`). Triples (`f"""..."""`) reservadas para v1.2.
- Prefijo `b` (bytes): `b"\x00\xff"` — **reservado para v1.x**, aún no implementado.
- Prefijo `r` (raw, sin escapes): `r"C:\ruta"` — **reservado para v1.x**, aún no implementado.

**Secuencias de escape:** `\n`, `\t`, `\r`, `\\`, `\'`, `\"`, `\0`, `\xHH`, `\uHHHH`, `\u{HHHHHH}`.

#### Listas, diccionarios, tuplas, conjuntos
```
[1, 2, 3]                 # lista
(1, 2, 3)                 # tupla
{1, 2, 3}                 # conjunto
{"clave": "valor"}        # diccionario
{}                        # diccionario vacío (no conjunto)
conjunto()                # conjunto vacío
```

### 2.6 Comentarios

```
# Comentario de una línea hasta fin de línea

"""
Comentario multilínea (técnicamente una cadena documental
si está al inicio de un módulo, función o clase, en cuyo caso
se asocia como docstring accesible vía .__documentación__).
"""
```

### 2.7 Bloques

Cornamusa delimita los bloques con palabras clave explícitas, no con indentación significativa. La decisión y su justificación están en [`decisiones/B1-modelo-de-bloques.md`](decisiones/B1-modelo-de-bloques.md).

**Forma general:**

```
<cabecera>:
    <sentencias>
fin <etiqueta>
```

Donde `<etiqueta>` es la palabra clave que abrió el bloque.

#### Etiquetas de cierre

| Apertura | Cierre |
|---|---|
| `si` / `sino si` / `sino` | `fin si` |
| `mientras` | `fin mientras` |
| `para` | `fin para` |
| `funcion` / `función` | `fin funcion` |
| `clase` | `fin clase` |
| `intentar` / `atrapar` / `finalmente` | `fin intentar` |

**Cláusulas continuadoras** (`sino si`, `sino`, `atrapar`, `finalmente`) no son bloques independientes. Comparten el `fin` del bloque que las contiene:

```cornamusa
si x > 0:
    imprimir("positivo")
sino si x < 0:
    imprimir("negativo")
sino:
    imprimir("cero")
fin si
```

#### One-liners

Si tras `:` viene una sentencia simple en la misma línea, no se requiere `fin`:

```cornamusa
si x > 0: imprimir(x)
```

Si tras `:` viene un salto de línea, es un bloque multilínea y **exige** `fin <etiqueta>`.

#### Bloque vacío

Se rellena con `pasar`:

```cornamusa
funcion no_hace_nada():
    pasar
fin funcion
```

#### Indentación

- **No es semántica**: el lexer la ignora.
- **Recomendación de estilo**: 4 espacios, sin tabuladores.
- Editores y formateadores pueden imponerla por convención, pero el lenguaje no.

#### Anti-pattern

`fin` desnudo (sin etiqueta) es **error de sintaxis**:

```cornamusa
si x:
    hacer()
fin           # ✗ ErrorDeSintaxis: 'fin' requiere etiqueta ('fin si')
```

---

## 3. Tipos primitivos del runtime

| Tipo | Descripción | Inmutable |
|---|---|---|
| `entero` | Precisión arbitraria desde v0.4 (decisión [B3](decisiones/B3-representacion-numerica.md)). Sin overflow: `factorial(100)` funciona. Implementación: bignum boxed en v0.4-v0.5; tagged i63 + bignum desde v0.6 (fast path 1-3 ciclos, slow path con promoción transparente). | sí |
| `decimal` | Coma flotante IEEE 754 64-bit | sí |
| `booleano` | `verdadero` / `falso` | sí |
| `nulo` | Único valor: `nulo` | sí |
| `cadena` | Texto Unicode UTF-8 | sí |
| `bytes` | Secuencia inmutable de bytes | sí |
| `lista` | Secuencia mutable indexada | no |
| `tupla` | Secuencia inmutable indexada | sí |
| `diccionario` | Mapa hash mutable. **Orden de inserción NO preservado en v0.11.5** (`claves(d)` y `valores(d)` devuelven orden de tabla hash interna). Preservar el orden está reservado para v1.x. | no |
| `conjunto` | Conjunto hash mutable | no |
| `función` | Callable de primera clase | — |
| `clase` | Plantilla de objetos | — |
| `instancia` | Objeto creado a partir de una clase | — |

---

## 4. Built-ins

### 4.1 Funciones globales

Esta es la lista **real** de built-ins disponibles en v1.1.0 (registrados en `src/nativos.c`).

#### E/S y conversión
| Cornamusa | Equivalente Python | Descripción |
|---|---|---|
| `imprimir(...)` | `print(*args)` | Imprime valores separados por espacio + salto de línea |
| `leer([prompt])` | `input([prompt])` | Lee una línea de stdin. Sin prompt o con cadena prompt. EOF inmediato → `""`. |
| `tipo(x)` | `type(x).__name__` | Devuelve el nombre del tipo como cadena ("entero", "lista", ...) |
| `cadena(x)` | `str(x)` | Coerción a cadena (representación canónica de `imprimir`). Idempotente sobre cadenas. |
| `entero(x)` | `int(x)` | Coerción a entero. Acepta entero, decimal (truncar), booleano, cadena (parse base 10). |
| `decimal(x)` | `float(x)` | Coerción a decimal. Acepta decimal, entero, booleano, cadena (`strtod`). |
| `booleano(x)` | `bool(x)` | Coerción a booleano según truthiness §6.2. Siempre éxito. |
| `lista(iter)` | `list(iter)` | Materializa un iterable como lista. Acepta lista/tupla/conjunto/cadena/rango/dicc. |
| `tupla(iter)` | `tuple(iter)` | Como `lista(iter)` pero devuelve tupla inmutable. |
| `diccionario(pares)` | `dict(pares)` | Construye dicc desde iterable de pares `(clave, valor)` o desde otro dicc. |

#### Tamaño e iteración
| Cornamusa | Equivalente Python | Descripción |
|---|---|---|
| `longitud(x)` | `len(x)` | Tamaño de cadena, lista, dicc, conjunto, tupla, rango |
| `rango(fin)`, `rango(inicio, fin)`, `rango(inicio, fin, paso)` | `range` | Construye un iterador entero perezoso |

#### Colecciones (mutación / consulta)
| Cornamusa | Descripción |
|---|---|
| `agregar(lista, valor)` | Añade `valor` al final de `lista`. Devuelve `nulo`. |
| `quitar(lista_o_dicc_o_conj, clave_o_indice)` | Quita por índice (lista) / clave (dicc, conjunto). Devuelve el valor quitado. |
| `insertar(lista, indice, valor)` | Inserta en posición `indice`, desplazando el resto. |
| `invertir(lista)` | Invierte una lista in-place. Devuelve `nulo`. |
| `ordenar(lista, invertido=falso)` | Ordena in-place. `invertido=verdadero` para descendente. |
| `claves(dicc)` | Lista con las claves del diccionario. |
| `valores(dicc)` | Lista con los valores del diccionario. |
| `conjunto(iter?)` | Construye un conjunto vacío `conjunto()` o desde un iterable. |

#### Excepciones (constructores de clases built-in)
| Cornamusa | Descripción |
|---|---|
| `Excepcion(mensaje)` | Excepción base. |
| `ErrorAritmetico(mensaje)` | Error matemático (división por cero, overflow lógico). |
| `ErrorDeTipo(mensaje)` | Operación sobre tipo incorrecto. |
| `ErrorDeValor(mensaje)` | Valor de tipo correcto pero inválido (e.g. `factorial(-1)`). |
| `ErrorDeIndice(mensaje)` | Índice fuera de rango. |
| `ErrorDeClave(mensaje)` | Clave no presente en diccionario. |
| `ErrorDeNombre(mensaje)` | Identificador no definido (lanzado automáticamente por la VM). |

#### Sistema y memoria
| Cornamusa | Descripción |
|---|---|
| `recolectar()` | Fuerza una pasada del GC mark-sweep. Devuelve `nulo`. |
| `obtener_argv()` | Lista de cadenas con los argumentos del programa (incluido el nombre del .cor en posición 0). Expuesta también via `sistema.argv` (importar `sistema`). |
| `salir(codigo=0)` | Termina el proceso inmediatamente con el código indicado. No retorna. |

#### Numéricos y reflexión (v1.11)
| Cornamusa | Equivalente Python | Descripción |
|---|---|---|
| `absoluto(n)` | `abs(n)` | Valor absoluto de entero (incluido bignum), decimal o booleano. |
| `redondear(n)`, `redondear(n, k)` | `round(n)` (half-away-from-zero) | A entero (1 arg) o a decimal con `k` cifras (2 args). |
| `instancia_de(obj, clase)` | `isinstance(obj, clase)` | Verdadero si `obj` es instancia de `clase` o subclase. Tipos primitivos siempre falso. |
| `subclase_de(A, B)` | `issubclass(A, B)` | Verdadero si A == B o A hereda de B. Reflexivo. |
| `id(obj)` | `id(obj)` | Entero único de la identidad del objeto. |
| `repr(obj)` | `repr(obj)` | Cadena con la representación literal (con comillas para cadenas). |

### 4.2 Built-ins planeados (no en v1.1.0)

**Implementados en v1.11**:

- `absoluto`, `redondear`, `instancia_de`, `subclase_de`, `id`, `repr` —
  built-ins globales en C. Ver §4.1.
- `mapear`, `filtrar`, `reducir`, `enumerar`, `enumerar_desde`, `suma`,
  `suma_desde`, `mínimo`/`minimo`, `máximo`/`maximo`, `cualquiera`,
  `todos` — viven en el módulo importable `funcionales` (Cornamusa
  puro). Ejemplo:

  ```cornamusa
  importar funcionales
  total = funcionales.reducir(lambda a, x: a + x, [1, 2, 3], 0)
  ```

**Aplazados a v1.13+** (requieren cambios al runtime o a la sintaxis):

- `abrir(ruta, modo)` → context manager con `__entrar__`/`__salir__`
  (planeado para v1.13 con keyword `con`).
- `iterar`, `siguiente` → cuando los dunders `__iterar__`/`__siguiente__`
  se implementen para `para ... en` sobre instancias custom (v1.12).
- `resumen` → reservado, sin diseño cerrado.

Hasta entonces, escribir `abrir(...)` o cualquier nombre aplazado da
`ErrorDeNombre` igual que cualquier identificador no definido.

### 4.3 Métodos especiales (dunders)

La filosofía de Cornamusa (decisión [B5+B6](decisiones/B5-B6-yo-y-dunders.md)) es que los dunders son **castellanos** (`__iniciar__` no `__init__`).

**Implementados en v1.3.0** (solo bytecode VM — tree-walking no soporta clases):

| Dunder | Operador / built-in | Aridad |
|---|---|---|
| `__iniciar__` | `Foo(args)` (constructor) | n+1 (yo + args) |
| `__sumar__` | `+` | 2 (yo, otro) |
| `__restar__` | `-` (binario) | 2 |
| `__multiplicar__` | `*` | 2 |
| `__dividir__` | `/` | 2 |
| `__dividir_entero__` | `//` | 2 |
| `__modulo__` | `%` | 2 |
| `__potencia__` | `**` | 2 |
| `__igual__` | `==` | 2 |
| `__distinto__` | `!=` | 2 |
| `__menor__` | `<` | 2 |
| `__menor_igual__` | `<=` | 2 |
| `__mayor__` | `>` | 2 |
| `__mayor_igual__` | `>=` | 2 |
| `__cadena__` | `f"{obj}"`, `imprimir(obj)`, `cadena(obj)` | 1 (yo) |
| `__indice__` | `obj[k]` | 2 (yo, clave) |
| `__asignar_indice__` | `obj[k] = v` | 3 (yo, clave, valor) |
| `__longitud__` | `longitud(obj)` | 1 (yo) |
| `__llamar__` | `obj(args)` | n+1 (yo + args) |
| `__sumar_derecho__` | `5 + V(...)` cuando V no tiene `__sumar__` | 2 (yo, otro) |
| `__restar_derecho__` | `5 - V(...)` etc. | 2 |
| `__multiplicar_derecho__` | `3 * V(...)` etc. | 2 |
| `__dividir_derecho__`, `__dividir_entero_derecho__`, `__modulo_derecho__`, `__potencia_derecho__` | reflejados aritméticos | 2 |

Reglas:
- El dunder normal solo se invoca si el operando IZQUIERDO es VAL_INSTANCIA y la clase lo define. Si izq no maneja, se busca el dunder reflejado en el lado DERECHO (solo aritméticos).
- En reflejado, el receptor (`yo`) es el operando DERECHO y el primer argumento (`otro`) es el IZQUIERDO.
- `__cadena__` debe retornar cadena. Si retorna otro tipo, ErrorDeTipo.
- `__longitud__` y los aritméticos pueden retornar cualquier tipo, pero el caller suele asumir entero (e.g. `len(obj) * 2`).
- Para `==`, `<`, etc. el resultado se interpreta según las reglas de truthiness §6.2 cuando se usa en `si`.
- `==` y `!=` NO se auto-derivan: si defines solo `__igual__`, `obj != obj` cae al path de identidad por defecto.

**Aplazados a v1.4+**: dunders de iteración (`__iterar__`, `__siguiente__`), `__hash__` y `__es_hashable__`.

```cornamusa
clase Persona:
    funcion __iniciar__(yo, nombre):       # ← este SÍ se invoca por Persona("Ana")
        yo.nombre = nombre
    fin funcion

    funcion __sumar__(yo, otro):           # ← invocado por p1 + p2 (v1.2)
        retornar Persona(yo.nombre + otro.nombre)
    fin funcion
fin clase
```

Para v1.x se planea soporte de los siguientes dunders (la sintaxis se acepta hoy para forward-compatibility):

- **Construcción/representación**: `__cadena__`, `__repr__`, `__booleano__`, `__finalizar__`.
- **Comparaciones**: `__igual__`, `__distinto__`, `__menor__`, `__menor_igual__`, `__mayor__`, `__mayor_igual__`, `__resumen__`.
- **Colecciones**: `__longitud__`, `__obtener__`, `__establecer__`, `__borrar__`, `__contiene__`, `__iterar__`, `__siguiente__`.
- **Aritméticos**: `__sumar__`, `__restar__`, `__multiplicar__`, `__dividir__`, `__div_entera__`, `__modulo__`, `__potencia__`, `__negar__`, `__positivar__`, `__absoluto__`.
- **Llamada/contexto**: `__llamar__`, `__entrar__`, `__salir__` (con `con` también planeado).
- **Atributos dinámicos**: `__obtener_atributo__`, `__establecer_atributo__`, `__borrar_atributo__`.

### 4.4 Biblioteca estándar (stdlib)

En v0.11.4 hay tres módulos importables. Los archivos viven en `stdlib/*.cor` y se cargan via `importar`:

#### `matematicas`
Constantes y funciones matemáticas escritas en Cornamusa puro.

```cornamusa
importar matematicas
imprimir(matematicas.PI)              # 3.141592653589793
imprimir(matematicas.E)               # 2.718281828459045
imprimir(matematicas.cuadrado(5))     # 25
imprimir(matematicas.cubo(4))         # 64
imprimir(matematicas.absoluto(-3))    # 3
imprimir(matematicas.maximo(7, 2))    # 7
imprimir(matematicas.minimo(7, 2))    # 2
imprimir(matematicas.signo(-5))       # -1
imprimir(matematicas.factorial(10))   # 3628800
imprimir(matematicas.suma_rango(1, 11))   # 1+2+...+10 = 55
imprimir(matematicas.es_par(4))       # verdadero
imprimir(matematicas.es_impar(7))     # verdadero
imprimir(matematicas.mcd(12, 18))     # 6
```

#### `cadenas`
Operaciones sobre texto que requieren indexación UTF-8 (introducidas en v0.9.1).

```cornamusa
importar cadenas
imprimir(cadenas.repetir("=", 20))                # "===================="
imprimir(cadenas.empieza_con("hola.cor", "hola")) # verdadero
imprimir(cadenas.termina_con("foo.txt", ".txt"))  # verdadero
imprimir(cadenas.contar("aaabaa", "aa"))          # 2
imprimir(cadenas.caracter("Cornamusa", 4))        # "a"
```

#### `sistema`
Acceso a metadatos del proceso.

```cornamusa
importar sistema
para arg en sistema.argv:
    imprimir(arg)
fin para
si longitud(sistema.argv) < 2:
    imprimir("Uso: programa <archivo>")
    salir(1)
fin si
```

`salir(codigo)` está disponible como built-in global directamente, sin necesidad de importar `sistema`.

---

## 5. Gramática (PEG)

Notación: `←` define una regla, `/` es elección priorizada (PEG), `*` repetición ≥0, `+` repetición ≥1, `?` opcional, `&` y `!` son lookaheads positivo/negativo, `[abc]` clases de caracteres, `"..."` literales.

```peg
# ───── Programa ─────
programa       ← LF* sentencia* FIN

# ───── Sentencias ─────
sentencia      ← sent_compuesta
                / sent_simple LF

sent_compuesta ← sent_si
                / sent_mientras
                / sent_para
                / sent_funcion
                / sent_clase
                / sent_intentar
                / sent_con

sent_simple    ← sent_asignacion
                / sent_expresion
                / sent_retornar
                / sent_lanzar
                / sent_importar
                / sent_global
                / sent_nolocal
                / "romper"
                / "continuar"
                / "pasar"

# ───── Si / Sino si / Sino ─────
sent_si        ← "si" expr ":" cuerpo
                ("sino" "si" expr ":" cuerpo)*
                ("sino" ":" cuerpo)?
                "fin" "si"

sent_mientras  ← "mientras" expr ":" cuerpo
                ("sino" ":" cuerpo)?
                "fin" "mientras"

sent_para      ← "para" lista_objetivos "en" expr ":" cuerpo
                ("sino" ":" cuerpo)?
                "fin" "para"

# ───── Funciones ─────
sent_funcion   ← decoradores? ("función" / "funcion")
                IDENT "(" parametros? ")" anot_retorno? ":" cuerpo
                "fin" ("función" / "funcion")

parametros     ← parametro ("," parametro)*
parametro      ← IDENT (":" expr)? ("=" expr)?

anot_retorno   ← "->" expr

# ───── Clases ─────
sent_clase     ← decoradores? "clase" IDENT
                ("extiende" expr ("," expr)*)? ":" cuerpo
                "fin" "clase"

# ───── Excepciones ─────
sent_intentar  ← "intentar" ":" cuerpo
                ("atrapar" expr ("como" IDENT)? ":" cuerpo)+
                ("sino" ":" cuerpo)?
                ("finalmente" ":" cuerpo)?
                "fin" "intentar"

sent_lanzar    ← "lanzar" expr? ("desde" expr)?

# ───── Imports ─────
sent_importar  ← "importar" ruta_modulo ("como" IDENT)?
                / "desde" ruta_modulo "importar"
                  (IDENT ("como" IDENT)? ("," IDENT ("como" IDENT)?)* / "*")

ruta_modulo    ← IDENT ("." IDENT)*

# ───── Asignación ─────
sent_asignacion ← lista_objetivos "=" expr
                / objetivo aug_op expr      # +=, -=, etc.

aug_op         ← "+=" / "-=" / "*=" / "/=" / "//=" / "%=" / "**="
                / "&=" / "|=" / "^=" / "<<=" / ">>="

# ───── Cuerpo de bloque ─────
# El cierre con "fin <etiqueta>" lo hace cada sentencia compuesta arriba.
# Aquí solo definimos el contenido entre apertura y cierre.
cuerpo         ← LF sentencia+         # cuerpo multilínea
                / sent_simple LF       # one-liner: "si x: hacer()"

# ───── Expresiones (Pratt parser, precedencia ascendente) ─────
expr           ← expr_o
expr_o         ← expr_y ("o" expr_y)*
expr_y         ← expr_no ("y" expr_no)*
expr_no        ← "no" expr_no / expr_comp
expr_comp      ← expr_bit_o (op_comp expr_bit_o)*
op_comp        ← "==" / "!=" / "<=" / ">=" / "<" / ">"
                / "es" "no"? / "no"? "en"

expr_bit_o     ← expr_bit_x ("|" expr_bit_x)*
expr_bit_x     ← expr_bit_y ("^" expr_bit_y)*
expr_bit_y     ← expr_desp ("&" expr_desp)*
expr_desp      ← expr_suma (("<<" / ">>") expr_suma)*

expr_suma      ← expr_mult (("+" / "-") expr_mult)*
expr_mult      ← expr_unario (("*" / "/" / "//" / "%") expr_unario)*
expr_unario    ← ("+" / "-" / "~") expr_unario / expr_pot
expr_pot       ← expr_postfijo ("**" expr_unario)?

expr_postfijo  ← expr_atomo postfijo*
postfijo       ← "(" args? ")"
                / "[" rebanada "]"
                / "." IDENT

expr_atomo     ← literal / IDENT / "(" expr ")" / lista_lit / dicc_lit
                / "yo" / "super" "." IDENT / cadena_f / lambda_lit

lambda_lit     ← "lambda" parametros? ":" expr
```

(Esta gramática es indicativa. Ambigüedades menores se resuelven con lookaheads en el parser PEG.)

---

## 6. Semántica clave

### 6.1 Modelo de scoping

- **Léxico** (estilo Python).
- Resolución LEGB-equivalente: Local → Enclosing (closures) → Global (módulo) → Builtins.
- Variables se definen por asignación; `global` y `nolocal` requeridos para escribir en scopes externos.

### 6.2 Verdadez (truthiness)

Son **falsos**: `falso`, `nulo`, `0`, `0.0`, `""`, `[]`, `()`, `{}`, `conjunto()`, y cualquier objeto que defina `__booleano__` y devuelva `falso`.  
Todo lo demás es **verdadero**.

### 6.3 Comparación de igualdad

- `==` invoca `__igual__`. Por defecto, identidad (`es`).
- `es` compara identidad de objeto (mismo puntero en memoria).

### 6.4 Iteración

`para x en y:` invoca `iterar(y)` y luego `siguiente(it)` hasta que se levante `FinDeIteración`.

### 6.5 Excepciones

Jerarquía mínima:
```
Excepción
├── ErrorRuntime
│   ├── ErrorDeTipo
│   ├── ErrorDeValor
│   ├── ErrorDeNombre
│   ├── ErrorDeAtributo
│   ├── ErrorDeIndice
│   ├── ErrorDeClave
│   ├── ErrorDivisiónPorCero
│   └── DesbordeDePila
├── ErrorDeSintaxis
├── ErrorDeImportación
├── ErrorDeIO
├── FinDeIteración    # señal, no error
└── InterrupciónTeclado
```

### 6.6 Modelo de objetos

- Toda entidad es un objeto con tipo (`tipo(x)`) y identidad (`id(x)`).
- Atributos accesibles vía `obj.atributo`.
- Acceso dinámico mediante `__obtener_atributo__` / `__establecer_atributo__` / `__borrar_atributo__` (ver §4).
- En v0.10.0 se introducen **shapes / hidden classes** (paper SELF) para optimizar acceso, transparente al usuario.

#### Métodos de instancia y la convención `yo`

Los métodos de una clase reciben **explícitamente** la instancia como primer parámetro (estilo Python, no Java/Ruby). El nombre del parámetro es **libre**: no es palabra clave reservada. La convención de Cornamusa es llamarlo `yo` (decisión [B5+B6](decisiones/B5-B6-yo-y-dunders.md)).

```cornamusa
clase Persona:
    funcion __iniciar__(yo, nombre):     # convención
        yo.nombre = nombre
    fin funcion

    funcion saludar(propio):              # también válido (nombre libre)
        retornar f"Hola, {propio.nombre}"
    fin funcion
fin clase
```

La stdlib y los ejemplos oficiales usan `yo` consistentemente. Los formateadores y linters (futuros) sugerirán `yo` por convención, sin imponerlo.

#### Operadores y dunders

Los operadores se desazucaran a llamadas a dunders. Por ejemplo:

| Sintaxis | Búsqueda en runtime |
|---|---|
| `a + b` | `tipo(a).__sumar__(a, b)`; si no existe o devuelve `NoImplementado`, prueba `tipo(b).__sumar_derecha__(b, a)` |
| `longitud(x)` | `tipo(x).__longitud__(x)` |
| `x en y` | `tipo(y).__contiene__(y, x)` |
| `para x en y:` | `it = tipo(y).__iterar__(y); valor = tipo(it).__siguiente__(it); ...` |
| `obj[k]` | `tipo(obj).__obtener__(obj, k)` |

Si la clase no implementa el dunder correspondiente, el runtime lanza `ErrorDeTipo` con mensaje específico (ej. *"el tipo `Persona` no soporta el operador `+`"*).

> **Nota v0.11.4**: solo `__iniciar__` se invoca automáticamente. Los demás se mencionan aquí como contrato de diseño para v1.x. Ver §4.3.

### 6.7 Módulos

Cornamusa carga código de otros archivos `.cor` mediante el sistema de módulos (decisión y diseño en código de F9). Los módulos viven en:

1. El directorio del programa principal (resolución relativa).
2. `stdlib/` adyacente al ejecutable de Cornamusa.

#### Sintaxis

```cornamusa
importar matematicas                 # global `matematicas` accesible
importar matematicas como mat        # alias: matematicas registrado como `mat`
desde matematicas importar PI, factorial   # selectivo: PI y factorial directos
desde matematicas importar factorial como fact  # con alias
importar mat.geometria               # subsegmentos: busca mat/geometria.cor
```

#### Semántica

- Cada módulo se carga **una vez**: posteriores `importar` retornan el mismo objeto cacheado.
- El módulo es un valor de tipo `modulo` con atributos accesibles via `m.x`.
- Las funciones definidas en un módulo **capturan las globales del módulo** (closures sobre el dicc del módulo). Una función importada y llamada desde el importador sigue viendo las globales originales.

```cornamusa
# matematicas.cor
PI = 3.141592653589793

funcion area_circulo(r):
    retornar PI * r * r            # PI viene de matematicas, no del importador
fin funcion
```

```cornamusa
# uso.cor
desde matematicas importar area_circulo
PI = 3.0    # local del importador, NO afecta a matematicas.PI
imprimir(area_circulo(2))   # 12.566... usando matematicas.PI, no el local
```

#### Limitaciones (v0.11.4)

- `desde X importar *` no soportado (anti-patrón).
- Los módulos no soportan `__iniciar__` ni código de inicialización condicional avanzada — todo el cuerpo del módulo se ejecuta secuencialmente al cargar.

### 6.8 Closures, lambdas y upvalues

Las funciones definidas dentro de otra función capturan las variables del scope enclosing como **upvalues**. **Nota importante**: a diferencia de Python, en Cornamusa la asignación a una variable existente en un scope envolvente YA va a ese scope por default (semántica Lua). `nolocal` es una declaración EXPLÍCITA que hace la intención clara y valida que el nombre exista en algún scope padre — sin ella el código sigue funcionando, pero con ella se cazan typos en compile-time:

```cornamusa
funcion contador():
    n = 0
    funcion siguiente():
        nolocal n             # explícito: 'n' es del scope envolvente.
        n = n + 1
        retornar n
    fin funcion
    retornar siguiente
fin funcion

c = contador()
imprimir(c())   # 1
imprimir(c())   # 2
imprimir(c())   # 3
```

`nolocal x, y, z` declara múltiples nombres a la vez. Es error de compilación:
- Usar `nolocal` fuera de una función.
- Usar `nolocal x` cuando `x` ya es local del scope actual.
- Usar `nolocal x` cuando `x` no existe como local en ningún scope envolvente.

`lambda` define una función anónima de una sola expresión:

```cornamusa
cuadrado = lambda x: x * x
imprimir(cuadrado(7))   # 49

# Útil con primitivas que aceptan callables:
ordenar(personas, clave=lambda p: p.edad)   # (clave= aún no implementado en ordenar v0.11)
```

Los upvalues se cierran (capturan el valor actual del slot del frame enclosing) cuando la función enclosing retorna, manteniendo viva la closure incluso después.

### 6.9 Slicing e indexación

Las **listas** y **cadenas** soportan indexación con `[i]` (positivo o negativo) y rebanadas `[i:f:p]`:

```cornamusa
xs = [10, 20, 30, 40, 50]
imprimir(xs[0])      # 10
imprimir(xs[-1])     # 50 (último)
imprimir(xs[1:4])    # [20, 30, 40]
imprimir(xs[::-1])   # [50, 40, 30, 20, 10] (paso negativo)
imprimir(xs[::2])    # [10, 30, 50]

s = "Cornamusa"
imprimir(s[0])       # "C"
imprimir(s[-2])      # "s"
# (slicing de cadenas planeado para v1.x; en v0.11.4 solo s[i] de un carácter.)
```

Los **diccionarios** se indexan por clave (no por posición):

```cornamusa
d = {"nombre": "Ana", "edad": 30}
imprimir(d["nombre"])    # "Ana"
d["edad"] = 31           # asignación
```

Acceso a clave inexistente lanza `ErrorDeClave`.

### 6.10 Operadores `es` y `en`

- `a es b` → identidad (mismo objeto en memoria). Para inmutables pequeños (enteros, booleanos), puede dar resultados implementación-dependientes; usar `==` para comparación de valor.
- `a no es b` → identidad negada.
- `x en y` → pertenencia. Funciona con listas, tuplas, conjuntos, diccionarios (busca clave), cadenas (busca subcadena).
- `x no en y` → pertenencia negada.

### 6.11 Aritmética entera y bignum (B3 + B9)

Los enteros son de **precisión arbitraria** (decisión [B3](decisiones/B3-representacion-numerica.md)). `2 ** 1000` es válido sin overflow. Internamente (decisión [B9](decisiones/B9-small-int-tagging.md)):

- Enteros que caben en 63 bits viven inline en el `Valor` (representación SMALL).
- Enteros más grandes son punteros a `mp_int` (libtommath, BIG).
- Las operaciones SMALL+SMALL se ejecutan inline con detección de overflow; cuando un resultado excede 63 bits se promueve a BIG transparentemente.

Esta distinción es **invisible al programa**: `tipo(5)` y `tipo(2 ** 1000)` ambos devuelven `"entero"`. Los programas no necesitan distinguir SMALL de BIG.

---

## 7. Programa de ejemplo (sintaxis canónica)

```cornamusa
# Hola mundo
imprimir("¡Hola, mundo!")

# Función con argumentos por defecto
funcion saludar(nombre, idioma="es"):
    si idioma == "es":
        retornar f"Hola, {nombre}"
    sino si idioma == "en":
        retornar f"Hello, {nombre}"
    sino:
        lanzar ErrorDeValor(f"Idioma no soportado: {idioma}")
    fin si
fin funcion

# Bucle e iteración
para i en rango(1, 11):
    si i % 3 == 0 y i % 5 == 0:
        imprimir("FizzBuzz")
    sino si i % 3 == 0:
        imprimir("Fizz")
    sino si i % 5 == 0:
        imprimir("Buzz")
    sino:
        imprimir(i)
    fin si
fin para

# Clases y herencia
clase Animal:
    funcion __iniciar__(yo, nombre):
        yo.nombre = nombre
    fin funcion

    funcion hablar(yo):
        lanzar ErrorRuntime("método abstracto")
    fin funcion
fin clase

clase Perro extiende Animal:
    funcion hablar(yo):
        retornar f"{yo.nombre} dice guau"
    fin funcion
fin clase

# Manejo de excepciones
intentar:
    archivo = abrir("datos.txt")
    contenido = archivo.leer()
atrapar ErrorDeIO como e:
    imprimir(f"No se pudo leer: {e}")
finalmente:
    si archivo no es nulo:
        archivo.cerrar()
    fin si
fin intentar
```

---

## 8. Diferencias intencionadas con Python

| Aspecto | Python | Cornamusa |
|---|---|---|
| Constructor | `__init__` | `__iniciar__` |
| `self` | parámetro convencional | `yo`, parámetro convencional |
| `True`/`False`/`None` | constantes | `verdadero`/`falso`/`nulo` |
| `and`/`or`/`not` | keywords | `y`/`o`/`no` |
| `is` | keyword | `es` |
| Herencia | `class A(B):` | `clase A extiende B:` |
| Excepción | `try/except` | `intentar/atrapar` |
| `print` | función | `imprimir` |
| `len` | función | `longitud` |
| `range` | función | `rango` |

---

## 9. Decisiones cerradas y trabajo futuro

### Decisiones de diseño (ADRs en `decisiones/`)

| ID | Tema | Estado |
|---|---|---|
| B1 | Modelo de bloques (`fin <etiqueta>`) | ✅ `decisiones/B1-modelo-de-bloques.md` |
| B2 | Tree-walking + bytecode (AST compartido) | ✅ `decisiones/B2-tree-walking-vs-bytecode.md` |
| B3 | Representación numérica (bignum desde día 1) | ✅ Cerrado en v0.4 (boxed mp_int); refinado en v0.11 con tagged SMALL+BIG (B9) |
| B4 | Tildes y Unicode (keywords ASCII, ids Unicode NFC) | ✅ `decisiones/B4-tildes-y-unicode.md` |
| B5+B6 | `yo` + dunders castellanos | ✅ `decisiones/B5-B6-yo-y-dunders.md` |
| B7 | Formato numérico (`.` decimal, `_` miles) | ✅ `decisiones/B7-formato-numerico.md` |
| B8 | Inline caching tipo PEP 659 | ✅ `decisiones/B8-inline-caching.md` (v0.10.0) |
| B9 | Small-int tagging (i63 inline) | ✅ `decisiones/B9-small-int-tagging.md` (v0.11.0) |
| B10 | Scope de v1.0 (docs sobre GC generacional) | ✅ `decisiones/B10-scope-de-v1.md` |

### Reservas para v1.x (no implementadas en v0.11.4)

1. **F-strings con expresiones**. El parser acepta `f"hola {nombre}"`, pero la VM aún no las interpola. Llegarán en v1.1+.
2. **`con` (context managers)**. Reservada como keyword. Implementación pendiente.
3. **Pattern matching (`coincidir`)**. Reservada como keyword. Diseño pendiente.
4. **`borrar` (`del` de Python)**. Reservada.
5. **Generadores (`producir` ≈ `yield`)**. Reservada.
6. **Async/await (`asincrono`/`esperar`)**. Reservadas. v2.0+.
7. **Tipos numéricos exactos**. `Fraccion`, `Decimal` en stdlib; v1.x.
8. **Anotaciones de tipo (`: tipo`)**. La gramática las acepta pero el runtime las ignora.
9. **Decoradores (`@deco`)**. La gramática los acepta. Runtime: parcial.
10. **Dunders distintos de `__iniciar__`**. Reservados. v1.x.

### Trabajo de runtime pendiente (no afecta a la sintaxis)

- **GC generacional** — postergado a post-v1.0 (decisión B10). El GC mark-sweep tri-color actual es suficiente para los workloads conocidos.
- **Threaded code dispatch** — descartado tras análisis ROI/coste post-v0.11.
- **JIT / tracing** — Fase 12+ del plan; aplazadas indefinidamente.
- **Concurrencia / hilos** — decisión I3 aplazada al post-v1.0.

---

## 10. Glosario rápido

| Término | Definición |
|---|---|
| **Chunk** | Bloque compilado de bytecode + constantes + line info |
| **Frame** | Contexto de ejecución de una llamada (stack pointer, IP, locals) |
| **Shape** | Estructura interna de un objeto (orden de slots), compartida entre instancias |
| **PIC** | Polymorphic Inline Cache, caché en el call site con varios casos |
| **Quickening** | Sustituir un opcode genérico por uno especializado tras observar tipos |
| **Tree-walking** | Intérprete que evalúa el AST recursivamente, sin bytecode |
| **Tri-color marking** | Algoritmo de GC con conjuntos blanco/gris/negro |

---

*Documento vivo. Última edición acompaña el commit que la introduce.*
