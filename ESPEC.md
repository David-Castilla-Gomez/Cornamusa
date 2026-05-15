# Especificación del lenguaje Cornamusa

**Versión del documento:** 1.43.0
**Estado:** Estable.
**Última revisión:** 2026-05-15 — actualizada a v1.43 (`__siguiente__` + `ErrorDeIteracion`: iteradores lazy stateful).

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
| `como` | renombrado en import, `atrapar` o patrón de `coincidir` |
| `nolocal` | declarar variable no-local en closure (escritura a upvalue) |
| `producir` | producir un valor desde un generador (`producir`, `producir desde`) |

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

#### Control de bloques adicionales
| Keyword | Significado |
|---|---|
| `con` | context manager (`con expr como x:`, v1.13) |
| `coincidir` / `cuando` | pattern matching (v1.15) |

#### Reservadas para futuro
`global` (declarar global escribible — reservada, sin implementar en la VM), `asincrono`, `esperar` (async/await, v2.x), `borrar` (`del` de Python).

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

**Formato en E/S**: el módulo `formato` de la biblioteca estándar ofrece presentación numérica:

```cornamusa
importar formato

imprimir(formato.con_decimales(3.14159, 2))           # → "3.14"
imprimir(formato.numero_con_separador(1000000))       # → "1_000_000"
imprimir(formato.porcentaje(0.215))                   # → "21.50%"
imprimir(formato.como_hex(255))                       # → "0xff"
```

#### Cadenas
- Comilla simple: `'hola'`
- Comilla doble: `"hola"`
- Triple comilla (multilínea): `"""..."""` o `'''...'''`
- Prefijo `f` (interpolación, **completo desde v1.1**): `f"hola, {nombre}, en {edad+10} anos"`. Cada `{expr}` se evalúa como expresión Cornamusa completa y se concatena con las partes literales. Llaves dobles `{{` y `}}` se preservan como llave literal. Anidación soportada (`f"{f'{x}'}"`). Triples f-cadenas (`f"""..."""`) soportadas desde v1.14.
- Prefijo `b` (bytes): `b"\x00\xff"` — **reservado**, aún no implementado.
- Prefijo `r` (raw, sin escapes): `r"C:\ruta"` — **reservado**, aún no implementado.

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
| `lista` | Secuencia mutable indexada | no |
| `tupla` | Secuencia inmutable indexada | sí |
| `diccionario` | Mapa hash mutable. **Preserva el orden de inserción** desde v1.20 (`claves(d)` y `valores(d)` siguen el orden en que se insertaron las claves). | no |
| `conjunto` | Conjunto hash mutable | no |
| `funcion` | Callable de primera clase (funciones, lambdas, nativas, métodos ligados) | — |
| `generador` | Estado suspendible de una función con `producir`; iterable perezoso (v1.31) | — |
| `clase` | Plantilla de objetos | — |
| `instancia` | Objeto creado a partir de una clase | instancia: no |

> El tipo `bytes` (y los literales `b"..."`) está **reservado**, aún no implementado.

---

## 4. Built-ins

### 4.1 Funciones globales

Esta es la lista **real** de built-ins disponibles (registrados en `src/nativos.c`).

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
| `ErrorDeNombre(mensaje)` | Identificador no definido. |
| `ErrorDeSistema(mensaje)` | Fallo del sistema operativo. |
| `ErrorDeIO(mensaje)` | Fallo de entrada/salida. |

`ErrorDeAtributo` (atributo/método inexistente en instancia o módulo) lo lanza la VM; es atrapable aunque no tenga constructor global.

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
| `instancia_de(obj, clase)` | `isinstance(obj, clase)` | Verdadero si `obj` es instancia de `clase` (de usuario) o subclase. |
| `subclase_de(A, B)` | `issubclass(A, B)` | Verdadero si A == B o A hereda de B. Reflexivo. |
| `id(obj)` | `id(obj)` | Entero único de la identidad del objeto. |
| `repr(obj)` | `repr(obj)` | Cadena con la representación literal (con comillas para cadenas). |

#### Built-ins de bajo nivel (envueltos por la stdlib)

Registrados como globales pero pensados para usarse a través del módulo de stdlib correspondiente (§4.4), que ofrece nombres legibles:

`archivo_leer`, `archivo_escribir`, `archivo_existe`, `archivo_lineas`, `archivo_agregar` (→ módulo `archivos`); `json_parsear`, `json_serializar` (→ `json`); `tiempo_actual`, `tiempo_descomponer`, `tiempo_componer`, `tiempo_formato` (→ `fechas`); `azar_decimal`, `azar_entero`, `azar_semilla` (→ `azar`); `proceso_ejecutar` (→ `proceso`); `regex_coincide`, `regex_buscar`, `regex_todos`, `regex_reemplazar` (→ `regex`); `red_http_obtener` (→ `red`).

### 4.2 Identificadores reservados sin implementar

`abrir(ruta, modo)` y `siguiente(it)` (iteración perezosa con `__siguiente__`) están reservados pero **no implementados**: invocarlos da `ErrorDeNombre` como cualquier identificador no definido. La iteración de clases se hace hoy con `__iterar__` (§4.3), que devuelve un iterable nativo. La I/O de archivos vive en el módulo `archivos`.

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

**Añadidos en v1.12**:

| Dunder | Operador / built-in | Aridad |
|---|---|---|
| `__iterar__` | `para x en obj` | 1 (yo) |

`__iterar__` debe retornar un iterable nativo (lista, tupla, conjunto,
dicc, rango, cadena). La VM materializa al inicio del bucle: el
dunder se invoca una vez y el iterable resultante se recorre con la
maquinaria existente. Errores: clase sin `__iterar__` o `__iterar__`
que retorna no-iterable → `ErrorDeTipo` atrapable.

**Añadidos en v1.13**:

| Dunder | Operador / built-in | Aridad |
|---|---|---|
| `__entrar__` | inicio del bloque `con` | 1 (yo) |
| `__salir__` | fin del bloque `con` (siempre, vía `finalmente`) | 1 (yo) |

`__entrar__` se invoca al entrar al bloque. Su valor de retorno se
liga al alias `como x` si lo hay; si no, se descarta. `__salir__`
se invoca al salir del bloque, sea por flujo normal, `retornar`,
`romper`, `continuar` o excepción no atrapada. En v1.13 `__salir__`
no recibe información de la excepción — la firma extendida
`__salir__(yo, tipo_exc, valor_exc, traceback)` queda para versiones
posteriores.

Reglas:
- El dunder normal solo se invoca si el operando IZQUIERDO es VAL_INSTANCIA y la clase lo define. Si izq no maneja, se busca el dunder reflejado en el lado DERECHO (solo aritméticos).
- En reflejado, el receptor (`yo`) es el operando DERECHO y el primer argumento (`otro`) es el IZQUIERDO.
- `__cadena__` debe retornar cadena. Si retorna otro tipo, ErrorDeTipo.
- `__longitud__` y los aritméticos pueden retornar cualquier tipo, pero el caller suele asumir entero (e.g. `len(obj) * 2`).
- Para `==`, `<`, etc. el resultado se interpreta según las reglas de truthiness §6.2 cuando se usa en `si`.
- `==` y `!=` NO se auto-derivan: si defines solo `__igual__`, `obj != obj` cae al path de identidad por defecto.

```cornamusa
clase Persona:
    funcion __iniciar__(yo, nombre):       # invocado por Persona("Ana")
        yo.nombre = nombre
    fin funcion

    funcion __sumar__(yo, otro):           # invocado por p1 + p2
        retornar Persona(yo.nombre + otro.nombre)
    fin funcion

    funcion __cadena__(yo):                # invocado por imprimir(p), f"{p}"
        retornar f"Persona({yo.nombre})"
    fin funcion
fin clase
```

**Añadidos en v1.41**:

| Dunder | Operador / built-in | Aridad |
|---|---|---|
| `__repr__` | `repr(obj)` | 1 (yo) |
| `__booleano__` | `si obj:`, `mientras obj:`, `y`, `o`, `no obj` | 1 (yo) |

`__repr__` debe retornar cadena (la VM lo valida — `ErrorDeTipo` atrapable si no). `__booleano__` debe retornar booleano; si retorna otra instancia con `__booleano__` se cae en recursión, cortada por el límite de frames.

**Añadidos en v1.42**:

| Dunder | Operador / built-in | Aridad |
|---|---|---|
| `__hash__` | clave de dict/conjunto (`d[obj]`, `obj en s`, `{obj: ...}`) | 1 (yo) |
| `__igual__` (despacho en dicc/conj) | colisiones de hash en dict/conjunto | 2 (yo, otro) |

Las instancias siempre son hashables — por identidad por defecto, por valor si definen `__hash__`. Para usarlas como claves por valor define **ambos** dunders coherentes: si dos instancias son iguales según `__igual__`, deben dar el mismo `__hash__`. La VM cachea el hash por instancia tras el primer despacho (Python: `__hash__` debe ser estable).

`__hash__` debe retornar entero (`ErrorDeTipo` atrapable si no). Internamente la VM ejecuta el dunder vía sub-VM síncrono: empuja un frame, corre un sub-loop del dispatcher, vuelve con el valor. Excepciones dentro del dunder se propagan al `intentar/atrapar` del caller mediante una bandera one-shot (`handler_techo` impide que escapen sin control).

**Añadidos en v1.43**:

| Dunder | Operador / built-in | Aridad |
|---|---|---|
| `__siguiente__` | siguiente valor de un iterador lazy (despachado por OP_ITER_SIGUIENTE en `para`) | 1 (yo) |

`__iterar__` puede ahora devolver **una instancia** con `__siguiente__` (no solo iterables nativos). En cada iteración la VM despacha el dunder vía sub-VM síncrono. El fin se señala lanzando `ErrorDeIteracion`, que el `para` atrapa internamente — el usuario también puede atraparlo manualmente.

Patrón Python `__iter__(self): return self` soportado: si la instancia define ambos dunders, `__siguiente__` tiene prioridad y la instancia se usa como iterador directamente.

**Todos los dunders del modelo de datos están implementados desde v1.43.** No quedan `Reservados, aún no invocados` en esta sección.

### 4.4 Biblioteca estándar (stdlib)

Doce módulos. Los archivos viven en `stdlib/*.cor` y se cargan vía `importar`. La lista completa de funciones de cada módulo está en la [referencia rápida](docs/referencia.md#16-biblioteca-estándar-stdlib); aquí solo el resumen:

| Módulo | Contenido |
|---|---|
| `matematicas` | `PI`, `E`, `cuadrado`, `cubo`, `absoluto`, `maximo`, `minimo`, `signo`, `factorial`, `suma_rango`, `es_par`, `es_impar`, `mcd` |
| `cadenas` | `repetir`, `unir`, `separar`, `reemplazar`, `recortar`(`_izquierda`/`_derecha`), `empieza_con`, `termina_con`, `contiene`, `indice_de`, `contar`, `caracter`, `mayusculas_ascii`, `minusculas_ascii`, `es_vacia` |
| `funcionales` | `mapear`, `filtrar`, `reducir`, `enumerar`, `cualquiera`, `todos`, `suma`, `minimo`, `maximo` |
| `formato` | `rellenar`, `alinear_derecha`, `centrar`, `con_decimales`, `numero_con_separador`, `porcentaje`, `como_hex`, `como_binario`, `linea`, `fila` |
| `archivos` | `leer`, `escribir`, `agregar`, `lineas`, `existe` |
| `json` | `parsear`, `serializar`, `serializar_indentado` |
| `fechas` | `ahora`, `componentes`, `construir`, `formato`, `iso8601`, `legible`, aritmética de fechas, calendario; constantes `SEGUNDO`…`SEMANA` |
| `azar` | `decimal`, `entero`, `semilla`, `elegir`, `barajar`, `muestra`, `booleano`, `uniforme` |
| `proceso` | `ejecutar`, `capturar`, `codigo` — lanzar procesos externos (cross-platform) |
| `regex` | `coincide`, `buscar`, `todos`, `reemplazar`, `contiene`, `extraer` — motor backtracking propio |
| `red` | `obtener`, `descargar_cuerpo`, `parsear_cabeceras` — cliente HTTP/1.1 plano (sin TLS) |
| `sistema` | `argv` — argumentos del programa |

```cornamusa
importar matematicas
imprimir(matematicas.factorial(10))   # 3628800

importar funcionales
imprimir(funcionales.mapear(lambda x: x * x, [1, 2, 3]))   # [1, 4, 9]

importar sistema
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
                / sent_coincidir

sent_simple    ← sent_asignacion
                / sent_expresion
                / sent_retornar
                / sent_lanzar
                / sent_producir
                / sent_importar
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

sent_para      ← "para" IDENT "en" expr ":" cuerpo
                ("sino" ":" cuerpo)?
                "fin" "para"
# Nota: el destino de `para` es un único IDENT. Para desempaquetar
# pares se destructura dentro del cuerpo (`clave, valor = par`).

# ───── Funciones ─────
sent_funcion   ← decoradores? ("función" / "funcion")
                IDENT "(" parametros? ")" anot_retorno? ":" cuerpo
                "fin" ("función" / "funcion")

parametros     ← parametro ("," parametro)*
parametro      ← "**" IDENT                 # **kwargs (recoge en dict)
                / "*" IDENT                 # *args   (recoge en tupla)
                / IDENT (":" expr)? ("=" expr)?   # fijo, anotación, default

anot_retorno   ← "->" expr

# ───── Clases ─────
sent_clase     ← decoradores? "clase" IDENT
                ("extiende" expr ("," expr)*)? ":" cuerpo
                "fin" "clase"

# ───── Context managers ─────
sent_con       ← "con" expr ("como" IDENT)? ":" cuerpo
                "fin" "con"

# ───── Pattern matching ─────
sent_coincidir ← "coincidir" expr ":" LF
                ("cuando" patron ("si" expr)? ":" cuerpo)+
                "fin" "coincidir"
patron         ← patron_or
patron_or      ← patron_atomo ("|" patron_atomo)*    # OR-pattern
patron_atomo   ← literal                             # literal
                / "_"                                # comodín
                / IDENT "(" ")" ("como" IDENT)?      # type-match + bind
                / "[" (patron ("," patron)* ("," "*" IDENT)?)? "]"   # lista, con *resto
                / "(" patron ("," patron)* ")"       # tupla (anidable)
                / IDENT                              # bind

# ───── Generadores ─────
# Una función cuyo cuerpo contiene `producir` es un generador.
sent_producir  ← "producir" "desde" expr      # delegación (yield from)
                / "producir" expr             # produce un valor

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
# El objetivo puede ser un nombre, un índice, un atributo, o un patrón
# de destructuring (tupla/lista de objetivos, anidable).
sent_asignacion ← lista_objetivos "=" expr
                / objetivo aug_op expr      # +=, -=, etc.

objetivo       ← IDENT
                / expr_postfijo ("[" rebanada "]" / "." IDENT)   # xs[i], obj.campo
lista_objetivos ← objetivo_destr ("," objetivo_destr)*           # a, b = ...
objetivo_destr ← objetivo
                / "[" lista_objetivos "]"   # [a, b] = ...  (anidable)
                / "(" lista_objetivos ")"   # (a, (b, c)) = ...

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

# Argumentos de llamada: posicionales, *spread, keyword, **spread.
# Restricción: no se puede combinar `*` con keyword/`**` en una misma
# llamada (se rechaza en compilación).
args           ← arg ("," arg)*
arg            ← "**" expr            # **dict spread
                / "*" expr            # *iterable spread
                / IDENT "=" expr      # keyword argument
                / expr                # posicional

expr_atomo     ← literal / IDENT / lista_lit / dicc_lit / conj_lit
                / "(" expr ")" / "(" expr_para ")"   # paréntesis o generator expression
                / "yo" / "super" "." IDENT / cadena_f / lambda_lit

# Comprehensions y generator expressions comparten la cláusula `para`.
lista_lit      ← "[" (expr_para / (expr ("," expr)*)?) "]"   # lista o list-comp
dicc_lit       ← "{" (expr ":" expr expr_para
                      / (expr ":" expr ("," expr ":" expr)*)?) "}"   # dict o dict-comp
conj_lit       ← "{" (expr expr_para / expr ("," expr)*) "}"          # conjunto o set-comp
expr_para      ← "para" IDENT "en" expr ("si" expr)?   # cláusula de comprehension

lambda_lit     ← "lambda" parametros? ":" expr
```

(Esta gramática es indicativa. Ambigüedades menores se resuelven con lookaheads en el parser PEG.)

---

## 6. Semántica clave

### 6.1 Modelo de scoping

- **Léxico** (estilo Python).
- Resolución LEGB-equivalente: Local → Enclosing (closures) → Global (módulo) → Builtins.
- Las variables se definen por asignación en el scope local. `nolocal` declara que un nombre pertenece a un scope envolvente y permite escribirlo (§6.8). `global` (escritura a global desde una función) está reservada pero **no implementada** en la VM.

### 6.2 Verdadez (truthiness)

Son **falsos**: `falso`, `nulo`, `0`, `0.0`, `""`, `[]`, `()`, `{}`, `conjunto()`, y cualquier objeto que defina `__booleano__` y devuelva `falso`.  
Todo lo demás es **verdadero**.

### 6.3 Comparación de igualdad

- `==` invoca `__igual__`. Por defecto, identidad (`es`).
- `es` compara identidad de objeto (mismo puntero en memoria).

### 6.4 Iteración

`para x en y:` itera sobre `y` si es un iterable nativo (lista, tupla, conjunto, diccionario —itera claves—, cadena, rango, generador). Si `y` es una instancia, la VM invoca su dunder `__iterar__`, que debe devolver un iterable nativo, y recorre el resultado. Los generadores se reanudan bajo demanda hasta agotarse (§6.12).

### 6.5 Excepciones

Tipos de excepción atrapables (todos derivan de `Excepcion`):

```
Excepcion
├── ErrorAritmetico    # división por cero, overflow lógico
├── ErrorDeTipo        # operación sobre tipo incorrecto
├── ErrorDeValor       # valor del tipo correcto pero inválido
├── ErrorDeIndice      # índice fuera de rango
├── ErrorDeClave       # clave no presente en diccionario
├── ErrorDeNombre      # identificador no definido
├── ErrorDeAtributo    # atributo/método inexistente (lanzado por la VM)
├── ErrorDeSistema     # fallo del sistema operativo
└── ErrorDeIO          # fallo de entrada/salida
```

`atrapar Excepcion` captura cualquiera de ellas. Cuando una excepción no se atrapa, la VM imprime un **traceback multi-frame** con la cadena de llamadas y la línea de fuente. Los errores de sintaxis y de importación se reportan en fase de carga, antes de ejecutar, y no son atrapables.

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
| `a + b` | `__sumar__(a, b)` sobre la clase de `a`; si `a` no lo define, prueba `__sumar_derecho__(b, a)` sobre la clase de `b` |
| `imprimir(x)`, `cadena(x)`, `f"{x}"` | `__cadena__(x)` |
| `longitud(x)` | `__longitud__(x)` |
| `para v en x:` | `__iterar__(x)` (debe devolver un iterable nativo) |
| `obj[k]` / `obj[k] = v` | `__indice__(obj, k)` / `__asignar_indice__(obj, k, v)` |
| `obj(args)` | `__llamar__(obj, args)` |
| `con obj como x:` | `__entrar__(obj)` al entrar, `__salir__(obj)` al salir |

Lista completa de dunders en §4.3. Si la clase no implementa el dunder correspondiente, el runtime lanza `ErrorDeTipo` con un mensaje específico (p.ej. *"el tipo `Persona` no soporta el operador `+`"*).

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

#### Limitaciones

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
```

Las lambdas admiten defaults, `*args` y `**kwargs` igual que las funciones con nombre.

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
imprimir(s[0:4])     # "Corn" — slicing de cadenas (UTF-8 correcto)
imprimir(s[::-1])    # "asumanroC"
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

### 6.12 Generadores y comprehensions

Una función cuyo cuerpo contiene `producir` es un **generador**. Llamarla no ejecuta el cuerpo: devuelve un valor de tipo `generador` con su frame congelado. Cada iteración (`para v en gen`) reanuda el frame —restaurando locales y posición— hasta el siguiente `producir`, que entrega un valor y vuelve a suspender. Cuando la función retorna, el generador queda **agotado** y no produce más valores.

- `producir EXPR` entrega un valor.
- `producir desde ITERABLE` delega: produce todos los valores de un sub-generador o iterable.
- Un generador se consume una sola vez; volver a iterarlo no produce nada.

Las **comprehensions** construyen una colección en una expresión: `[expr para v en iter si guarda]` (lista), `{k: v para ...}` (dict), `{expr para ...}` (conjunto). La variable del `para` es un único nombre y la guarda `si` es opcional. Entre paréntesis, `(expr para v en iter)` es una **generator expression**: perezosa, no materializa, captura variables externas como upvalues.

### 6.13 Destructuring

El lado izquierdo de `=` puede ser un patrón de nombres en lugar de un único objetivo: `a, b = par`, `[x, y, z] = lista`, `(a, (b, c)) = anidado`. El lado derecho debe ser iterable y de la aridad correcta; en caso contrario se lanza `ErrorDeValor` (aridad) o `ErrorDeTipo` (no iterable), ambos atrapables. El idiom `a, b = b, a` intercambia sin variable temporal. El destino de `para` **no** admite destructuring: se desempaqueta dentro del cuerpo.

### 6.14 Argumentos de función

- **Por defecto**: `funcion f(a, b=10):` — los parámetros con default son opcionales.
- **`*args`**: `funcion f(*resto):` recoge los posicionales sobrantes en una tupla.
- **`**kwargs`**: `funcion f(**kw):` recoge los keyword args no declarados en un diccionario.
- **Keyword en la llamada**: `f(x=1, y=2)` — pasa argumentos por nombre, en cualquier orden.
- **Spread en la llamada**: `f(*iterable)` expande posicionales; `f(**dict)` expande keyword args. No se puede combinar `*` con `**`/keyword en la misma llamada.

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
        lanzar ErrorDeTipo("método abstracto")
    fin funcion
fin clase

clase Perro extiende Animal:
    funcion hablar(yo):
        retornar f"{yo.nombre} dice guau"
    fin funcion
fin clase

# Manejo de excepciones e I/O
importar archivos

intentar:
    contenido = archivos.leer("datos.txt")
    imprimir(contenido)
atrapar ErrorDeIO como e:
    imprimir(f"No se pudo leer: {e}")
finalmente:
    imprimir("listo")
fin intentar

# Pattern matching, comprehensions y generadores
funcion pares_hasta(n):
    para i en rango(n):
        si i % 2 == 0:
            producir i
        fin si
    fin para
fin funcion

dobles = [x * 2 para x en pares_hasta(10)]
coincidir longitud(dobles):
    cuando 0:
        imprimir("vacío")
    cuando n si n > 3:
        imprimir(f"{n} elementos")
    cuando _:
        imprimir("pocos")
fin coincidir
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

### Implementado desde v1.1 (eran reservas en versiones anteriores)

- **F-strings con expresiones** (v1.1) y triples (v1.14).
- **Dunders** completos: aritméticos y de coerción (v1.2), reflejados / `__llamar__` / `__longitud__` (v1.3), `__iterar__` (v1.12), `__entrar__`/`__salir__` (v1.13).
- **`nolocal`** — escritura a upvalue (v1.4).
- **`con` (context managers)** (v1.13).
- **Pattern matching `coincidir`** (v1.15-v1.16): literales, bind, wildcard, guardas, patrones estructurales, OR-patterns, star-patterns, type-match.
- **Destructuring**, `*args`, keyword args, `**kwargs`, spread `*`/`**` (v1.21-v1.25).
- **Comprehensions** y **generadores** (`producir`, `producir desde`, generator expressions) (v1.30-v1.34).
- **Stdlib amplia** — doce módulos (v1.8-v1.29).

### Reservas pendientes (la sintaxis puede aceptarse, el runtime no las implementa)

1. **`borrar` (`del` de Python)**. Keyword reservada; `quitar(...)` cubre el caso por ahora.
2. **`global`** (escritura a global desde función). Keyword reservada, sin implementar en la VM.
3. **Async/await (`asincrono`/`esperar`)**. Keywords reservadas. v2.x.
4. **Dunders del modelo de datos** — todos implementados (v1.41 `__repr__`/`__booleano__`, v1.42 `__hash__`, v1.43 `__siguiente__`).
5. **Prefijos de cadena `r"..."` (raw) y `b"..."` (bytes)**. Reservados.
6. **Tipos numéricos exactos** (`Fraccion`, `Decimal`). En stdlib, futuro.
7. **Anotaciones de tipo (`: tipo`)**. La gramática las acepta; el runtime las ignora.
8. **Decoradores (`@deco`)**. Soporte parcial en el parser.

### Trabajo de runtime pendiente (no afecta a la sintaxis)

- **GC generacional** — postergado (decisión B10). El GC mark-sweep tri-color actual es suficiente para los workloads conocidos.
- **Computed-goto y PGO** en el dispatch — evaluados y descartados con datos (regresión o sin mejora medible); ver `benchmarks/RESULTS.md`. `vm.c` usa `switch` + `-O3` + LTO.
- **JIT / tracing** — aplazado indefinidamente.
- **Concurrencia / hilos** — aplazada a post-v1.x.

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
