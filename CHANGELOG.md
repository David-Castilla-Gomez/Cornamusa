# Registro de cambios

Todos los cambios notables a este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto adhiere a [Versionado Semántico](https://semver.org/lang/es/).

## [No publicado]

### En desarrollo (Fase 4 — Evaluador tree-walking, objetivo v0.4.0)
- ✅ Sesión 1: vendoreo libtommath + tipo `Valor` con bignum boxed + `Entorno` con tabla hash y scope chain.
- ✅ Sesión 2: evaluador de expresiones (literales, identificadores, aritmética bignum, comparaciones, lógica con cortocircuito, unarios, cadenas, identidad/membership).
- ✅ Sesión 3: evaluador de sentencias (asignación, `si`/`mientras`/`para`, `romper`/`continuar`, cláusulas `sino` de bucle, iteración UTF-8).
- ⏳ Sesión 4: funciones top-level + built-ins (imprimir, longitud, tipo, rango).
- ⏳ Sesión 5: REPL ejecutable + integración + tag v0.4.0 (primer release jugable).

### Añadido (Fase 4 sesión 3)
- **Evaluador de sentencias** en `evaluador.{h,c}`: nueva API `evaluador_ejecutar_sent` y `evaluador_ejecutar_programa`. Modelo de control de flujo sin `setjmp`: nuevo enum `ControlFlujo` (`EJEC_NORMAL`, `EJEC_ROMPER`, `EJEC_CONTINUAR`, `EJEC_RETORNAR`). Las construcciones envolventes (bucles, llamadas) inspeccionan y resetean `ev->control`.
- **`SENT_ASIGNAR`**: solo destino `EXPR_IDENT` en v0.4 (tuple destructuring, atributos e índices como destino quedan para v0.3.1+/F5). La asignación crea o sobrescribe en el entorno actual con `entorno_definir`. Sin tipos: la misma variable puede pasar de entero a cadena a decimal.
- **`SENT_ASIGNAR_AUG`** (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`): obtiene el valor actual (clon) del entorno, evalúa el operando derecho, aplica el operador binario equivalente y reasigna. La variable debe estar previamente definida (semántica Python: `ErrorDeNombre` si no existe). `x /= 2` produce decimal aunque `x` sea entero.
- **Refactor de `eval_binario`**: extraída `aplicar_binario(ev, op, a, b, e)` que toma posesión de dos valores ya evaluados. Reutilizada por `SENT_ASIGNAR_AUG` para no duplicar la lógica.
- **`SENT_PASAR`**: no-op explícito.
- **`SENT_ROMPER` / `SENT_CONTINUAR`**: marcan `ev->control` y dejan que el bucle envolvente lo gestione. Si `evaluador_ejecutar_programa` detecta control de flujo no consumido al volver al top-level, produce error explícito ("control de flujo fuera de su contexto").
- **`SENT_SI`**: itera sobre la cadena de `RamaSi` (`si` + `sino si`* + `sino`?) y ejecuta la primera rama cuya condición sea verdadera; la rama final `sino` tiene `condicion=NULL` y siempre se toma si se llega.
- **`SENT_MIENTRAS`**: bucle clásico con `romper`/`continuar`. Cláusula `sino` con semántica Python: se ejecuta sólo si el bucle terminó por condición falsa, NO si se rompió.
- **`SENT_PARA`** sobre cadenas: itera **code points UTF-8** (no bytes), de modo que `"niño"` produce 4 iteraciones (`'n'`, `'i'`, `'ñ'`, `'o'`). Cada iteración crea un nuevo `Valor` cadena de 1 code point y lo asigna al objetivo. `romper`, `continuar` y cláusula `sino` con la misma semántica que `mientras`. Otros iterables (rango, lista, diccionario) llegarán en S4/F5. Iterable no soportado produce `ErrorDeTipo` específico.
- **`SENT_BLOQUE`**: secuencia de sentencias; para al primer error o cuando aparece control de flujo no normal (que el bloque envolvente recogerá).
- **Aplazadas con error explícito**: `SENT_FUNCION`/`SENT_RETORNAR` (S4), `SENT_CLASE`/`SENT_INTENTAR`/`SENT_LANZAR`/`SENT_IMPORTAR`/`SENT_DESDE_IMPORTAR`/`SENT_GLOBAL`/`SENT_NOLOCAL` (F5+).
- **`tests/unit/test_runtime_sentencias.c`** con 12 grupos de tests sobre programas completos parseados y ejecutados:
  - Asignación simple, múltiples variables, cambio de tipo libre.
  - Asignación aumentada (todas las variantes incluyendo concatenación de cadenas con `+=` y true-div con `/=`).
  - `si`/`sino si`/`sino` en cascada y one-liner.
  - `mientras` clásico (suma 1..10), `romper`, `continuar` (suma de pares), cláusula `sino` ejecutada y NO ejecutada.
  - `para` sobre cadena ASCII y UTF-8 (`"niño"` → 4 iteraciones), concatenación durante iteración, `romper`, cláusula `sino`, error con iterable entero.
  - Programas realistas: factorial(25) con bignum (26 dígitos), conteo de vocales en `"murcielago"`, Fibonacci(30) iterativo, 2^64 (20 dígitos).
  - Anidamiento: `si` en `mientras`, `mientras` en `para`.
- **33 tests verde** (13 unit + 20 integración).

### Añadido (Fase 4 sesión 2)
- **`src/evaluador.{h,c}`** — evaluador tree-walking de expresiones. Modelo de errores sin `setjmp`: cada función devuelve `Valor` y rellena `Evaluador.error` (con línea, columna y mensaje) en caso de fallo. El cliente comprueba `evaluador_tiene_error` tras cada evaluación.
- **Literales**: `EXPR_LITERAL_ENTERO` parsea decimal/hex/oct/bin con `_` separadores; `EXPR_LITERAL_DECIMAL` con notación científica; `EXPR_LITERAL_CADENA` quita comillas y procesa escapes mínimos (`\n \t \r \\ \' \"`); `EXPR_LITERAL_BOOLEANO`, `EXPR_LITERAL_NULO`. `EXPR_LITERAL_F_CADENA` produce error explícito (interpolación llega en F4 S5 + parser de sub-expresiones).
- **Identificadores**: lookup en el entorno actual con scope chain por punteros a padre. Si el nombre no existe, error `ErrorDeNombre: nombre 'X' no esta definido`.
- **Aritmética entero⊕entero** vía libtommath: `+`, `-`, `*`, `//` (floor division estilo Python para negativos), `%` (módulo matemático con resultado siempre del signo del divisor), `**` (potencia con exponente que cabe en `int`; exponente negativo promociona a decimal `pow()`). Sin overflow: `2 ** 100` da el bignum exacto de 31 dígitos, `10 ** 100` el gugol completo.
- **True division `/`**: siempre produce `VAL_DECIMAL` (estilo Python 3), incluso para enteros divisibles (`6 / 2` → `3.0`).
- **Promoción mixta entero/decimal**: cualquier operación con un decimal convierte el otro operando a doble. `1 + 2.5` → `3.5`. Para enteros muy grandes la conversión a doble pierde precisión, conducta documentada y consistente con Python.
- **Aritmética decimal⊕decimal** con `pow()`, `floor()` y módulo Python (`a - floor(a/b)*b` — resultado del signo del divisor: `-7.5 % 3.0 == 1.5`).
- **Bitwise**: `&`, `|`, `^` vía `mp_and`/`mp_or`/`mp_xor`. `<<` (`mp_mul_2d`) y `>>` (`mp_div_2d` con ajuste a floor para negativos). `~` (complemento a uno) vía `mp_complement`. Booleanos se promueven a entero (1/0). Errores específicos para desplazamiento negativo o demasiado grande.
- **Comparaciones**: `==`, `!=`, `<`, `<=`, `>`, `>=`. Función `comparar_valores` con `Orden` (LT/EQ/GT/INCOMP). `==` y `!=` permiten tipos distintos (devuelven `false`); `<` etc. dan `ErrorDeTipo` si los tipos no son comparables. Cross-tipo numérico: entero/decimal/booleano se comparan matemáticamente. Cadenas: lexicográfico byte a byte (UTF-8 preservado).
- **`valor_iguales` extendido**: ahora trata `verdadero == 1`, `falso == 0`, `verdadero == 1.0` como verdadero (Python: bool es subclase de int).
- **Lógica con cortocircuito**: `y` y `o` evalúan el operando derecho solo si el izquierdo no decide. Devuelven el **valor decisor original** (no booleano), igual que Python: `0 o 42` → `42`, `1 y "x"` → `"x"`. El test `verdadero o (1 // 0)` pasa porque la división por cero nunca se evalúa.
- **Unarios**: `-x` (negación numérica con `mp_neg`), `+x` (identidad), `no x` (negación lógica usando `valor_es_verdadero`), `~x` (complemento a uno).
- **Cadenas**: `+` concatena (nuevo buffer en heap, `dueno_cadena=true`), `*` con entero repite (con detección de overflow del tamaño total), comparaciones lexicográficas, `subcadena en cadena` mediante búsqueda lineal.
- **`es` (identidad)**: para funciones/nativas compara puntero. Para inmutables (entero, decimal, cadena, booleano, nulo) coincide con `valor_iguales` por ahora — se refinará cuando lleguen instancias y objetos heap.
- **`en` (membership)**: solo soportado para `subcadena en cadena` en esta sesión. Listas/diccionarios llegan en F5.
- **Aplazadas a sesiones siguientes** (devuelven error explícito): `EXPR_LLAMADA`, `EXPR_ATRIBUTO`, `EXPR_LAMBDA`, colecciones (`EXPR_LISTA`, `EXPR_DICCIONARIO`, `EXPR_CONJUNTO`, `EXPR_TUPLA`), `EXPR_INDICE`, `EXPR_REBANADA`, f-string con interpolación parseada.
- **`tests/unit/test_runtime_evaluador.c`** con ~70 verificaciones agrupadas en 14 grupos: literales (cada base, escapes), aritmética entera (precedencia, asociatividad, bignum 31 dígitos), división y mixto, decimales, comparaciones (mismo tipo y cross-tipo), bitwise (incluido `~`), unarios (incluida doble negación), lógica con cortocircuito demostrado, cadenas (concat/repetición/membership), identidad (`es`, `no es`, `es no`), identificadores con entorno definido, errores (división por cero, nombre, tipo), y combinaciones realistas (gugol, promedio, condiciones encadenadas).
- **32 tests verde** (12 unit + 20 integración).

### Añadido (Fase 4 sesión 1)
- **Vendoreado [libtommath 1.3.0](https://github.com/libtom/libtommath)** en `vendor/libtommath/` (~150 archivos `.c`, Public Domain). Bignum desde día 1 según decisión [B3](decisiones/B3-representacion-numerica.md). Compilado como librería estática separada en CMake.
- **`src/valor.{h,c}`** — tipo `Valor` con tagged union de 7 variantes:
  - `VAL_NULO`, `VAL_BOOLEANO`, `VAL_DECIMAL` (IEEE 754 double).
  - `VAL_ENTERO` con `mp_int *` boxed (precisión arbitraria; `factorial(100)` produce número de 158 dígitos sin overflow).
  - `VAL_CADENA` con bandera `dueno_cadena` (referencia al buffer fuente vs heap).
  - `VAL_FUNCION`, `VAL_NATIVA` (preparados para sesión 4).
- **Constructores**: `valor_nulo()`, `valor_booleano()`, `valor_decimal()`, `valor_decimal_de_lexema()`, `valor_entero_de_long()`, `valor_entero_de_lexema()` (acepta decimal, hex `0xff`, octal `0o755`, binario `0b1010`, con `_` separadores), `valor_cadena_referencia()`, `valor_cadena_duplicar()`.
- **Operaciones**: `valor_destruir`, `valor_clonar` (deep), `valor_imprimir`, `valor_a_cadena`, `valor_nombre_tipo`, `valor_es_verdadero` (truthiness ESPEC §6.2), `valor_iguales` (igualdad ESPEC §6.3 incluyendo `1 == 1.0`).
- **`src/entorno.{h,c}`** — `Entorno` (scope chain) con tabla hash de probing lineal:
  - Hash FNV-1a 32-bit, factor de carga 0.75, redimensionamiento dinámico.
  - API: `entorno_iniciar`, `entorno_destruir`, `entorno_definir`, `entorno_obtener` (devuelve clon), `entorno_asignar` (mutación), `entorno_existe`.
  - Scope chain por puntero a `padre`: una variable se busca aquí y, si no, en los entornos enclosing.
  - El entorno es **dueño** de los Valores; al destruirse libera todos sus mp_int y cadenas con dueño.
- **Sin GC** en Fase 4 (decisión B2 + B3): liberación eager. Cuando un entorno se destruye, todos los valores locales se liberan. En Fase 7 se añade GC mark-sweep.
- **`tests/unit/test_runtime_valor.c`** con ~25 tests: construcción de cada tipo, bignum (factorial 100 = 158 dígitos), verdadez, igualdad (incluyendo `1 == 1.0`), clonación, operaciones de entorno (definir, obtener, asignar, scope chain con padre, shadowing, redimensionamiento al añadir 100 variables).
- **31 tests verde** (11 unit + 20 integración).

### En desarrollo (Fase 3 — Parser y AST, objetivo v0.3.0)
- ✅ Sesión 1: AST + arena allocator + Pratt parser para expresiones.
- ✅ Sesión 2: sentencias simples + control de flujo + validación `fin <etiqueta>`.
- ✅ Sesión 3: funciones, clases, lambda.
- ✅ Sesión 4: excepciones, módulos, global/nolocal.
- ✅ Sesión 5: literales de colección, indexación, slicing, operadores de identidad/membership, `--ast` flag, tests de integración del parser, tag v0.3.0.

### Añadido (Fase 3 sesión 5)
- **Literales de colección** (`EXPR_LISTA`, `EXPR_DICCIONARIO`, `EXPR_CONJUNTO`, `EXPR_TUPLA`) con todas las variantes:
  - `[1, 2, 3]` lista; `[]` lista vacía; trailing comma permitida.
  - `{"k": "v"}` diccionario; `{}` diccionario vacío.
  - `{1, 2, 3}` conjunto.
  - `()` tupla vacía; `(x,)` tupla de 1; `(a, b)` tupla de 2+.
  - **Distinción tupla vs grupo**: `(x)` es grupo, `(x,)` es tupla.
- **Indexación** (`EXPR_INDICE`): `lista[0]`, `dicc[clave]`, `obj.attr[i]`, encadenamientos `matriz[i][j]`.
- **Slicing** (`EXPR_REBANADA`): `lista[a:b]`, `lista[a:b:c]`, con omisiones (`[:b]`, `[a:]`, `[:]`, `[::c]`).
- **Operadores de identidad y membership** (ESPEC §5 `op_comp`):
  - `a es b` → identidad.
  - `a es no b` → identidad negada (forma ESPEC).
  - `a no es b` → identidad negada (forma natural castellana).
  - `a en b` → pertenencia.
  - `a no en b` → pertenencia negada.
  - Las formas con `no` se desazucaran a `(uop "no" (op "es" / "en" izq der))`.
- **Flag `--ast`** en `cornamusa` que vuelca el AST del programa en formato S-expression. Ejemplo: `cornamusa --ast programa.cor`.
- **`tests/unit/test_parser_colecciones.c`** con ~30 tests cubriendo cada forma de literal, indexación, slicing, distinción tupla/grupo, anidamiento.
- **Tests de integración del parser**: 8 ejemplos parsean correctamente con `--ast`:
  - ✅ 01_hola_mundo, 02_fizzbuzz, 04_factorial, 07_clases_herencia, 08_excepciones, 09_closures, 11_iterador, 12_modulos.
- **30 tests verde** (10 unit + 12 integración del lexer + 8 integración del parser).

### Aplazado a v0.3.1 (parsean en sesiones futuras de Fase 3)
- **Multi-target assignment** (`a, b = b, a + b`) — usado en 03_fibonacci.
- **Iteración con tuple destructuring** (`para palabra, conteo en pares.elementos():`) — usado en 06_diccionarios.
- **List comprehensions** (`[x*x para x en y si cond]`) — usadas en 05_listas y 10_quicksort.
- **f-strings con interpolación parseada** (actualmente `EXPR_LITERAL_F_CADENA` almacena el lexema completo; las expresiones `{...}` no se parsean como sub-AST todavía).

### Añadido (Fase 3 sesión 4)
- **AST de excepciones, módulos y declaraciones**:
  - `SENT_INTENTAR`: cuerpo + lista de cláusulas `atrapar` + `sino` opcional + `finalmente` opcional.
  - `SENT_LANZAR`: expresión opcional (NULL = re-raise).
  - `SENT_IMPORTAR`: ruta dotted + alias opcional.
  - `SENT_DESDE_IMPORTAR`: ruta + items con aliases opcionales (o `*`).
  - `SENT_GLOBAL` / `SENT_NOLOCAL`: lista de nombres.
- **Tipos auxiliares**: `Nombre` (puntero+longitud al lexema), `ItemImportado` (nombre + alias opcional), `ClausulaAtrapar` (tipo + alias + cuerpo).
- **Parser de excepciones**:
  - `intentar:` con cero o más `atrapar [TipoExc [como alias]]:`, opcional `sino:` (rama sin excepción), opcional `finalmente:`, cerrado con `fin intentar`.
  - Validación: `intentar` requiere al menos un `atrapar` O `finalmente`. Error específico si ambos faltan.
  - `atrapar`/`finalmente` ahora son terminadores válidos de bloque (extendido `en_inicio_de_termino`).
- **Parser de `lanzar`**: `lanzar expr` con expresión, o `lanzar` desnudo en la misma línea de un atrapar como re-raise. Heurística para detectar bare lanzar: nuevo line o token de cierre tras el keyword.
- **Parser de imports**:
  - Helper `parsear_ruta_modulo` consume `IDENT ('.' IDENT)*`.
  - `importar X.Y.Z [como W]`.
  - `desde X.Y importar A [como A2], B, C` o `desde X importar *`.
- **Parser de `global`/`nolocal`**: lista de identificadores separados por coma.
- **`tests/unit/test_parser_excepciones_modulos.c`** con ~22 tests cubriendo: cada forma de `intentar`/`atrapar`/`finalmente`/`sino`, `lanzar` con valor y bare, imports simples/dotted/con-alias, `desde X importar Y` con uno/varios items/alias/`*`, `global` y `nolocal` con uno/varios nombres, anidamiento realista (función con `intentar` dentro como en `examples/08_excepciones.cor`, closure con `nolocal` como en `examples/09_closures.cor`), y errores específicos.
- **21 tests verde** (9 unit + 12 integración).

### Añadido (Fase 3 sesión 3)
- **`SENT_FUNCION`** en AST: nombre, parámetros, anotación de retorno opcional, cuerpo.
- **`SENT_CLASE`** en AST: nombre, lista de superclases (`extiende A, B, C`), cuerpo.
- **`EXPR_LAMBDA`** en AST: parámetros + cuerpo (una sola expresión, no bloque).
- **`Parametro`** struct: nombre + anotación de tipo opcional + valor por defecto opcional.
- **Parser de funciones**:
  - `funcion nombre(p1, p2, ...) [-> tipo]:`
  - Parámetros con anotación de tipo (`n: entero`) y valor por defecto (`idioma="es"`) en cualquier combinación.
  - Anotación de retorno con `-> tipo`.
  - Cuerpo: bloque multilínea cerrado con `fin funcion`, o one-liner.
- **Parser de clases**:
  - `clase Nombre [extiende A, B, ...]:`
  - Multi-herencia sintácticamente aceptada (semántica MRO en runtime).
  - Cuerpo cerrado con `fin clase`. Métodos son sentencias `funcion` dentro.
- **Parser de lambda**:
  - `lambda x, y, n=10: x + y + n`
  - Parámetros sin paréntesis. Defaults permitidos. **Anotaciones de tipo NO permitidas** en lambda (el `:` siempre es terminador).
  - Cuerpo es una sola expresión.
- **Pretty-printer extendido**: `(funcion nombre (param x) (param y (defecto ...)) (retorno ...) (bloque ...))`, `(clase Nombre (extiende ...) (bloque ...))`, `(lambda (param x) <expr-cuerpo>)`.
- **Validación de etiquetas extendida**: `fin funcion` y `fin clase` ahora se validan correctamente. `fin si` cerrando una función produce mensaje específico.
- **`tests/unit/test_parser_funciones.c`** con ~20 tests cubriendo:
  - Funciones con 0/1/varios parámetros, anotaciones de tipo, defaults, anotación de retorno, one-liner.
  - Clases vacías, con métodos, con herencia simple y múltiple, ejemplo realista del `examples/07_clases_herencia.cor`.
  - Lambdas vacías, con uno/varios parámetros, con defaults, anidadas en llamadas (`mapear(lambda x: x*2, lista)`).
  - Validación: `fin funcion` no cierra `si` (y viceversa).
  - Errores: función sin nombre, sin `(`, clase sin nombre, lambda sin cuerpo.
  - Anidamiento realista: función con `si` dentro (patrón fibonacci).
- **Limitación documentada**: las palabras `y`, `o`, `no`, `en`, `es` (operadores lógicos/comparativos como palabra) **son keywords y no se pueden usar como identificadores**. Tests usan nombres alternativos (`z`, `n`).
- **20 tests verde** (8 unit + 12 integración).

### Añadido (Fase 3 sesión 2)
- **AST de sentencias** en `ast.{h,c}`: 11 variantes (`SENT_EXPR`, `SENT_ASIGNAR`, `SENT_ASIGNAR_AUG`, `SENT_PASAR`, `SENT_ROMPER`, `SENT_CONTINUAR`, `SENT_RETORNAR`, `SENT_SI` con cadena de `RamaSi`, `SENT_MIENTRAS`, `SENT_PARA`, `SENT_BLOQUE`). Pretty-printer en S-expression.
- **Parser de sentencias**: `parser_parsear_sentencia` y `parser_parsear_programa`. Maneja:
  - Sentencias simples: `pasar`, `romper`, `continuar`, `retornar [expr]`.
  - **Asignación simple** (`x = expr`) y **aumentada** (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`).
  - **Sentencia-expresión** (cualquier expresión usada como sentencia: `imprimir(x)`).
  - **Bloques `si`/`sino si`/`sino`** con cadena completa de ramas, cerrado con `fin si`.
  - **`mientras`/`fin mientras`** con cláusula `sino` opcional.
  - **`para X en Y:`/`fin para`** con cláusula `sino` opcional.
- **Detección de one-liners**: si tras `:` el siguiente token está en la misma línea, se parsea una sola sentencia sin requerir `fin <X>`. Si va a línea siguiente, se exige bloque multilínea cerrado con `fin <etiqueta>`.
- **Validación de `fin <etiqueta>`** mediante stack de bloques abiertos en el parser (`pila_bloques[64]`):
  - `fin si` solo cierra `si`. `fin para` solo cierra `para`. Etc.
  - Mensaje específico cuando la etiqueta no coincide:
    *"se esperaba 'fin si' (bloque abierto en línea 9), encontrado 'fin para'"*.
  - Mensaje específico cuando falta el `fin`:
    *"se esperaba 'fin si' para cerrar el bloque abierto en línea 9"*.
- **Recuperación de errores** con panic mode: tras un error, el parser sale del modo pánico al inicio de cada sentencia para poder reportar varios errores en un programa.
- **Anidamiento arbitrario**: `si` dentro de `para` dentro de `mientras` funciona; cada bloque tiene su propia entrada en el stack.
- **`tests/unit/test_parser_sentencias.c`** con ~30 tests cubriendo: cada sentencia simple, asignaciones, todas las variantes de `si`/`mientras`/`para` (con/sin `sino`, one-liner vs multilínea), anidamiento, validación de etiquetas (`fin para` cerrando un `si` da error, etc.), errores de sintaxis (`fin` desnudo, falta `:`, falta `fin`), y un programa completo de varias sentencias.
- **19 tests verde** (7 unit + 12 integración del lexer).

### Añadido (Fase 3 sesión 1)
- **`src/arena.{h,c}`** — arena allocator con bloques crecientes (~80 líneas). Aloca alineado a 8 bytes, libera todo en una sola llamada con `arena_destruir`. Patrón estándar para ASTs (lo usan V8, GCC, LLVM).
- **`src/ast.{h,c}`** — AST tipado con tagged union. Esta sesión define **expresiones** con 13 variantes:
  - Literales: `EXPR_LITERAL_ENTERO`, `EXPR_LITERAL_DECIMAL`, `EXPR_LITERAL_CADENA`, `EXPR_LITERAL_F_CADENA`, `EXPR_LITERAL_BOOLEANO`, `EXPR_LITERAL_NULO`.
  - `EXPR_IDENT`, `EXPR_BINARIO`, `EXPR_UNARIO`, `EXPR_LOGICA` (`y`/`o`).
  - `EXPR_LLAMADA`, `EXPR_ATRIBUTO`, `EXPR_GRUPO`.
  - Pretty-printer en formato S-expression (`(op "+" (lit-int 1) (lit-int 2))`) para tests y depuración.
- **`src/parser.{h,c}`** — Parser estilo **Pratt** con tabla de reglas (prefijo, infijo, precedencia). Maneja:
  - **14 niveles de precedencia** desde `o` (más bajo) hasta llamada/atributo (más alto).
  - **Asociatividad correcta**: izquierda para `+ - * / // % == != < > <= >= y o & | ^ << >>`, derecha para `**`.
  - **Llamadas con argumentos** (0 o más, separados por coma).
  - **Acceso a atributo encadenado** (`a.b.c`).
  - **Operadores unarios**: `-x`, `+x`, `no x`, `~x`.
  - **Recuperación de errores** con panic mode + flag `tuvo_error`.
  - **Mensajes de error con caret** reusando `error_imprimir_token` de Fase 2.
- **`tests/unit/test_parser_expresiones.c`** — 35+ tests cubriendo: literales (cada tipo), identificadores, operadores con precedencia y asociatividad correctas (`1 + 2 * 3` → `1 + (2*3)`; `2 ** 3 ** 4` → `2 ** (3**4)`), unarios anidados, lógicas (`y`/`o` con precedencia entre ellos y vs `no`), agrupación, llamadas con varios args y anidadas, atributos encadenados, métodos (`obj.metodo(arg)`), combinaciones realistas extraídas de ejemplos (`tipo(yo).__nombre__`, `n * factorial(n - 1)`, `x > 0 y x < 100`), y errores (paréntesis sin cerrar, atributo sin nombre, operador sin operando).
- Build verde con flags estrictos. **18 tests verde** (6 unit + 12 integración del lexer).

### En desarrollo (Fase 2 — Lexer, objetivo v0.2.0)
- ✅ Sesión 1: esqueleto del lexer + tokens simples (símbolos, operadores, comentarios).
- ✅ Sesión 2: literales numéricos y cadenas básicas.
- ✅ Sesión 3: identificadores Unicode + NFC + tabla de keywords.
- ✅ Sesión 4: f-strings y triple-quoted strings.
- ✅ Sesión 5: mensajes de error pulidos siguiendo MENSAJES.md + tests exhaustivos.

### Añadido (Fase 2 sesión 5)
- **Refactor `Token`**: nuevo campo `mensaje` (NULL para tokens normales, contiene el mensaje de error para `TT_ERROR`). El campo `inicio`/`longitud` ahora describe siempre el span en la fuente — para errores, el fragmento problemático que producirá el caret indicator. Esto permite mensajes de error con calidad de Rust/Python 3.10.
- **`struct Token` con nombre** (en lugar de typedef anónimo) para permitir forward declarations entre módulos.
- **`error_imprimir_token`** en `errores.{h,c}`: formatea un token de error siguiendo MENSAJES.md §2 con anatomía completa:
  ```
  ErrorDeSintaxis en archivo.cor:3:18
          retornar 1__2
                   ^^
  no se permiten guiones bajos consecutivos en literales numéricos
  ```
  Carets dibujados a partir de `columna` y `longitud` del token. La línea de fuente se localiza en el buffer original sin copiar.
- **`error_imprimir`** extendida para aceptar `fuente` y `longitud_span` opcionales. Si se proporcionan, dibuja el contexto de línea + carets.
- **`main.c` reescrita**: pipeline completo `archivo → fuente_cargar_archivo (NFC) → Lexer → tokens`. Reporta errores léxicos con `error_imprimir_token`. Nuevo flag `--tokens` que vuelca todos los tokens en formato debug `LINEA:COL TIPO "lexema"`.
- **Tests de integración**: 12 tests CTest (uno por ejemplo en `examples/`) que invocan `cornamusa <archivo.cor>` y verifican exit code 0 (sin errores léxicos). Etiquetados con label `integracion` en CTest.
- Tests unitarios actualizados: `t.inicio` → `t.mensaje` en las verificaciones de mensajes de error (4 archivos, ~15 ocurrencias).
- Verificado manualmente: los 12 ejemplos en `examples/` lexán sin error. El error de muestra (`1__2` en código) produce el caret indicator correcto bajo el span ofensivo.

**Total tests al cerrar Fase 2:** 17 (5 unit + 12 integración), 100% verde con build Release y -O3.

### Añadido (Fase 2 sesión 4)
- Lexer reconoce **f-strings** (`TT_F_CADENA`):
  - Prefijo `f` o `F` inmediatamente seguido de comilla simple o doble.
  - Interpolación `{expresión}` con tracking de profundidad de llaves balanceadas.
  - `{{` y `}}` son llaves literales (no abren ni cierran interpolación).
  - El lexema completo (incluyendo `f` y comillas) se almacena en el token; el parser/AST hará el mini-parse de cada interpolación cuando llegue Fase 3.
- Lexer reconoce **cadenas triple-quoted** (`"""..."""` y `'''...'''`):
  - Multilínea: el contador de líneas avanza correctamente al ver `\n` interno.
  - Comillas dobles o simples sueltas dentro no cierran la triple (solo tres consecutivas idénticas a la apertura).
  - Compatible con prefijo `f`: `f"""..."""` y `f'''...'''` se reconocen como `TT_F_CADENA`.
- Refactor interno: `escanear_cadena` es ahora dispatcher entre `escanear_cadena_simple` y `escanear_cadena_triple`. Helpers `procesar_escape` y `saltar_interpolacion` factorizan la lógica de escapes y brace tracking. Firma `bool` para señalar errores limpiamente.
- Errores nuevos:
  - `f"hola {sin cerrar` → "interpolación de f-cadena sin cerrar antes del fin de archivo".
  - `f"hola {x\ny}"` (newline dentro de interp en f-string simple) → mensaje específico.
  - `f"hola }"` → "'}' inesperado en f-cadena (usa '}}' para llave literal)".
  - `"""sin cerrar` → "cadena triple sin cerrar antes del fin de archivo".
- `tests/unit/test_lexer_f_cadenas.c` añadido con 36 tests cubriendo: f-strings sin/con interpolación, mayúsculas (`F`), comillas simples, llaves literales, triple-quoted con conteo de líneas correcto, combinación f+triple, escapes, errores específicos, distinción `f"..."` vs `f` + `"..."` (ident + cadena), lexemas y secuencias realistas inspiradas en `examples/03_fibonacci.cor` y `06_diccionarios.cor`.
- `tests/unit/test_lexer_literales.c` renombrado a `tests/unit/test_lexer_numeros_cadenas.c` por consistencia (el nombre describe mejor el contenido).
- 5/5 tests verde con build Release optimizado: smoke + simbolos + numeros_cadenas + identificadores + f_cadenas.

### Añadido (Fase 2 sesión 3)
- Vendoreado [utf8proc 2.10.0](https://github.com/JuliaStrings/utf8proc) en `vendor/utf8proc/` (~700 KB) para soporte Unicode y NFC. Compilado como librería estática que se enlaza al binario y los tests.
- Lexer reconoce **identificadores ASCII** (`TT_IDENT`): letras, dígitos (no al inicio), `_`, `$`. Camino rápido sin decodificación UTF-8.
- Lexer reconoce **identificadores Unicode**: cualquier letra Unicode (categorías Lu, Ll, Lt, Lm, Lo, Nl) puede iniciar un identificador; continuación admite además dígitos (Nd), marks (Mn, Mc) y connector punctuation (Pc).
- Ejemplos válidos: `niño`, `año_actual`, `función_principal`, `días_vividos`, `contar_niños`.
- **Tabla de keywords castellanas** (~33 entradas) implementada como switch sobre el primer carácter:
  - Control de flujo: `si`, `sino`, `mientras`, `para`, `en`, `romper`, `continuar`, `retornar`, `pasar`, `fin`.
  - Funciones, clases, módulos: `funcion`, `lambda`, `clase`, `extiende`, `super`, `importar`, `desde`, `como`, `global`, `nolocal`.
  - Excepciones: `intentar`, `atrapar`, `finalmente`, `lanzar`.
  - Lógicas: `y`, `o`, `no`, `es`.
  - Literales: `verdadero`, `falso`, `nulo`.
  - Reservadas para futuro: `producir`, `asincrono`, `esperar`, `con`, `borrar`, `coincidir`.
- Las keywords son **case-sensitive y solo en minúscula** (decisión B4): `Si`, `FUNCION` son identificadores. `función` (con tilde) es identificador. `silencio` no es `si`.
- Multi-token keywords (`fin si`, `sino si`, `es no`) se emiten como tokens separados por decisión B1; la combinación se hace en el parser.
- Bytes UTF-8 inválidos producen `TT_ERROR` con mensaje "byte UTF-8 inválido".
- `src/fuente.{h,c}` añadidos: utility de carga (`fuente_cargar_archivo`, `fuente_normalizar`) que lee un archivo del disco, salta BOM UTF-8 si lo hay, valida UTF-8 y normaliza a NFC con `utf8proc_NFC`. Usa estructura `FuenteCargada` con código de error explícito y mensaje. Aún no conectado a `main.c` (sesión 4 o 5).
- `tests/unit/test_lexer_identificadores.c` añadido con 35+ tests cubriendo identificadores ASCII, Unicode (con `ñ` y tildes), las 33 keywords, casos delicados (palabra que empieza con keyword, case-sensitivity, keyword con tilde), errores UTF-8, y secuencias realistas (`funcion saludar(nombre):`, clase con método, etc.).
- `tests/unit/test_lexer_simbolos.c`: actualizado `test_secuencia_realista` que ahora reconoce `a` y `b` como `TT_IDENT`.
- Build verde con CMake; ctest 4/4 tests pasan (test_smoke, test_lexer_simbolos, test_lexer_literales, test_lexer_identificadores).

### Añadido (Fase 2 sesión 2)
- Lexer reconoce literales numéricos `TT_ENTERO`:
  - Decimales con guiones bajos opcionales (`42`, `1_000_000`, `1_00_00`).
  - Hexadecimal (`0xff`, `0xCAFE`, `0xCa_fE`, `0x_ff`).
  - Octal (`0o755`).
  - Binario (`0b1010`, `0b1010_1010`).
- Lexer reconoce literales decimales `TT_DECIMAL`:
  - Punto decimal (`3.14`, `0.5`).
  - Notación científica (`1e10`, `1.5E-3`, `2.5e+10`, `3E5`).
- Reglas de guiones bajos en numéricos: prohibidos al inicio del literal, al final, y consecutivos. `0x_ff` permitido (tras prefijo de base) por ergonomía visual.
- `1.` (sin dígito tras el punto) tokeniza como `TT_ENTERO 1` + `TT_PUNTO .`. Evita ambigüedad con acceso a atributo `obj.metodo`.
- Lexer reconoce literales de cadena `TT_CADENA` con comilla doble `"..."` o simple `'...'`. El lexema incluye las comillas (parser hará el unescape al construir el AST).
- Escape sequences aceptadas: `\n \t \r \\ \' \" \0 \x \u`. Validación profunda de los argumentos de `\xHH` y `\uHHHH` se aplaza a sesión 5.
- Errores específicos:
  - `1__2` → "no se permiten guiones bajos consecutivos".
  - `12_` → "literal numérico no puede terminar en '_'".
  - `0x` / `0o` / `0b` sin dígitos → mensaje específico por base.
  - `1e` / `1e+` → "exponente vacío en literal decimal".
  - `\z` → "secuencia de escape no reconocida".
  - Cadena con `\n` interno → "cadena sin cerrar antes del fin de línea".
  - Cadena que llega a EOF → "cadena sin cerrar antes del fin de archivo".
- `tests/unit/test_lexer_literales.c` añadido con 38 tests cubriendo enteros decimales, las tres bases especiales, decimales con punto y científica, cadenas con ambos delimitadores, escape sequences, errores y secuencias mixtas realistas.
- `tests/unit/test_lexer_simbolos.c` actualizado: `test_secuencia_realista` reconoce ahora `10` como `TT_ENTERO`.
- Build verde con 3/3 tests pasando (test_smoke, test_lexer_simbolos, test_lexer_literales).

### Añadido (Fase 2 sesión 1)
- `src/lexer.{h,c}` — esqueleto del lexer con enum `TipoToken` (~70 tipos), struct `Token`, struct `Lexer` y funciones `lexer_iniciar()` / `lexer_siguiente()` / `tipo_token_nombre()`.
- En esta sesión se reconocen: símbolos individuales (`(`, `)`, `[`, `]`, `{`, `}`, `,`, `.`, `:`, `;`, `@`, `~`), operadores aritméticos y sus formas compuestas (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`), comparaciones (`==`, `!=`, `<`, `<=`, `>`, `>=`), bitwise (`&`, `|`, `^`, `<<`, `>>`), y la flecha `->`.
- Whitespace y comentarios `# ...` se ignoran. Saltos de línea avanzan correctamente el contador de línea y reinician el cómputo de columna.
- Caracteres no reconocidos producen `TT_ERROR` con mensaje. `!` aislado sugiere `!=`.
- `src/errores.{h,c}` — infraestructura mínima de errores (struct `Error`, `error_iniciar()`, `error_destruir()`, `error_set_mensaje()`, `error_set_sugerencia()`, `error_imprimir()`). Formato siguiendo MENSAJES.md §2 sin caret indicators todavía (sesión 5).
- `tests/unit/test_lexer_simbolos.c` — 18 tests cubriendo: fuente vacía, whitespace, saltos de línea, todos los símbolos individuales, operadores compuestos, comentarios en distintas posiciones, tracking de línea/columna, errores léxicos, EOF idempotente, lexema apunta a fuente original.
- Build verde con CMake; `ctest` 2/2 tests pasan.

### Decisiones de diseño
- **[B1](decisiones/B1-modelo-de-bloques.md):** Modelo de delimitación de bloques resuelto. Cornamusa usa apertura con `:` y cierre explícito con `fin <etiqueta>` (`fin si`, `fin funcion`, `fin clase`, etc.), inspirado en la tradición castellana de PSeInt y Latino. La indentación es estilística, no semántica. Se descartó la indentación significativa por coste de implementación y peor calidad de errores.
- **[B4](decisiones/B4-tildes-y-unicode.md):** Reglas de tildes y Unicode resueltas. Las palabras clave del lenguaje son **ASCII puro sin tildes** (`funcion`, no `función`); los identificadores definidos por el usuario admiten cualquier letra Unicode (`niño`, `año_actual` válidos). El lexer normaliza a NFC obligatoriamente. Identificadores case-sensitive.
- **[B7](decisiones/B7-formato-numerico.md):** Formato numérico resuelto. El separador decimal en código es siempre `.` (universal); el separador de miles es `_` opcional. La convención castellana de coma decimal se gestiona en la biblioteca estándar (`formato.formatear` y `formato.leer_numero` con parámetro `locale`), no en la sintaxis.
- **[B5+B6](decisiones/B5-B6-yo-y-dunders.md):** Convención del primer parámetro y nomenclatura de dunders resueltos en un único ADR. El primer parámetro de métodos de instancia es **`yo` por convención** (no keyword: el nombre es libre, la stdlib y ejemplos oficiales usan `yo`). Los **dunders se nombran en castellano** según lista canónica de ~32 nombres (`__iniciar__`, `__cadena__`, `__longitud__`, `__sumar__`, etc.). Excepción razonada: `__repr__` mantiene su forma inglesa por brevedad y uso técnico universal.
- **[B2](decisiones/B2-tree-walking-vs-bytecode.md):** Arquitectura del pipeline de ejecución resuelta. **AST compartido** entre dos backends: tree-walking (Fase 4-5) y bytecode (Fase 6+). El tree-walking es minimalista (sin closures/clases/excepciones), sirve como primer release jugable y queda **congelado en v0.5** como referencia ejecutable de regresión. La VM bytecode es el motor de producción y destino de todas las optimizaciones. Esta arquitectura habilita tiered execution futura (Fase 12 JIT) sin reestructuración. Se descartó la opción A (ambos motores activos) tras analizar que es redundancia, no potencia — la potencia real a largo plazo viene de tiered execution sobre bytecode.
- **[B3](decisiones/B3-representacion-numerica.md):** Representación numérica de enteros resuelta. **Polimórfico fasado**: bignum boxed con [libtommath](https://www.libtom.net/LibTomMath/) (Public Domain, vendoreada) desde v0.4 con semántica matemáticamente correcta sin overflow; transición a tagged i63 + bignum en Fase 6 (fast path 1-3 ciclos, promoción transparente); especialización en Fase 10 con inline caching. **Sin breaking changes entre versiones** — `factorial(100)` funciona idéntico en v0.4 y v1.0, solo cambia velocidad. Descartadas: i64 puro (rompe pedagogía), bignum siempre (~50x más lento incluso en hot loops), tagged desde día 1 (complejidad innecesaria en tree-walking).
- **[I2](MENSAJES.md):** Estándar de calidad de mensajes de error definido. Documento normativo `MENSAJES.md` con anatomía formal de un error (categoría + ubicación + caret + mensaje + sugerencia), reglas de tono (tutear, no culpar, sugerir cuando aplica), 12 plantillas canónicas para los errores más comunes (variable no definida con "did you mean", tipo incompatible, bloque mal cerrado, división por cero, índice fuera de rango, etc.), anti-patterns explícitos, plan de implementación por fases (lexer en v0.2 con plantillas 5.5-5.6, parser en v0.3, runtime en v0.4) y estructura técnica (`Error` en C + tabla de mensajes preparada para futuro i18n).
- **[I5]** UTF-8 en consola Windows configurado en `src/main.c`. Función `configurar_consola_utf8()` llama `SetConsoleOutputCP(CP_UTF8)` y `SetConsoleCP(CP_UTF8)` al inicio del programa cuando se compila para Windows. Sin Windows-specific en otras plataformas (Linux/macOS ya son UTF-8 por defecto). Arregla mojibake al imprimir `ñ`, `á`, `¡` en cmd.exe / PowerShell.

### Cambios derivados de B1
- `ESPEC.md`: actualizada la sección 1 (filosofía), 2.7 (renombrada de "Indentación" a "Bloques"), tabla de keywords (añadido `fin`), gramática PEG sección 5, y programa de ejemplo sección 7.
- `examples/`: los 12 ejemplos `.cor` reescritos con `fin <etiqueta>`.
- `examples/11_iterador.cor`: campo `fin` renombrado a `limite` (colisión con keyword reservada).

### Cambios derivados de B4
- `ESPEC.md`: sección 1 (filosofía) reformulada — eliminada regla "tildes opcionales", añadidas reglas de keywords ASCII e identificadores Unicode con NFC.
- `ESPEC.md`: sección 2.2 (identificadores) — añadida normalización NFC y aclaración de case-sensitivity.
- `ESPEC.md`: sección 2.3 (keywords) — eliminada la columna "Forma sin tilde" de todas las tablas; `función` → `funcion` como única forma; `asíncrono` → `asincrono` en reservadas para futuro.

### Cambios derivados de B7
- `ESPEC.md`: sección 2.5 (literales numéricos) — añadida nota explicando el uso universal de `.` decimal y `_` separador de miles, con referencia al módulo `formato` para E/S localizada.
- Plan: módulo `formato` añadido a la stdlib mínima de Fase 9 con funciones `formatear()` y `leer_numero()` con parámetro `locale`.

### Cambios derivados de B5+B6
- `ESPEC.md` §2.2: añadida convención del primer parámetro `yo` con referencia a §6.6.
- `ESPEC.md` §2.3: `yo` eliminado de la tabla de keywords (es convención, no keyword reservada).
- `ESPEC.md` §4 (Métodos especiales): tabla reescrita con la lista canónica de dunders castellanos, organizada por categorías (construcción, comparaciones, colecciones, aritméticos, llamada, atributos dinámicos).
- `ESPEC.md` §6.6 (Modelo de objetos): expandida con sección sobre métodos de instancia y la convención `yo`, y mapeo de operadores → dunders.
- `examples/`: verificados — los dunders ya usados (`__iniciar__`, `__iterar__`, `__siguiente__`, `__cadena__`, `__nombre__`) coinciden con la lista canónica. Sin cambios necesarios.

### Cambios derivados de B2
- `ESPEC.md` §9: "Cuestiones abiertas" reescrita como índice de ADRs y pendientes menores.
- `README.md`: hoja de ruta con tabla de features explícitas por release y nota arquitectónica sobre AST compartido.

### Cambios derivados de B3
- ESPEC.md §3 (tipos primitivos): `entero` actualizado con descripción de precisión arbitraria desde v0.4 + transición tagged en Fase 6.
- ESPEC.md §2.5 (literales numéricos): añadida nota explícita sobre precisión arbitraria con ejemplo `gugol = 10 ** 100`.
- Plan: módulo `formato` añadido a la stdlib mínima de Fase 9 con funciones `formatear()` y `leer_numero()` con parámetro `locale`.

### Cambios derivados de I2
- Nuevo documento normativo `MENSAJES.md` (~600 líneas) en raíz del repo.
- Estándar aplicable a errores producidos por lexer (Fase 2), parser (Fase 3), tree-walking (Fase 4) y bytecode VM (Fase 6+).
- Plan de implementación detallado por fase con plantillas concretas listas para usar.

### Cambios derivados de I5
- `src/main.c`: añadido `#include <windows.h>` con guard `#ifdef _WIN32`.
- `src/main.c`: nueva función `configurar_consola_utf8()` llamada al inicio de `main()`.
- Verificado: `cornamusa.exe --version` y `--ayuda` ahora producen UTF-8 correcto en Windows. Sin impacto en Linux/macOS.

## [0.1.0] — 2026-04-27

### Añadido
- Estructura del repositorio: `src/`, `tests/`, `examples/`, `stdlib/`, `docs/`, `benchmarks/`.
- Build system con CMake (multiplataforma) y Makefile de conveniencia.
- Configuración de CI con GitHub Actions para Linux, Windows y macOS.
- `ESPEC.md`: especificación formal del lenguaje (gramática PEG, keywords, semántica).
- 12 programas de ejemplo en `examples/` que validan el diseño sintáctico.
- `README.md` en castellano con hoja de ruta y badges.
- Licencia MIT.
- `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `.editorconfig`, `.gitignore`.
- REPL trivial (eco) en `src/main.c` como esqueleto inicial.

<!-- TODO al publicar el repo: añadir enlaces de comparación de versiones -->
<!-- [No publicado]: https://github.com/USUARIO/cornamusa/compare/v0.1.0...HEAD -->
<!-- [0.1.0]: https://github.com/USUARIO/cornamusa/releases/tag/v0.1.0 -->

