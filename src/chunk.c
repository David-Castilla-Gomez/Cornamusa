#include "chunk.h"

#include <stdlib.h>
#include <string.h>

#define CAPACIDAD_INICIAL 8

const char *opcode_nombre(OpCode op) {
    switch (op) {
        case OP_CONST:           return "OP_CONST";
        case OP_CONST_LARGO:     return "OP_CONST_LARGO";
        case OP_NULO:            return "OP_NULO";
        case OP_VERDADERO:       return "OP_VERDADERO";
        case OP_FALSO:           return "OP_FALSO";
        case OP_SUMAR:           return "OP_SUMAR";
        case OP_RESTAR:          return "OP_RESTAR";
        case OP_MULTIPLICAR:     return "OP_MULTIPLICAR";
        case OP_DIVIDIR:         return "OP_DIVIDIR";
        case OP_DIVIDIR_ENTERO:  return "OP_DIVIDIR_ENTERO";
        case OP_MODULO:          return "OP_MODULO";
        case OP_POTENCIA:        return "OP_POTENCIA";
        case OP_NEGAR:           return "OP_NEGAR";
        case OP_NO:              return "OP_NO";
        case OP_IGUAL:           return "OP_IGUAL";
        case OP_DISTINTO:        return "OP_DISTINTO";
        case OP_MENOR:           return "OP_MENOR";
        case OP_MENOR_IGUAL:     return "OP_MENOR_IGUAL";
        case OP_MAYOR:           return "OP_MAYOR";
        case OP_MAYOR_IGUAL:     return "OP_MAYOR_IGUAL";
        case OP_ES:              return "OP_ES";
        case OP_EN:              return "OP_EN";
        case OP_DESCARTAR:       return "OP_DESCARTAR";
        case OP_DUP_2:           return "OP_DUP_2";
        case OP_SALTAR:          return "OP_SALTAR";
        case OP_SALTAR_SI_FALSO: return "OP_SALTAR_SI_FALSO";
        case OP_BUCLE:           return "OP_BUCLE";
        case OP_OBTENER_LOCAL:   return "OP_OBTENER_LOCAL";
        case OP_ASIGNAR_LOCAL:   return "OP_ASIGNAR_LOCAL";
        case OP_OBTENER_GLOBAL:  return "OP_OBTENER_GLOBAL";
        case OP_DEFINIR_GLOBAL:  return "OP_DEFINIR_GLOBAL";
        case OP_ASIGNAR_GLOBAL:  return "OP_ASIGNAR_GLOBAL";
        case OP_LLAMAR:          return "OP_LLAMAR";
        case OP_IMPRIMIR:        return "OP_IMPRIMIR";
        case OP_BUILD_LISTA:     return "OP_BUILD_LISTA";
        case OP_BUILD_TUPLA:     return "OP_BUILD_TUPLA";
        case OP_BUILD_DICC:      return "OP_BUILD_DICC";
        case OP_BUILD_CONJUNTO:  return "OP_BUILD_CONJUNTO";
        case OP_INDICE:          return "OP_INDICE";
        case OP_ASIGNAR_INDICE:  return "OP_ASIGNAR_INDICE";
        case OP_ITER_INICIAR:    return "OP_ITER_INICIAR";
        case OP_ITER_SIGUIENTE:  return "OP_ITER_SIGUIENTE";
        case OP_RETORNAR:        return "OP_RETORNAR";
    }
    return NULL;
}

void chunk_iniciar(Chunk *c) {
    c->codigo = NULL;
    c->lineas = NULL;
    c->cuenta = 0;
    c->capacidad = 0;
    c->constantes = NULL;
    c->constantes_cuenta = 0;
    c->constantes_capacidad = 0;
}

void chunk_destruir(Chunk *c) {
    if (c == NULL) return;
    free(c->codigo);
    free(c->lineas);
    if (c->constantes) {
        for (int i = 0; i < c->constantes_cuenta; i++) {
            valor_destruir(&c->constantes[i]);
        }
        free(c->constantes);
    }
    chunk_iniciar(c);  /* deja el chunk en estado válido tras destruir */
}

static bool asegurar_capacidad_codigo(Chunk *c, int necesario) {
    if (c->capacidad >= necesario) return true;
    int nueva_cap = c->capacidad < CAPACIDAD_INICIAL
                  ? CAPACIDAD_INICIAL : c->capacidad;
    while (nueva_cap < necesario) nueva_cap *= 2;

    uint8_t *nuevo_cod = (uint8_t *)realloc(c->codigo,
        sizeof(uint8_t) * (size_t)nueva_cap);
    if (!nuevo_cod) return false;
    c->codigo = nuevo_cod;

    int *nuevas_lineas = (int *)realloc(c->lineas,
        sizeof(int) * (size_t)nueva_cap);
    if (!nuevas_lineas) return false;
    c->lineas = nuevas_lineas;

    c->capacidad = nueva_cap;
    return true;
}

static bool asegurar_capacidad_const(Chunk *c, int necesario) {
    if (c->constantes_capacidad >= necesario) return true;
    int nueva_cap = c->constantes_capacidad < CAPACIDAD_INICIAL
                  ? CAPACIDAD_INICIAL : c->constantes_capacidad;
    while (nueva_cap < necesario) nueva_cap *= 2;
    Valor *nuevas = (Valor *)realloc(c->constantes,
        sizeof(Valor) * (size_t)nueva_cap);
    if (!nuevas) return false;
    c->constantes = nuevas;
    c->constantes_capacidad = nueva_cap;
    return true;
}

void chunk_emitir_byte(Chunk *c, uint8_t b, int linea) {
    if (!asegurar_capacidad_codigo(c, c->cuenta + 1)) return;
    c->codigo[c->cuenta] = b;
    c->lineas[c->cuenta] = linea;
    c->cuenta++;
}

void chunk_emitir_byte2(Chunk *c, uint8_t a, uint8_t b, int linea) {
    if (!asegurar_capacidad_codigo(c, c->cuenta + 2)) return;
    c->codigo[c->cuenta] = a;
    c->lineas[c->cuenta] = linea;
    c->cuenta++;
    c->codigo[c->cuenta] = b;
    c->lineas[c->cuenta] = linea;
    c->cuenta++;
}

int chunk_agregar_constante(Chunk *c, Valor v) {
    if (!asegurar_capacidad_const(c, c->constantes_cuenta + 1)) {
        valor_destruir(&v);
        return -1;
    }
    c->constantes[c->constantes_cuenta] = v;
    return c->constantes_cuenta++;
}

void chunk_emitir_constante(Chunk *c, Valor v, int linea) {
    int idx = chunk_agregar_constante(c, v);
    if (idx < 0) return;
    if (idx <= UINT8_MAX) {
        chunk_emitir_byte2(c, (uint8_t)OP_CONST, (uint8_t)idx, linea);
    } else {
        /* OP_CONST_LARGO: 3 bytes en little endian. */
        chunk_emitir_byte(c, (uint8_t)OP_CONST_LARGO, linea);
        chunk_emitir_byte(c, (uint8_t)(idx & 0xff), linea);
        chunk_emitir_byte(c, (uint8_t)((idx >> 8) & 0xff), linea);
        chunk_emitir_byte(c, (uint8_t)((idx >> 16) & 0xff), linea);
    }
}

/* ──────────────────────────────────────────────────────────────────
 * FuncionBC
 * ────────────────────────────────────────────────────────────────── */

FuncionBC *funcion_bc_nueva(const char *nombre, int len_nombre, int aridad) {
    FuncionBC *f = (FuncionBC *)malloc(sizeof(FuncionBC));
    if (!f) return NULL;
    char *copia = (char *)malloc((size_t)len_nombre + 1);
    if (!copia) { free(f); return NULL; }
    if (len_nombre > 0) memcpy(copia, nombre, (size_t)len_nombre);
    copia[len_nombre] = '\0';
    f->nombre = copia;
    f->longitud_nombre = len_nombre;
    f->aridad = aridad;
    chunk_iniciar(&f->chunk);
    f->refcount = 1;
    return f;
}

void funcion_bc_retener(FuncionBC *f) { if (f) f->refcount++; }

void funcion_bc_liberar(FuncionBC *f) {
    if (!f) return;
    f->refcount--;
    if (f->refcount > 0) return;
    chunk_destruir(&f->chunk);
    free(f->nombre);
    free(f);
}

Valor valor_funcion_bc(FuncionBC *f) {
    Valor v;
    v.tipo = VAL_FUNCION_BC;
    v.dueno_cadena = false;
    v.como.funcion_bc = f;
    return v;
}
