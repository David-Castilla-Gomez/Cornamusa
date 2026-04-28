#include "vm.h"

#include <stdio.h>

#include "evaluador.h"   /* evaluador_aplicar_binario / unario */
#include "lexer.h"       /* TipoToken */
#include "valor.h"

/* ──────────────────────────────────────────────────────────────────
 * Helpers de pila. Inline para que el dispatch loop no incurra en
 * coste de llamada en cada operación.
 * ────────────────────────────────────────────────────────────────── */

static inline void empujar(VM *vm, Valor v) {
    /* Capacidad fija: si nos pasamos, error explícito. En las próximas
       sesiones la pila se hará dinámica con frames. */
    if (vm->tope - vm->pila >= VM_PILA_MAX) {
        valor_destruir(&v);
        if (!vm->error.tuvo_error) {
            vm->error.tuvo_error = true;
            snprintf(vm->error.mensaje, sizeof(vm->error.mensaje),
                "Desbordamiento de pila");
        }
        return;
    }
    *vm->tope++ = v;
}

static inline Valor sacar(VM *vm) {
    if (vm->tope == vm->pila) {
        if (!vm->error.tuvo_error) {
            vm->error.tuvo_error = true;
            snprintf(vm->error.mensaje, sizeof(vm->error.mensaje),
                "Pila vacia (bug del compilador)");
        }
        return valor_nulo();
    }
    return *(--vm->tope);
}

static inline int linea_actual(const VM *vm) {
    int offset = (int)(vm->ip - vm->chunk->codigo - 1);  /* `op` ya leído */
    if (offset < 0) offset = 0;
    return vm->chunk->lineas[offset];
}

/* ──────────────────────────────────────────────────────────────────
 * Mapeo OpCode → TipoToken para reusar evaluador_aplicar_binario y
 * evaluador_aplicar_unario.
 * ────────────────────────────────────────────────────────────────── */

static int opcode_a_token_binario(OpCode op) {
    switch (op) {
        case OP_SUMAR:           return TT_MAS;
        case OP_RESTAR:          return TT_MENOS;
        case OP_MULTIPLICAR:     return TT_ASTERISCO;
        case OP_DIVIDIR:         return TT_BARRA;
        case OP_DIVIDIR_ENTERO:  return TT_DOBLE_BARRA;
        case OP_MODULO:          return TT_PORCENTAJE;
        case OP_POTENCIA:        return TT_DOBLE_ASTERISCO;
        case OP_IGUAL:           return TT_IGUAL;
        case OP_DISTINTO:        return TT_DISTINTO;
        case OP_MENOR:           return TT_MENOR;
        case OP_MENOR_IGUAL:     return TT_MENOR_IGUAL;
        case OP_MAYOR:           return TT_MAYOR;
        case OP_MAYOR_IGUAL:     return TT_MAYOR_IGUAL;
        default:                 return -1;
    }
}

/* ──────────────────────────────────────────────────────────────────
 * API pública
 * ────────────────────────────────────────────────────────────────── */

void vm_iniciar(VM *vm) {
    vm->tope = vm->pila;
    vm->chunk = NULL;
    vm->ip = NULL;
    vm->globales = dicc_nuevo();
    vm->error.tuvo_error = false;
    vm->error.mensaje[0] = '\0';
    vm->error.linea = 0;
    vm->error.columna = 0;
}

void vm_destruir(VM *vm) {
    /* Liberar lo que quede en la pila (caso error). */
    while (vm->tope > vm->pila) {
        Valor v = *(--vm->tope);
        valor_destruir(&v);
    }
    if (vm->globales) {
        dicc_liberar(vm->globales);
        vm->globales = NULL;
    }
}

/* Macro local para el dispatch loop: lee un byte y avanza el ip. */
#define LEER_BYTE() (*vm->ip++)

ResultadoVM vm_ejecutar(VM *vm, const Chunk *chunk, Valor *resultado_out) {
    vm->chunk = chunk;
    vm->ip = chunk->codigo;
    vm->tope = vm->pila;
    vm->error.tuvo_error = false;

    for (;;) {
        uint8_t opbyte = *vm->ip++;
        OpCode op = (OpCode)opbyte;

        switch (op) {
            case OP_CONST: {
                uint8_t idx = LEER_BYTE();
                empujar(vm, valor_clonar(&chunk->constantes[idx]));
                break;
            }
            case OP_CONST_LARGO: {
                uint32_t b0 = (uint32_t)LEER_BYTE();
                uint32_t b1 = (uint32_t)LEER_BYTE();
                uint32_t b2 = (uint32_t)LEER_BYTE();
                uint32_t idx = b0 | (b1 << 8) | (b2 << 16);
                empujar(vm, valor_clonar(&chunk->constantes[idx]));
                break;
            }
            case OP_NULO:        empujar(vm, valor_nulo()); break;
            case OP_VERDADERO:   empujar(vm, valor_booleano(true)); break;
            case OP_FALSO:       empujar(vm, valor_booleano(false)); break;

            /* ─── Aritmética y comparaciones binarias ─── */
            case OP_SUMAR: case OP_RESTAR: case OP_MULTIPLICAR:
            case OP_DIVIDIR: case OP_DIVIDIR_ENTERO: case OP_MODULO:
            case OP_POTENCIA:
            case OP_IGUAL: case OP_DISTINTO:
            case OP_MENOR: case OP_MENOR_IGUAL:
            case OP_MAYOR: case OP_MAYOR_IGUAL: {
                int linea = linea_actual(vm);
                Valor b = sacar(vm);
                Valor a = sacar(vm);
                int tt = opcode_a_token_binario(op);
                Valor r = evaluador_aplicar_binario(&vm->error, tt,
                                                      a, b, linea, 0);
                if (vm->error.tuvo_error) {
                    valor_destruir(&r);
                    return VM_ERROR_RUNTIME;
                }
                empujar(vm, r);
                break;
            }

            /* ─── Unarios ─── */
            case OP_NEGAR: {
                int linea = linea_actual(vm);
                Valor v = sacar(vm);
                Valor r = evaluador_aplicar_unario(&vm->error, TT_MENOS, v,
                                                    linea, 0);
                if (vm->error.tuvo_error) {
                    valor_destruir(&r);
                    return VM_ERROR_RUNTIME;
                }
                empujar(vm, r);
                break;
            }
            case OP_NO: {
                int linea = linea_actual(vm);
                Valor v = sacar(vm);
                Valor r = evaluador_aplicar_unario(&vm->error, TT_NO, v,
                                                    linea, 0);
                if (vm->error.tuvo_error) {
                    valor_destruir(&r);
                    return VM_ERROR_RUNTIME;
                }
                empujar(vm, r);
                break;
            }

            case OP_DESCARTAR: {
                Valor v = sacar(vm);
                valor_destruir(&v);
                break;
            }

            case OP_RETORNAR: {
                Valor r = sacar(vm);
                if (resultado_out) {
                    *resultado_out = r;
                } else {
                    valor_destruir(&r);
                }
                return VM_OK;
            }

            /* ─── Globales ─── */
            case OP_DEFINIR_GLOBAL: {
                /* nombre = constantes[idx] (cadena), valor = tope. */
                uint8_t idx = LEER_BYTE();
                Valor nombre = valor_clonar(&vm->chunk->constantes[idx]);
                Valor valor = sacar(vm);
                if (!dicc_asignar(vm->globales, nombre, valor)) {
                    vm->error.tuvo_error = true;
                    vm->error.linea = linea_actual(vm);
                    snprintf(vm->error.mensaje, sizeof(vm->error.mensaje),
                        "memoria insuficiente al definir global");
                    return VM_ERROR_RUNTIME;
                }
                break;
            }
            case OP_OBTENER_GLOBAL: {
                uint8_t idx = LEER_BYTE();
                const Valor *nombre = &vm->chunk->constantes[idx];
                Valor v;
                if (!dicc_obtener(vm->globales, nombre, &v)) {
                    vm->error.tuvo_error = true;
                    vm->error.linea = linea_actual(vm);
                    snprintf(vm->error.mensaje, sizeof(vm->error.mensaje),
                        "ErrorDeNombre: nombre '%.*s' no esta definido",
                        nombre->como.cadena.longitud,
                        nombre->como.cadena.texto);
                    return VM_ERROR_RUNTIME;
                }
                empujar(vm, v);
                break;
            }
            case OP_ASIGNAR_GLOBAL: {
                /* Como OP_DEFINIR_GLOBAL pero requiere que la clave ya
                   exista. Cornamusa no distingue declaración de
                   asignación, así que en la práctica el compilador
                   solo emite OP_DEFINIR_GLOBAL. Esta forma queda para
                   futuras semánticas más estrictas. */
                uint8_t idx = LEER_BYTE();
                const Valor *nombre = &vm->chunk->constantes[idx];
                if (!dicc_contiene(vm->globales, nombre)) {
                    vm->error.tuvo_error = true;
                    vm->error.linea = linea_actual(vm);
                    snprintf(vm->error.mensaje, sizeof(vm->error.mensaje),
                        "ErrorDeNombre: nombre '%.*s' no esta definido",
                        nombre->como.cadena.longitud,
                        nombre->como.cadena.texto);
                    return VM_ERROR_RUNTIME;
                }
                Valor clave_clon = valor_clonar(nombre);
                Valor valor = sacar(vm);
                dicc_asignar(vm->globales, clave_clon, valor);
                break;
            }

            /* ─── Built-in print ─── */
            case OP_IMPRIMIR: {
                uint8_t n = LEER_BYTE();
                /* Args en stack en orden de izq a der; los sacamos en
                   orden inverso, los imprimimos en el orden correcto.
                   El buffer fijo de 256 cabe el máximo (uint8_t). */
                Valor args[256];
                for (int i = n - 1; i >= 0; i--) args[i] = sacar(vm);
                char buffer[1024];
                for (int i = 0; i < n; i++) {
                    if (i > 0) fputc(' ', stdout);
                    valor_a_cadena(&args[i], buffer, sizeof(buffer));
                    fputs(buffer, stdout);
                }
                fputc('\n', stdout);
                fflush(stdout);
                for (int i = 0; i < n; i++) valor_destruir(&args[i]);
                /* `imprimir(...)` es una expresión: empuja nulo. */
                empujar(vm, valor_nulo());
                break;
            }

            /* Opcodes reservados para sesiones siguientes. */
            case OP_SALTAR:
            case OP_SALTAR_SI_FALSO:
            case OP_BUCLE:
            case OP_OBTENER_LOCAL:
            case OP_ASIGNAR_LOCAL:
            case OP_LLAMAR:
                vm->error.tuvo_error = true;
                vm->error.linea = linea_actual(vm);
                snprintf(vm->error.mensaje, sizeof(vm->error.mensaje),
                    "OpCode %s no implementado en esta version",
                    opcode_nombre(op));
                return VM_ERROR_RUNTIME;
        }
    }
}

#undef LEER_BYTE
