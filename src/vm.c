#include "vm.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "compilador.h"
#include "evaluador.h"   /* evaluador_aplicar_binario / unario */
#include "lexer.h"       /* TipoToken */
#include "nativos.h"     /* nativos_registrar para los built-ins */
#include "parser.h"
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

/*
 * Captura un upvalue para la posición `slot` del stack. Si ya hay un
 * upvalue abierto apuntando a esa posición, devuelve el existente (un
 * solo upvalue compartido entre todas las closures que capturan la
 * misma variable). Si no, crea uno nuevo y lo inserta en la lista
 * ordenada por posición decreciente.
 */
static Upvalue *capturar_upvalue(VM *vm, Valor *slot) {
    Upvalue *previo = NULL;
    Upvalue *actual = vm->open_upvalues;
    /* Lista ordenada por posición DESCENDENTE (más arriba en el stack
       primero). Avanzamos mientras `actual` esté por encima de `slot`. */
    while (actual != NULL && actual->posicion > slot) {
        previo = actual;
        actual = actual->siguiente;
    }
    if (actual != NULL && actual->posicion == slot) {
        upvalue_retener(actual);
        return actual;
    }
    Upvalue *nuevo = upvalue_nuevo(slot);
    if (!nuevo) return NULL;
    nuevo->siguiente = actual;
    if (previo == NULL) vm->open_upvalues = nuevo;
    else previo->siguiente = nuevo;
    upvalue_retener(nuevo);   /* uno para el llamador, uno se queda en lista */
    return nuevo;
}

/*
 * Cierra todos los upvalues abiertos cuya posición esté en o por
 * encima de `desde` (es decir, dentro de la región del stack que
 * está a punto de quedar inválida). Cerrar significa: copiar el valor
 * a `cerrado` y reapuntar `posicion` a esa copia.
 */
static void cerrar_upvalues_hasta(VM *vm, Valor *desde) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->posicion >= desde) {
        Upvalue *u = vm->open_upvalues;
        /* Transferir ownership del slot al upvalue: copiamos la
           struct y dejamos el slot original con `nulo` para que el
           cleanup posterior del frame no haga doble-free. */
        u->cerrado = *u->posicion;
        *u->posicion = valor_nulo();
        u->posicion = &u->cerrado;
        vm->open_upvalues = u->siguiente;
        u->siguiente = NULL;
        upvalue_liberar(u);
    }
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

/* Forward decl del adapter; definición tras vm_iniciar. */
static void gc_marcar_raices_adapter(void *ctx);

/* ──────────────────────────────────────────────────────────────────
 * Carga de módulos (Fase 9)
 *
 * `cargar_modulo_desde_archivo` busca y compila un archivo .cor por
 * nombre de módulo. Devuelve un Chunk recién alocado en heap, o NULL
 * si falla. El cliente toma posesión del Chunk y debe destruirlo +
 * free cuando termine de usarlo (lo guarda en CallFrame.chunk_modulo).
 *
 * Estrategia de búsqueda (en orden):
 *   1. ./{nombre}.cor (cwd)
 *   2. stdlib/{nombre}.cor (relativo a cwd)
 *
 * Limitaciones v0.9.0:
 *   - Sin soporte para subdirectorios (`mat.geometria` → archivo
 *     `mat/geometria.cor`).
 *   - Sin variable de entorno CORNAMUSA_PATH.
 *   - Sin error detallado si el archivo existe pero falla parsing —
 *     el caller recibe NULL y emite un VM_ERROR genérico.
 * ────────────────────────────────────────────────────────────────── */

/* Lee un archivo entero a un buffer alocado con malloc. Devuelve
   NULL si no existe o no se puede leer. *len_out recibe la longitud. */
static char *leer_archivo_completo(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t leidos = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (leidos != (size_t)sz) { free(buf); return NULL; }
    buf[sz] = '\0';
    if (len_out) *len_out = (size_t)sz;
    return buf;
}

/* Compila el contenido del módulo. Devuelve un Chunk* alocado con malloc o
   NULL si falla. El caller es dueño del Chunk + de su contenido y debe
   destruirlo via chunk_destruir + free.
   `fuente` y `nombre_archivo` deben sobrevivir al menos hasta el fin de
   parser_parsear_programa (no a la vida del Chunk — las constantes ya
   se duplicaron). */
static Chunk *compilar_fuente_a_chunk(const char *fuente,
                                        const char *nombre_archivo) {
    Lexer l;
    lexer_iniciar(&l, fuente, nombre_archivo);

    Arena arena;
    arena_iniciar(&arena, 16384);

    Parser p;
    parser_iniciar(&p, &l, &arena, fuente, nombre_archivo);

    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (!prog || p.tuvo_error) {
        arena_destruir(&arena);
        return NULL;
    }

    Chunk *ch = (Chunk *)malloc(sizeof(Chunk));
    if (!ch) { arena_destruir(&arena); return NULL; }
    chunk_iniciar(ch);

    Compilador c;
    compilador_iniciar(&c, ch);
    if (!compilador_compilar_programa(&c, prog, n)) {
        chunk_destruir(ch);
        free(ch);
        arena_destruir(&arena);
        return NULL;
    }
    arena_destruir(&arena);
    return ch;
}

/* Busca y carga el módulo `nombre`. Devuelve Chunk* (con malloc'd) o NULL. */
static Chunk *cargar_modulo_desde_archivo(const char *nombre, int len_nombre) {
    char path[512];
    char *fuente = NULL;
    size_t flen = 0;

    /* Intento 1: ./{nombre}.cor */
    snprintf(path, sizeof(path), "%.*s.cor", len_nombre, nombre);
    fuente = leer_archivo_completo(path, &flen);

    /* Intento 2: stdlib/{nombre}.cor */
    if (!fuente) {
        snprintf(path, sizeof(path), "stdlib/%.*s.cor", len_nombre, nombre);
        fuente = leer_archivo_completo(path, &flen);
    }

    if (!fuente) return NULL;

    Chunk *ch = compilar_fuente_a_chunk(fuente, path);
    free(fuente);
    return ch;
}

/* ──────────────────────────────────────────────────────────────────
 * API pública
 * ────────────────────────────────────────────────────────────────── */

void vm_iniciar(VM *vm) {
    vm->tope = vm->pila;
    vm->n_frames = 0;
    vm->open_upvalues = NULL;
    vm->n_handlers = 0;
    vm->error.tuvo_error = false;
    vm->error.mensaje[0] = '\0';
    vm->error.linea = 0;
    vm->error.columna = 0;
    /*
     * Fase 7: inicializar el GC e instalarlo como memoria global
     * antes de crear el diccionario de globales (que ya pasa por
     * gc_alocar). El callback se registra aquí, pero `gc_habilitado`
     * queda en false: durante la fase de compilación, el cliente
     * crea heap-objects referenciados solo desde C-locals (FuncionBC,
     * plantillas, etc.) que aún no son alcanzables desde las raíces
     * de la VM; si el GC triggerara, los barrería. El flag se activa
     * al entrar a `vm_ejecutar` y se desactiva al salir.
     */
    memoria_iniciar(&vm->memoria);
    gc_instalar(&vm->memoria);
    gc_set_marcador_raices(&vm->memoria, gc_marcar_raices_adapter, vm);
#ifdef CORNAMUSA_GC_STRESS
    vm->memoria.gc_stress = true;
#endif

    vm->globales = dicc_nuevo();
    vm->cache_modulos = dicc_nuevo();
    /* Registrar built-ins en globales: imprimir, longitud, tipo, rango,
       agregar, quitar, insertar, invertir, ordenar, claves, valores,
       conjunto. */
    if (vm->globales) nativos_registrar_dicc(vm->globales);
}

/* Adapter para FnMarcarRaices (firma `void(void *ctx)`). */
static void gc_marcar_raices_adapter(void *ctx) {
    gc_marcar_raices((VM *)ctx);
}

void gc_marcar_raices(VM *vm) {
    if (!vm) return;
    /* Stack: cada slot vivo. */
    for (Valor *p = vm->pila; p < vm->tope; p++) {
        gc_marcar_valor(p);
    }
    /* Globales (Diccionario): la propia tabla y todos sus pares. */
    if (vm->globales) {
        gc_marcar_objeto(&vm->globales->obj);
    }
    /* Cache de módulos cargados: idem. */
    if (vm->cache_modulos) {
        gc_marcar_objeto(&vm->cache_modulos->obj);
    }
    /* Frames pueden tener globales_pre_modulo (durante carga de módulo)
       y modulo_en_carga — ambos son raíces. */
    for (int i = 0; i < vm->n_frames; i++) {
        if (vm->frames[i].globales_pre_modulo) {
            gc_marcar_objeto(&vm->frames[i].globales_pre_modulo->obj);
        }
        if (vm->frames[i].modulo_en_carga) {
            gc_marcar_objeto(&vm->frames[i].modulo_en_carga->obj);
        }
    }
    /* Frames: cada uno tiene una closure (NULL en top-level). Marcar
       también los `constantes` del chunk activo: el frame top-level
       no tiene closure pero sí un chunk con plantillas y cadenas
       dueñas que deben sobrevivir al GC. Para frames con closure,
       marcar el closure ya cubre la plantilla y sus constantes via
       propagación; pero marcar las constantes del chunk explícitamente
       es idempotente y no añade coste apreciable. */
    for (int i = 0; i < vm->n_frames; i++) {
        Closure *c = vm->frames[i].closure;
        if (c) gc_marcar_objeto(&c->obj);
        const Chunk *ch = vm->frames[i].chunk;
        if (ch) {
            for (int j = 0; j < ch->constantes_cuenta; j++) {
                gc_marcar_valor(&ch->constantes[j]);
            }
        }
    }
    /* Open upvalues: linked list. Aunque sus posiciones están en stack
       y se marcaron arriba, el propio Upvalue es heap-rastreado. */
    for (Upvalue *u = vm->open_upvalues; u != NULL; u = u->siguiente) {
        gc_marcar_objeto(&u->obj);
    }
    /* HandlerFrame: solo guardan ip + offsets — sin Valores propios. */
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
    if (vm->cache_modulos) {
        dicc_liberar(vm->cache_modulos);
        vm->cache_modulos = NULL;
    }
    /*
     * Fase 7 S1: barrer cualquier objeto que el refcount no haya
     * liberado (ciclos), des-instalar la memoria global y destruir.
     * En S4 con sweep activo, esto se reemplazará por una
     * recolección final + memoria_destruir.
     */
    memoria_destruir(&vm->memoria);
    gc_desinstalar();
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

/* Dispatch interno; la wrapper pública vm_ejecutar gestiona el flag
   gc_habilitado en cualquier path de salida. */
static ResultadoVM vm_ejecutar_dispatch(VM *vm, const Chunk *chunk,
                                          Valor *resultado_out) {
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
    frame->closure = NULL;
    frame->es_constructor = false;
    frame->modulo_en_carga = NULL;
    frame->globales_pre_modulo = NULL;
    frame->chunk_modulo = NULL;
    frame->globales_pre_llamada = NULL;

    /* `gc_habilitado` lo gestiona el wrapper público vm_ejecutar. */

    for (;;) {
        /*
         * v0.8.1: trigger del GC en frontera de opcode (deferred).
         * `gc_alocar` set `trigger_pendiente` cuando detecta que el
         * GC debería correr; aquí, con el stack en estado consistente
         * entre opcodes, lo ejecutamos. La protección `recolectando`
         * bloquea recursión si gc_recolectar internamente alocara.
         */
        Memoria *mem = &vm->memoria;
        if (mem->trigger_pendiente && mem->gc_habilitado &&
            !mem->recolectando) {
            mem->recolectando = true;
            gc_recolectar(mem, mem->fn_marcar_raices, mem->contexto_raices);
            mem->trigger_pendiente = false;
            mem->recolectando = false;
            /* Ajustar umbral para el siguiente ciclo (estilo Lua/clox):
               doble del uso actual con un mínimo razonable. */
            size_t nuevo_umbral = mem->total_alocado * 2;
            if (nuevo_umbral < (1024 * 1024)) nuevo_umbral = 1024 * 1024;
            mem->umbral_gc = nuevo_umbral;
        }

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
                /* Si este frame ejecutó __iniciar__, descartamos el
                   valor de retorno y devolvemos la instancia (slot 1
                   = receptor) en su lugar. Coincide con la semántica
                   Python: el constructor no decide el retorno. */
                if (frame->es_constructor) {
                    valor_destruir(&r);
                    r = valor_clonar(&frame->base_pila[1]);
                }
                /* v0.9.0: si la llamada cambió globales (función de
                   módulo invocada desde otro scope), restaurar antes
                   de procesar el módulo en carga. */
                if (frame->globales_pre_llamada != NULL) {
                    vm->globales = frame->globales_pre_llamada;
                }
                /* Si este frame fue creado por OP_IMPORTAR, finalizar el
                   módulo: capturar `vm->globales` (el dicc del módulo
                   poblado durante la ejecución) en `mod->atributos`,
                   restaurar el dicc principal y registrar el módulo
                   como global del importador. */
                if (frame->modulo_en_carga) {
                    Modulo *mod = frame->modulo_en_carga;
                    /* Transferir vm->globales al módulo. */
                    if (mod->atributos) dicc_liberar(mod->atributos);
                    mod->atributos = vm->globales;
                    /* Restaurar el dicc principal. */
                    vm->globales = frame->globales_pre_modulo;
                    /* Liberar el chunk del módulo (lo necesitábamos
                       solo para la ejecución; las constantes que
                       sobreviven están en globales/atributos). */
                    if (frame->chunk_modulo) {
                        chunk_destruir(frame->chunk_modulo);
                        free(frame->chunk_modulo);
                    }
                    /* Registrar global `nombre = <modulo>` y cachear. */
                    Valor clave_global = valor_cadena_duplicar(
                        mod->nombre, mod->longitud_nombre);
                    Valor val_global = valor_modulo(mod);
                    modulo_retener(mod);   /* uno para globales, uno para cache */
                    Valor val_cache = valor_modulo(mod);
                    Valor clave_cache = valor_cadena_duplicar(
                        mod->nombre, mod->longitud_nombre);
                    dicc_asignar(vm->globales, clave_global, val_global);
                    dicc_asignar(vm->cache_modulos, clave_cache, val_cache);
                    /* El valor de retorno (r, típicamente nulo) se
                       descarta — `importar X` es una sentencia, no
                       una expresión, y no produce valor. */
                    valor_destruir(&r);
                    r = valor_nulo();
                }
                /* Antes de descartar el frame, cerrar todos los
                   upvalues abiertos que apunten a slots de este frame
                   (su contenido se copia al heap). */
                cerrar_upvalues_hasta(vm, frame->base_pila);
                /* Pop el CallFrame. */
                vm->n_frames--;
                if (vm->n_frames == 0) {
                    if (resultado_out) {
                        *resultado_out = r;
                    } else {
                        valor_destruir(&r);
                    }
                    return VM_OK;
                }
                /* Liberar todos los slots del frame que termina. */
                while (vm->tope > frame->base_pila) {
                    Valor v = *(--vm->tope);
                    valor_destruir(&v);
                }
                empujar(vm, r);
                frame = &vm->frames[vm->n_frames - 1];
                break;
            }

            /* ─── Módulos (Fase 9) ─── */
            case OP_IMPORTAR: {
                uint8_t name_idx = LEER_BYTE();
                const Valor *nombre = &frame->chunk->constantes[name_idx];
                if (nombre->tipo != VAL_CADENA) {
                    VM_ERROR("estado interno corrupto: nombre de modulo no es cadena");
                    return VM_ERROR_RUNTIME;
                }
                /* 1. Cache hit: solo asignar la global. */
                Valor cached;
                if (dicc_obtener(vm->cache_modulos, nombre, &cached)) {
                    Valor clave = valor_clonar(nombre);
                    dicc_asignar(vm->globales, clave, cached);
                    break;
                }
                /* 2. Cargar archivo y compilar. */
                Chunk *ch_mod = cargar_modulo_desde_archivo(
                    nombre->como.cadena.texto, nombre->como.cadena.longitud);
                if (!ch_mod) {
                    VM_ERROR("ErrorDeImportacion: no se pudo cargar el modulo '%.*s' (archivo no encontrado o invalido)",
                             nombre->como.cadena.longitud, nombre->como.cadena.texto);
                    return VM_ERROR_RUNTIME;
                }
                /* 3. Crear el Modulo + nuevo dicc de globales para él. */
                Modulo *mod = modulo_nuevo(nombre->como.cadena.texto,
                                             nombre->como.cadena.longitud);
                if (!mod) {
                    chunk_destruir(ch_mod); free(ch_mod);
                    VM_ERROR("memoria insuficiente al crear modulo");
                    return VM_ERROR_RUNTIME;
                }
                /* Sustituir vm->globales por el dicc del módulo
                   (poblado por las nativas inicialmente para que el
                   código del módulo pueda usar `imprimir`, etc.). */
                Diccionario *globales_modulo = dicc_nuevo();
                if (!globales_modulo) {
                    modulo_liberar(mod);
                    chunk_destruir(ch_mod); free(ch_mod);
                    VM_ERROR("memoria insuficiente al crear globales del modulo");
                    return VM_ERROR_RUNTIME;
                }
                nativos_registrar_dicc(globales_modulo);
                /* 4. Push frame del módulo. */
                if (vm->n_frames >= VM_FRAMES_MAX) {
                    dicc_liberar(globales_modulo);
                    modulo_liberar(mod);
                    chunk_destruir(ch_mod); free(ch_mod);
                    VM_ERROR("desbordamiento de pila de llamadas");
                    return VM_ERROR_RUNTIME;
                }
                CallFrame *fr_mod = &vm->frames[vm->n_frames++];
                fr_mod->chunk = ch_mod;
                fr_mod->ip = ch_mod->codigo;
                fr_mod->base_pila = vm->tope;
                fr_mod->closure = NULL;
                fr_mod->es_constructor = false;
                fr_mod->modulo_en_carga = mod;
                fr_mod->globales_pre_modulo = vm->globales;
                fr_mod->chunk_modulo = ch_mod;
                fr_mod->globales_pre_llamada = NULL;
                /* Cambiar a las globales del módulo. */
                vm->globales = globales_modulo;
                frame = fr_mod;
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

            /* ─── Slicing ─── */
            case OP_REBANADA: {
                /* Stack: [..., obj, inicio, fin, paso]. nulo = default. */
                Valor paso_v   = sacar(vm);
                Valor fin_v    = sacar(vm);
                Valor inicio_v = sacar(vm);
                Valor obj      = sacar(vm);

                if (obj.tipo != VAL_LISTA) {
                    VM_ERROR("ErrorDeTipo: '%s' no soporta slicing en bytecode v0.6.2",
                             valor_nombre_tipo(&obj));
                    valor_destruir(&obj); valor_destruir(&inicio_v);
                    valor_destruir(&fin_v); valor_destruir(&paso_v);
                    return VM_ERROR_RUNTIME;
                }

                long paso = 1;
                if (paso_v.tipo != VAL_NULO) {
                    if (paso_v.tipo != VAL_ENTERO && paso_v.tipo != VAL_BOOLEANO) {
                        VM_ERROR("ErrorDeTipo: paso de rebanada debe ser entero");
                        valor_destruir(&obj); valor_destruir(&inicio_v);
                        valor_destruir(&fin_v); valor_destruir(&paso_v);
                        return VM_ERROR_RUNTIME;
                    }
                    if (paso_v.tipo == VAL_BOOLEANO) {
                        paso = paso_v.como.booleano ? 1 : 0;
                    } else {
                        paso = (long)mp_get_i64(paso_v.como.entero);
                    }
                    if (paso == 0) {
                        VM_ERROR("ErrorDeValor: el paso de una rebanada no puede ser 0");
                        valor_destruir(&obj); valor_destruir(&inicio_v);
                        valor_destruir(&fin_v); valor_destruir(&paso_v);
                        return VM_ERROR_RUNTIME;
                    }
                }

                int total = obj.como.lista->cuenta;

                long inicio;
                if (inicio_v.tipo == VAL_NULO) {
                    inicio = (paso > 0) ? 0 : total - 1;
                } else if (inicio_v.tipo == VAL_ENTERO || inicio_v.tipo == VAL_BOOLEANO) {
                    inicio = (inicio_v.tipo == VAL_BOOLEANO)
                                ? (inicio_v.como.booleano ? 1 : 0)
                                : (long)mp_get_i64(inicio_v.como.entero);
                    if (inicio < 0) inicio += total;
                } else {
                    VM_ERROR("ErrorDeTipo: inicio de rebanada debe ser entero");
                    valor_destruir(&obj); valor_destruir(&inicio_v);
                    valor_destruir(&fin_v); valor_destruir(&paso_v);
                    return VM_ERROR_RUNTIME;
                }

                long fin;
                if (fin_v.tipo == VAL_NULO) {
                    fin = (paso > 0) ? total : -1;
                } else if (fin_v.tipo == VAL_ENTERO || fin_v.tipo == VAL_BOOLEANO) {
                    fin = (fin_v.tipo == VAL_BOOLEANO)
                                ? (fin_v.como.booleano ? 1 : 0)
                                : (long)mp_get_i64(fin_v.como.entero);
                    if (fin < 0) fin += total;
                } else {
                    VM_ERROR("ErrorDeTipo: fin de rebanada debe ser entero");
                    valor_destruir(&obj); valor_destruir(&inicio_v);
                    valor_destruir(&fin_v); valor_destruir(&paso_v);
                    return VM_ERROR_RUNTIME;
                }

                /* Clamp silencioso (semántica Python). */
                if (paso > 0) {
                    if (inicio < 0) inicio = 0;
                    if (inicio > total) inicio = total;
                    if (fin < 0) fin = 0;
                    if (fin > total) fin = total;
                } else {
                    if (inicio < 0) inicio = -1;
                    if (inicio >= total) inicio = total - 1;
                    if (fin < -1) fin = -1;
                    if (fin >= total) fin = total - 1;
                }

                Lista *resultado = lista_nueva(0);
                if (!resultado) {
                    VM_ERROR("memoria insuficiente al rebanar");
                    valor_destruir(&obj); valor_destruir(&inicio_v);
                    valor_destruir(&fin_v); valor_destruir(&paso_v);
                    return VM_ERROR_RUNTIME;
                }
                Lista *src = obj.como.lista;
                if (paso > 0) {
                    for (long i = inicio; i < fin; i += paso) {
                        lista_agregar(resultado, valor_clonar(&src->elementos[i]));
                    }
                } else {
                    for (long i = inicio; i > fin; i += paso) {
                        lista_agregar(resultado, valor_clonar(&src->elementos[i]));
                    }
                }
                valor_destruir(&obj); valor_destruir(&inicio_v);
                valor_destruir(&fin_v); valor_destruir(&paso_v);
                empujar(vm, valor_lista(resultado));
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
                Valor nuevo = sacar(vm);
                Valor *destino = &frame->base_pila[slot];
                /*
                 * v0.8.3: detectar aliasing. Si el slot del local
                 * coincide con el slot que acabamos de pop (común
                 * cuando se asigna un valor recién pushed a un local
                 * en la posición exacta del pop), `valor_destruir`
                 * sobre `destino` liberaría el Excepcion/Lista/...
                 * que `nuevo` aún apunta — use-after-free. En ese
                 * caso, `nuevo` ya tiene la struct correcta; solo
                 * reescribimos sin destruir.
                 */
                if (destino == vm->tope) {
                    *destino = nuevo;
                } else {
                    valor_destruir(destino);
                    *destino = nuevo;
                }
                break;
            }

            /* ─── Excepciones (v0.6.3) ─── */
            case OP_INTENTAR_INICIAR: {
                /* Push un handler con frame_idx, tope_offset, n_open_upvalues
                   actuales y el ip del handler (calculado del offset). */
                uint8_t hi = LEER_BYTE();
                uint8_t lo = LEER_BYTE();
                uint16_t offset = ((uint16_t)hi << 8) | lo;
                if (vm->n_handlers >= VM_HANDLERS_MAX) {
                    VM_ERROR("desbordamiento de pila de handlers (>%d)",
                             VM_HANDLERS_MAX);
                    return VM_ERROR_RUNTIME;
                }
                HandlerFrame *h = &vm->handlers[vm->n_handlers++];
                h->frame_idx = vm->n_frames;
                h->tope_offset = (int)(vm->tope - vm->pila);
                /* Contar open upvalues (linked list). */
                int n_uv = 0;
                for (Upvalue *u = vm->open_upvalues; u != NULL; u = u->siguiente) n_uv++;
                h->n_open_upvalues = n_uv;
                h->ip_handler = frame->ip + offset;
                break;
            }
            case OP_INTENTAR_FIN: {
                /* Salida limpia del intentar: pop el handler. */
                if (vm->n_handlers == 0) {
                    VM_ERROR("OP_INTENTAR_FIN sin handler activo");
                    return VM_ERROR_RUNTIME;
                }
                vm->n_handlers--;
                break;
            }
            case OP_LANZAR: {
                /* Pop la excepción del tope. Convierte cadena a Excepcion
                   genérica si hace falta. Si no es VAL_EXCEPCION ni
                   VAL_CADENA → error de tipo. */
                Valor exc_v = sacar(vm);
                if (exc_v.tipo == VAL_CADENA) {
                    Excepcion *e = excepcion_nueva("Excepcion", 9,
                        exc_v.como.cadena.texto, exc_v.como.cadena.longitud);
                    valor_destruir(&exc_v);
                    if (!e) { VM_ERROR("memoria insuficiente"); return VM_ERROR_RUNTIME; }
                    exc_v = valor_excepcion(e);
                }
                if (exc_v.tipo != VAL_EXCEPCION) {
                    VM_ERROR("ErrorDeTipo: solo se pueden lanzar excepciones, no '%s'",
                             valor_nombre_tipo(&exc_v));
                    valor_destruir(&exc_v);
                    return VM_ERROR_RUNTIME;
                }
                if (vm->n_handlers == 0) {
                    /* Excepción sin atrapar: produce error en el VM
                       con clase + mensaje. */
                    const Excepcion *ex = exc_v.como.excepcion;
                    vm->error.tuvo_error = true;
                    vm->error.linea = linea_actual_frame(frame);
                    snprintf(vm->error.mensaje, sizeof(vm->error.mensaje),
                        "%.*s: %.*s",
                        ex->longitud_clase, ex->clase,
                        ex->longitud_mensaje, ex->mensaje);
                    valor_destruir(&exc_v);
                    return VM_ERROR_RUNTIME;
                }
                /* Pop el handler top y unwind. */
                HandlerFrame h = vm->handlers[--vm->n_handlers];
                /* Cerrar upvalues que estén por encima del handler tope. */
                cerrar_upvalues_hasta(vm, vm->pila + h.tope_offset);
                /* Descartar slots del stack hasta volver al nivel del
                   handler. */
                while (vm->tope > vm->pila + h.tope_offset) {
                    Valor v = *(--vm->tope);
                    valor_destruir(&v);
                }
                /* Pop frames hasta el del handler. */
                while (vm->n_frames > h.frame_idx) {
                    vm->n_frames--;
                }
                /* Empujar la excepción para que el handler la consuma. */
                empujar(vm, exc_v);
                /* Saltar al handler. */
                frame = &vm->frames[vm->n_frames - 1];
                frame->ip = h.ip_handler;
                break;
            }
            case OP_COMPROBAR_TIPO_EXC: {
                /*
                 * v0.8.3: peek la excepción top, compara su `clase`
                 * (cadena del nombre del tipo de excepción) con la
                 * cadena en constantes[idx]. Empuja un bool sin descartar
                 * la excepción para que el atrapar pueda re-lanzar si
                 * no coincide.
                 */
                uint8_t idx = LEER_BYTE();
                const Valor *esperado = &frame->chunk->constantes[idx];
                if (vm->tope == vm->pila || vm->tope[-1].tipo != VAL_EXCEPCION) {
                    VM_ERROR("estado interno corrupto: OP_COMPROBAR_TIPO_EXC sin excepcion en stack");
                    return VM_ERROR_RUNTIME;
                }
                if (esperado->tipo != VAL_CADENA) {
                    VM_ERROR("estado interno corrupto: tipo de excepcion no es cadena");
                    return VM_ERROR_RUNTIME;
                }
                const Excepcion *e = vm->tope[-1].como.excepcion;
                bool igual =
                    e->longitud_clase == esperado->como.cadena.longitud
                    && memcmp(e->clase, esperado->como.cadena.texto,
                              (size_t)e->longitud_clase) == 0;
                /* Caso especial: el atrapador puede usar "Excepcion"
                   como tipo genérico (Excepcion atrapa cualquier
                   excepción, igual que `except Exception` en Python). */
                if (!igual && esperado->como.cadena.longitud == 9
                    && memcmp(esperado->como.cadena.texto, "Excepcion", 9) == 0) {
                    igual = true;
                }
                empujar(vm, valor_booleano(igual));
                break;
            }

            /* ─── Clases / atributos (Fase 8 v0.7.0) ─── */
            case OP_CLASE: {
                /* Crea una Clase con el nombre indicado en la constante. */
                uint8_t idx = LEER_BYTE();
                const Valor *nombre = &frame->chunk->constantes[idx];
                if (nombre->tipo != VAL_CADENA) {
                    VM_ERROR("OP_CLASE sin nombre cadena en constante");
                    return VM_ERROR_RUNTIME;
                }
                Clase *cl = clase_nueva(nombre->como.cadena.texto,
                                         nombre->como.cadena.longitud);
                if (!cl) {
                    VM_ERROR("memoria insuficiente al crear clase");
                    return VM_ERROR_RUNTIME;
                }
                empujar(vm, valor_clase(cl));
                break;
            }
            case OP_OBTENER_ATRIBUTO: {
                uint8_t idx = LEER_BYTE();
                const Valor *nombre = &frame->chunk->constantes[idx];
                Valor obj = sacar(vm);
                /* Módulos: lookup en sus atributos. */
                if (obj.tipo == VAL_MODULO) {
                    Valor v;
                    if (!dicc_obtener(obj.como.modulo->atributos, nombre, &v)) {
                        VM_ERROR("ErrorDeAtributo: el modulo '%.*s' no tiene atributo '%.*s'",
                                 obj.como.modulo->longitud_nombre,
                                 obj.como.modulo->nombre,
                                 nombre->como.cadena.longitud,
                                 nombre->como.cadena.texto);
                        valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    valor_destruir(&obj);
                    empujar(vm, v);
                    break;
                }
                if (obj.tipo != VAL_INSTANCIA) {
                    VM_ERROR("ErrorDeTipo: '%s' no tiene atributos accesibles",
                             valor_nombre_tipo(&obj));
                    valor_destruir(&obj);
                    return VM_ERROR_RUNTIME;
                }
                /* Lookup: primero atributos de instancia (overrides),
                   después métodos de la clase (creando MetodoLigado). */
                Valor v;
                if (dicc_obtener(obj.como.instancia->atributos, nombre, &v)) {
                    valor_destruir(&obj);
                    empujar(vm, v);
                    break;
                }
                Valor met_v;
                if (dicc_obtener(obj.como.instancia->clase->metodos,
                                  nombre, &met_v)) {
                    if (met_v.tipo != VAL_FUNCION_BC) {
                        valor_destruir(&met_v); valor_destruir(&obj);
                        VM_ERROR("estado interno corrupto: metodo no es closure");
                        return VM_ERROR_RUNTIME;
                    }
                    MetodoLigado *bm = metodo_ligado_nuevo(&obj,
                        met_v.como.closure);
                    valor_destruir(&met_v);
                    valor_destruir(&obj);
                    if (!bm) {
                        VM_ERROR("memoria insuficiente al ligar metodo");
                        return VM_ERROR_RUNTIME;
                    }
                    empujar(vm, valor_metodo_ligado(bm));
                    break;
                }
                VM_ERROR("ErrorDeAtributo: instancia de '%.*s' no tiene atributo '%.*s'",
                         obj.como.instancia->clase->longitud_nombre,
                         obj.como.instancia->clase->nombre,
                         nombre->como.cadena.longitud,
                         nombre->como.cadena.texto);
                valor_destruir(&obj);
                return VM_ERROR_RUNTIME;
            }
            case OP_ASIGNAR_ATRIBUTO: {
                uint8_t idx = LEER_BYTE();
                const Valor *nombre = &frame->chunk->constantes[idx];
                /* Stack: [..., obj, valor]. */
                Valor valor = sacar(vm);
                Valor obj = sacar(vm);
                if (obj.tipo != VAL_INSTANCIA) {
                    VM_ERROR("ErrorDeTipo: '%s' no admite asignacion de atributos",
                             valor_nombre_tipo(&obj));
                    valor_destruir(&valor); valor_destruir(&obj);
                    return VM_ERROR_RUNTIME;
                }
                Valor clave_clon = valor_clonar(nombre);
                if (!dicc_asignar(obj.como.instancia->atributos,
                                   clave_clon, valor)) {
                    VM_ERROR("memoria insuficiente al asignar atributo");
                    valor_destruir(&obj);
                    return VM_ERROR_RUNTIME;
                }
                valor_destruir(&obj);
                empujar(vm, valor_nulo());  /* la sentencia descarta */
                break;
            }
            case OP_METODO: {
                /* Stack: [..., clase, closure]. Pop closure y guardarla
                   en clase.metodos[name]. La clase queda en el tope.
                   v0.8.2: además set closure->clase_definicion = clase
                   (con retención) para que `super` multinivel resuelva
                   correctamente desde dentro de este método. */
                uint8_t idx = LEER_BYTE();
                const Valor *nombre = &frame->chunk->constantes[idx];
                Valor closure = sacar(vm);
                if (vm->tope == vm->pila || vm->tope[-1].tipo != VAL_CLASE) {
                    valor_destruir(&closure);
                    VM_ERROR("estado interno corrupto: OP_METODO sin clase en stack");
                    return VM_ERROR_RUNTIME;
                }
                Clase *cl = vm->tope[-1].como.clase;
                /* Set clase_definicion. Si OP_HEREDAR copió este closure
                   desde un padre, ya tendrá clase_definicion=Padre — no
                   sobreescribimos en ese caso (super seguiría apuntando
                   al abuelo). Pero OP_METODO se invoca específicamente
                   cuando la clase actual (cl) define o redefine el
                   método, así que este es el lugar correcto para
                   actualizar. */
                if (closure.tipo == VAL_FUNCION_BC && closure.como.closure) {
                    Closure *cl_obj = closure.como.closure;
                    if (cl_obj->clase_definicion) {
                        clase_liberar(cl_obj->clase_definicion);
                    }
                    clase_retener(cl);
                    cl_obj->clase_definicion = cl;
                }
                Valor clave_clon = valor_clonar(nombre);
                if (!dicc_asignar(cl->metodos, clave_clon, closure)) {
                    VM_ERROR("memoria insuficiente al registrar metodo");
                    return VM_ERROR_RUNTIME;
                }
                break;
            }
            case OP_SUPER_INVOCAR: {
                /*
                 * Stack: [..., yo, arg1, ..., argN].
                 *
                 * v0.8.2: resuelve `clase_definicion.superclase.metodos[name]`
                 * (no `yo.clase.superclase`). Esto hace que la búsqueda
                 * sea correcta para herencia multinivel: dentro de un
                 * método declarado en Hijo, super.X busca en Padre,
                 * incluso si yo es de Nieto. La clase definicional la
                 * guarda OP_METODO en `closure->clase_definicion`.
                 *
                 * Si por alguna razón el closure no tiene
                 * clase_definicion (función llamada como método de
                 * instancia sin pasar por una declaración de clase),
                 * cae al esquema antiguo (`yo.clase.superclase`) como
                 * fallback.
                 */
                uint8_t name_idx = LEER_BYTE();
                uint8_t n_args = LEER_BYTE();
                const Valor *nombre = &frame->chunk->constantes[name_idx];

                Valor *base_nuevo = vm->tope - n_args - 1;
                Valor yo = *base_nuevo;
                if (yo.tipo != VAL_INSTANCIA) {
                    VM_ERROR("'super' solo puede usarse en metodos de instancia");
                    return VM_ERROR_RUNTIME;
                }
                Clase *clase_origen = NULL;
                if (frame->closure && frame->closure->clase_definicion) {
                    clase_origen = frame->closure->clase_definicion;
                } else {
                    clase_origen = yo.como.instancia->clase;
                }
                Clase *super = clase_origen->superclase;
                if (!super) {
                    VM_ERROR("la clase '%.*s' no tiene superclase",
                             clase_origen->longitud_nombre,
                             clase_origen->nombre);
                    return VM_ERROR_RUNTIME;
                }
                Valor met_v;
                if (!dicc_obtener(super->metodos, nombre, &met_v)) {
                    VM_ERROR("ErrorDeAtributo: la superclase '%.*s' no tiene metodo '%.*s'",
                             super->longitud_nombre, super->nombre,
                             nombre->como.cadena.longitud,
                             nombre->como.cadena.texto);
                    return VM_ERROR_RUNTIME;
                }
                if (met_v.tipo != VAL_FUNCION_BC) {
                    valor_destruir(&met_v);
                    VM_ERROR("estado interno corrupto: super metodo no es closure");
                    return VM_ERROR_RUNTIME;
                }
                Closure *cl = met_v.como.closure;
                FuncionBC *fn = cl->plantilla;
                /* Aridad: igual que un bound method, n_args + 1 == aridad. */
                if (n_args + 1 != fn->aridad) {
                    valor_destruir(&met_v);
                    VM_ERROR("ErrorDeTipo: %.*s() esperaba %d argumentos, recibio %d",
                             fn->longitud_nombre, fn->nombre,
                             fn->aridad - 1, n_args);
                    return VM_ERROR_RUNTIME;
                }
                if (vm->n_frames >= VM_FRAMES_MAX) {
                    valor_destruir(&met_v);
                    VM_ERROR("desbordamiento de pila de llamadas (>%d frames)",
                             VM_FRAMES_MAX);
                    return VM_ERROR_RUNTIME;
                }
                /*
                 * Stack ya tiene la disposición correcta: [yo, arg1, ..., argN].
                 * Solo necesitamos reemplazar el slot del callee (que era
                 * `yo`) por la closure y poner el receptor (yo) en slot 1.
                 *
                 * Hacemos: push slot, shift; o más simple, igual que
                 * VAL_METODO_LIGADO: memmove args arriba 1 puesto, escribir
                 * closure en slot 0 y receptor (clonado) en slot 1.
                 */
                if (vm->tope - vm->pila >= VM_PILA_MAX) {
                    valor_destruir(&met_v);
                    VM_ERROR("Desbordamiento de pila");
                    return VM_ERROR_RUNTIME;
                }
                if (n_args > 0) {
                    memmove(base_nuevo + 2, base_nuevo + 1,
                            sizeof(Valor) * (size_t)n_args);
                }
                vm->tope++;
                Valor receptor_clon = valor_clonar(&yo);
                Valor old = *base_nuevo;        /* era yo (transferido) */
                *base_nuevo = met_v;             /* closure */
                base_nuevo[1] = receptor_clon;
                valor_destruir(&old);

                frame = &vm->frames[vm->n_frames++];
                frame->chunk = &fn->chunk;
                frame->ip = fn->chunk.codigo;
                frame->base_pila = base_nuevo;
                frame->closure = cl;
                frame->es_constructor = false;
                frame->modulo_en_carga = NULL;
                frame->globales_pre_modulo = NULL;
                frame->chunk_modulo = NULL;
                if (cl->globales_definicion != NULL
                    && cl->globales_definicion != vm->globales) {
                    frame->globales_pre_llamada = vm->globales;
                    vm->globales = cl->globales_definicion;
                } else {
                    frame->globales_pre_llamada = NULL;
                }
                break;
            }
            case OP_HEREDAR: {
                /* Stack: [..., clase, super]. Pop super y enlazar a la
                   clase: copiar todos los métodos heredados en
                   `clase.metodos` y guardar referencia a super en
                   `clase.superclase`. La clase queda en el tope. */
                Valor super_v = sacar(vm);
                if (vm->tope == vm->pila || vm->tope[-1].tipo != VAL_CLASE) {
                    valor_destruir(&super_v);
                    VM_ERROR("estado interno corrupto: OP_HEREDAR sin clase en stack");
                    return VM_ERROR_RUNTIME;
                }
                if (super_v.tipo != VAL_CLASE) {
                    VM_ERROR("ErrorDeTipo: solo se puede heredar de una clase, no de '%s'",
                             valor_nombre_tipo(&super_v));
                    valor_destruir(&super_v);
                    return VM_ERROR_RUNTIME;
                }
                Clase *clase_hija = vm->tope[-1].como.clase;
                Clase *clase_padre = super_v.como.clase;
                /* Copiar métodos heredados. Si el cuerpo de la subclase
                   define luego un método con el mismo nombre, OP_METODO
                   sobreescribe la entrada (semántica esperada). */
                const Diccionario *src = clase_padre->metodos;
                for (int i = 0; i < src->capacidad; i++) {
                    if (!src->entradas[i].ocupada) continue;
                    Valor k = valor_clonar(&src->entradas[i].clave);
                    Valor v = valor_clonar(&src->entradas[i].valor);
                    if (!dicc_asignar(clase_hija->metodos, k, v)) {
                        valor_destruir(&super_v);
                        VM_ERROR("memoria insuficiente al heredar metodos");
                        return VM_ERROR_RUNTIME;
                    }
                }
                /* Enlazar superclase (transferir refcount de super_v). */
                if (clase_hija->superclase) {
                    clase_liberar(clase_hija->superclase);
                }
                clase_retener(clase_padre);
                clase_hija->superclase = clase_padre;
                valor_destruir(&super_v);
                break;
            }

            /* ─── Closures (v0.6.2) ─── */
            case OP_CLOSURE: {
                /*
                 * Lee la plantilla del pool de constantes y crea una
                 * Closure NUEVA con upvalues conectados al stack.
                 * Operandos: [byte fn_idx] [n_upvalues * (is_local, index)].
                 */
                uint8_t fn_idx = LEER_BYTE();
                Valor plantilla_v = frame->chunk->constantes[fn_idx];
                if (plantilla_v.tipo != VAL_PLANTILLA_BC) {
                    VM_ERROR("OP_CLOSURE sin plantilla en constante");
                    return VM_ERROR_RUNTIME;
                }
                FuncionBC *fn = plantilla_v.como.plantilla;
                Closure *cl = closure_nuevo(fn);
                if (!cl) {
                    VM_ERROR("memoria insuficiente al crear closure");
                    return VM_ERROR_RUNTIME;
                }
                /* v0.9.0: cierre de globales — la función "captura" el
                   diccionario de globales del scope de su creación.
                   Solo lo guardamos si vm->globales != NULL para evitar
                   ciclos en el caso top-level del programa principal
                   (que no es un módulo y no necesita preservar nada
                   distinto al global). En la práctica esto significa
                   que solo las funciones definidas dentro de un módulo
                   capturan su dicc. */
                if (vm->globales) {
                    dicc_retener(vm->globales);
                    cl->globales_definicion = vm->globales;
                }
                for (int i = 0; i < fn->n_upvalues; i++) {
                    uint8_t es_local = LEER_BYTE();
                    uint8_t indice = LEER_BYTE();
                    Upvalue *uv;
                    if (es_local) {
                        uv = capturar_upvalue(vm, &frame->base_pila[indice]);
                    } else {
                        /* Captura un upvalue del frame actual (padre
                           directo de la función creada). */
                        uv = frame->closure->upvalues[indice];
                        upvalue_retener(uv);
                    }
                    cl->upvalues[i] = uv;
                }
                empujar(vm, valor_closure(cl));
                break;
            }
            case OP_OBTENER_UPVALUE: {
                uint8_t slot = LEER_BYTE();
                Upvalue *uv = frame->closure->upvalues[slot];
                empujar(vm, valor_clonar(uv->posicion));
                break;
            }
            case OP_ASIGNAR_UPVALUE: {
                uint8_t slot = LEER_BYTE();
                Upvalue *uv = frame->closure->upvalues[slot];
                Valor nuevo = sacar(vm);
                valor_destruir(uv->posicion);
                *uv->posicion = nuevo;
                break;
            }
            case OP_CERRAR_UPVALUE: {
                cerrar_upvalues_hasta(vm, vm->tope - 1);
                Valor v = sacar(vm);
                valor_destruir(&v);
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
                    Closure *cl = callee.como.closure;
                    FuncionBC *fn = cl->plantilla;
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
                    frame = &vm->frames[vm->n_frames++];
                    frame->chunk = &fn->chunk;
                    frame->ip = fn->chunk.codigo;
                    frame->base_pila = base_nuevo;
                    frame->closure = cl;
                    frame->es_constructor = false;
                    frame->modulo_en_carga = NULL;
                    frame->globales_pre_modulo = NULL;
                    frame->chunk_modulo = NULL;
                    /*
                     * v0.9.0: si la closure cerró un dicc de globales
                     * distinto al actual (caso típico: función definida
                     * en un módulo invocada desde el importador),
                     * cambiar a sus globales y guardar las actuales
                     * para restaurar al retornar.
                     */
                    if (cl->globales_definicion != NULL
                        && cl->globales_definicion != vm->globales) {
                        frame->globales_pre_llamada = vm->globales;
                        vm->globales = cl->globales_definicion;
                    } else {
                        frame->globales_pre_llamada = NULL;
                    }
                    break;
                }

                if (callee.tipo == VAL_CLASE) {
                    /*
                     * Llamar una clase crea una instancia. Si la clase
                     * tiene `__iniciar__`, lo invocamos como un método
                     * con la instancia recién creada como receptor.
                     * El frame del constructor se marca con
                     * `es_constructor = true` para que su OP_RETORNAR
                     * descarte el valor retornado y devuelva la
                     * instancia (Python-like).
                     */
                    Clase *cl_class = callee.como.clase;
                    Instancia *inst = instancia_nueva(cl_class);
                    if (!inst) {
                        VM_ERROR("memoria insuficiente al crear instancia");
                        return VM_ERROR_RUNTIME;
                    }

                    /* Buscar __iniciar__ en los métodos de la clase. */
                    Valor clave_init = valor_cadena_referencia("__iniciar__", 11);
                    Valor met_v;
                    bool tiene_init = dicc_obtener(cl_class->metodos,
                                                     &clave_init, &met_v);

                    if (!tiene_init) {
                        if (n_args != 0) {
                            instancia_liberar(inst);
                            VM_ERROR("ErrorDeTipo: %.*s() no acepta argumentos (sin __iniciar__)",
                                     cl_class->longitud_nombre, cl_class->nombre);
                            return VM_ERROR_RUNTIME;
                        }
                        /* Sin __iniciar__: solo crear instancia y empujar. */
                        Valor cv = *(--vm->tope);
                        valor_destruir(&cv);
                        empujar(vm, valor_instancia(inst));
                        break;
                    }

                    /* Tiene __iniciar__: validar aridad incluyendo `yo`. */
                    if (met_v.tipo != VAL_FUNCION_BC) {
                        valor_destruir(&met_v);
                        instancia_liberar(inst);
                        VM_ERROR("estado interno corrupto: __iniciar__ no es closure");
                        return VM_ERROR_RUNTIME;
                    }
                    Closure *cl = met_v.como.closure;
                    FuncionBC *fn = cl->plantilla;
                    if (n_args + 1 != fn->aridad) {
                        valor_destruir(&met_v);
                        instancia_liberar(inst);
                        VM_ERROR("ErrorDeTipo: %.*s() esperaba %d argumentos, recibio %d",
                                 cl_class->longitud_nombre, cl_class->nombre,
                                 fn->aridad - 1, n_args);
                        return VM_ERROR_RUNTIME;
                    }
                    if (vm->n_frames >= VM_FRAMES_MAX) {
                        valor_destruir(&met_v);
                        instancia_liberar(inst);
                        VM_ERROR("desbordamiento de pila de llamadas (>%d frames)",
                                 VM_FRAMES_MAX);
                        return VM_ERROR_RUNTIME;
                    }
                    if (vm->tope - vm->pila >= VM_PILA_MAX) {
                        valor_destruir(&met_v);
                        instancia_liberar(inst);
                        VM_ERROR("Desbordamiento de pila");
                        return VM_ERROR_RUNTIME;
                    }
                    /* Insertar la instancia como receptor: shift de
                       args y reemplazar callee con la closure. */
                    if (n_args > 0) {
                        memmove(base_nuevo + 2, base_nuevo + 1,
                                sizeof(Valor) * (size_t)n_args);
                    }
                    vm->tope++;
                    Valor old = *base_nuevo;
                    *base_nuevo = met_v;                   /* closure (ya retenido por dicc_obtener) */
                    base_nuevo[1] = valor_instancia(inst);
                    valor_destruir(&old);

                    frame = &vm->frames[vm->n_frames++];
                    frame->chunk = &fn->chunk;
                    frame->ip = fn->chunk.codigo;
                    frame->base_pila = base_nuevo;
                    frame->closure = cl;
                    frame->es_constructor = true;
                    frame->modulo_en_carga = NULL;
                    frame->globales_pre_modulo = NULL;
                    frame->chunk_modulo = NULL;
                    if (cl->globales_definicion != NULL
                        && cl->globales_definicion != vm->globales) {
                        frame->globales_pre_llamada = vm->globales;
                        vm->globales = cl->globales_definicion;
                    } else {
                        frame->globales_pre_llamada = NULL;
                    }
                    break;
                }

                if (callee.tipo == VAL_METODO_LIGADO) {
                    /*
                     * Llamada a un método con receptor ligado:
                     *   stack antes: [..., bound, arg1, arg2, ..., argN]
                     *   stack despues: [..., closure, receptor, arg1, ..., argN]
                     *
                     * El receptor se inserta como primer parámetro del frame
                     * (slot 1, ya que slot 0 es el callee). El compilador
                     * ya garantiza que el primer parámetro es `yo`
                     * (convención).
                     */
                    MetodoLigado *bm = callee.como.metodo_ligado;
                    Closure *cl = bm->metodo;
                    FuncionBC *fn = cl->plantilla;
                    if (n_args + 1 != fn->aridad) {
                        VM_ERROR("ErrorDeTipo: %.*s() esperaba %d argumentos, recibio %d",
                                 fn->longitud_nombre, fn->nombre,
                                 fn->aridad - 1, n_args);
                        return VM_ERROR_RUNTIME;
                    }
                    if (vm->n_frames >= VM_FRAMES_MAX) {
                        VM_ERROR("desbordamiento de pila de llamadas (>%d frames)",
                                 VM_FRAMES_MAX);
                        return VM_ERROR_RUNTIME;
                    }
                    if (vm->tope - vm->pila >= VM_PILA_MAX) {
                        VM_ERROR("Desbordamiento de pila");
                        return VM_ERROR_RUNTIME;
                    }
                    /* Hacer hueco para el receptor: mover los n_args
                       slots un puesto arriba. */
                    if (n_args > 0) {
                        memmove(base_nuevo + 2, base_nuevo + 1,
                                sizeof(Valor) * (size_t)n_args);
                    }
                    vm->tope++;
                    /* Reemplazar el callee (bound method) por la closure
                       y poner el receptor en slot 1. Necesitamos retener
                       refs antes de destruir el bound method (que
                       contiene la única referencia al closure y al
                       receptor). */
                    closure_retener(cl);
                    Valor receptor = valor_clonar(&bm->receptor);
                    Valor bound_old = *base_nuevo;
                    *base_nuevo = valor_closure(cl);
                    base_nuevo[1] = receptor;
                    valor_destruir(&bound_old);

                    frame = &vm->frames[vm->n_frames++];
                    frame->chunk = &fn->chunk;
                    frame->ip = fn->chunk.codigo;
                    frame->base_pila = base_nuevo;
                    frame->closure = cl;
                    frame->es_constructor = false;
                    frame->modulo_en_carga = NULL;
                    frame->globales_pre_modulo = NULL;
                    frame->chunk_modulo = NULL;
                    if (cl->globales_definicion != NULL
                        && cl->globales_definicion != vm->globales) {
                        frame->globales_pre_llamada = vm->globales;
                        vm->globales = cl->globales_definicion;
                    } else {
                        frame->globales_pre_llamada = NULL;
                    }
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

ResultadoVM vm_ejecutar(VM *vm, const Chunk *chunk, Valor *resultado_out) {
    vm->memoria.gc_habilitado = true;
    ResultadoVM r = vm_ejecutar_dispatch(vm, chunk, resultado_out);
    vm->memoria.gc_habilitado = false;
    return r;
}
