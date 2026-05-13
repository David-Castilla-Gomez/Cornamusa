#include "chunk.h"

#include <stdlib.h>
#include <string.h>

#include "memoria.h"   /* gc_alocar / gc_desenlazar */

#define CAPACIDAD_INICIAL 8

const char *opcode_nombre(OpCode op) {
    switch (op) {
        case OP_CONST:           return "OP_CONST";
        case OP_CONST_LARGO:     return "OP_CONST_LARGO";
        case OP_NULO:            return "OP_NULO";
        case OP_VERDADERO:       return "OP_VERDADERO";
        case OP_FALSO:           return "OP_FALSO";
        case OP_SUMAR:           return "OP_SUMAR";
        case OP_SUMAR_INT_INT:   return "OP_SUMAR_INT_INT";
        case OP_RESTAR:          return "OP_RESTAR";
        case OP_RESTAR_INT_INT:  return "OP_RESTAR_INT_INT";
        case OP_MULTIPLICAR:     return "OP_MULTIPLICAR";
        case OP_MULTIPLICAR_INT_INT: return "OP_MULTIPLICAR_INT_INT";
        case OP_DIVIDIR:         return "OP_DIVIDIR";
        case OP_DIVIDIR_ENTERO:  return "OP_DIVIDIR_ENTERO";
        case OP_MODULO:          return "OP_MODULO";
        case OP_POTENCIA:        return "OP_POTENCIA";
        case OP_NEGAR:           return "OP_NEGAR";
        case OP_NO:              return "OP_NO";
        case OP_IGUAL:           return "OP_IGUAL";
        case OP_DISTINTO:        return "OP_DISTINTO";
        case OP_MENOR:           return "OP_MENOR";
        case OP_MENOR_INT_INT:   return "OP_MENOR_INT_INT";
        case OP_MENOR_IGUAL:     return "OP_MENOR_IGUAL";
        case OP_MENOR_IGUAL_INT_INT: return "OP_MENOR_IGUAL_INT_INT";
        case OP_MAYOR:           return "OP_MAYOR";
        case OP_MAYOR_INT_INT:   return "OP_MAYOR_INT_INT";
        case OP_MAYOR_IGUAL:     return "OP_MAYOR_IGUAL";
        case OP_MAYOR_IGUAL_INT_INT: return "OP_MAYOR_IGUAL_INT_INT";
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
        case OP_OBTENER_GLOBAL_CACHE: return "OP_OBTENER_GLOBAL_CACHE";
        case OP_DEFINIR_GLOBAL:  return "OP_DEFINIR_GLOBAL";
        case OP_ASIGNAR_GLOBAL:  return "OP_ASIGNAR_GLOBAL";
        case OP_LLAMAR:          return "OP_LLAMAR";
        case OP_LLAMAR_NATIVA:   return "OP_LLAMAR_NATIVA";
        case OP_LLAMAR_BC:       return "OP_LLAMAR_BC";
        case OP_LLAMAR_CLASE:    return "OP_LLAMAR_CLASE";
        case OP_LLAMAR_METODO_LIGADO: return "OP_LLAMAR_METODO_LIGADO";
        case OP_LISTA_AGREGAR:   return "OP_LISTA_AGREGAR";
        case OP_LISTA_EXTENDER:  return "OP_LISTA_EXTENDER";
        case OP_LLAMAR_SPREAD:   return "OP_LLAMAR_SPREAD";
        case OP_LLAMAR_KW:       return "OP_LLAMAR_KW";
        case OP_CLOSURE:         return "OP_CLOSURE";
        case OP_OBTENER_UPVALUE: return "OP_OBTENER_UPVALUE";
        case OP_ASIGNAR_UPVALUE: return "OP_ASIGNAR_UPVALUE";
        case OP_CERRAR_UPVALUE:  return "OP_CERRAR_UPVALUE";
        case OP_INTENTAR_INICIAR: return "OP_INTENTAR_INICIAR";
        case OP_INTENTAR_FIN:    return "OP_INTENTAR_FIN";
        case OP_LANZAR:          return "OP_LANZAR";
        case OP_COMPROBAR_TIPO_EXC: return "OP_COMPROBAR_TIPO_EXC";
        case OP_CLASE:           return "OP_CLASE";
        case OP_OBTENER_ATRIBUTO: return "OP_OBTENER_ATRIBUTO";
        case OP_OBTENER_ATRIBUTO_INSTANCIA: return "OP_OBTENER_ATRIBUTO_INSTANCIA";
        case OP_ASIGNAR_ATRIBUTO: return "OP_ASIGNAR_ATRIBUTO";
        case OP_METODO:          return "OP_METODO";
        case OP_HEREDAR:         return "OP_HEREDAR";
        case OP_SUPER_INVOCAR:   return "OP_SUPER_INVOCAR";
        case OP_IMPORTAR:        return "OP_IMPORTAR";
        case OP_IMPORTAR_PARA_DESDE: return "OP_IMPORTAR_PARA_DESDE";
        case OP_DUP:             return "OP_DUP";
        case OP_IMPRIMIR:        return "OP_IMPRIMIR";
        case OP_BUILD_LISTA:     return "OP_BUILD_LISTA";
        case OP_BUILD_TUPLA:     return "OP_BUILD_TUPLA";
        case OP_BUILD_DICC:      return "OP_BUILD_DICC";
        case OP_BUILD_CONJUNTO:  return "OP_BUILD_CONJUNTO";
        case OP_INDICE:          return "OP_INDICE";
        case OP_ASIGNAR_INDICE:  return "OP_ASIGNAR_INDICE";
        case OP_REBANADA:        return "OP_REBANADA";
        case OP_ITER_INICIAR:    return "OP_ITER_INICIAR";
        case OP_ITER_SIGUIENTE:  return "OP_ITER_SIGUIENTE";
        case OP_FORMATO_F:       return "OP_FORMATO_F";
        case OP_ASEGURAR_CADENA: return "OP_ASEGURAR_CADENA";
        case OP_LONGITUD:        return "OP_LONGITUD";
        case OP_ES_TUPLA:        return "OP_ES_TUPLA";
        case OP_ES_LISTA:        return "OP_ES_LISTA";
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
    FuncionBC *f = (FuncionBC *)gc_alocar(sizeof(FuncionBC), GC_TIPO_FUNCION_BC);
    if (!f) return NULL;
    char *copia = (char *)malloc((size_t)len_nombre + 1);
    if (!copia) { gc_desenlazar(&f->obj); free(f); return NULL; }
    if (len_nombre > 0) memcpy(copia, nombre, (size_t)len_nombre);
    copia[len_nombre] = '\0';
    f->nombre = copia;
    f->longitud_nombre = len_nombre;
    f->aridad = aridad;
    chunk_iniciar(&f->chunk);
    f->refcount = 1;
    f->n_upvalues = 0;
    f->inline_desc.tipo = DUNDER_INLINE_NONE;
    f->inline_desc.attr_yo = NULL;
    f->inline_desc.attr_otro = NULL;
    f->inline_desc.init_attr1 = NULL;
    f->inline_desc.init_attr2 = NULL;
    f->inline_desc.nombre_clase = NULL;
    f->inline_desc.ctor_arg2_attr_yo = NULL;
    f->inline_desc.ctor_arg2_attr_otro = NULL;
    f->n_defaults = 0;     /* v1.17: el compilador lo setea si hay defaults */
    f->tiene_estrella = false;  /* v1.22: lo setea el compilador si hay *resto */
    f->nombres_params = NULL;       /* v1.23: setea el compilador */
    f->long_nombres_params = NULL;
    return f;
}

void funcion_bc_retener(FuncionBC *f) { if (f) f->refcount++; }

void funcion_bc_liberar(FuncionBC *f) {
    if (!f) return;
    f->refcount--;
    if (f->refcount > 0) return;
    chunk_destruir(&f->chunk);
    free(f->nombre);
    /* v1.5+: liberar descriptor inline. Cualquier campo no-NULL es heap. */
    if (f->inline_desc.attr_yo) free(f->inline_desc.attr_yo);
    if (f->inline_desc.attr_otro) free(f->inline_desc.attr_otro);
    if (f->inline_desc.init_attr1) free(f->inline_desc.init_attr1);
    if (f->inline_desc.init_attr2) free(f->inline_desc.init_attr2);
    if (f->inline_desc.nombre_clase) free(f->inline_desc.nombre_clase);
    if (f->inline_desc.ctor_arg2_attr_yo) free(f->inline_desc.ctor_arg2_attr_yo);
    if (f->inline_desc.ctor_arg2_attr_otro) free(f->inline_desc.ctor_arg2_attr_otro);
    /* v1.23: liberar nombres de params si están. */
    if (f->nombres_params) {
        for (int i = 0; i < f->aridad; i++) {
            free(f->nombres_params[i]);
        }
        free(f->nombres_params);
        free(f->long_nombres_params);
    }
    gc_desenlazar(&f->obj);
    free(f);
}

/* ──────────────────────────────────────────────────────────────────
 * Upvalue
 * ────────────────────────────────────────────────────────────────── */

Upvalue *upvalue_nuevo(Valor *slot) {
    Upvalue *u = (Upvalue *)gc_alocar(sizeof(Upvalue), GC_TIPO_UPVALUE);
    if (!u) return NULL;
    u->posicion = slot;
    u->cerrado = valor_nulo();
    u->siguiente = NULL;
    u->refcount = 1;
    return u;
}

void upvalue_retener(Upvalue *u) { if (u) u->refcount++; }

void upvalue_liberar(Upvalue *u) {
    if (!u) return;
    u->refcount--;
    if (u->refcount > 0) return;
    /* Si está cerrado, `posicion` apunta al campo `cerrado`. Lo
       destruimos. Si está abierto, no hacemos nada — el slot del
       stack no es nuestro. */
    if (u->posicion == &u->cerrado) {
        valor_destruir(&u->cerrado);
    }
    gc_desenlazar(&u->obj);
    free(u);
}

/* ──────────────────────────────────────────────────────────────────
 * Closure
 * ────────────────────────────────────────────────────────────────── */

Closure *closure_nuevo(FuncionBC *fn) {
    Closure *c = (Closure *)gc_alocar(sizeof(Closure), GC_TIPO_CLOSURE);
    if (!c) return NULL;
    c->plantilla = fn;
    funcion_bc_retener(fn);
    c->refcount = 1;
    c->clase_definicion = NULL;   /* set por OP_METODO si llega a ser método */
    c->globales_definicion = NULL; /* set por OP_CLOSURE para cerrar globales */
    c->defaults = NULL;            /* v1.17: set por OP_CLOSURE si fn tiene defaults */
    if (fn->n_upvalues > 0) {
        c->upvalues = (Upvalue **)calloc((size_t)fn->n_upvalues,
                                            sizeof(Upvalue *));
        if (!c->upvalues) {
            funcion_bc_liberar(fn);
            gc_desenlazar(&c->obj);
            free(c);
            return NULL;
        }
    } else {
        c->upvalues = NULL;
    }
    return c;
}

void closure_retener(Closure *c) { if (c) c->refcount++; }

void closure_liberar(Closure *c) {
    if (!c) return;
    c->refcount--;
    if (c->refcount > 0) return;
    if (c->upvalues) {
        for (int i = 0; i < c->plantilla->n_upvalues; i++) {
            upvalue_liberar(c->upvalues[i]);
        }
        free(c->upvalues);
    }
    funcion_bc_liberar(c->plantilla);
    if (c->clase_definicion) {
        clase_liberar(c->clase_definicion);
    }
    if (c->globales_definicion) {
        dicc_liberar(c->globales_definicion);
    }
    if (c->defaults) {
        for (int i = 0; i < c->plantilla->n_defaults; i++) {
            valor_destruir(&c->defaults[i]);
        }
        free(c->defaults);
    }
    gc_desenlazar(&c->obj);
    free(c);
}

Valor valor_closure(Closure *c) {
    Valor v;
    v.tipo = VAL_FUNCION_BC;
    v.dueno_cadena = false;
    v.como.closure = c;
    return v;
}

Valor valor_plantilla(FuncionBC *fn) {
    Valor v;
    v.tipo = VAL_PLANTILLA_BC;
    v.dueno_cadena = false;
    v.como.plantilla = fn;
    return v;
}
