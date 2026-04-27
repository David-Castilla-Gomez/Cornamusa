# Especificación del lenguaje Cornamusa

**Versión del documento:** 0.1.0-borrador  
**Estado:** En diseño activo (Fase 0 del plan de desarrollo)

Este documento define la sintaxis, semántica y vocabulario de Cornamusa, un lenguaje de programación dinámico interpretado con identidad castellana. La especificación es el contrato que une al implementador con el usuario del lenguaje: cualquier cambio aquí debe propagarse a `lexer.c`, `parser.c` y la documentación de usuario.

---

## 1. Filosofía

1. **Castellano natural, no traducción literal.** Las palabras clave deben sonar idiomáticas a un hispanohablante, no como traducciones automáticas del inglés.
2. **Tildes opcionales.** Toda keyword acentuada admite también su forma sin tilde (`función` ≡ `funcion`). La forma canónica para el ecosistema es **sin tilde** por portabilidad en sistemas con configuraciones de teclado limitadas.
3. **UTF-8 universal.** Identificadores admiten `ñ`, vocales acentuadas y otros caracteres Unicode de la categoría `Letter`. El compilador es UTF-8 nativo.
4. **Tipado dinámico fuerte.** Los valores tienen tipo en tiempo de ejecución; las variables no.
5. **Indentación significativa**, estilo Python. Bloques delimitados por `:` y nivel de indentación (espacios; tabuladores no permitidos).
6. **Una sola forma evidente** de hacer cada cosa cuando sea posible.

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

**Convenciones recomendadas:**
- Variables y funciones: `serpiente_minuscula` (`mi_variable`, `calcular_total`).
- Clases: `MayusculaCamello` (`Persona`, `ListaEnlazada`).
- Constantes: `MAYUSCULAS_CON_GUION` (`PI`, `MAX_INTENTOS`).

### 2.3 Palabras clave

#### Control de flujo
| Keyword | Forma sin tilde | Significado |
|---|---|---|
| `si` | — | condicional |
| `sino` | — | rama alternativa |
| `sino si` | — | rama alternativa con condición (compuesta de dos tokens) |
| `mientras` | — | bucle por condición |
| `para` | — | bucle por iteración |
| `en` | — | operador de pertenencia / iteración |
| `romper` | — | salir del bucle |
| `continuar` | — | siguiente iteración |
| `retornar` | — | devolver valor |
| `pasar` | — | sentencia vacía |

#### Funciones, clases y módulos
| Keyword | Forma sin tilde | Significado |
|---|---|---|
| `función` | `funcion` | declaración de función |
| `lambda` | — | función anónima (convención matemática internacional) |
| `clase` | — | declaración de clase |
| `extiende` | — | herencia (`clase Hija extiende Madre:`) |
| `super` | — | acceso a la superclase |
| `yo` | — | referencia a la instancia (equivalente a `self`/`this`) |
| `importar` | — | importación de módulo |
| `desde` | — | importación selectiva (`desde X importar Y`) |
| `como` | — | renombrado en import o except |
| `global` | — | declarar variable global |
| `nolocal` | — | declarar variable no-local en closure |

#### Manejo de excepciones
| Keyword | Forma sin tilde | Significado |
|---|---|---|
| `intentar` | — | bloque protegido |
| `atrapar` | — | manejo de excepción |
| `finalmente` | — | bloque siempre ejecutado |
| `lanzar` | — | lanzar excepción |

#### Operadores lógicos y comparativos (palabras)
| Keyword | Forma sin tilde | Significado |
|---|---|---|
| `y` | — | conjunción lógica |
| `o` | — | disyunción lógica |
| `no` | — | negación lógica |
| `es` | — | identidad de objetos |
| `es no` | — | identidad negada |

#### Literales
| Keyword | Significado |
|---|---|
| `verdadero` | booleano cierto |
| `falso` | booleano falso |
| `nulo` | ausencia de valor |

#### Reservadas para futuro
`producir`, `asíncrono`/`asincrono`, `esperar`, `con`, `borrar`, `coincidir` (match-case).

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

### 2.7 Indentación

- Bloques delimitados por `:` al final de la línea de cabecera y un incremento del nivel de indentación.
- Solo **espacios**. Tabuladores producen error de sintaxis.
- Nivel recomendado: **4 espacios**.
- El lexer emite tokens virtuales `INDENT` y `DEDENT`, comparable al algoritmo del lexer de Python.

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

### Métodos especiales (dunder)

| Cornamusa | Python | Propósito |
|---|---|---|
| `__iniciar__(yo, ...)` | `__init__` | Constructor |
| `__cadena__(yo)` | `__str__` | Representación legible |
| `__repr__(yo)` | `__repr__` | Representación inequívoca |
| `__igual__(yo, otro)` | `__eq__` | Igualdad |
| `__resumen__(yo)` | `__hash__` | Hash |
| `__longitud__(yo)` | `__len__` | Tamaño |
| `__llamar__(yo, ...)` | `__call__` | Hacer llamable la instancia |
| `__obtener__(yo, k)` | `__getitem__` | `obj[k]` |
| `__establecer__(yo, k, v)` | `__setitem__` | `obj[k] = v` |
| `__borrar__(yo, k)` | `__delitem__` | `del obj[k]` |
| `__contiene__(yo, x)` | `__contains__` | `x en obj` |
| `__iterar__(yo)` | `__iter__` | Devuelve iterador |
| `__siguiente__(yo)` | `__next__` | Avance del iterador |
| `__sumar__(yo, otro)` | `__add__` | `yo + otro` |
| `__restar__`, `__multiplicar__`, `__dividir__`, `__modulo__`, `__potencia__`, ... | aritméticos correspondientes | |

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
sent_si        ← "si" expr ":" bloque
                ("sino" "si" expr ":" bloque)*
                ("sino" ":" bloque)?

sent_mientras  ← "mientras" expr ":" bloque
                ("sino" ":" bloque)?

sent_para      ← "para" lista_objetivos "en" expr ":" bloque
                ("sino" ":" bloque)?

# ───── Funciones ─────
sent_funcion   ← decoradores? ("función" / "funcion")
                IDENT "(" parametros? ")" anot_retorno? ":" bloque

parametros     ← parametro ("," parametro)*
parametro      ← IDENT (":" expr)? ("=" expr)?

anot_retorno   ← "->" expr

# ───── Clases ─────
sent_clase     ← decoradores? "clase" IDENT
                ("extiende" expr ("," expr)*)? ":" bloque

# ───── Excepciones ─────
sent_intentar  ← "intentar" ":" bloque
                ("atrapar" expr ("como" IDENT)? ":" bloque)+
                ("sino" ":" bloque)?
                ("finalmente" ":" bloque)?

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

# ───── Bloque ─────
bloque         ← LF INDENT sentencia+ DEDENT
                / sent_simple LF        # bloque en una línea: "si x: hacer()"

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
- Acceso dinámico mediante `__obtener_atributo__` / `__establecer_atributo__` (en clases que lo implementen).
- En v0.10.0 se introducen **shapes / hidden classes** (paper SELF) para optimizar acceso, transparente al usuario.

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

# Clases y herencia
clase Animal:
    funcion __iniciar__(yo, nombre):
        yo.nombre = nombre

    funcion hablar(yo):
        lanzar ErrorRuntime("método abstracto")

clase Perro extiende Animal:
    funcion hablar(yo):
        retornar f"{yo.nombre} dice guau"

# Manejo de excepciones
intentar:
    archivo = abrir("datos.txt")
    contenido = archivo.leer()
atrapar ErrorDeIO como e:
    imprimir(f"No se pudo leer: {e}")
finalmente:
    si archivo no es nulo:
        archivo.cerrar()
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
