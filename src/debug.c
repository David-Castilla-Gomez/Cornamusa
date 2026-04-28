#include "debug.h"

#include <stdint.h>
#include <stdio.h>

#include "valor.h"

/*
 * Helpers para imprimir cada formato de instrucción. Cada uno
 * devuelve el offset del siguiente byte de instrucción.
 *
 * Convenciones:
 *   - "simple": opcode sin operandos (1 byte).
 *   - "byte": opcode + 1 operando byte (2 bytes).
 *   - "u16": opcode + operando big-endian de 2 bytes (3 bytes).
 *   - "u24": opcode + operando little-endian de 3 bytes (4 bytes).
 */

static int instruccion_simple(const char *nombre, int offset, FILE *out) {
    fprintf(out, "%-20s\n", nombre);
    return offset + 1;
}

static int instruccion_byte(const char *nombre, const Chunk *c, int offset,
                             FILE *out) {
    uint8_t arg = c->codigo[offset + 1];
    fprintf(out, "%-20s %4d\n", nombre, arg);
    return offset + 2;
}

static int instruccion_u16(const char *nombre, const Chunk *c, int offset,
                            int signo, FILE *out) {
    uint16_t arg = (uint16_t)((c->codigo[offset + 1] << 8)
                              | c->codigo[offset + 2]);
    int destino = offset + 3 + signo * (int)arg;
    fprintf(out, "%-20s %4d -> %04d\n", nombre, arg, destino);
    return offset + 3;
}

static void imprimir_constante_repr(const Valor *v, FILE *out) {
    char buffer[256];
    valor_a_repr(v, buffer, sizeof(buffer));
    fputs(buffer, out);
}

static int instruccion_const_corta(const Chunk *c, int offset, FILE *out) {
    uint8_t idx = c->codigo[offset + 1];
    fprintf(out, "%-20s %4d '", "OP_CONST", idx);
    if (idx < c->constantes_cuenta) {
        imprimir_constante_repr(&c->constantes[idx], out);
    } else {
        fprintf(out, "<idx fuera de rango>");
    }
    fputs("'\n", out);
    return offset + 2;
}

static int instruccion_const_larga(const Chunk *c, int offset, FILE *out) {
    int idx = c->codigo[offset + 1]
            | (c->codigo[offset + 2] << 8)
            | (c->codigo[offset + 3] << 16);
    fprintf(out, "%-20s %4d '", "OP_CONST_LARGO", idx);
    if (idx >= 0 && idx < c->constantes_cuenta) {
        imprimir_constante_repr(&c->constantes[idx], out);
    } else {
        fprintf(out, "<idx fuera de rango>");
    }
    fputs("'\n", out);
    return offset + 4;
}

int desensamblar_instruccion(const Chunk *c, int offset, FILE *out) {
    fprintf(out, "%04d ", offset);

    /* Línea: si coincide con la anterior usamos `|` para no repetir. */
    if (offset > 0 && c->lineas[offset] == c->lineas[offset - 1]) {
        fprintf(out, "    | ");
    } else {
        fprintf(out, "%5d ", c->lineas[offset]);
    }

    uint8_t op = c->codigo[offset];
    switch ((OpCode)op) {
        case OP_CONST:           return instruccion_const_corta(c, offset, out);
        case OP_CONST_LARGO:     return instruccion_const_larga(c, offset, out);

        case OP_NULO:            return instruccion_simple("OP_NULO", offset, out);
        case OP_VERDADERO:       return instruccion_simple("OP_VERDADERO", offset, out);
        case OP_FALSO:           return instruccion_simple("OP_FALSO", offset, out);

        case OP_SUMAR:           return instruccion_simple("OP_SUMAR", offset, out);
        case OP_RESTAR:          return instruccion_simple("OP_RESTAR", offset, out);
        case OP_MULTIPLICAR:     return instruccion_simple("OP_MULTIPLICAR", offset, out);
        case OP_DIVIDIR:         return instruccion_simple("OP_DIVIDIR", offset, out);
        case OP_DIVIDIR_ENTERO:  return instruccion_simple("OP_DIVIDIR_ENTERO", offset, out);
        case OP_MODULO:          return instruccion_simple("OP_MODULO", offset, out);
        case OP_POTENCIA:        return instruccion_simple("OP_POTENCIA", offset, out);
        case OP_NEGAR:           return instruccion_simple("OP_NEGAR", offset, out);

        case OP_NO:              return instruccion_simple("OP_NO", offset, out);
        case OP_IGUAL:           return instruccion_simple("OP_IGUAL", offset, out);
        case OP_DISTINTO:        return instruccion_simple("OP_DISTINTO", offset, out);
        case OP_MENOR:           return instruccion_simple("OP_MENOR", offset, out);
        case OP_MENOR_IGUAL:     return instruccion_simple("OP_MENOR_IGUAL", offset, out);
        case OP_MAYOR:           return instruccion_simple("OP_MAYOR", offset, out);
        case OP_MAYOR_IGUAL:     return instruccion_simple("OP_MAYOR_IGUAL", offset, out);

        case OP_DESCARTAR:       return instruccion_simple("OP_DESCARTAR", offset, out);

        case OP_SALTAR:          return instruccion_u16("OP_SALTAR", c, offset, +1, out);
        case OP_SALTAR_SI_FALSO: return instruccion_u16("OP_SALTAR_SI_FALSO", c, offset, +1, out);
        case OP_BUCLE:           return instruccion_u16("OP_BUCLE", c, offset, -1, out);

        case OP_OBTENER_LOCAL:   return instruccion_byte("OP_OBTENER_LOCAL", c, offset, out);
        case OP_ASIGNAR_LOCAL:   return instruccion_byte("OP_ASIGNAR_LOCAL", c, offset, out);
        case OP_OBTENER_GLOBAL:  return instruccion_byte("OP_OBTENER_GLOBAL", c, offset, out);
        case OP_DEFINIR_GLOBAL:  return instruccion_byte("OP_DEFINIR_GLOBAL", c, offset, out);
        case OP_ASIGNAR_GLOBAL:  return instruccion_byte("OP_ASIGNAR_GLOBAL", c, offset, out);
        case OP_LLAMAR:          return instruccion_byte("OP_LLAMAR", c, offset, out);

        case OP_IMPRIMIR:        return instruccion_simple("OP_IMPRIMIR", offset, out);
        case OP_RETORNAR:        return instruccion_simple("OP_RETORNAR", offset, out);
    }

    fprintf(out, "<opcode desconocido %d>\n", op);
    return offset + 1;
}

void desensamblar_chunk(const Chunk *c, const char *nombre, FILE *out) {
    fprintf(out, "== %s ==\n", nombre ? nombre : "(sin nombre)");
    int offset = 0;
    while (offset < c->cuenta) {
        offset = desensamblar_instruccion(c, offset, out);
    }
}
