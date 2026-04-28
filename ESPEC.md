# Especificación del lenguaje Cornamusa

**Versión del documento:** 0.1.0-borrador  
**Estado:** En diseño activo (Fase 0 del plan de desarrollo)

Este documento define la sintaxis, semántica y vocabulario de Cornamusa, un lenguaje de programación dinámico interpretado con identidad castellana. La especificación es el contrato que une al implementador con el usuario del lenguaje: cualquier cambio aquí debe propagarse a `lexer.c`, `parser.c` y la documentación de usuario.

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

```cornamusa
pi = 3.14159
poblacion = 47_500_000
porcentaje_iva = 0.21
distancia = 1.496e11        # notación científica
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
- Prefijo `f` (interpolación): `f"hola, {nombre}"`
- Prefijo `b` (bytes): `b"\x00\xff"`
- Prefijo `r` (raw, sin escapes): `r"C:\ruta"`

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
| `entero` | Entero de precisión arbitraria (en Fase 4: i64; en v1.0: bignum) | sí |
| `decimal` | Coma flotante IEEE 754 64-bit | sí |
| `booleano` | `verdadero` / `falso` | sí |
| `nulo` | Único valor: `nulo` | sí |
| `cadena` | Texto Unicode UTF-8 | sí |
| `bytes` | Secuencia inmutable de bytes | sí |
| `lista` | Secuencia mutable indexada | no |
| `tupla` | Secuencia inmutable indexada | sí |
| `diccionario` | Mapa hash mutable (preserva orden de inserción) | no |
| `conjunto` | Conjunto hash mutable | no |
| `función` | Callable de primera clase | — |
| `clase` | Plantilla de objetos | — |
| `instancia` | Objeto creado a partir de una clase | — |

---

## 4. Built-ins

### Funciones globales

| Cornamusa | Equivalente Python | Descripción |
|---|---|---|
| `imprimir(...)` | `print` | Imprime valores separados por espacio + salto de línea |
| `leer(prompt="")` | `input` | Lee una línea del stdin |
| `longitud(x)` | `len` | Tamaño de secuencia / colección |
| `tipo(x)` | `type` | Tipo del valor |
| `rango(...)` | `range` | Iterador numérico (1, 2 o 3 args) |
| `enumerar(it, inicio=0)` | `enumerate` | Pares (índice, valor) |
| `mapear(f, it)` | `map` | Aplicar función a iterable |
| `filtrar(f, it)` | `filter` | Filtrar iterable por predicado |
| `reducir(f, it, ini?)` | `functools.reduce` | Plegar iterable |
| `ordenar(it, clave=nulo, invertido=falso)` | `sorted` | Lista ordenada |
| `invertir(seq)` | `reversed` | Iterador inverso |
| `suma(it, inicio=0)` | `sum` | Suma elementos |
| `mínimo(...)` / `minimo` | `min` | Mínimo |
| `máximo(...)` / `maximo` | `max` | Máximo |
| `absoluto(x)` | `abs` | Valor absoluto |
| `redondear(x, n=0)` | `round` | Redondeo |
| `cadena(x)` | `str` | Convertir a cadena |
| `entero(x, base=10)` | `int` | Convertir a entero |
| `decimal(x)` | `float` | Convertir a decimal |
| `booleano(x)` | `bool` | Convertir a booleano |
| `lista(it)` | `list` | Convertir a lista |
| `tupla(it)` | `tuple` | Convertir a tupla |
| `diccionario(...)` | `dict` | Construir diccionario |
| `conjunto(it)` | `set` | Construir conjunto |
| `abrir(ruta, modo="l")` | `open` | Abrir archivo (`l`=lectura, `e`=escritura, `a`=añadir, `b`=binario) |
| `iterar(x)` | `iter` | Obtener iterador |
| `siguiente(it, def?)` | `next` | Siguiente del iterador |
| `instancia_de(x, T)` | `isinstance` | Comprobación de tipo |
| `subclase_de(C, P)` | `issubclass` | Comprobación de subclase |
| `id(x)` | `id` | Identidad numérica |
| `resumen(x)` | `hash` | Hash para colecciones |
| `repr(x)` | `repr` | Representación textual |

### Métodos especiales (dunders)

Los nombres de métodos especiales en Cornamusa son **castellanos** (decisión [B5+B6](decisiones/B5-B6-yo-y-dunders.md)). El runtime invoca estos nombres canónicos directamente; los nombres en inglés de Python no son reconocidos. Por ejemplo, `longitud(obj)` busca `__longitud__`, no `__len__`.

#### Construcción y representación
| Dunder | Python equivalente | Activación |
|---|---|---|
| `__iniciar__(yo, ...)` | `__init__` | Constructor: `Persona("Ana")` |
| `__finalizar__(yo)` | `__del__` | Destructor (raro de implementar) |
| `__cadena__(yo)` | `__str__` | `cadena(obj)`, `f"{obj}"`, `imprimir(obj)` |
| `__repr__(yo)` | `__repr__` | `repr(obj)` — préstamo aceptado por brevedad |
| `__booleano__(yo)` | `__bool__` | `booleano(obj)`, contexto de truthiness |

#### Comparaciones
| Dunder | Python equivalente | Activación |
|---|---|---|
| `__igual__(yo, otro)` | `__eq__` | `a == b` |
| `__distinto__(yo, otro)` | `__ne__` | `a != b` |
| `__menor__(yo, otro)` | `__lt__` | `a < b` |
| `__menor_igual__(yo, otro)` | `__le__` | `a <= b` |
| `__mayor__(yo, otro)` | `__gt__` | `a > b` |
| `__mayor_igual__(yo, otro)` | `__ge__` | `a >= b` |
| `__resumen__(yo)` | `__hash__` | `resumen(obj)`, claves de dict/set |

#### Colecciones e iteración
| Dunder | Python equivalente | Activación |
|---|---|---|
| `__longitud__(yo)` | `__len__` | `longitud(obj)` |
| `__obtener__(yo, k)` | `__getitem__` | `obj[k]` |
| `__establecer__(yo, k, v)` | `__setitem__` | `obj[k] = v` |
| `__borrar__(yo, k)` | `__delitem__` | `borrar obj[k]` |
| `__contiene__(yo, x)` | `__contains__` | `x en obj` |
| `__iterar__(yo)` | `__iter__` | `para x en obj:` |
| `__siguiente__(yo)` | `__next__` | `siguiente(it)` |

#### Aritméticos
| Dunder | Python equivalente | Activación |
|---|---|---|
| `__sumar__(yo, otro)` | `__add__` | `a + b` |
| `__restar__(yo, otro)` | `__sub__` | `a - b` |
| `__multiplicar__(yo, otro)` | `__mul__` | `a * b` |
| `__dividir__(yo, otro)` | `__truediv__` | `a / b` |
| `__div_entera__(yo, otro)` | `__floordiv__` | `a // b` |
| `__modulo__(yo, otro)` | `__mod__` | `a % b` |
| `__potencia__(yo, otro)` | `__pow__` | `a ** b` |
| `__negar__(yo)` | `__neg__` | `-a` |
| `__positivar__(yo)` | `__pos__` | `+a` |
| `__absoluto__(yo)` | `__abs__` | `absoluto(a)` |

#### Llamada y contexto
| Dunder | Python equivalente | Activación |
|---|---|---|
| `__llamar__(yo, ...)` | `__call__` | `obj(args)` |
| `__entrar__(yo)` | `__enter__` | `con obj como ...` (futuro) |
| `__salir__(yo, exc)` | `__exit__` | `con obj como ...` (futuro) |

#### Atributos dinámicos
| Dunder | Python equivalente | Activación |
|---|---|---|
| `__obtener_atributo__(yo, nombre)` | `__getattr__` | `obj.x` cuando `x` no existe |
| `__establecer_atributo__(yo, nombre, v)` | `__setattr__` | `obj.x = v` |
| `__borrar_atributo__(yo, nombre)` | `__delattr__` | `borrar obj.x` |

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

## 9. Cuestiones abiertas (decidir antes de Fase 4)

1. **Bignum vs i64 en Fase 4.** Empezar con i64 y migrar a bignum en v1.0; documentar overflow como warning.
2. **Memoization de `from-import`.** Caché de módulos por ruta canónica.
3. **String interpolation `f""`.** ¿Permitir expresiones complejas o solo `{ident}`? Decisión: expresiones completas, igual que Python.
4. **Tipos numéricos exactos.** ¿`fracción` en stdlib? Aplazado a v1.1.
5. **Coincidir (pattern matching).** Reservada como keyword pero no implementada hasta v1.1+.
6. **Async/await.** Reservadas pero no implementadas hasta v2.0.

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
