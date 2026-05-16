#ifndef CORNAMUSA_CHUNK_H
#define CORNAMUSA_CHUNK_H

#include <stdbool.h>
#include <stdint.h>

#include "valor.h"

/*
 * Versión del formato de chunk de bytecode (decisión I7).
 *
 * Este número se bumpea cada vez que cambia el layout binario de un
 * chunk de un modo que rompa la compatibilidad: cambios en el orden
 * o significado de OpCodes, en el tamaño de operandos, en la
 * semántica de instrucciones, en la introducción de slots de cache
 * inline (F10), etc.
 *
 * Hoy los chunks NO se serializan a disco — se compilan en memoria
 * cada vez que se ejecuta un .cor. La constante está aquí como
 * marcador para herramientas futuras (cache .cornc para arranque
 * rápido, inspector externo de bytecode, etc.) y para que la decisión
 * de bumpear sea consciente cuando F10 empiece a alterar el layout.
 *
 * Historial:
 *   1 — v0.6.0 hasta v0.9.2 inclusive. Layout original con opcodes
 *       sin slots de cache.
 *   2 — reservado para F10 cuando se introduzcan opcodes
 *       especializados con cache slots inline (PEP 659 style).
 */
#define CORNAMUSA_BYTECODE_VERSION 1

/*
 * Bytecode chunk de Cornamusa (Fase 6).
 *
 * Un `Chunk` es la unidad básica de bytecode emitida por el
 * compilador y consumida por la VM. Contiene tres arrays paralelos:
 *
 *   - `codigo`: secuencia de bytes con instrucciones (opcodes y sus
 *     operandos inline).
 *   - `constantes`: pool de Valores referenciados por OP_CONST y
 *     similares.
 *   - `lineas`: número de línea fuente que originó cada byte de
 *     `codigo` (sin compresión todavía — un `int` por byte). Suficiente
 *     para mensajes de error con ubicación.
 *
 * Para empezar (sesión 1) el conjunto de opcodes es minimal — el
 * resto se irá añadiendo en las sesiones siguientes a medida que
 * compilador y VM los necesiten.
 *
 * Diseño tomado de clox cap. 14 ("Chunks of Bytecode") con renombres
 * al castellano:
 *   - Chunk = Chunk
 *   - codigo = code
 *   - constantes = constants
 *   - lineas = lines
 *
 * Cada chunk es DUEÑO de los Valores en `constantes`: al destruir el
 * chunk se llama `valor_destruir` sobre cada uno.
 */

typedef enum {
    /* ---- Carga de constantes y literales ---- */
    OP_CONST,           /* CONST [byte index]: empuja constantes[index] */
    OP_CONST_LARGO,     /* CONST_LARGO [3-byte index]: para >256 constantes */
    OP_NULO,            /* empuja nulo */
    OP_VERDADERO,       /* empuja verdadero */
    OP_FALSO,           /* empuja falso */

    /*
     * ---- Aritmética y comparaciones binarias ----
     *
     * Layout 1 byte (sin operandos). Las variantes especializadas
     * (sufijo _INT_INT) son fast paths del IC F10 que verifican que
     * ambos operandos son VAL_ENTERO y llaman libtommath directamente,
     * saltándose el switch de tipos del slow path. Miss → degradan al
     * opcode base (sin sufijo) y reejecutan.
     */
    OP_SUMAR,
    OP_SUMAR_INT_INT,
    OP_RESTAR,
    OP_RESTAR_INT_INT,
    OP_MULTIPLICAR,
    OP_MULTIPLICAR_INT_INT,
    OP_DIVIDIR,         /* true division → decimal */
    OP_DIVIDIR_ENTERO,  /* floor division (//) */
    OP_MODULO,
    OP_POTENCIA,        /* ** */
    OP_NEGAR,           /* unario -x */

    /* ---- Comparación / lógica ---- */
    OP_NO,              /* unario `no x` */
    OP_IGUAL,
    OP_DISTINTO,
    /* MENOR/MAYOR especializados a INT_INT — útil en `n < 2`,
       `i < limite`, etc. Comparan mp_int directamente. */
    OP_MENOR,
    OP_MENOR_INT_INT,
    OP_MENOR_IGUAL,
    OP_MENOR_IGUAL_INT_INT,
    OP_MAYOR,
    OP_MAYOR_INT_INT,
    OP_MAYOR_IGUAL,
    OP_MAYOR_IGUAL_INT_INT,
    OP_ES,           /* identidad */
    OP_EN,           /* membership */

    /* ---- Stack management ---- */
    OP_DESCARTAR,       /* pop sin usar */
    OP_DUP_2,           /* duplica los 2 valores del tope (a, b -> a, b, a, b) */

    /* ---- Control de flujo (sesión 4) ---- */
    /* Los siguientes opcodes se reservan ahora para que el orden
       quede estable; la VM los implementa cuando llegue el momento. */
    OP_SALTAR,                  /* JUMP [u16 offset] */
    OP_SALTAR_SI_FALSO,         /* JUMP_IF_FALSE [u16 offset] */
    OP_BUCLE,                   /* LOOP [u16 offset] (salto hacia atrás) */

    /* ---- Funciones y locales (sesión 5) ---- */
    OP_OBTENER_LOCAL,           /* GET_LOCAL [byte slot] */
    OP_ASIGNAR_LOCAL,           /* SET_LOCAL [byte slot] */
    /*
     * GET_GLOBAL: layout 6 bytes desde v0.10 (F10).
     *   [opcode] [name_idx u8] [cache_ver u16 BE] [cache_slot u16 BE]
     * Slow path: busca por nombre, rellena cache, reescribe el primer
     * byte a OP_OBTENER_GLOBAL_CACHE y empuja el valor.
     */
    OP_OBTENER_GLOBAL,
    /*
     * Forma quickened de OP_OBTENER_GLOBAL. Mismo layout 6 bytes.
     * Fast path: si los 16 bits bajos de vm->globales->version coinciden
     * con cache_ver, lee directamente vm->globales->entradas[cache_slot].
     * Miss → revierte el primer byte a OP_OBTENER_GLOBAL y rebobina ip.
     */
    OP_OBTENER_GLOBAL_CACHE,
    OP_DEFINIR_GLOBAL,
    OP_ASIGNAR_GLOBAL,
    /*
     * CALL [byte n_args]. Mismo layout 2 bytes en TODAS las variantes
     * (slow + 4 especializadas). El opcode codifica el tipo esperado
     * del callee — no hay cache slot, el byte ES el cache:
     *   OP_LLAMAR             — slow path: switch sobre callee.tipo,
     *                            promueve a la variante correspondiente
     *                            tras el primer éxito.
     *   OP_LLAMAR_NATIVA      — fast: si callee es VAL_NATIVA llama
     *                            directo. Miss → degrada a OP_LLAMAR.
     *   OP_LLAMAR_BC          — fast para VAL_FUNCION_BC (closure).
     *   OP_LLAMAR_CLASE       — fast para VAL_CLASE (instanciación).
     *   OP_LLAMAR_METODO_LIGADO — fast para VAL_METODO_LIGADO.
     */
    OP_LLAMAR,
    OP_LLAMAR_NATIVA,
    OP_LLAMAR_BC,
    OP_LLAMAR_CLASE,
    OP_LLAMAR_METODO_LIGADO,

    /* v1.22: llamadas con desempaquetado de iterables (`*lista` como arg).
       OP_LISTA_AGREGAR  — TOS=valor; debajo lista. Pop valor, append.
       OP_LISTA_EXTENDER — TOS=iterable; debajo lista. Pop, append cada elem.
       OP_LLAMAR_SPREAD  — TOS=lista args; bajo callee. Llama con args
                            expandidos como n_args = longitud(lista). */
    OP_LISTA_AGREGAR,
    OP_LISTA_EXTENDER,
    OP_LLAMAR_SPREAD,
    /* v1.30: para set comprehension. TOS=valor; debajo=conjunto. Pop valor,
       agregar al conjunto. Valor debe ser hashable. */
    OP_CONJUNTO_AGREGAR,

    /* v1.31: `producir EXPR` en generador. TOS=valor producido. La VM
       suspende el frame del generador (guarda IP y stack) y retorna al
       caller con el valor producido. Próximo `iter_siguiente` resume. */
    OP_PRODUCIR,

    /* v1.23: llamadas con keyword arguments `f(x=1, y=2)`.
       Layout en stack al ejecutar:
         [..., callee, pos0..posN-1, key0, val0, key1, val1, ...]
       donde keyI son cadenas (VAL_CADENA) y valI son los valores.
       Operandos: [n_pos] [n_kw]. La VM hace matching key→param y
       rellena defaults para los no-cubiertos. */
    OP_LLAMAR_KW,

    /* v1.25: llamadas con `**dict` spread o mezclas runtime.
       OP_DICC_AGREGAR_PAR — TOS=valor, debajo=clave, debajo=dict.
                              Asigna dict[clave]=valor (sin pop de dict).
       OP_DICC_EXTENDER    — TOS=dict_otro, debajo=dict. Merge claves
                              de otro en éste; pop ambos? — pop solo
                              dict_otro, deja dict en TOS.
       OP_LLAMAR_KW_DICT   — TOS=dict_kw, debajo n_pos posicionales,
                              debajo callee. Operando: [n_pos]. */
    OP_DICC_AGREGAR_PAR,
    OP_DICC_EXTENDER,
    OP_LLAMAR_KW_DICT,

    /* ---- Closures (v0.6.2) ---- */
    OP_CLOSURE,                 /* [byte fn_idx] [n_upvalues * (is_local, index)] */
    OP_OBTENER_UPVALUE,         /* [byte slot] */
    OP_ASIGNAR_UPVALUE,         /* [byte slot] */
    OP_CERRAR_UPVALUE,          /* cierra el upvalue del slot top y descarta */

    /* ---- Excepciones (v0.6.3) ---- */
    OP_INTENTAR_INICIAR,        /* [u16 offset_handler] empuja un handler frame */
    OP_INTENTAR_FIN,            /* pop el handler frame al salir limpio del intentar */
    OP_LANZAR,                  /* pop la excepción del tope, salta al handler */
    /* v0.8.3: chequear el tipo (nombre de clase) de la excepción top
       contra una cadena del pool. Empuja bool sin descartar la
       excepción. Útil para `atrapar Tipo:`. */
    OP_COMPROBAR_TIPO_EXC,      /* [byte name_idx] */

    /* ---- Clases / atributos (v0.7.0 Fase 8 sesión 1) ---- */
    OP_CLASE,                   /* [byte name_idx]: crea Clase y empuja VAL_CLASE */
    /*
     * OBTENER_ATRIBUTO: layout 6 bytes desde v0.10 (F10).
     *   [opcode] [name_idx u8] [clase_hash u16 BE] [slot_idx u16 BE]
     * Slow path maneja MODULO/INSTANCIA-attr/INSTANCIA-method y, si
     * es un atributo de instancia, rellena el cache y promueve a
     * OP_OBTENER_ATRIBUTO_INSTANCIA.
     */
    OP_OBTENER_ATRIBUTO,
    /*
     * Forma quickened: si obj es VAL_INSTANCIA, los 16 bits bajos del
     * puntero a su clase coinciden con cached_clase_hash, y
     * atributos[slot_idx] está ocupado y su clave coincide con el
     * nombre esperado, lee el valor directo. Miss → degrada a
     * OP_OBTENER_ATRIBUTO y rebobina ip.
     */
    OP_OBTENER_ATRIBUTO_INSTANCIA,
    OP_ASIGNAR_ATRIBUTO,        /* [byte name_idx]: pop valor, pop obj, set obj.attr=valor, push nulo */

    /* ---- Métodos (v0.7.0 Fase 8 sesión 2) ---- */
    OP_METODO,                  /* [byte name_idx]: pop closure, set clase.metodos[name] = closure (clase queda en stack) */

    /* ---- Herencia (v0.7.0 Fase 8 sesión 4) ---- */
    OP_HEREDAR,                 /* pop super (sin operando): copia super.metodos → clase.metodos y enlaza superclase. Stack: [..., clase, super] → [..., clase]. */

    /* ---- super (v0.7.1) ---- */
    OP_SUPER_INVOCAR,           /* [byte name_idx] [byte n_args]: stack [..., yo, arg1, ..., argN]. Despacha al método name de yo.clase.superclase. */

    /* ---- Módulos (v0.9.0 Fase 9) ---- */
    OP_IMPORTAR,                /* [byte module_idx] [byte binding_idx]: carga modulo, push frame; al retornar registra global con binding_idx. */
    OP_IMPORTAR_PARA_DESDE,     /* [byte module_idx]: como OP_IMPORTAR pero al retornar deja el modulo en el tope del stack (sin binding global). Usado por `desde X importar Y, Z` (v0.9.1). */
    OP_DUP,                     /* duplica el valor en el tope del stack (v0.9.1). */

    /* ---- Built-in print (atajo del compilador) ---- */
    OP_IMPRIMIR,

    /* ---- Construcción de colecciones (Fase 6 sesión 6) ---- */
    OP_BUILD_LISTA,    /* [n_elementos] → pop n, push lista */
    OP_BUILD_TUPLA,    /* [n_elementos] → pop n, push tupla */
    OP_BUILD_DICC,     /* [n_pares]     → pop n*2 (k,v,k,v...), push dicc */
    OP_BUILD_CONJUNTO, /* [n_elementos] → pop n, push conjunto */

    /* ---- Indexación (lectura y escritura) ---- */
    OP_INDICE,         /* pop key, pop obj, push obj[key] */
    OP_ASIGNAR_INDICE, /* pop value, pop key, pop obj — sets obj[key] = value */
    OP_REBANADA,       /* pop paso, fin, inicio, obj — push obj[i:f:p].
                          Cualquier campo nulo significa "default". */

    /* ---- Iteración ---- */
    OP_ITER_INICIAR,   /* pop iterable, push iterador (estado interno) */
    OP_ITER_SIGUIENTE, /* operando u16 = offset_fin; lee tope (iterador),
                          si hay siguiente push valor; si no, pop y salta */

    /* ---- F-strings (v1.1) ---- */
    /*
     * OP_FORMATO_F: pop el TOS, lo convierte a VAL_CADENA con la
     * representación canónica de `imprimir`/`cadena()` y empuja la
     * cadena resultante.
     *
     * v1.2: si TOS es VAL_INSTANCIA con `__cadena__` definido, la VM
     * invoca el dunder (despachando un frame). El resultado puede ser
     * de cualquier tipo — el opcode siguiente, OP_ASEGURAR_CADENA,
     * verifica que sea cadena. Para no-instancias OP_FORMATO_F siempre
     * deja una cadena directamente.
     */
    OP_FORMATO_F,

    /*
     * OP_ASEGURAR_CADENA (v1.2): verifica que el TOS sea VAL_CADENA;
     * si no, emite ErrorDeTipo claro mencionando que `__cadena__` debe
     * retornar cadena. Sin operandos. Usado tras OP_FORMATO_F en
     * f-cadenas para validar el retorno de `__cadena__`. Reusado por
     * OP_REPR para validar el retorno de `__repr__`.
     */
    OP_ASEGURAR_CADENA,

    /*
     * OP_REPR (v1.41): pop TOS y empuja su representación "inspeccionable"
     * como VAL_CADENA — cadenas entre comillas, listas como `[a, b]`,
     * etc. — la salida de la función global `repr()`.
     *
     * Si TOS es VAL_INSTANCIA y su clase define `__repr__`, la VM
     * invoca el dunder (despachando un frame). El compilador SIEMPRE
     * emite `OP_ASEGURAR_CADENA` justo después, que valida que el
     * resultado sea cadena y emite ErrorDeTipo si no. Sin instancia o
     * sin dunder, OP_REPR delega en `valor_a_repr` y deja una cadena
     * directamente.
     *
     * Atajo del compilador para `repr(arg)` con un solo arg, análogo
     * al atajo de `cadena(arg)` que emite OP_FORMATO_F.
     */
    OP_REPR,

    /*
     * OP_ASIGNAR_REBANADA (v1.44): `xs[i:j:k] = iterable` sobre listas.
     * Stack: [..., lista, inicio, fin, paso, iterable]
     *   - paso=1 admite que iterable tenga longitud != (fin-inicio):
     *     la lista crece o encoge.
     *   - paso!=1: iterable debe tener exactamente la misma longitud
     *     que la rebanada — sustitución 1-a-1.
     * Tras ejecución pop los 5 operandos y empuja `nulo` (resultado
     * de la asignación, que el llamador descarta).
     */
    OP_ASIGNAR_REBANADA,

    /*
     * OP_FORMATO_F_SPEC (v1.45): coerce TOS a cadena aplicando el
     * format spec almacenado en constantes[u8].
     *
     * Operando: índice (u8) de la cadena spec en chunk->constantes.
     *
     * Sintaxis del spec (subset Python):
     *   [relleno][alineación][ancho][.precisión][tipo]
     *
     *   - relleno      cualquier carácter (default ' '); requiere alineación
     *   - alineación   `<` izq | `>` der | `^` centro
     *   - ancho        dígitos (>= 0); rellena hasta ese ancho
     *   - .precisión   dígitos (.N); para tipos `f`/`e` controla decimales;
     *                   para `s` trunca la cadena a N caracteres
     *   - tipo:        `d` entero, `f` decimal, `e` notación científica,
     *                  `x`/`X` hex sin/con mayúsculas, `b` binario, `s` cadena
     *
     * Si el spec es vacío, equivale a OP_FORMATO_F (sin dunder dispatch —
     * stringificación canónica). Validación de tipos en runtime: ErrorDeValor
     * si el spec exige un tipo numérico y TOS no es numérico.
     */
    OP_FORMATO_F_SPEC,

    /*
     * OP_LLAMAR_SPREAD_KW_DICT (v1.46): llamada que combina `*args` y
     * keyword args / `**dict` en la misma invocación.
     *
     * Stack: [..., callee, args_list, kwargs_dict]
     *   - args_list      lista construida con OP_BUILD_LISTA + OP_LISTA_*
     *                     (mismo formato que OP_LLAMAR_SPREAD).
     *   - kwargs_dict    dict construido con OP_BUILD_DICC + OP_DICC_*
     *                     (mismo formato que OP_LLAMAR_KW_DICT).
     *
     * Sin operandos. Expande la lista a posicionales en el stack,
     * extrae los pares (clave, valor) del dict, y despacha a
     * `ejecutar_llamar_kw` con `n_pos` calculado en runtime.
     */
    OP_LLAMAR_SPREAD_KW_DICT,

    /*
     * OP_LONGITUD (v1.3): pop TOS, push longitud (entero).
     *   - VAL_INSTANCIA con `__longitud__`: dispatch al dunder.
     *   - Cadena/lista/dicc/conjunto/tupla/rango: cuenta elementos
     *     (semántica de la nativa `longitud`).
     *   - Otros tipos: ErrorDeTipo.
     * Atajo del compilador para `longitud(arg)` con un solo arg.
     */
    OP_LONGITUD,

    /* ---- Pattern matching (v1.16) ---- */
    /*
     * Test de tipo de TOS. NO consumen el valor (peek).
     * Empujan booleano al stack: verdadero si TOS es del tipo, falso
     * si no. Usados por el compilador de `coincidir` para verificar
     * patrones estructurales antes de extraer elementos.
     */
    OP_ES_TUPLA,
    OP_ES_LISTA,

    /* ---- Retorno ---- */
    OP_RETORNAR,
} OpCode;

/*
 * Devuelve el nombre del opcode como cadena estática (para debug).
 * NULL si `op` no es válido.
 */
const char *opcode_nombre(OpCode op);

typedef struct {
    uint8_t *codigo;
    int *lineas;        /* paralelo a `codigo` — un int por byte */
    int cuenta;
    int capacidad;

    Valor *constantes;
    int constantes_cuenta;
    int constantes_capacidad;
} Chunk;

void chunk_iniciar(Chunk *c);
void chunk_destruir(Chunk *c);

/*
 * Añade un byte (instrucción u operando) al final del chunk con la
 * línea fuente asociada para debug y mensajes de error. Crece
 * exponencialmente.
 */
void chunk_emitir_byte(Chunk *c, uint8_t b, int linea);

/*
 * Variante que emite dos bytes consecutivos compartiendo la misma
 * línea — ideal para opcodes con un único operando byte.
 */
void chunk_emitir_byte2(Chunk *c, uint8_t a, uint8_t b, int linea);

/*
 * Añade un Valor al pool de constantes y devuelve su índice. El chunk
 * toma posesión del Valor — al destruirlo se libera. Devuelve -1 si OOM.
 */
int chunk_agregar_constante(Chunk *c, Valor v);

/*
 * Atajo: emite la instrucción que carga la constante `v` (OP_CONST con
 * índice de 1 byte si cabe, OP_CONST_LARGO con 3 bytes si supera 255).
 * Toma posesión de `v`.
 */
void chunk_emitir_constante(Chunk *c, Valor v, int linea);

/*
 * Función compilada a bytecode (plantilla — no incluye upvalues
 * cerrados, eso lo hace Closure).
 *
 * Una `FuncionBC` representa el código de una función: su Chunk
 * propio, aridad, nombre y la metadata de upvalues (`info_upvalues`
 * + `n_upvalues`) que el compilador llenó al ver capturas de scope
 * enclosing. Esta metadata la usa OP_CLOSURE para construir las
 * Closure instances en runtime.
 *
 * Las funciones se comparten via refcount entre Closures (cada
 * Closure referencia una FuncionBC).
 */

/*
 * Metadata de un upvalue desde el punto de vista del compilador:
 *   - `es_local`: true si el upvalue captura una variable LOCAL del
 *     scope padre directo. false si captura un upvalue del padre
 *     (es decir, una variable más arriba en la cadena).
 *   - `indice`: si es_local, el slot de la local en el padre.
 *     Si no, el índice del upvalue en la tabla del padre.
 */
typedef struct {
    bool es_local;
    uint8_t indice;
} InfoUpvalue;

#define FN_BC_UPVALUES_MAX 256

/*
 * v1.5: descriptor de "dunder inlinable". Si el cuerpo del método es
 * un único `retornar` con un patrón trivial reconocido, el compilador
 * lo detecta y rellena este descriptor. La VM, al despachar el
 * dunder, salta la creación de CallFrame y ejecuta el patrón
 * directamente — speedup ~1.5-2x en bucles hot.
 *
 * Patrones soportados (v1.5):
 *   DUNDER_INLINE_BIN_ATTR_OP_ATTR  →  `retornar yo.A OP otro.B`
 *     Para __sumar__/__restar__/etc. con cuerpo aritmético simple
 *     entre atributos de yo y otro.
 *   DUNDER_INLINE_UNARIO_ATTR       →  `retornar yo.A`
 *     Para dunders unarios (poco común, pero útil para wrappers).
 *
 * Las cadenas `attr_yo` / `attr_otro` se duplican en heap y se
 * liberan junto con la `FuncionBC` (refcount llega a 0).
 */
typedef enum {
    DUNDER_INLINE_NONE = 0,
    DUNDER_INLINE_BIN_ATTR_OP_ATTR,
    /*
     * v1.6: `retornar yo.A`. Aridad 1. Útil para `__cadena__`
     * que envuelve un atributo cadena o `__longitud__` que delega.
     */
    DUNDER_INLINE_UNARIO_ATTR,
    /*
     * v1.7: `__iniciar__` trivial con exactamente 2 atributos:
     *   funcion __iniciar__(yo, p1, p2):
     *     yo.A = p1
     *     yo.B = p2
     *   fin funcion
     * Permite que el constructor inline sin frame.
     */
    INIT_INLINE_TRIVIAL_2,
    /*
     * v1.7: `__sumar__/__restar__/...` con constructor de 2 args:
     *   funcion __sumar__(yo, otro):
     *     retornar V(yo.A OP otro.B, yo.C OP2 otro.D)
     *   fin funcion
     * Combinado con INIT_INLINE_TRIVIAL_2 en la clase referenciada,
     * permite ejecutar `v + w` sin crear ningún CallFrame.
     */
    DUNDER_INLINE_BIN_CTOR_2,
} TipoDunderInline;

typedef struct {
    TipoDunderInline tipo;
    /* Compartido por BIN_ATTR_OP_ATTR y UNARIO_ATTR (y arg1 del CTOR_2): */
    char *attr_yo;
    int len_attr_yo;
    char *attr_otro;
    int len_attr_otro;
    int op_token;            /* TipoToken: TT_MAS, TT_MENOS, TT_MENOR, etc. */

    /* INIT_INLINE_TRIVIAL_2: los 2 atributos asignados en orden. */
    char *init_attr1;
    int init_attr1_len;
    char *init_attr2;
    int init_attr2_len;

    /* DUNDER_INLINE_BIN_CTOR_2:
     *   - `nombre_clase`: nombre del constructor (resuelto en runtime
     *     contra globals).
     *   - arg1: usa los campos compartidos `attr_yo`/`attr_otro`/`op_token`.
     *   - arg2: campos dedicados ctor_arg2_*.
     */
    char *nombre_clase;
    int len_nombre_clase;
    char *ctor_arg2_attr_yo;
    int ctor_arg2_len_yo;
    char *ctor_arg2_attr_otro;
    int ctor_arg2_len_otro;
    int ctor_arg2_op;
} DunderInlineDesc;

struct FuncionBC {
    GCObject obj;            /* Fase 7 S2: header GC; primer campo. */
    char *nombre;            /* duplicado en heap; se libera con la función */
    int longitud_nombre;
    int aridad;
    Chunk chunk;
    int refcount;
    /* Metadata de upvalues para OP_CLOSURE. n_upvalues = 0 para
       funciones que no capturan nada. */
    InfoUpvalue info_upvalues[FN_BC_UPVALUES_MAX];
    int n_upvalues;
    /* v1.5: descriptor de inline dunder. tipo=DUNDER_INLINE_NONE si
       no aplica (caso default). */
    DunderInlineDesc inline_desc;
    /* v1.17: cantidad de parámetros con valor por defecto. Los
       defaults SIEMPRE están en cola (parser lo valida). El compilador
       emite las expresiones de default antes de OP_CLOSURE; al ejecutar
       OP_CLOSURE, la VM pop estos N valores y los guarda en
       `closure->defaults`. */
    int n_defaults;
    /* v1.22: si tiene_estrella, el ÚLTIMO parámetro (slot `aridad-1`)
       recoge los args sobrantes en una tupla. `aridad` cuenta los
       fixed más el `*resto`. n_args ≥ aridad-1 es válido. */
    bool tiene_estrella;
    /* v1.24: si tiene_doble_estrella, el ÚLTIMO parámetro recoge los
       keyword args no-matched en un diccionario. Si también tiene
       estrella, `*resto` está en `aridad-2` y `**kw` en `aridad-1`. */
    bool tiene_doble_estrella;
    /* v1.31: si el cuerpo contiene `producir`, la función NO ejecuta
       al llamarse — crea un VAL_GENERADOR con frame congelado. */
    bool es_generador;
    /* v1.23: nombres de parámetros para matching de keyword args.
       Arrays paralelos de longitud `aridad`. Duplicados en heap;
       freed en funcion_bc_liberar. NULL si no se setearon (función
       compilada pre-v1.23 — no soporta kwargs). */
    char **nombres_params;
    int *long_nombres_params;
};

/*
 * Crea una FuncionBC con chunk vacío y refcount=1. El cliente
 * compila el cuerpo en `chunk` y luego envuelve la función en un
 * Closure con `closure_nuevo` (que añade los upvalues runtime).
 * El nombre se duplica.
 */
FuncionBC *funcion_bc_nueva(const char *nombre, int len_nombre, int aridad);
void funcion_bc_retener(FuncionBC *f);
void funcion_bc_liberar(FuncionBC *f);

/*
 * Upvalue: una referencia compartida entre la closure y el slot de
 * stack original (mientras la función enclosing está activa). Cuando
 * la enclosing retorna, el upvalue se "cierra": el valor se copia a
 * `cerrado` y `posicion` apunta a ese campo, no al stack.
 *
 * `siguiente`: linked list de upvalues abiertos en la VM, ordenada
 * por posición decreciente en stack (estilo clox cap. 25).
 */
struct Upvalue {
    GCObject obj;        /* Fase 7 S2: header GC; primer campo. */
    Valor *posicion;     /* &stack_slot mientras esté abierto; &cerrado si cerrado */
    Valor cerrado;
    Upvalue *siguiente;
    int refcount;
};

Upvalue *upvalue_nuevo(Valor *slot);
void upvalue_retener(Upvalue *u);
void upvalue_liberar(Upvalue *u);

/*
 * Closure: instancia ejecutable de una FuncionBC. Cada vez que se
 * encuentra una `funcion ... fin funcion` en runtime se crea una
 * Closure nueva con su array de upvalues. Las closures se comparten
 * por refcount entre Valores.
 */
struct Closure {
    GCObject obj;             /* Fase 7 S2: header GC; primer campo. */
    FuncionBC *plantilla;
    Upvalue **upvalues;       /* array dinámico, longitud = plantilla->n_upvalues */
    int refcount;
    /*
     * Clase donde este closure fue registrado como método (v0.8.2).
     * NULL si no es método (función top-level, lambda, función nested).
     *
     * Crítico para `super` multinivel: cuando un método de Hijo hace
     * `super.x()`, debemos buscar `x` en `Hijo.superclase`, no en
     * `yo.clase.superclase` (que sería Hijo si yo es de Nieto).
     *
     * Antes de v0.8.2 (con refcount sin GC) este campo crearía un
     * ciclo (Clase → metodos[m] → Closure → clase_definicion → Clase)
     * que refcount no podía romper. Ahora con GC mark-sweep se rompe
     * automáticamente cuando la clase deja de ser alcanzable.
     */
    Clase *clase_definicion;
    /*
     * Diccionario de globales del scope donde el closure fue creado
     * (v0.9.0). Crítico para módulos: una función definida en un
     * módulo debe ver las globales de ese módulo cuando la invoca el
     * importador. Sin este campo, la función vería las globales del
     * importador y `ErrorDeNombre` para todas sus referencias internas.
     *
     * NULL si el closure no pertenece a un módulo (e.g. funciones
     * top-level del programa principal).
     */
    Diccionario *globales_definicion;
    /* v1.17: valores de default evaluados al crear este closure. NULL
       si plantilla->n_defaults == 0. Array de longitud
       plantilla->n_defaults; cada Valor es dueño (clonar para usar). */
    Valor *defaults;
};

Closure *closure_nuevo(FuncionBC *fn);
void closure_retener(Closure *c);
void closure_liberar(Closure *c);

/* Construye un Valor de tipo VAL_FUNCION_BC tomando posesión del refcount. */
Valor valor_closure(Closure *c);

/* Construye un Valor de tipo VAL_PLANTILLA_BC tomando posesión del refcount. */
Valor valor_plantilla(FuncionBC *fn);

#endif /* CORNAMUSA_CHUNK_H */
