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

    /* ---- Stack management ---- */
    OP_DESCARTAR,       /* pop sin usar */

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
 * Función compilada a bytecode (Fase 6 sesión 5).
 *
 * Una función bytecode tiene su propio `Chunk` con el código de su
 * cuerpo, una aridad fija (cantidad de parámetros) y un nombre con
 * fines de debug. Es propietaria del Chunk (no compartido). Se
 * comparte por refcount entre Valores.
 *
 * Cornamusa v0.6 no implementa closures todavía: las funciones
 * solo acceden a parámetros, locales propias y globales — no
 * capturan variables de funciones enclosing. Las closures con
 * upvalues están planeadas para Fase 6 sesión 5b o Fase 6 sesión 6.
 */
struct FuncionBC {
    char *nombre;            /* duplicado en heap; se libera con la función */
    int longitud_nombre;
    int aridad;
    Chunk chunk;
    int refcount;
};

/*
 * Crea una FuncionBC con chunk vacío y refcount=1. El cliente
 * compila el cuerpo en `chunk` y luego envuelve la función en un
 * Valor con `valor_funcion_bc`. El nombre se duplica.
 */
FuncionBC *funcion_bc_nueva(const char *nombre, int len_nombre, int aridad);
void funcion_bc_retener(FuncionBC *f);
void funcion_bc_liberar(FuncionBC *f);

/* Construye un Valor de tipo VAL_FUNCION_BC tomando posesión del refcount. */
Valor valor_funcion_bc(FuncionBC *f);

#endif /* CORNAMUSA_CHUNK_H */
