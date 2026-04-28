#ifndef CORNAMUSA_DEBUG_H
#define CORNAMUSA_DEBUG_H

#include <stdio.h>

#include "chunk.h"

/*
 * Disassembler de bytecode (Fase 6 sesión 1).
 *
 * Imprime la secuencia de instrucciones de un `Chunk` en formato
 * legible. Útil para verificar que el compilador emite el código
 * esperado y para depurar la VM.
 *
 * Formato (estilo clox cap. 14):
 *
 *   == nombre ==
 *   0000  123 OP_CONST            7 '42'
 *   0002    | OP_RETORNAR
 *
 * Donde:
 *   - Primera columna: offset en bytes desde el inicio del chunk.
 *   - Segunda columna: número de línea fuente (`|` indica que coincide
 *     con la línea de la instrucción anterior).
 *   - Tercera columna: nombre del opcode.
 *   - Resto: operandos según el tipo de instrucción.
 */
void desensamblar_chunk(const Chunk *c, const char *nombre, FILE *salida);

/*
 * Desensambla una sola instrucción a partir del offset dado y devuelve
 * el offset de la siguiente. Útil para tracing en la VM.
 */
int desensamblar_instruccion(const Chunk *c, int offset, FILE *salida);

#endif /* CORNAMUSA_DEBUG_H */
