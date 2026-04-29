#ifndef CORNAMUSA_CHUNK_H
#define CORNAMUSA_CHUNK_H

#include <stdbool.h>
#include <stdint.h>

#include "valor.h"

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

    /* ---- Aritmética ---- */
    OP_SUMAR,
    OP_RESTAR,
    OP_MULTIPLICAR,
    OP_DIVIDIR,         /* true division → decimal */
    OP_DIVIDIR_ENTERO,  /* floor division (//) */
    OP_MODULO,
    OP_POTENCIA,        /* ** */
    OP_NEGAR,           /* unario -x */

    /* ---- Comparación / lógica ---- */
    OP_NO,              /* unario `no x` */
    OP_IGUAL,
    OP_DISTINTO,
    OP_MENOR,
    OP_MENOR_IGUAL,
    OP_MAYOR,
    OP_MAYOR_IGUAL,
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
    OP_OBTENER_GLOBAL,          /* GET_GLOBAL [byte name-idx] */
    OP_DEFINIR_GLOBAL,
    OP_ASIGNAR_GLOBAL,
    OP_LLAMAR,                  /* CALL [byte n_args] */

    /* ---- Closures (v0.6.2) ---- */
    OP_CLOSURE,                 /* [byte fn_idx] [n_upvalues * (is_local, index)] */
    OP_OBTENER_UPVALUE,         /* [byte slot] */
    OP_ASIGNAR_UPVALUE,         /* [byte slot] */
    OP_CERRAR_UPVALUE,          /* cierra el upvalue del slot top y descarta */

    /* ---- Excepciones (v0.6.3) ---- */
    OP_INTENTAR_INICIAR,        /* [u16 offset_handler] empuja un handler frame */
    OP_INTENTAR_FIN,            /* pop el handler frame al salir limpio del intentar */
    OP_LANZAR,                  /* pop la excepción del tope, salta al handler */

    /* ---- Clases / atributos (v0.7.0 Fase 8 sesión 1) ---- */
    OP_CLASE,                   /* [byte name_idx]: crea Clase y empuja VAL_CLASE */
    OP_OBTENER_ATRIBUTO,        /* [byte name_idx]: pop obj, push obj.attr */
    OP_ASIGNAR_ATRIBUTO,        /* [byte name_idx]: pop valor, pop obj, set obj.attr=valor, push nulo */

    /* ---- Métodos (v0.7.0 Fase 8 sesión 2) ---- */
    OP_METODO,                  /* [byte name_idx]: pop closure, set clase.metodos[name] = closure (clase queda en stack) */

    /* ---- Herencia (v0.7.0 Fase 8 sesión 4) ---- */
    OP_HEREDAR,                 /* pop super (sin operando): copia super.metodos → clase.metodos y enlaza superclase. Stack: [..., clase, super] → [..., clase]. */

    /* ---- super (v0.7.1) ---- */
    OP_SUPER_INVOCAR,           /* [byte name_idx] [byte n_args]: stack [..., yo, arg1, ..., argN]. Despacha al método name de yo.clase.superclase. */

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

struct FuncionBC {
    char *nombre;            /* duplicado en heap; se libera con la función */
    int longitud_nombre;
    int aridad;
    Chunk chunk;
    int refcount;
    /* Metadata de upvalues para OP_CLOSURE. n_upvalues = 0 para
       funciones que no capturan nada. */
    InfoUpvalue info_upvalues[FN_BC_UPVALUES_MAX];
    int n_upvalues;
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
    FuncionBC *plantilla;
    Upvalue **upvalues;       /* array dinámico, longitud = plantilla->n_upvalues */
    int refcount;
};

Closure *closure_nuevo(FuncionBC *fn);
void closure_retener(Closure *c);
void closure_liberar(Closure *c);

/* Construye un Valor de tipo VAL_FUNCION_BC tomando posesión del refcount. */
Valor valor_closure(Closure *c);

/* Construye un Valor de tipo VAL_PLANTILLA_BC tomando posesión del refcount. */
Valor valor_plantilla(FuncionBC *fn);

#endif /* CORNAMUSA_CHUNK_H */
