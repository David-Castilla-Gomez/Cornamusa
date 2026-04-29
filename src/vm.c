#include "vm.h"

#include <limits.h>
#include <stdio.h>

#include "evaluador.h"   /* evaluador_aplicar_binario / unario */
#include "lexer.h"       /* TipoToken */
#include "nativos.h"     /* nativos_registrar para los built-ins */
#include "tommath.h"
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

static inline int linea_actual_frame(const CallFrame *frame) {
    int offset = (int)(frame->ip - frame->chunk->codigo - 1);  /* `op` ya leído */
    if (offset < 0) offset = 0;
    return frame->chunk->lineas[offset];
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
        case OP_ES:              return TT_ES;
        case OP_EN:              return TT_EN;
        default:                 return -1;
    }
}

/* ──────────────────────────────────────────────────────────────────
 * API pública
 * ────────────────────────────────────────────────────────────────── */

void vm_iniciar(VM *vm) {
    vm->tope = vm->pila;
    vm->n_frames = 0;
    vm->globales = dicc_nuevo();
    vm->error.tuvo_error = false;
    vm->error.mensaje[0] = '\0';
    vm->error.linea = 0;
    vm->error.columna = 0;
    /* Registrar built-ins en globales: imprimir, longitud, tipo, rango,
       agregar, quitar, insertar, invertir, ordenar, claves, valores,
       conjunto. Funcionan idénticamente al evaluador tree-walking porque
       las nativas usan EvalError* (no Evaluador*) tras el refactor de S6. */
    if (vm->globales) nativos_registrar_dicc(vm->globales);
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

/* Macros locales para el dispatch loop. `frame` es el CallFrame
 * activo (siempre el último de vm->frames). */
#define LEER_BYTE() (*frame->ip++)

/* Comodín: poner error en `vm` con la línea del opcode actual. */
#define VM_ERROR(...)                                                          \
    do {                                                                       \
        vm->error.tuvo_error = true;                                           \
        vm->error.linea = linea_actual_frame(frame);                           \
        snprintf(vm->error.mensaje, sizeof(vm->error.mensaje), __VA_ARGS__);   \
    } while (0)

ResultadoVM vm_ejecutar(VM *vm, const Chunk *chunk, Valor *resultado_out) {
    vm->tope = vm->pila;
    vm->n_frames = 0;
    vm->error.tuvo_error = false;

    /* Crear el frame top-level. base_pila apunta al inicio de la pila;
       slot 0 lo reservamos como "callee virtual" para mantener la
       convención (incluso aunque no haya función llamada). */
    CallFrame *frame = &vm->frames[vm->n_frames++];
    frame->chunk = chunk;
    frame->ip = chunk->codigo;
    frame->base_pila = vm->pila;

    for (;;) {
        uint8_t opbyte = *frame->ip++;
        OpCode op = (OpCode)opbyte;

        switch (op) {
            case OP_CONST: {
                uint8_t idx = LEER_BYTE();
                empujar(vm, valor_clonar(&frame->chunk->constantes[idx]));
                break;
            }
            case OP_CONST_LARGO: {
                uint32_t b0 = (uint32_t)LEER_BYTE();
                uint32_t b1 = (uint32_t)LEER_BYTE();
                uint32_t b2 = (uint32_t)LEER_BYTE();
                uint32_t idx = b0 | (b1 << 8) | (b2 << 16);
                empujar(vm, valor_clonar(&frame->chunk->constantes[idx]));
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
            case OP_MAYOR: case OP_MAYOR_IGUAL:
            case OP_ES: case OP_EN: {
                int linea = linea_actual_frame(frame);
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
                int linea = linea_actual_frame(frame);
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
                int linea = linea_actual_frame(frame);
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
            case OP_DUP_2: {
                /* Stack: [..., a, b] → [..., a, b, a, b]. */
                Valor a = vm->tope[-2];
                Valor b = vm->tope[-1];
                empujar(vm, valor_clonar(&a));
                empujar(vm, valor_clonar(&b));
                break;
            }

            case OP_RETORNAR: {
                /* Pop el resultado del frame actual. */
                Valor r = sacar(vm);
                /* Pop el CallFrame. */
                vm->n_frames--;
                if (vm->n_frames == 0) {
                    /* Volvemos del top-level: termina la ejecución. */
                    if (resultado_out) {
                        *resultado_out = r;
                    } else {
                        valor_destruir(&r);
                    }
                    return VM_OK;
                }
                /* Liberar todos los slots del frame que termina (callee
                   + parámetros + locales). El llamador ya no los necesita. */
                while (vm->tope > frame->base_pila) {
                    Valor v = *(--vm->tope);
                    valor_destruir(&v);
                }
                /* Empujar el resultado y volver al frame anterior. */
                empujar(vm, r);
                frame = &vm->frames[vm->n_frames - 1];
                break;
            }

            /* ─── Globales ─── */
            case OP_DEFINIR_GLOBAL: {
                /* nombre = constantes[idx] (cadena), valor = tope. */
                uint8_t idx = LEER_BYTE();
                Valor nombre = valor_clonar(&frame->chunk->constantes[idx]);
                Valor valor = sacar(vm);
                if (!dicc_asignar(vm->globales, nombre, valor)) {
                    vm->error.tuvo_error = true;
                    vm->error.linea = linea_actual_frame(frame);
                    snprintf(vm->error.mensaje, sizeof(vm->error.mensaje),
                        "memoria insuficiente al definir global");
                    return VM_ERROR_RUNTIME;
                }
                break;
            }
            case OP_OBTENER_GLOBAL: {
                uint8_t idx = LEER_BYTE();
                const Valor *nombre = &frame->chunk->constantes[idx];
                Valor v;
                if (!dicc_obtener(vm->globales, nombre, &v)) {
                    vm->error.tuvo_error = true;
                    vm->error.linea = linea_actual_frame(frame);
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
                const Valor *nombre = &frame->chunk->constantes[idx];
                if (!dicc_contiene(vm->globales, nombre)) {
                    vm->error.tuvo_error = true;
                    vm->error.linea = linea_actual_frame(frame);
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

            /* ─── Construcción de colecciones ─── */
            case OP_BUILD_LISTA: {
                uint8_t n = LEER_BYTE();
                Lista *l = lista_nueva(n);
                if (!l) {
                    VM_ERROR("memoria insuficiente al construir lista");
                    return VM_ERROR_RUNTIME;
                }
                /* Slots en stack: [..., e0, e1, ..., e(n-1)]. Para
                   preservar el orden los movemos directamente, sin
                   sacar+invertir. */
                Valor *base = vm->tope - n;
                for (int i = 0; i < n; i++) {
                    lista_agregar(l, base[i]);  /* toma posesión */
                }
                vm->tope = base;
                empujar(vm, valor_lista(l));
                break;
            }
            case OP_BUILD_TUPLA: {
                uint8_t n = LEER_BYTE();
                Tupla *t = tupla_nueva(n);
                if (!t) {
                    VM_ERROR("memoria insuficiente al construir tupla");
                    return VM_ERROR_RUNTIME;
                }
                Valor *base = vm->tope - n;
                for (int i = 0; i < n; i++) t->elementos[i] = base[i];
                vm->tope = base;
                empujar(vm, valor_tupla(t));
                break;
            }
            case OP_BUILD_DICC: {
                uint8_t n_pares = LEER_BYTE();
                Diccionario *d = dicc_nuevo();
                if (!d) {
                    VM_ERROR("memoria insuficiente al construir diccionario");
                    return VM_ERROR_RUNTIME;
                }
                Valor *base = vm->tope - n_pares * 2;
                for (int i = 0; i < n_pares; i++) {
                    Valor k = base[i * 2];
                    Valor v = base[i * 2 + 1];
                    if (!valor_es_hashable(&k)) {
                        valor_destruir(&k); valor_destruir(&v);
                        for (int j = i + 1; j < n_pares; j++) {
                            valor_destruir(&base[j * 2]);
                            valor_destruir(&base[j * 2 + 1]);
                        }
                        dicc_liberar(d);
                        vm->tope = base;
                        VM_ERROR("ErrorDeTipo: clave no hashable en literal de diccionario");
                        return VM_ERROR_RUNTIME;
                    }
                    dicc_asignar(d, k, v);
                }
                vm->tope = base;
                empujar(vm, valor_diccionario(d));
                break;
            }
            case OP_BUILD_CONJUNTO: {
                uint8_t n = LEER_BYTE();
                Conjunto *cj = conj_nuevo();
                if (!cj) {
                    VM_ERROR("memoria insuficiente al construir conjunto");
                    return VM_ERROR_RUNTIME;
                }
                Valor *base = vm->tope - n;
                for (int i = 0; i < n; i++) {
                    Valor el = base[i];
                    if (!valor_es_hashable(&el)) {
                        valor_destruir(&el);
                        for (int j = i + 1; j < n; j++) valor_destruir(&base[j]);
                        conj_liberar(cj);
                        vm->tope = base;
                        VM_ERROR("ErrorDeTipo: elemento no hashable en literal de conjunto");
                        return VM_ERROR_RUNTIME;
                    }
                    conj_agregar(cj, el);
                }
                vm->tope = base;
                empujar(vm, valor_conjunto(cj));
                break;
            }

            /* ─── Indexación ─── */
            case OP_INDICE: {
                Valor key = sacar(vm);
                Valor obj = sacar(vm);
                Valor r = valor_nulo();
                /* Lista, tupla, diccionario, cadena. */
                if (obj.tipo == VAL_LISTA) {
                    if (key.tipo != VAL_ENTERO && key.tipo != VAL_BOOLEANO) {
                        VM_ERROR("ErrorDeTipo: indice de lista debe ser entero, no '%s'",
                                 valor_nombre_tipo(&key));
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    Lista *l = obj.como.lista;
                    long i;
                    if (key.tipo == VAL_BOOLEANO) i = key.como.booleano ? 1 : 0;
                    else if (mp_count_bits(key.como.entero) > 62) i = LONG_MAX;
                    else i = (long)mp_get_i64(key.como.entero);
                    if (i < 0) i += l->cuenta;
                    if (i < 0 || i >= l->cuenta) {
                        VM_ERROR("ErrorDeIndice: indice %ld fuera de rango (lista de %d)",
                                 i, l->cuenta);
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    r = valor_clonar(&l->elementos[i]);
                } else if (obj.tipo == VAL_TUPLA) {
                    if (key.tipo != VAL_ENTERO && key.tipo != VAL_BOOLEANO) {
                        VM_ERROR("ErrorDeTipo: indice de tupla debe ser entero");
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    Tupla *t = obj.como.tupla;
                    long i;
                    if (key.tipo == VAL_BOOLEANO) i = key.como.booleano ? 1 : 0;
                    else if (mp_count_bits(key.como.entero) > 62) i = LONG_MAX;
                    else i = (long)mp_get_i64(key.como.entero);
                    if (i < 0) i += t->cuenta;
                    if (i < 0 || i >= t->cuenta) {
                        VM_ERROR("ErrorDeIndice: indice fuera de rango (tupla de %d)",
                                 t->cuenta);
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    r = valor_clonar(&t->elementos[i]);
                } else if (obj.tipo == VAL_DICCIONARIO) {
                    if (!valor_es_hashable(&key)) {
                        VM_ERROR("ErrorDeTipo: '%s' no se puede usar como clave",
                                 valor_nombre_tipo(&key));
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    if (!dicc_obtener(obj.como.dicc, &key, &r)) {
                        char buf[128];
                        valor_a_repr(&key, buf, sizeof(buf));
                        VM_ERROR("ErrorDeClave: %s", buf);
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                } else {
                    VM_ERROR("ErrorDeTipo: '%s' no es indexable",
                             valor_nombre_tipo(&obj));
                    valor_destruir(&key); valor_destruir(&obj);
                    return VM_ERROR_RUNTIME;
                }
                valor_destruir(&key); valor_destruir(&obj);
                empujar(vm, r);
                break;
            }
            case OP_ASIGNAR_INDICE: {
                /* Stack: [..., obj, key, valor]. */
                Valor valor = sacar(vm);
                Valor key = sacar(vm);
                Valor obj = sacar(vm);
                if (obj.tipo == VAL_LISTA) {
                    if (key.tipo != VAL_ENTERO && key.tipo != VAL_BOOLEANO) {
                        VM_ERROR("ErrorDeTipo: indice de lista debe ser entero");
                        valor_destruir(&valor); valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    Lista *l = obj.como.lista;
                    long i;
                    if (key.tipo == VAL_BOOLEANO) i = key.como.booleano ? 1 : 0;
                    else i = (long)mp_get_i64(key.como.entero);
                    if (i < 0) i += l->cuenta;
                    if (i < 0 || i >= l->cuenta) {
                        VM_ERROR("ErrorDeIndice: indice fuera de rango (lista de %d)",
                                 l->cuenta);
                        valor_destruir(&valor); valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    lista_asignar(l, (int)i, valor);
                    valor_destruir(&key); valor_destruir(&obj);
                    empujar(vm, valor_nulo());  /* la sentencia descarta */
                } else if (obj.tipo == VAL_DICCIONARIO) {
                    if (!valor_es_hashable(&key)) {
                        VM_ERROR("ErrorDeTipo: clave no hashable");
                        valor_destruir(&valor); valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    dicc_asignar(obj.como.dicc, key, valor);
                    /* `key` y `valor` transferidos. */
                    valor_destruir(&obj);
                    empujar(vm, valor_nulo());
                } else {
                    VM_ERROR("ErrorDeTipo: '%s' no soporta asignacion por indice",
                             valor_nombre_tipo(&obj));
                    valor_destruir(&valor); valor_destruir(&key); valor_destruir(&obj);
                    return VM_ERROR_RUNTIME;
                }
                break;
            }

            /* ─── Iteradores ─── */
            case OP_ITER_INICIAR: {
                /* Pop iterable, push iterador. */
                Valor it_v = sacar(vm);
                if (!valor_es_iterable(&it_v)) {
                    VM_ERROR("ErrorDeTipo: 'para' no soporta iterar sobre '%s'",
                             valor_nombre_tipo(&it_v));
                    valor_destruir(&it_v);
                    return VM_ERROR_RUNTIME;
                }
                Iterador *iter = iter_nuevo(&it_v);
                valor_destruir(&it_v);
                if (!iter) {
                    VM_ERROR("memoria insuficiente al crear iterador");
                    return VM_ERROR_RUNTIME;
                }
                empujar(vm, valor_iterador(iter));
                break;
            }
            case OP_ITER_SIGUIENTE: {
                /* Operandos: [u8 slot] [u16 offset_fin].
                   El iterador vive en `frame->base_pila[slot]` (un local
                   oculto reservado por el compilador). Si tiene siguiente,
                   push valor. Si no, deja el slot intacto (se libera con
                   el frame al final) y salta `offset_fin` bytes adelante. */
                uint8_t slot = LEER_BYTE();
                uint8_t hi = LEER_BYTE();
                uint8_t lo = LEER_BYTE();
                uint16_t offset = ((uint16_t)hi << 8) | lo;
                Valor *iter_v = &frame->base_pila[slot];
                if (iter_v->tipo != VAL_ITERADOR) {
                    VM_ERROR("estado interno corrupto: OP_ITER_SIGUIENTE sin iterador en slot %u",
                             slot);
                    return VM_ERROR_RUNTIME;
                }
                Valor v;
                if (iter_siguiente(iter_v->como.iterador, &v)) {
                    empujar(vm, v);
                } else {
                    frame->ip += offset;
                }
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

            /* ─── Control de flujo ─── */
            case OP_SALTAR: {
                uint8_t hi = LEER_BYTE();
                uint8_t lo = LEER_BYTE();
                uint16_t offset = ((uint16_t)hi << 8) | lo;
                frame->ip += offset;
                break;
            }
            case OP_SALTAR_SI_FALSO: {
                uint8_t hi = LEER_BYTE();
                uint8_t lo = LEER_BYTE();
                uint16_t offset = ((uint16_t)hi << 8) | lo;
                const Valor *tope = vm->tope - 1;
                if (!valor_es_verdadero(tope)) frame->ip += offset;
                break;
            }
            case OP_BUCLE: {
                uint8_t hi = LEER_BYTE();
                uint8_t lo = LEER_BYTE();
                uint16_t offset = ((uint16_t)hi << 8) | lo;
                frame->ip -= offset;
                break;
            }

            /* ─── Locales: acceso a slots del frame actual ─── */
            case OP_OBTENER_LOCAL: {
                uint8_t slot = LEER_BYTE();
                empujar(vm, valor_clonar(&frame->base_pila[slot]));
                break;
            }
            case OP_ASIGNAR_LOCAL: {
                uint8_t slot = LEER_BYTE();
                /* Pop el valor del tope, lo asignamos al slot. */
                Valor nuevo = sacar(vm);
                valor_destruir(&frame->base_pila[slot]);
                frame->base_pila[slot] = nuevo;
                break;
            }

            /* ─── Llamadas a función ─── */
            case OP_LLAMAR: {
                uint8_t n_args = LEER_BYTE();
                /* La pila tiene: [..., callee, arg1, ..., argN].
                   El callee está en `tope - n_args - 1`. */
                Valor *base_nuevo = vm->tope - n_args - 1;
                Valor callee = *base_nuevo;

                if (callee.tipo == VAL_NATIVA) {
                    /* Las nativas reciben los args sin tomar posesión;
                       el llamador los destruye al volver. */
                    int linea = linea_actual_frame(frame);
                    Valor *args = base_nuevo + 1;
                    Valor r = callee.como.nativa.fn(&vm->error, n_args,
                                                     args, linea, 0);
                    if (vm->error.tuvo_error) {
                        valor_destruir(&r);
                        return VM_ERROR_RUNTIME;
                    }
                    /* Limpiar args y callee del stack, empujar resultado. */
                    for (int i = 0; i < n_args; i++) {
                        Valor v = *(--vm->tope);
                        valor_destruir(&v);
                    }
                    Valor cv = *(--vm->tope);
                    valor_destruir(&cv);
                    empujar(vm, r);
                    break;
                }

                if (callee.tipo == VAL_FUNCION_BC) {
                    FuncionBC *fn = callee.como.funcion_bc;
                    if (n_args != fn->aridad) {
                        VM_ERROR("ErrorDeTipo: %.*s() esperaba %d argumentos, recibio %d",
                                 fn->longitud_nombre, fn->nombre,
                                 fn->aridad, n_args);
                        return VM_ERROR_RUNTIME;
                    }
                    if (vm->n_frames >= VM_FRAMES_MAX) {
                        VM_ERROR("desbordamiento de pila de llamadas (>%d frames)",
                                 VM_FRAMES_MAX);
                        return VM_ERROR_RUNTIME;
                    }
                    /* Crear nuevo frame con base_pila apuntando al callee
                       (que ocupa el slot 0 del frame; los args quedan en
                       slots 1..n_args). */
                    frame = &vm->frames[vm->n_frames++];
                    frame->chunk = &fn->chunk;
                    frame->ip = fn->chunk.codigo;
                    frame->base_pila = base_nuevo;
                    break;
                }

                VM_ERROR("ErrorDeTipo: '%s' no es invocable",
                         valor_nombre_tipo(&callee));
                return VM_ERROR_RUNTIME;
            }
        }
    }
}

#undef LEER_BYTE
#undef VM_ERROR
