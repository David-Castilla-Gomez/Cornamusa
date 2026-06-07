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

/*
 * OP_OBTENER_GLOBAL / OP_OBTENER_GLOBAL_CACHE (F10): 6 bytes.
 *   [opcode][name_idx][cache_ver_hi][cache_ver_lo][slot_hi][slot_lo]
 * Mostramos name_idx + el nombre y, si el cache está rellenado
 * (no todo cero), también la versión y el slot guardados.
 */
static int instruccion_obtener_global(const char *nombre, const Chunk *c,
                                       int offset, FILE *out) {
    uint8_t name_idx = c->codigo[offset + 1];
    uint16_t cached_ver = (uint16_t)((c->codigo[offset + 2] << 8)
                                     | c->codigo[offset + 3]);
    uint16_t cached_slot = (uint16_t)((c->codigo[offset + 4] << 8)
                                      | c->codigo[offset + 5]);
    fprintf(out, "%-24s %4d", nombre, name_idx);
    if (name_idx < c->constantes_cuenta) {
        fputs(" '", out);
        imprimir_constante_repr(&c->constantes[name_idx], out);
        fputc('\'', out);
    }
    if (cached_ver != 0 || cached_slot != 0) {
        fprintf(out, "  [cache: ver=%u slot=%u]", cached_ver, cached_slot);
    }
    fputc('\n', out);
    return offset + 6;
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
        case OP_SUMAR_INT_INT:   return instruccion_simple("OP_SUMAR_INT_INT", offset, out);
        case OP_RESTAR:          return instruccion_simple("OP_RESTAR", offset, out);
        case OP_RESTAR_INT_INT:  return instruccion_simple("OP_RESTAR_INT_INT", offset, out);
        case OP_MULTIPLICAR:     return instruccion_simple("OP_MULTIPLICAR", offset, out);
        case OP_MULTIPLICAR_INT_INT:
            return instruccion_simple("OP_MULTIPLICAR_INT_INT", offset, out);
        case OP_DIVIDIR:         return instruccion_simple("OP_DIVIDIR", offset, out);
        case OP_DIVIDIR_ENTERO:  return instruccion_simple("OP_DIVIDIR_ENTERO", offset, out);
        case OP_MODULO:          return instruccion_simple("OP_MODULO", offset, out);
        case OP_POTENCIA:        return instruccion_simple("OP_POTENCIA", offset, out);
        case OP_NEGAR:           return instruccion_simple("OP_NEGAR", offset, out);
        case OP_TILDE_BIT:       return instruccion_simple("OP_TILDE_BIT", offset, out);
        case OP_POSITIVO:        return instruccion_simple("OP_POSITIVO", offset, out);
        case OP_BIT_Y:           return instruccion_simple("OP_BIT_Y", offset, out);
        case OP_BIT_O:           return instruccion_simple("OP_BIT_O", offset, out);
        case OP_BIT_XOR:         return instruccion_simple("OP_BIT_XOR", offset, out);
        case OP_DESPL_IZQ:       return instruccion_simple("OP_DESPL_IZQ", offset, out);
        case OP_DESPL_DER:       return instruccion_simple("OP_DESPL_DER", offset, out);

        case OP_NO:              return instruccion_simple("OP_NO", offset, out);
        case OP_IGUAL:           return instruccion_simple("OP_IGUAL", offset, out);
        case OP_DISTINTO:        return instruccion_simple("OP_DISTINTO", offset, out);
        case OP_MENOR:           return instruccion_simple("OP_MENOR", offset, out);
        case OP_MENOR_INT_INT:   return instruccion_simple("OP_MENOR_INT_INT", offset, out);
        case OP_MENOR_IGUAL:     return instruccion_simple("OP_MENOR_IGUAL", offset, out);
        case OP_MENOR_IGUAL_INT_INT:
            return instruccion_simple("OP_MENOR_IGUAL_INT_INT", offset, out);
        case OP_MAYOR:           return instruccion_simple("OP_MAYOR", offset, out);
        case OP_MAYOR_INT_INT:   return instruccion_simple("OP_MAYOR_INT_INT", offset, out);
        case OP_MAYOR_IGUAL:     return instruccion_simple("OP_MAYOR_IGUAL", offset, out);
        case OP_MAYOR_IGUAL_INT_INT:
            return instruccion_simple("OP_MAYOR_IGUAL_INT_INT", offset, out);
        case OP_ES:              return instruccion_simple("OP_ES", offset, out);
        case OP_EN:              return instruccion_simple("OP_EN", offset, out);

        case OP_DESCARTAR:       return instruccion_simple("OP_DESCARTAR", offset, out);
        case OP_DUP_2:           return instruccion_simple("OP_DUP_2", offset, out);
        case OP_INTERCAMBIAR:    return instruccion_simple("OP_INTERCAMBIAR", offset, out);

        case OP_SALTAR:          return instruccion_u16("OP_SALTAR", c, offset, +1, out);
        case OP_SALTAR_SI_FALSO: return instruccion_u16("OP_SALTAR_SI_FALSO", c, offset, +1, out);
        case OP_BUCLE:           return instruccion_u16("OP_BUCLE", c, offset, -1, out);

        case OP_OBTENER_LOCAL:   return instruccion_byte("OP_OBTENER_LOCAL", c, offset, out);
        case OP_ASIGNAR_LOCAL:   return instruccion_byte("OP_ASIGNAR_LOCAL", c, offset, out);
        case OP_OBTENER_GLOBAL:  return instruccion_obtener_global("OP_OBTENER_GLOBAL", c, offset, out);
        case OP_OBTENER_GLOBAL_CACHE:
            return instruccion_obtener_global("OP_OBTENER_GLOBAL_CACHE", c, offset, out);
        case OP_DEFINIR_GLOBAL:  return instruccion_byte("OP_DEFINIR_GLOBAL", c, offset, out);
        case OP_ASIGNAR_GLOBAL:  return instruccion_byte("OP_ASIGNAR_GLOBAL", c, offset, out);
        case OP_LLAMAR:          return instruccion_byte("OP_LLAMAR", c, offset, out);
        case OP_LLAMAR_NATIVA:   return instruccion_byte("OP_LLAMAR_NATIVA", c, offset, out);
        case OP_LLAMAR_BC:       return instruccion_byte("OP_LLAMAR_BC", c, offset, out);
        case OP_LLAMAR_CLASE:    return instruccion_byte("OP_LLAMAR_CLASE", c, offset, out);
        case OP_LLAMAR_METODO_LIGADO:
            return instruccion_byte("OP_LLAMAR_METODO_LIGADO", c, offset, out);
        case OP_LISTA_AGREGAR:   return instruccion_simple("OP_LISTA_AGREGAR", offset, out);
        case OP_LISTA_EXTENDER:  return instruccion_simple("OP_LISTA_EXTENDER", offset, out);
        case OP_LLAMAR_SPREAD:   return instruccion_simple("OP_LLAMAR_SPREAD", offset, out);
        case OP_CONJUNTO_AGREGAR: return instruccion_simple("OP_CONJUNTO_AGREGAR", offset, out);
        case OP_PRODUCIR:        return instruccion_simple("OP_PRODUCIR", offset, out);
        case OP_LLAMAR_KW: {
            /* 2 bytes: n_pos, n_kw. */
            uint8_t np = c->codigo[offset + 1];
            uint8_t nk = c->codigo[offset + 2];
            fprintf(out, "%-22s n_pos=%d n_kw=%d\n", "OP_LLAMAR_KW", np, nk);
            return offset + 3;
        }
        case OP_DICC_AGREGAR_PAR: return instruccion_simple("OP_DICC_AGREGAR_PAR", offset, out);
        case OP_DICC_EXTENDER:   return instruccion_simple("OP_DICC_EXTENDER", offset, out);
        case OP_LLAMAR_KW_DICT:  return instruccion_byte("OP_LLAMAR_KW_DICT", c, offset, out);
        case OP_CLOSURE: {
            /* Formato: [byte fn_idx] [n_upvalues * (is_local, index)].
               No sabemos n_upvalues sin leer la FuncionBC del pool, así
               que solo imprimimos el índice. */
            uint8_t fn_idx = c->codigo[offset + 1];
            fprintf(out, "%-20s %4d\n", "OP_CLOSURE", fn_idx);
            /* Avanzar saltando los pares (is_local, index) — necesitamos
               n_upvalues. Lo extraemos de la constante. */
            int after = offset + 2;
            if (fn_idx < c->constantes_cuenta
                && c->constantes[fn_idx].tipo == VAL_PLANTILLA_BC) {
                int n_uv = c->constantes[fn_idx].como.plantilla->n_upvalues;
                for (int i = 0; i < n_uv; i++) {
                    fprintf(out, "%04d    |                       %s %4d\n",
                        after, c->codigo[after] ? "local" : "upval",
                        c->codigo[after + 1]);
                    after += 2;
                }
            }
            return after;
        }
        case OP_OBTENER_UPVALUE: return instruccion_byte("OP_OBTENER_UPVALUE", c, offset, out);
        case OP_ASIGNAR_UPVALUE: return instruccion_byte("OP_ASIGNAR_UPVALUE", c, offset, out);
        case OP_CERRAR_UPVALUE:  return instruccion_simple("OP_CERRAR_UPVALUE", offset, out);
        case OP_INTENTAR_INICIAR: return instruccion_u16("OP_INTENTAR_INICIAR", c, offset, +1, out);
        case OP_INTENTAR_FIN:    return instruccion_simple("OP_INTENTAR_FIN", offset, out);
        case OP_LANZAR:          return instruccion_simple("OP_LANZAR", offset, out);
        case OP_COMPROBAR_TIPO_EXC: return instruccion_byte("OP_COMPROBAR_TIPO_EXC", c, offset, out);
        case OP_CLASE:           return instruccion_byte("OP_CLASE", c, offset, out);
        case OP_OBTENER_ATRIBUTO:
            return instruccion_obtener_global("OP_OBTENER_ATRIBUTO", c, offset, out);
        case OP_OBTENER_ATRIBUTO_INSTANCIA:
            return instruccion_obtener_global("OP_OBTENER_ATRIBUTO_INSTANCIA", c, offset, out);
        case OP_ASIGNAR_ATRIBUTO: return instruccion_byte("OP_ASIGNAR_ATRIBUTO", c, offset, out);
        case OP_METODO:          return instruccion_byte("OP_METODO", c, offset, out);
        case OP_HEREDAR:         return instruccion_simple("OP_HEREDAR", offset, out);
        case OP_SUPER_INVOCAR: {
            uint8_t name_idx = c->codigo[offset + 1];
            uint8_t n_args = c->codigo[offset + 2];
            fprintf(out, "%-20s %4d ", "OP_SUPER_INVOCAR", name_idx);
            if (name_idx < c->constantes_cuenta) {
                fputs("'", out);
                imprimir_constante_repr(&c->constantes[name_idx], out);
                fputs("' ", out);
            }
            fprintf(out, "(%d args)\n", n_args);
            return offset + 3;
        }
        case OP_IMPORTAR: {
            /* [byte modulo_idx] [byte binding_idx] */
            uint8_t mod_idx = c->codigo[offset + 1];
            uint8_t bind_idx = c->codigo[offset + 2];
            fprintf(out, "%-20s %4d %4d\n", "OP_IMPORTAR", mod_idx, bind_idx);
            return offset + 3;
        }
        case OP_IMPORTAR_PARA_DESDE: return instruccion_byte("OP_IMPORTAR_PARA_DESDE", c, offset, out);
        case OP_DUP:             return instruccion_simple("OP_DUP", offset, out);

        case OP_IMPRIMIR:        return instruccion_byte("OP_IMPRIMIR", c, offset, out);
        case OP_BUILD_LISTA:     return instruccion_byte("OP_BUILD_LISTA", c, offset, out);
        case OP_BUILD_TUPLA:     return instruccion_byte("OP_BUILD_TUPLA", c, offset, out);
        case OP_BUILD_DICC:      return instruccion_byte("OP_BUILD_DICC", c, offset, out);
        case OP_BUILD_CONJUNTO:  return instruccion_byte("OP_BUILD_CONJUNTO", c, offset, out);
        case OP_INDICE:          return instruccion_simple("OP_INDICE", offset, out);
        case OP_ASIGNAR_INDICE:  return instruccion_simple("OP_ASIGNAR_INDICE", offset, out);
        case OP_REBANADA:        return instruccion_simple("OP_REBANADA", offset, out);
        case OP_ITER_INICIAR:    return instruccion_simple("OP_ITER_INICIAR", offset, out);
        case OP_ITER_SIGUIENTE: {
            /* Formato: [byte slot] [u16 offset]. */
            uint8_t slot = c->codigo[offset + 1];
            uint16_t off = (uint16_t)((c->codigo[offset + 2] << 8)
                                       | c->codigo[offset + 3]);
            int destino = offset + 4 + off;
            fprintf(out, "%-20s %4d %4d -> %04d\n",
                "OP_ITER_SIGUIENTE", slot, off, destino);
            return offset + 4;
        }
        case OP_FORMATO_F:       return instruccion_simple("OP_FORMATO_F", offset, out);
        case OP_ASEGURAR_CADENA: return instruccion_simple("OP_ASEGURAR_CADENA", offset, out);
        case OP_REPR:            return instruccion_simple("OP_REPR", offset, out);
        case OP_ASIGNAR_REBANADA: return instruccion_simple("OP_ASIGNAR_REBANADA", offset, out);
        case OP_FORMATO_F_SPEC: return instruccion_const_corta(c, offset, out);
        case OP_LLAMAR_SPREAD_KW_DICT: return instruccion_simple("OP_LLAMAR_SPREAD_KW_DICT", offset, out);
        case OP_LONGITUD:        return instruccion_simple("OP_LONGITUD", offset, out);
        case OP_ES_TUPLA:        return instruccion_simple("OP_ES_TUPLA", offset, out);
        case OP_ES_LISTA:        return instruccion_simple("OP_ES_LISTA", offset, out);
        case OP_BORRAR_INDICE:   return instruccion_simple("OP_BORRAR_INDICE", offset, out);
        case OP_BORRAR_ATRIBUTO: return instruccion_byte("OP_BORRAR_ATRIBUTO", c, offset, out);
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
