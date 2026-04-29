# Registro de cambios

Todos los cambios notables a este proyecto se documentan en este archivo.

El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y este proyecto adhiere a [Versionado Semántico](https://semver.org/lang/es/).

## [No publicado]

## [0.6.2] — 2026-04-29 — closures + lambdas + slicing en bytecode

El motor bytecode ahora ejecuta el lenguaje completo módulo
excepciones, atributos y módulos. **8 de 9 ejemplos jugables
v0.5 + el nuevo 19_closures corren con `--bytecode`**.

### Añadido (v0.6.2)
- **`OP_REBANADA`** y compilación de `EXPR_REBANADA`: slicing `lista[a:b:c]` con cualquier campo opcional. Operandos faltantes se emiten como `OP_NULO` (sentinela). VM despacha con la misma semántica que el evaluador tree-walking: defaults dependientes del signo del paso, índices negativos cuentan desde el final, fuera de rango se clampea silenciosamente, paso 0 → `ErrorDeValor`.
- **Closures con upvalues** (estilo clox cap. 25). Refactor mayor:
  - **Separación `FuncionBC` (plantilla, en pool) vs `Closure` (instancia, en stack)**:
    - `FuncionBC` mantiene chunk + nombre + aridad + metadata de upvalues (`info_upvalues[]`).
    - `Closure` envuelve un `FuncionBC` y añade `Upvalue **upvalues` runtime.
    - Nuevo `VAL_PLANTILLA_BC` para el constant pool; `VAL_FUNCION_BC` ahora apunta a `Closure`.
  - **`Upvalue` runtime**: linked-list ordenada por posición decreciente en stack (`vm->open_upvalues`). Refcount para compartir entre múltiples closures que capturan la misma variable.
  - **`OP_CLOSURE [byte fn_idx] [n_upvalues * (es_local, indice)]`**: lee la plantilla del pool, crea Closure nuevo, conecta cada upvalue a su slot del frame actual (vía `capturar_upvalue`) o a un upvalue existente.
  - **`OP_OBTENER_UPVALUE [slot]`** y **`OP_ASIGNAR_UPVALUE [slot]`**: lectura/escritura via `frame->closure->upvalues[slot]->posicion`.
  - **`OP_RETORNAR` cierra upvalues** del frame que termina con `cerrar_upvalues_hasta`. Al cerrar, el valor del slot se **transfiere** (no se duplica) al campo `cerrado` del upvalue, y el slot original se vacía a `nulo` para evitar double-free.
  - `CallFrame` ahora tiene un puntero `closure` (NULL en frame top-level) que la VM usa para resolver upvalues durante la ejecución.
- **Compilador con resolución de upvalues recursiva**: `EXPR_IDENT`, `compilar_asignar` y `compilar_asignar_aug` siguen el orden **local → upvalue → global**. La función `resolver_upvalue(scope)` busca la variable en el scope padre directo (como local) y, si no la encuentra, recurre subiendo la cadena de scopes (capturando como "upvalue de upvalue"). El compilador rellena la metadata de upvalues en la `FuncionBC` para que `OP_CLOSURE` la use en runtime.
- **Funciones anidadas dentro de función ahora se registran como locales** (no como globales como antes en S5). El compilador detecta `c->actual->es_funcion` y emite `OP_DEFINIR_GLOBAL` solo en top-level.
- **Lambdas (`EXPR_LAMBDA`)** compiladas como funciones anónimas con cuerpo expresión:
  - Crea `FuncionBC` con nombre `"lambda"`.
  - Abre scope hijo con parámetros como locales.
  - Compila el cuerpo como expresión y emite `OP_RETORNAR`.
  - Emite en el padre `OP_CLOSURE` con metadata de upvalues capturados.
  - Sin valores por defecto (mismo que `SENT_FUNCION` en bytecode); error explícito si se usan.
- **Nuevo ejemplo `examples/19_closures_jugable.cor`** que demuestra contadores con estado y lambdas factory. Bytecode-only (decisión B2: el tree-walking no implementa closures).
- **`tests/unit/test_bytecode_closures.c`** con 7 grupos:
  - Captura simple de local desde función anidada.
  - Contador clásico (3 invocaciones).
  - Closures independientes (factoría produce instancias con estado separado).
  - Captura de dos niveles arriba (upvalue de upvalue).
  - Lambda sin captura.
  - Lambda con captura (factory de multiplicadores).
  - Slicing varios casos (omisiones, paso negativo, fuera de rango).
- **`bc_run_*` ampliados**: `16_lista_busqueda` (slicing) y `19_closures_jugable` ahora se ejecutan con `--bytecode` y verifican salida.
- **Versión** bump a `0.6.2`.
- **69 tests verde** (26 unit + 43 integración).

### Aplazado a v0.6.3+
- **Excepciones** (`intentar`/`atrapar`/`finalmente`/`lanzar`).
- **Atributos** (`obj.attr`) — necesita primero el sistema de objetos (Fase 8).
- **Módulos** (`importar`).
- **`global`/`nolocal`** declaraciones explícitas.
- **Valores por defecto** en parámetros bytecode (sí en tree-walking).

## [0.6.1] — 2026-04-29 — bytecode con iteración

Bytecode amplía soporte: `SENT_PARA` con iteradores genéricos sobre
listas, tuplas, cadenas (UTF-8), rangos, diccionarios y conjuntos.
Asignación aumentada con destino índice (`dicc[k] += 1`). 7 de 9
ejemplos jugables corren ya por bytecode.

### Añadido (v0.6.1)
- **`VAL_ITERADOR`** y **`struct Iterador`** en `valor.{h,c}`: tipo VM-only (no expuesto al usuario) que mantiene el estado de iteración. Campos: copia con refcount del iterable + cursor int. La función `iter_siguiente` despacha por tipo:
  - **Lista/Tupla**: cursor = índice.
  - **Cadena**: cursor = byte position; avanza por code points UTF-8 con `utf8proc_iterate`.
  - **Diccionario/Conjunto**: cursor = slot interno; salta entradas vacías; emite claves (dict) o elementos (conjunto).
  - **Rango**: cursor = número de iteración; calcula `inicio + cursor*paso` cada vez.
- **`OP_ITER_INICIAR`**: pop iterable, push iterador (validado con `valor_es_iterable`).
- **`OP_ITER_SIGUIENTE [byte slot] [u16 offset]`**: lee el iterador del slot dado del frame actual. Si tiene siguiente, push valor; si no, salta `offset` bytes (los `OP_ASIGNAR_LOCAL` siguientes no se ejecutan, el slot iterador se libera con el frame).
- **`SENT_PARA` en compilador**:
  - Compila iterable + `OP_ITER_INICIAR`.
  - Reserva un local oculto `$iter` con el iterador en su slot.
  - Si el objetivo es local: pre-asigna con `OP_NULO` y emite `OP_ASIGNAR_LOCAL` en cada iteración (evita el bug "asignar al top sobre sí mismo").
  - Si es top-level: emite `OP_DEFINIR_GLOBAL` en cada iteración.
  - Soporta `romper`/`continuar` y cláusula `sino` (ejecutada solo al agotarse el iterador, no por break).
- **Locales en scope top-level**: el compilador permite registrar locales (vía `agregar_local`) incluso en `es_funcion=false`. Esto habilita el slot oculto `$iter` y evita interferencias con globales del usuario.
- **`OP_DUP_2`**: duplica los dos valores del tope (`a, b → a, b, a, b`). Necesario para implementar aug-assign en índice sin reevaluar `obj` y `key`.
- **`compilar_asignar_aug` con destino `EXPR_INDICE`**: compila como `obj key OP_DUP_2 OP_INDICE valor OP_op OP_ASIGNAR_INDICE OP_DESCARTAR`. Permite `dicc[k] += 1`, `lista[i] *= 2`, etc.
- **`OP_ES` y `OP_EN`** añadidos al enum `OpCode` y mapeados desde `TT_ES`/`TT_EN` en el compilador. La VM los despacha a `evaluador_aplicar_binario` igual que las comparaciones — la lógica completa (identidad, membership en cadena/lista/dicc/conjunto/tupla) se reusa del refactor de S2.
- **Versión** bump a `0.6.1`.
- **`tests/unit/test_bytecode_iter.c`** con 10 grupos: `para` sobre cadena (incluido UTF-8), rango (con paso negativo y `rango(n)`), lista/tupla, dicc/conjunto, `romper`, `continuar`, cláusula `sino`, `para` dentro de función (conteo de vocales en `"murcielago"`), aug-assign con índice (frecuencia de letras en `"abracadabra"`, mutación de lista), factorial(25) iterativo via bytecode (26 dígitos).
- **`bc_run_*` ampliados**: `02_fizzbuzz`, `14_contar_vocales`, `15_fizzbuzz_jugable`, `17_dicc_frecuencia`, `18_conj_y_tupla` ahora se ejecutan con `--bytecode` y verifican misma salida que tree-walking.
- **65 tests verde** (25 unit + 40 integración: 12 lex + 8 parse + 13 run + 7 bc_run).

### Aplazado a v0.6.2+
- **Closures con upvalues** (estilo clox cap. 25): funciones anidadas que capturan locales del scope enclosing. Requiere `OP_CLOSURE`, `OP_GET_UPVALUE`, `OP_SET_UPVALUE`, `OP_CLOSE_UPVALUE` y tracking runtime de upvalues abiertos.
- **Lambdas** (`lambda x: x*2`): mismo modelo que `SENT_FUNCION` pero como expresión.
- **Slicing** (`lista[a:b:c]`): `OP_REBANADA`.
- **Atributos** (`obj.attr`): hace falta primero el sistema de objetos (Fase 8).
- **Excepciones** (`intentar`/`atrapar`/`finalmente`): tabla de excepciones por chunk, manejo de stack unwinding.

## [0.6.0] — 2026-04-29 — motor bytecode (opt-in)

Cierre de Fase 6 según el plan: compilador AST → bytecode + VM
stack-based ejecutando expresiones, sentencias, control de flujo,
funciones con recursión y colecciones. Motor opt-in con flag
`--bytecode`; el tree-walking sigue siendo el por defecto en v0.6.0
para preservar la cobertura completa del lenguaje (incluida la
iteración `para`, que el bytecode aún no soporta).

### Añadido (Fase 6 sesión 6)
- **`FnNativa` refactorizado para `EvalError *`**: las funciones nativas (built-ins) ya no dependen del struct `Evaluador` — toman `EvalError *` directamente. Esto permite invocarlas tanto desde el evaluador tree-walking como desde la VM bytecode sin acoplarlas a uno de los dos motores. Cambio de firma propagado a todas las nativas (`imprimir`, `longitud`, `tipo`, `rango`, `agregar`, `quitar`, `insertar`, `invertir`, `ordenar`, `claves`, `valores`, `conjunto`).
- **`nativos_registrar_dicc(Diccionario *globales)`** en paralelo a `nativos_registrar(Entorno *)`: ambos iteran una **lista canónica única** de nativas (`NATIVAS[]` en `nativos.c`) garantizando que tree-walking y bytecode ofrezcan los mismos built-ins.
- **VM `vm_iniciar` ahora registra los built-ins** en `vm->globales` automáticamente — los programas bytecode ya tienen acceso a `imprimir`, `longitud`, `tipo`, `rango`, etc. desde el primer byte de ejecución.
- **`OP_LLAMAR` en VM extendido para `VAL_NATIVA`**: cuando el callee es una nativa, la VM la invoca pasando `&vm->error` y limpia los args/callee del stack al volver. Las funciones definidas por el usuario (`VAL_FUNCION_BC`) siguen creando un `CallFrame` nuevo como en S5.
- **Colecciones literales en bytecode** con cuatro nuevos opcodes:
  - `OP_BUILD_LISTA [n]`: pop n elementos, push lista.
  - `OP_BUILD_TUPLA [n]`: pop n elementos, push tupla.
  - `OP_BUILD_DICC [n_pares]`: pop 2n elementos (k,v intercalados), push diccionario.
  - `OP_BUILD_CONJUNTO [n]`: pop n elementos hashables, push conjunto.
- **Indexación en bytecode**:
  - `OP_INDICE`: pop key, pop obj, push obj[key]. Despacha por tipo (lista/tupla/diccionario) en runtime con mensajes de error específicos (`ErrorDeIndice`, `ErrorDeClave`).
  - `OP_ASIGNAR_INDICE`: pop value, pop key, pop obj — `obj[key] = value`. Soporta listas y diccionarios.
- **Compilador con `EXPR_LISTA`/`EXPR_TUPLA`/`EXPR_DICCIONARIO`/`EXPR_CONJUNTO`/`EXPR_INDICE`**: producen los nuevos opcodes. Los literales con más de 255 elementos producen error explícito (limitación del operando byte).
- **`SENT_ASIGNAR` con destino `EXPR_INDICE`**: el compilador emite el bytecode `obj key valor OP_ASIGNAR_INDICE OP_DESCARTAR`.
- **`VM_PILA_MAX`** ampliado a 8192, **`VM_FRAMES_MAX`** a 256 para acomodar recursión profunda como `factorial(100)` (64 dígitos vía bytecode).
- **`OP_ITER_INICIAR` y `OP_ITER_SIGUIENTE`** reservados en el enum pero no implementados todavía — `SENT_PARA` queda aplazado a v0.6.1 (necesita un VAL_ITERADOR ad hoc o desazucar a `mientras` con manejo de slots temporales). El motor tree-walking sigue siendo el camino para programas que usan `para`.
- **Integración con `cornamusa`** (motor opt-in):
  - **Flag `--bytecode`** en `main.c` activa el pipeline `lex → parse → compilar → vm_ejecutar`. Sin la flag, el motor sigue siendo tree-walking (default en v0.6.0).
  - Errores de compilación o de runtime se reportan con `imprimir_error_runtime` (mismo formato MENSAJES.md §2 con caret).
- **Tests `bc_run_*`**: ejemplos `01_hola_mundo` y `13_factorial_jugable` (recursión que deja `100!` con 158 dígitos vía bytecode) se ejecutan también con `--bytecode` y verifican misma salida que el tree-walking.
- **`tests/unit/test_bytecode_colecciones.c`** con 9 grupos: literales (lista, tupla, dicc, conjunto), indexación (lista/tupla/dicc + errores), asignación a índice, nativas sobre colecciones via OP_LLAMAR, programas mixtos (función que indexa, dicc acumulador).
- **`test_bytecode_funciones.c` extendido** con grupo de nativas vía OP_LLAMAR (longitud sobre cadena UTF-8, rango, tipo).
- **Limitaciones documentadas para v0.6.0**:
  - `SENT_PARA` (`para X en Y`) **no compila a bytecode**; se ejecuta solo en tree-walking.
  - **Closures con upvalues** no implementadas (decisión B2 ya las excluía del tree-walking; las añadiremos en v0.6.1+).
  - Atributos (`obj.attr`), lambda, slicing, f-string interpolada, intentar/atrapar, importar — todos siguen aplazados.
- **Versión** bump a `0.6.0` en `common.h`, `CMakeLists.txt`, smoke test.
- **59 tests verde** (24 unit + 35 integración: 12 lex + 8 parse + 13 run tree-walking + 2 bc_run bytecode).

### Añadido (Fase 6 sesión 5)
- ⏳ Sesión 6: colecciones + transición a tagged i63 + flag `--tree-walking` + tests diferenciales + tag v0.6.0.

### Añadido (Fase 6 sesión 1)
- **`src/chunk.{h,c}`**: estructura `Chunk` con bytecode, pool de constantes y array paralelo de números de línea fuente. Tres arrays paralelos siguiendo clox cap. 14 (renombrado al castellano):
  - `codigo[]` (uint8_t): instrucciones y operandos inline.
  - `constantes[]` (Valor): pool referenciado por `OP_CONST` y `OP_CONST_LARGO`. El chunk es DUEÑO de los Valores y los destruye al liberarse.
  - `lineas[]` (int): un número de línea por byte de código. Sin compresión (suficiente para v0.6; se podrá optimizar a run-length encoding más adelante).
- **`enum OpCode`** con 32 instrucciones reservadas para toda la fase: literales (`OP_CONST`, `OP_CONST_LARGO` para >256 constantes, `OP_NULO`, `OP_VERDADERO`, `OP_FALSO`), aritmética (`+`, `-`, `*`, `/`, `//`, `%`, `**`, negación), lógica/comparación (`no`, `==`, `!=`, `<`, `<=`, `>`, `>=`), stack (`DESCARTAR`), control de flujo (`SALTAR`, `SALTAR_SI_FALSO`, `BUCLE`), variables (locales y globales), llamadas y retorno. Reservadas ahora para que el orden quede estable; se implementan progresivamente en S2-S5.
- **API**:
  - `chunk_iniciar`/`chunk_destruir` (idempotente).
  - `chunk_emitir_byte` y `chunk_emitir_byte2` con crecimiento ×2 amortizado.
  - `chunk_agregar_constante` devuelve índice; `chunk_emitir_constante` elige entre `OP_CONST` (1 byte de índice) y `OP_CONST_LARGO` (3 bytes little-endian) automáticamente cuando el pool supera 255 entradas.
  - `opcode_nombre` para inspección/debug.
- **`src/debug.{h,c}`**: disassembler estilo clox. Formato:
  ```
  == nombre ==
  0000  123 OP_CONST            7 '42'
  0002    | OP_RETORNAR
  ```
  - Offset (4 dígitos), línea fuente (con `|` cuando coincide con la anterior), nombre del opcode alineado a 20 caracteres, operandos formateados según el tipo de instrucción (constante, byte, u16 con destino calculado para saltos).
  - `desensamblar_chunk(chunk, nombre, salida)` y `desensamblar_instruccion(chunk, offset, salida)` (devuelve siguiente offset).
  - Constantes se imprimen con `valor_a_repr` (cadenas con comillas).
- **Limitación documentada**: este es solo el armazón. Sin compilador ni VM funcional aún — eso llega en S2. La idea de S1 es congelar el formato del chunk antes de añadir muchos consumidores.
- **`tests/unit/test_chunk_disasm.c`** con 11 tests: chunk vacío + idempotencia destruir, crecimiento de capacidad (100 bytes), `emitir_byte2`, ownership de constantes (entero/decimal/cadena), `emitir_constante` con índice corto y largo (forzando el cambio a `OP_CONST_LARGO` con 256 constantes previas), disassembler simple/aritmético, marca `|` para línea repetida, `opcode_nombre`.
- **52 tests verde** (19 unit + 33 integración).

### Añadido (Fase 6 sesión 2)
- **Refactor del evaluador** para que la lógica de operadores sea reutilizable desde la VM bytecode sin duplicación:
  - Nueva función pública `evaluador_aplicar_binario(EvalError *err, int op_token, Valor a, Valor b, int linea, int columna)` que toma posesión de `a`/`b` y devuelve un Valor nuevo. Mismo modelo de error que la versión interna pero desacoplado del struct `Evaluador` (toma `EvalError *` directamente).
  - Análoga `evaluador_aplicar_unario(EvalError *err, ...)`.
  - Helpers internos (`entero_op_entero`, `decimal_op_decimal`, `cadena_concatenar`, `cadena_repetir`, `evaluar_comparacion`, `evaluar_en`) cambiados a tomar `EvalError *err` y `int linea, int columna` en lugar de `Evaluador *ev` y `const Expr *e`. El evaluador tree-walking sigue funcionando idénticamente — solo se ha movido el acoplamiento.
  - Wrapper interno `aplicar_binario(Evaluador *ev, ...)` y `aplicar_unario_pos` para que los call-sites del tree-walking (que tienen `Expr *e` a mano) no se vean obligados a desempaquetarlo.
- **`src/vm.{h,c}`**: máquina virtual stack-based estilo clox cap. 15.
  - Pila de 256 slots (capacidad fija por ahora; será dinámica con frames cuando lleguen llamadas en S5).
  - Dispatch loop con `for(;;) switch(*ip++)`. Tracking de línea fuente vía `chunk->lineas[ip - codigo - 1]` para mensajes de error.
  - Implementa: `OP_CONST`/`OP_CONST_LARGO` (con `valor_clonar` del pool), `OP_NULO`/`OP_VERDADERO`/`OP_FALSO`, los 7 operadores aritméticos, las 6 comparaciones, `OP_NEGAR`/`OP_NO`, `OP_DESCARTAR`, `OP_RETORNAR` (extrae el tope y lo devuelve al cliente). Opcodes reservados (saltos, locales, llamadas) emiten error explícito "no implementado en v0.6 sesión 2".
  - Reusa `evaluador_aplicar_binario`/`unario` con un mapeo `OpCode → TipoToken`. Cero duplicación de la aritmética bignum / comparaciones / cadenas.
- **`src/compilador.{h,c}`**: visita el AST y emite bytecode al chunk.
  - Compila `EXPR_LITERAL_*` (entero/decimal/cadena/booleano/nulo), `EXPR_BINARIO`, `EXPR_UNARIO`, `EXPR_GRUPO`.
  - Las constantes se almacenan en el pool del chunk; el chunk es DUEÑO y las destruye al liberarse (incluyendo `mp_int*` y cadenas con dueño).
  - Compilación de cadena literal procesa los escapes mínimos (`\n \t \r \\ \' \"`) igual que el evaluador tree-walking — ambos motores producen el mismo Valor cadena para la misma fuente.
  - Aplazadas con error explícito: `EXPR_IDENT`, `EXPR_LOGICA`, `EXPR_LLAMADA`, `EXPR_LAMBDA`, colecciones, indexación, slicing, f-strings, atributos.
- **`tests/unit/test_bytecode_expr.c`** con 8 grupos de tests end-to-end (`lex → parse → compilar → vm_ejecutar`): literales (cada tipo + escapes), aritmética bignum (precedencia, asociatividad, floor div Python, 2^100 = 31 dígitos), aritmética decimal y mixta (true div siempre decimal, promoción), comparaciones (todas + cross-tipo entero=decimal + lexicográfico de cadenas + tipos incomparables → error), unarios (`-`, `+`, `no`, doble negación), realistas (Pitágoras, promedio decimal, semántica izquierda-a-derecha de Cornamusa sin chained comparisons), errores de runtime (división por cero, tipo incompatible) y errores de compilación (identificadores/lambda/lógica explícitamente no implementados todavía).
- **53 tests verde** (20 unit + 33 integración).

### Añadido (Fase 6 sesión 3)
- **Variables globales en la VM**: la `VM` ahora contiene un `Diccionario *globales` (refcount) que persiste entre llamadas a `vm_ejecutar` (útil para REPL futuro). Inicializado en `vm_iniciar`, liberado en `vm_destruir`.
  - **`OP_DEFINIR_GLOBAL [idx]`**: lee el nombre de `chunk->constantes[idx]`, saca el valor del tope, define o sobrescribe en globales. Cornamusa no distingue declaración de asignación, así que esta es la operación habitual.
  - **`OP_OBTENER_GLOBAL [idx]`**: empuja al stack una copia del valor; si la clave no existe, `ErrorDeNombre: nombre 'X' no esta definido`.
  - **`OP_ASIGNAR_GLOBAL [idx]`**: variante estricta (la clave debe existir); reservada para futuras semánticas más rigurosas, no usada por el compilador en S3.
- **Built-in `imprimir(...)` en bytecode**: nuevo `OP_IMPRIMIR [n]` que saca `n` valores del stack, los imprime separados por espacio + newline, y empuja `nulo` (porque `imprimir(...)` es expresión y `SENT_EXPR` la envuelve con `OP_DESCARTAR`). Soporta hasta 255 argumentos.
- **`compilador_compilar_sent`** soporta:
  - **`SENT_PASAR`**: no-op explícito.
  - **`SENT_EXPR`**: compila la expresión y emite `OP_DESCARTAR`.
  - **`SENT_ASIGNAR`** con destino `EXPR_IDENT`: compila el valor y emite `OP_DEFINIR_GLOBAL` con el nombre como constante. Tuple destructuring, atributos e índices como destino quedan para S6+.
  - **`SENT_BLOQUE`**: compila secuencialmente cada sentencia.
  - Resto de sentencias (`if/while/for/funcion/clase/intentar/lanzar/importar`) producen error explícito con su sesión objetivo.
- **`compilador_compilar_programa(c, sents, n)`**: compila cada sentencia y emite `OP_NULO + OP_RETORNAR` al final, dejando el chunk listo para `vm_ejecutar`.
- **`EXPR_IDENT`** ahora se compila a `OP_OBTENER_GLOBAL` (lookup en globales).
- **`EXPR_LLAMADA` con callee `imprimir`** se detecta como caso especial en el compilador y emite `OP_IMPRIMIR [n]`. Otras llamadas siguen produciendo error "no implementado en bytecode v0.6 sesión 3" — el sistema completo de funciones definidas por el usuario llega en S5.
- **Limitación documentada**: nombres de globales con índice >255 en el pool del chunk dan error explícito ("demasiadas constantes para v0.6 (operando byte)"). Se resolverá con variantes `*_LARGO` cuando sea necesario.
- **`tests/unit/test_bytecode_programa.c`** con 5 grupos: asignación a global (incluye reasignación, varias variables, cambio de tipo libre), error de nombre no definido en runtime, captura de stdout para `imprimir(...)` (con redirección dup/dup2 portable Windows/POSIX), `pasar` y un programa combinado (Pitágoras 3-4-5 imprimiendo "hipotenusa: 5.0").
- **54 tests verde** (21 unit + 33 integración).

### Añadido (Fase 6 sesión 4)
- **`OP_SALTAR`, `OP_SALTAR_SI_FALSO`, `OP_BUCLE`** implementados en la VM con operandos `u16` big-endian. `OP_SALTAR_SI_FALSO` hace **PEEK** (no pop) — el compilador inserta `OP_DESCARTAR` donde toca. Estilo clox cap. 23.
- **Helpers de salto en el compilador**:
  - `emitir_salto(op, linea)`: emite el opcode con placeholder `0xffff`, devuelve el offset para parchear después.
  - `parchear_salto(offset)`: rellena el placeholder con la distancia hasta la posición actual del chunk. Reporta error si excede `UINT16_MAX`.
  - `emitir_bucle(inicio)`: emite `OP_BUCLE` con offset hacia atrás.
- **Stack de bucles abiertos** en el `Compilador` (`BucleAbierto bucles[16]`):
  - `inicio_continuar`: offset al que `continuar` debe saltar (la condición del `mientras`).
  - `parches_romper[]`: array dinámico de offsets de `OP_SALTAR` emitidos por `romper`, parcheados al cerrar el bucle.
  - `empujar_bucle` / `cerrar_bucle` mantienen la pila al entrar/salir.
- **`EXPR_LOGICA` con cortocircuito real**:
  - `a y b`: si `a` es falso, salta sobre `b` dejando `a` en stack; si verdad, descarta `a` y evalúa `b`.
  - `a o b`: si `a` es verdadero, salta sobre `b` dejando `a` en stack; si falso, descarta `a` y evalúa `b`.
  - Verificable porque `verdadero o (1 // 0)` no produce error de división por cero — la rama no se compila a saltar, sino a un OP_SALTAR_SI_FALSO + OP_SALTAR que evita ejecutar el lado derecho.
- **`SENT_SI`** con cadena arbitraria de `si` / `sino si` / `sino`:
  - Cada rama compila `cond → OP_SALTAR_SI_FALSO else → OP_DESCARTAR → cuerpo → OP_SALTAR fin`.
  - La rama final `sino` no tiene condición ni descart.
  - Hasta 64 ramas en una cadena (límite arbitrario, suficiente).
- **`SENT_MIENTRAS`** con cláusula `sino` y `romper`/`continuar`:
  - Loop estándar: `inicio: cond → OP_SALTAR_SI_FALSO salir → OP_DESCARTAR → cuerpo → OP_BUCLE inicio`.
  - `romper` emite `OP_SALTAR` patcheable al fin del bucle (DESPUÉS de la cláusula `sino` para que `romper` salte sobre ella, semántica Python).
  - `continuar` emite `OP_BUCLE` al inicio de la condición.
  - Cláusula `sino` ejecutada solo si terminamos por condición falsa (no por break) — gracias a que el OP_SALTAR_SI_FALSO `salir` apunta antes de `sino` y los `romper` saltan después.
- **`SENT_ASIGNAR_AUG`** (`x op= expr`) compila como `x = x op expr`: emite `OP_OBTENER_GLOBAL` + compilar expr + opcode binario + `OP_DEFINIR_GLOBAL`. Soporta `+= -= *= /= //= %= **=`.
- **Limitación documentada**: `SENT_PARA` queda para S6 (necesita iteración sobre cadena/rango/lista que se conectará con las colecciones en bytecode).
- **`tests/unit/test_bytecode_control.c`** con 8 grupos: lógica con cortocircuito demostrado, `si`/`sino si`/`sino`, `mientras` con romper/continuar/sino, asignación aumentada (todas las variantes), y programas realistas: factorial(25)=26 dígitos, Fibonacci(30)=832040, 2^64 (20 dígitos), anidamiento `mientras` en `mientras`.
- **55 tests verde** (22 unit + 33 integración).

### En desarrollo (Fase 6 — Compilador + VM bytecode)
- ✅ Sesión 1: infraestructura `Chunk` + enum `OpCode` + disassembler.
- ✅ Sesión 2: refactor del evaluador (helpers reutilizables) + compilador para expresiones + VM stack-based con dispatch loop.
- ✅ Sesión 3: variables globales (DEFINIR/OBTENER/ASIGNAR), `imprimir(...)` como built-in en bytecode, sentencias básicas (asignación, expresión, pasar, bloque).
- ✅ Sesión 4: control de flujo en bytecode (`si`/`mientras` con `romper`/`continuar`/`sino`, lógica con cortocircuito, asignación aumentada).
- ✅ Sesión 5: funciones top-level con recursión + variables locales + llamadas en bytecode.
- ✅ Sesión 6: nativas en VM via OP_LLAMAR + colecciones (lista/tupla/dicc/conjunto) en bytecode + indexación + flag `--bytecode` + tag v0.6.0.
- ⏳ Aplazado a v0.6.1: `SENT_PARA`, closures con upvalues, atributos, slicing, lambda, intentar/atrapar.

### Añadido (Fase 6 sesión 5)
- **`VAL_FUNCION_BC`** y **`struct FuncionBC`** en `chunk.{h,c}`: función compilada a bytecode con su propio `Chunk`, nombre (heap-duplicated), aridad y refcount. Comparte refcount con el resto de tipos colección. Diferente de `VAL_FUNCION` (que es para tree-walking) — coexisten para no romper el evaluador antiguo.
- **CallFrame stack en la VM**: refactor de la VM para soportar llamadas anidadas. Cada `CallFrame` contiene chunk activo, ip y `base_pila`. Stack de hasta 64 frames; pila de Valores ampliada a 1024 slots para acomodar varias llamadas. El frame[0] es el del chunk top-level.
- **`OP_LLAMAR [n_args]`** en la VM: lee el callee del slot `tope - n - 1`, valida que es `VAL_FUNCION_BC`, valida la aridad, crea un nuevo `CallFrame` con `base_pila = tope - n - 1`. Slot 0 del frame contiene el callee, slots 1..n los args, slots posteriores las locales.
- **`OP_RETORNAR` multi-frame**: pop el resultado, libera todos los slots del frame que termina (callee + args + locales) limpiamente, push el resultado en el frame anterior, decrementa `n_frames`. Si era el frame top-level, devuelve el resultado al cliente.
- **`OP_OBTENER_LOCAL [slot]`** y **`OP_ASIGNAR_LOCAL [slot]`**: acceso/escritura a `frame->base_pila[slot]`. Locales viven en el stack del frame; al retornar se liberan junto con el frame.
- **`ScopeCompilador`** en el compilador: representa una función en construcción. Mantiene el chunk de la función, la lista de locales (slot 0 = callee, slots 1..aridad = parámetros, posteriores = locales declaradas dinámicamente), y el stack de bucles abiertos para `romper`/`continuar` dentro de la función.
- **Lookup de identificadores con prioridad local → global**: `EXPR_IDENT` busca primero en `c->actual->locales`; si no encuentra, emite `OP_OBTENER_GLOBAL`.
- **Asignación con dispatch local/global**:
  - En el scope raíz (top-level): siempre globales.
  - Dentro de función: primera asignación a un nombre nuevo lo crea como **local** (sin emitir bytecode adicional — el valor ya quedó en el slot del stack); reasignaciones emiten `OP_ASIGNAR_LOCAL`. Mismo modelo para `+=`, `-=`, etc.
- **`SENT_FUNCION`** compilada con scope anidado:
  - `funcion_bc_nueva(nombre, aridad)` con chunk vacío.
  - Scope hijo con slot 0 = callee, slots 1..n = parámetros como locales.
  - Compila el cuerpo en el chunk hijo.
  - Emite `OP_NULO + OP_RETORNAR` implícitos al final (si el cuerpo no terminaba con `retornar`, esto cubre el caso `funcion f(): pasar fin funcion` → devuelve nulo).
  - Vuelve al scope padre y emite `OP_CONST <fn>` + `OP_DEFINIR_GLOBAL <nombre>`.
  - Limitación documentada: parámetros con valor por defecto NO soportados todavía en bytecode (sí en tree-walking) — error explícito.
- **`SENT_RETORNAR`**: compila el valor opcional + `OP_RETORNAR`. Error si está fuera de función.
- **`EXPR_LLAMADA` general**: callee + args + `OP_LLAMAR [n]`. El caso especial `imprimir(...)` sigue emitiendo `OP_IMPRIMIR` directamente (corto-circuitado solo cuando no hay un local llamado `imprimir` que sombrear).
- **Sin closures todavía**: una función definida dentro de otra NO captura las locales de la enclosing — solo accede a sus propias locales y a globales. Las closures con upvalues están planeadas para una sesión adicional o F6 S6.
- **`tests/unit/test_bytecode_funciones.c`** con 8 grupos: función básica con args, recursión (factorial 10/20, fib 10), variables locales (declaración + reasignación + sombrear global con local), aridad mal con mensaje específico, retornar con/sin valor + dentro de bucle + fuera de función, no invocable, locales aisladas (no filtran a globales), y factorial(50) bignum (64 dígitos) recursivo end-to-end por bytecode.
- **56 tests verde** (23 unit + 33 integración).

Cierre de Fase 5: tree-walking interpreter con todas las colecciones
básicas. **Último release con tree-walking activo** según decisión
[B2](decisiones/B2-tree-walking-vs-bytecode.md): desde v0.6 el motor
de producción será la VM bytecode y el tree-walking se congela como
referencia ejecutable de regresión.

### Añadido (Fase 5 sesión 5)
- **Versión `0.5.0`** en `common.h` y `CMakeLists.txt`. Smoke test ajustado.
- **Tres ejemplos jugables nuevos** que ejercitan las nuevas colecciones end-to-end:
  - [`16_lista_busqueda.cor`](examples/16_lista_busqueda.cor): construcción incremental, slicing, inversa con `[::-1]`, `ordenar()`, función auxiliar de búsqueda lineal.
  - [`17_dicc_frecuencia.cor`](examples/17_dicc_frecuencia.cor): conteo de letras en `"abracadabra"` con `dicc[c] += 1`, iteración `para letra en dicc`, `claves()`/`valores()`.
  - [`18_conj_y_tupla.cor`](examples/18_conj_y_tupla.cor): deduplicación con `conjunto(lista)`, mapa de coordenadas con tuplas como claves de diccionario.
- **Tres tests `run_X`** con `PASS_REGULAR_EXPRESSION` para verificar que cada ejemplo produce la salida esperada.
- **51 tests verde** (18 unit + 33 integración).

### Añadido (Fase 5 sesión 1)
- **`VAL_LISTA`** y **`struct Lista`** en `valor.{h,c}`: array dinámico de `Valor` con refcount manual (sin GC todavía — Fase 7). Operaciones: `lista_nueva`, `lista_retener` (++ref), `lista_liberar` (--ref + free si llega a 0), `lista_agregar` (toma posesión, crece ×2 amortizado), `lista_obtener_ref`, `lista_asignar`. Capacidad inicial 4.
- **Semántica de referencia compartida**: `valor_clonar` para `VAL_LISTA` hace `lista_retener` (no deep copy) → asignar `b = a` comparte el mismo objeto Python-style. Las cadenas dentro de la lista siguen su propia ownership (cadena con `dueno_cadena=true` se duplica al clonar, las referencias al fuente no).
- **Limitación documentada**: el refcount no detecta ciclos. Una lista que se contiene a sí misma filtrará memoria; aceptable hasta Fase 7 (mark-sweep real).
- **`valor_a_cadena` para lista**: produce `[a, b, c]` usando una nueva función `valor_a_repr` que envuelve cadenas en comillas (`[1, "hola"]` en lugar de `[1, hola]`). Recursivo para listas anidadas.
- **`valor_iguales` para lista**: comparación element-wise; mismo objeto (puntero) → `true` por short-circuit; longitudes distintas → `false`.
- **`valor_es_verdadero` para lista**: `cuenta > 0`. Lista vacía es falsa, no vacía es verdadera (Python).
- **`EXPR_LISTA` en evaluador**: evalúa cada elemento de izquierda a derecha; si alguno falla, libera lista parcial y propaga error. Trailing comma del parser ya soportada en sintaxis.
- **`EXPR_INDICE` en evaluador**: `lista[i]` con `i` entero o booleano. Soporta índice negativo (cuenta desde el final). Bounds check con `ErrorDeIndice` específico que reporta el índice y el tamaño. Cadenas, diccionarios y otros tipos quedan para sesiones siguientes.
- **`+` de listas**: `[1, 2] + [3, 4]` → `[1, 2, 3, 4]`. Lista NUEVA con refcount 1, deep-clona elementos (cadenas con dueño se duplican; bignum se copia; listas internas comparten refcount).
- **`*` de listas**: `[1, 2] * 3` y `3 * [1, 2]` → `[1, 2, 1, 2, 1, 2]`. Repetición negativa o por cero produce `[]`. Detecta overflow de tamaño.
- **`en` extendido** para listas: `valor en lista` con búsqueda lineal usando `valor_iguales`. Mantiene también `subcadena en cadena`.
- **`para x en lista`**: itera elementos en orden; cada iteración asigna un clon del elemento al objetivo. Soporta `romper`/`continuar` y cláusula `sino` con la misma semántica que sobre cadenas y rangos.
- **`longitud(lista)`** built-in: devuelve `cuenta` como entero. `tipo(lista)` devuelve `"lista"`.
- **`tests/unit/test_runtime_listas.c`** con 8 grupos: literal (vacío, mixto, anidado, trailing comma), indexación (positivo/negativo/fuera-de-rango, no-entero, anidado), operadores (`+`/`*`/`en`/`no en`/`==` con cross-tipo entero=decimal), iteración (suma, concat, romper, sino), built-ins, referencia compartida (asignar `b = a` no rompe), programa promedio decimal, construcción de cuadrados con `rango()` y concat.
- **42 tests verde** (15 unit + 27 integración).

### Añadido (Fase 5 sesión 2)
- **Mutación `lista[i] = valor`**: extendido `SENT_ASIGNAR` para aceptar `EXPR_INDICE` como destino. Soporta índices negativos. `ErrorDeIndice` específico cuando fuera de rango. La asignación destruye el valor previo y toma posesión del nuevo.
- **Mutación aumentada `lista[i] op= valor`**: `SENT_ASIGNAR_AUG` extendido con misma lógica. Lee, computa con `aplicar_binario`, escribe atómicamente. Funciona para todas las variantes (`+=`, `-=`, `*=`, `/=`, `//=`, `%=`, `**=`).
- **Semántica de referencia confirmada por tests**: `b = a; b[0] = 99` cambia también `a[0]` (Python-like). `agregar(b, 4); longitud(a)` reporta 4.
- **Slicing `lista[a:b:c]`** (`EXPR_REBANADA`) con semántica Python:
  - Cualquier campo opcional (`[:]`, `[a:]`, `[:b]`, `[::c]`, `[a:b:c]`).
  - Defaults dependientes del signo del paso: paso > 0 → `inicio=0`, `fin=cuenta`; paso < 0 → `inicio=cuenta-1`, `fin=-1`.
  - Índices negativos cuentan desde el final.
  - Índices fuera de rango se *clampean* silenciosamente (no error).
  - Paso negativo invierte: `[1,2,3,4,5][::-1] == [5,4,3,2,1]`.
  - Paso 0 produce `ErrorDeValor`.
- **Built-ins de mutación de listas** (estilo función-libre hasta que F8 traiga método-syntax `lista.agregar(x)`):
  - **`agregar(lista, x)`**: añade al final. Devuelve nulo.
  - **`quitar(lista, indice=-1)`**: elimina y devuelve el elemento. Sin índice quita el último. Negativos cuentan desde el final. Lista vacía → `ErrorDeIndice`.
  - **`insertar(lista, indice, valor)`**: inserta antes del índice. Indices fuera de rango se clampean a [0, cuenta] (Python `list.insert`).
  - **`invertir(lista)`**: invierte en sitio (O(n/2) swaps).
  - **`ordenar(lista)`**: ordena in-place con `qsort` libc + comparador propio. Numéricos (entero/decimal/booleano) por valor; cadenas lexicográfico. Tipos mixtos no comparables → error explícito.
- **`tests/unit/test_runtime_listas_mut.c`** con 11 grupos: mutación simple/aug, referencia compartida en mutación, slicing básico (omisiones, negativos, clamping), slicing con paso (positivo/negativo, paso 0 → error), `agregar`, `quitar` (con/sin índice, vacía), `insertar` (con clamping), `invertir`, `ordenar` (numérico/cadenas/mixto/incomparable), y un quicksort recursivo end-to-end como prueba de integración.
- **43 tests verde** (16 unit + 27 integración).

### Añadido (Fase 5 sesión 3)
- **`VAL_DICCIONARIO`** y **`struct Diccionario`** en `valor.{h,c}`: tabla hash con probing lineal, capacidad potencia de 2, factor de carga 0.75, refcount manual (mismo patrón que `Lista`). Funciones: `dicc_nuevo`, `dicc_retener`, `dicc_liberar`, `dicc_asignar` (toma posesión de clave/valor), `dicc_obtener` (devuelve clon), `dicc_contiene`, `dicc_quitar`.
- **Hash genérico de Valores** que cumple la invariante `a == b ⇒ hash(a) == hash(b)`:
  - Booleanos, enteros (que quepan en `int64`) y decimales con valor entero exacto comparten el camino rápido `hash_int64`. Por eso `dicc[1]`, `dicc[1.0]` y `dicc[verdadero]` acceden al mismo slot.
  - Bignums grandes hashean por dígitos + signo. Decimales no enteros por bit pattern del double. Cadenas con FNV-1a 64-bit. Funciones por puntero.
  - `valor_es_hashable` rechaza `lista`, `diccionario`, `rango` como claves.
- **`EXPR_DICCIONARIO` literal** `{clave: valor, ...}`. Diccionario vacío `{}` (resuelto por el parser distinguiéndolo del conjunto).
- **`dicc[clave]` (lectura)**: extiende `EXPR_INDICE`. Clave inexistente produce `ErrorDeClave: <repr>` con la representación del valor que faltaba.
- **`dicc[clave] = valor` (asignación e inserción)**: extiende `SENT_ASIGNAR` con destino `EXPR_INDICE` para diccionarios. Crea la entrada o sobrescribe el valor existente.
- **`dicc[clave] op= valor` (asignación aumentada)**: extiende `SENT_ASIGNAR_AUG` análogamente. La clave debe existir o se reporta `ErrorDeClave`.
- **`clave en dicc` y `clave no en dicc`**: extiende `evaluar_en` con búsqueda hash O(1) amortizado.
- **`para clave en dicc`**: itera las claves del diccionario en orden de slot (no inserción — limitación documentada). Soporta `romper`/`continuar`/cláusula `sino` igual que las otras iteraciones.
- **Igualdad estructural** dos diccionarios son iguales si tienen las mismas claves con valores iguales (orden irrelevante).
- **Built-ins nuevos**:
  - **`claves(dicc)`**: devuelve una lista con las claves.
  - **`valores(dicc)`**: devuelve una lista con los valores.
  - `longitud(dicc)` extendido para devolver `cuenta`.
  - `tipo(dicc)` devuelve `"diccionario"`.
- **Pretty-printer** produce `{"clave": valor, ...}` usando `valor_a_repr` en claves y valores.
- **Limitación documentada**: el orden de iteración (y de `claves`/`valores`) sigue el layout interno del hash table — NO el orden de inserción como Python 3.7+. Aceptable hasta v1.0; se puede cambiar a hash-table ordenada en una versión futura.
- **`tests/unit/test_runtime_diccionarios.c`** con 12 grupos: literal y acceso, asignación/aumentada, membership, iteración, `longitud`/`tipo`, `claves`/`valores`, igualdad estructural (orden distinto), hash unificado entero/decimal/booleano (`dicc[1]` y `dicc[1.0]` mismo slot), tipos no hashables como clave, referencia compartida (`b = a` y mutar `b` afecta `a`), programa de conteo de caracteres en `"abracadabra"` (resultado: `a` aparece 5 veces), diccionario anidado de personas.
- **44 tests verde** (17 unit + 27 integración).

### Añadido (Fase 5 sesión 4)
- **`VAL_CONJUNTO`** y **`struct Conjunto`** en `valor.{h,c}`: hash set construido con la misma estrategia de probing lineal y refcount que `Diccionario`. API: `conj_nuevo`, `conj_retener`, `conj_liberar`, `conj_agregar` (toma posesión, deduplica si ya existe), `conj_contiene`, `conj_quitar`. Sólo elementos hashables (no listas/diccionarios/conjuntos).
- **`VAL_TUPLA`** y **`struct Tupla`** en `valor.{h,c}`: secuencia inmutable con refcount. Sin operaciones de mutación (no hay agregar/asignar). API: `tupla_nueva` (aloca slots no inicializados), `tupla_retener`, `tupla_liberar`. Hashable si todos sus elementos lo son — combinable con `Diccionario` y `Conjunto` como clave.
- **`EXPR_CONJUNTO` literal `{a, b, c}`**: evalúa de izquierda a derecha y deduplica con la igualdad estructural. Vacío explícito requiere `conjunto()` porque `{}` es diccionario vacío.
- **`EXPR_TUPLA` literal**:
  - `()` tupla vacía.
  - `(x,)` tupla de un elemento (coma obligatoria).
  - `(a, b, ...)` tupla múltiple.
  - **Distinción** `(x)` (grupo) vs `(x,)` (tupla 1) ya manejada por el parser desde la sesión 3 sesión 5.
- **Pretty-printer** específico:
  - Conjunto vacío imprime `conjunto()` (no `{}`, que es diccionario).
  - Conjunto no vacío `{a, b, c}`.
  - Tupla `(a, b, c)`, vacía `()`, de uno `(x,)`.
- **`hash_valor` extendido** para tuplas: combina los hashes de cada elemento estilo Python `tuplehash`.
- **`valor_es_hashable` actualizado**: tupla es hashable si todos sus elementos lo son (recursivo); conjuntos NO son hashables.
- **`EXPR_INDICE` para tupla**: `t[i]` con índice positivo/negativo y `ErrorDeIndice` específico.
- **Operador `en`** extendido para conjunto (búsqueda hash O(1)) y tupla (búsqueda lineal).
- **Iteración `para x en conjunto`** y **`para x en tupla`** con la misma semántica que el resto: clon por iteración, soporte de `romper`/`continuar`/cláusula `sino`. El conjunto itera en orden de slot interno (no de inserción — limitación documentada).
- **`longitud(conjunto)`** y **`longitud(tupla)`** funcionan; `tipo()` devuelve `"conjunto"` o `"tupla"`.
- **`conjunto()` built-in** con dos formas:
  - `conjunto()` → conjunto vacío.
  - `conjunto(iterable)` con iterable lista o tupla → conjunto con sus elementos deduplicados.
- **`agregar(conjunto, x)` extendido**: ahora acepta listas Y conjuntos como primer argumento; sobre conjunto deduplica al añadir.
- **Igualdad estructural**: dos conjuntos iguales si tienen los mismos elementos (orden irrelevante); dos tuplas iguales si tienen los mismos elementos en el mismo orden.
- **`tests/unit/test_runtime_conj_tup.c`** con 12 grupos: literal y deduplicación, membership, iteración, igualdad (con hash unificado entero/decimal/booleano), tipos no hashables; tupla literal (vacía/uno/varios) con distinción grupo, indexación, iteración, igualdad cross-tipo (tupla != lista), membership, tupla como clave de dict (con error si contiene lista), `conjunto()` constructor, programa de palabras únicas.
- **45 tests verde** (18 unit + 27 integración).

## [0.4.0] — 2026-04-28 — primer release jugable

Cierre de Fase 4 según el plan: tree-walking interpreter completo y
jugable end-to-end. Decisión [B2](decisiones/B2-tree-walking-vs-bytecode.md):
este release sirve como referencia ejecutable y se congelará en v0.5
tras añadir colecciones; desde v0.6 el motor de producción será la VM
bytecode.

### Añadido (Fase 4 sesión 5)
- **`cornamusa <archivo.cor>`** ahora ejecuta el programa con el evaluador tree-walking en lugar de solo lexarlo. Errores de runtime se reportan con caret indicators reusando `error_imprimir` (formato MENSAJES.md §2). Exit codes: 0 OK, 64 uso, 65 error de parseo, 70 error de runtime, 74 error de E/S.
- **`cornamusa --tokens <archivo>`** (anteriormente el modo por defecto) sigue disponible para inspección del lexer.
- **`cornamusa --ast <archivo>`** sin cambios — vuelca el AST en S-expression.
- **REPL interactivo funcional**: `cornamusa` sin argumentos abre un prompt `>>> ` y `... ` para continuación. Variables y funciones definidas persisten entre líneas. Heurística de continuación: línea acabada en `:` abre bloque, `fin` lo cierra; al volver a profundidad 0 se ejecuta el buffer acumulado. Una línea vacía con buffer ejecuta y reinicia. `salir` o EOF terminan.
- **Persistencia REPL**: arena compartida durante toda la sesión + `strdup` de cada bloque ejecutado para que las claves del entorno y los nodos AST de funciones definidas previamente sigan vivos al ejecutar líneas posteriores. Las cadenas duplicadas se filtran deliberadamente — viven hasta el fin del proceso.
- **Tres ejemplos jugables nuevos**:
  - [`13_factorial_jugable.cor`](examples/13_factorial_jugable.cor): factorial recursivo con bignum (`100!` = 158 dígitos exactos).
  - [`14_contar_vocales.cor`](examples/14_contar_vocales.cor): `para letra en cadena` UTF-8 + acumulador + `o` encadenado + `longitud()`.
  - [`15_fizzbuzz_jugable.cor`](examples/15_fizzbuzz_jugable.cor): FizzBuzz clásico con `rango()`, `si`/`sino si`/`sino`, `%`.
- **Tests de ejecución end-to-end**: nuevos `run_X` con `PASS_REGULAR_EXPRESSION` para verificar que cada ejemplo jugable produce la salida esperada. Cubren los 4 casos representativos (hola_mundo, factorial, contar_vocales, fizzbuzz).
- **Tests `lex_X` ahora usan `--tokens`**: la verificación lexicográfica de los 12 ejemplos sigue intacta como paso de regresión, pero independiente del runtime — un ejemplo puede usar features futuras (closures, listas) y aún así pasar `lex_X`.
- **Versionado**: `CORNAMUSA_VERSION` actualizado a `"0.4.0"` en `common.h` y `CMakeLists.txt`. Smoke test ajustado.
- **41 tests verde** (14 unit + 27 integración: 12 lex + 8 parse + 4 run + 3 examples nuevos).

### Añadido (Fase 4 sesión 4)
- **Tipo `VAL_RANGO`**: nuevo variante en `Valor` con tres `mp_int *` (inicio, fin, paso). Iterable con bignum, ascendente o descendente. `valor_clonar` hace deep copy; `valor_destruir` libera los tres mp_int. `valor_es_verdadero` devuelve `true` si la iteración produciría al menos un elemento. `rango(a, b, paso)` se imprime como `"rango(a, b, paso)"`.
- **Refactor de `VAL_FUNCION` y `VAL_NATIVA`** a estructuras inline (sin allocations heap):
  - `VAL_FUNCION` referencia un `const Sent *def` del AST + un `Entorno *entorno_definicion`. Sin closures (decisión B2): el entorno_definicion siempre es global en S4 — campo reservado para closures futuros en Fase 6+.
  - `VAL_NATIVA` contiene nombre + puntero `FnNativa` (typedef en `valor.h`).
  - Ambos son trivialmente clonables (struct copy), no requieren ownership tracking, y `valor_iguales` compara por referencia subyacente.
- **`src/nativos.{h,c}`** con la API `nativos_registrar(globales)` que añade los built-ins al entorno:
  - **`imprimir(*args)`**: variádica. Imprime cada argumento separado por espacio + `\n`. Sin kwargs (`separador`, `final`) — se añadirán cuando lleguen kwargs al lenguaje. Devuelve nulo.
  - **`longitud(x)`**: cadena → número de **code points UTF-8** (no bytes); rango → número de elementos producidos. Otros tipos producen `ErrorDeTipo`.
  - **`tipo(x)`**: devuelve cadena con el nombre del tipo en castellano (`"entero"`, `"decimal"`, `"cadena"`, `"booleano"`, `"nulo"`, `"funcion"`, `"rango"`).
  - **`rango([inicio,] fin [, paso])`**: tres formas. Acepta solo enteros (booleano se promueve a 1/0). `paso == 0` produce `ErrorDeValor`. Bignum-friendly: `rango(0, 10**100, 1)` es válido (aunque su iteración tarde una eternidad).
- **`SENT_FUNCION` en evaluador**: crea un `VAL_FUNCION` y lo asigna en el entorno actual. La función puede llamarse a sí misma porque el nombre está definido antes de cualquier llamada.
- **`SENT_RETORNAR`**: evalúa la expresión opcional (`retornar` desnudo → nulo), guarda el valor en `ev->valor_retorno` y marca `ev->control = EJEC_RETORNAR`. El bucle envolvente o `llamar_usuario` la consume.
- **`EXPR_LLAMADA` en evaluador**: evalúa el callee, evalúa cada argumento, despacha:
  - `VAL_NATIVA`: invoca el puntero a función C con los args ya evaluados (ownership del cliente).
  - `VAL_FUNCION`: crea un nuevo `Entorno` hijo del entorno_definicion, liga parámetros (con valores por defecto si faltan), ejecuta el cuerpo, recoge `ev->valor_retorno` si apareció `EJEC_RETORNAR`, restaura entorno y control. Aridad validada con mensaje específico ("`f()` esperaba N argumentos, recibió M").
- **Recursión funcional**: factorial(50)=64 dígitos y factorial(100)=158 dígitos pasan tests recursivos sin stack-smashing (depth ≈ 100 frames).
- **`para` ahora itera también `VAL_RANGO`**: usa `mp_add` para avanzar y `mp_cmp` para terminar. Soporta paso ascendente y descendente. Combinable con `romper`/`continuar`/cláusula `sino` igual que con cadenas.
- **`Evaluador` ahora es `typedef struct Evaluador { ... }`** (con nombre explícito) para permitir forward declaration desde `valor.h` en la firma de `FnNativa`.
- **`tests/unit/test_runtime_funciones.c`** con 11 grupos de tests: definición y llamada simple, recursión (factorial 10/50, fib 15), parámetros con defaults, aridad mal con mensaje específico, no invocable, `retornar` con/sin valor y dentro de bucle, `tipo()` para cada tipo, `longitud()` UTF-8 + rangos, `rango()` 1/2/3 args con paso negativo y cero iteraciones, `imprimir()` no rompe, programa de pares con función auxiliar, factorial(100) recursivo (158 dígitos).
- **34 tests verde** (14 unit + 20 integración).

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

