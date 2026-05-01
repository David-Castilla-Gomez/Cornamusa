#include "vm.h"

#include <limits.h>
#include <stdarg.h>
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
#include "utf8proc.h"
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

/* Busca y carga el módulo `nombre`. Devuelve Chunk* (con malloc'd) o NULL.
 * Si el nombre contiene `.` (subsegmentos, v0.9.1), se traducen a `/` antes
 * del lookup: `mat.geometria` → busca `./mat/geometria.cor` y
 * `stdlib/mat/geometria.cor`. */
static Chunk *cargar_modulo_desde_archivo(const char *nombre, int len_nombre) {
    char path[512];
    char *fuente = NULL;
    size_t flen = 0;

    /* Construir el path traduciendo `.` a `/`. */
    char nombre_path[256];
    int copy_len = len_nombre < (int)sizeof(nombre_path) - 1
                 ? len_nombre : (int)sizeof(nombre_path) - 1;
    for (int i = 0; i < copy_len; i++) {
        nombre_path[i] = (nombre[i] == '.') ? '/' : nombre[i];
    }
    nombre_path[copy_len] = '\0';

    /* Intento 1: ./{nombre_path}.cor */
    snprintf(path, sizeof(path), "%s.cor", nombre_path);
    fuente = leer_archivo_completo(path, &flen);

    /* Intento 2: stdlib/{nombre_path}.cor */
    if (!fuente) {
        snprintf(path, sizeof(path), "stdlib/%s.cor", nombre_path);
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

/*
 * F10: helpers para inline cache de OP_LLAMAR.
 *
 * `opcode_addr` apunta al byte del opcode dentro de chunk->codigo. El
 * cast a (uint8_t *) es necesario porque CallFrame.chunk es const, pero
 * el campo `codigo` declarado en Chunk es uint8_t* (no const), así que
 * la escritura no viola el contrato de const.
 *
 * DEGRADAR_LLAMAR: reescribe el byte del opcode a OP_LLAMAR y rebobina
 * frame->ip al byte del opcode. La siguiente iteración del dispatch
 * leerá OP_LLAMAR y entrará por el slow path.
 *
 * PROMOVER_LLAMAR(variante): reescribe el byte del opcode a la variante
 * especializada indicada. Lo invoca el slow path de OP_LLAMAR tras el
 * primer éxito.
 */
#define DEGRADAR_LLAMAR()                                                  \
    do {                                                                    \
        uint8_t *_codigo = (uint8_t *)frame->chunk->codigo;                 \
        _codigo[(int)(opcode_addr - _codigo)] = (uint8_t)OP_LLAMAR;         \
        frame->ip = opcode_addr;                                            \
    } while (0)

/*
 * F10: macros para las variantes INT_INT de OP_SUMAR/RESTAR/MULTIPLICAR
 * y OP_MENOR/MENOR_IGUAL/MAYOR/MAYOR_IGUAL.
 *
 * - Verifican que ambos operandos en el tope del stack son VAL_ENTERO.
 * - Si lo son, llaman libtommath directamente y empujan el resultado
 *   sin pasar por el switch de tipos de evaluador_aplicar_binario.
 * - Si no, degradan el opcode al base (sin sufijo) y rebobinan ip
 *   para que la siguiente iteración del loop siga el slow path.
 *
 * El macro contiene su propio `break` en el camino de degradación
 * — sale del do-while-zero. El `break` que sigue a la invocación del
 * macro en cada case sale del case. En el camino feliz, el do-while
 * termina natural y el `break` del case sigue.
 */
/*
 * Aritmética IC (B9 v0.11): tres caminos
 *
 *   1. SMALL+SMALL — invoca evaluador_small_op_small() inline. Sin
 *      malloc/mp_init. La normalización del resultado a SMALL/BIG la
 *      hace evaluador_small_op_small via valor_entero_de_i64.
 *      Si el helper reporta overflow (no aplicable), caer al path BIG.
 *   2. BIG+BIG — path mp_* existente; el resultado pasa por
 *      valor_entero_de_mp_normalizado para demote si cabe en SMALL.
 *   3. Cualquier mezcla SMALL/BIG o tipo no-entero — degrade al
 *      slow path OP_BASE.
 *
 * `OP_TT` es el TipoToken (TT_MAS, etc.) que evaluador_small_op_small
 * espera. `MP_OP` es la función libtommath para el path BIG.
 */
#define BIN_INT_INT_ARITH(BASE_OP, MP_OP, OP_TT)                            \
    do {                                                                     \
        const uint8_t *opcode_addr = frame->ip - 1;                          \
        Valor *_pb = vm->tope - 1;                                           \
        Valor *_pa = vm->tope - 2;                                           \
        if (_pa->tipo == VAL_ENTERO_SMALL && _pb->tipo == VAL_ENTERO_SMALL) {\
            int64_t _a64 = _pa->como.entero_small;                           \
            int64_t _b64 = _pb->como.entero_small;                           \
            bool _aplic;                                                     \
            int _linea = linea_actual_frame(frame);                          \
            Valor _r = evaluador_small_op_small(&vm->error, (OP_TT),         \
                                                 _a64, _b64, _linea, 0, &_aplic);\
            if (_aplic) {                                                    \
                if (vm->error.tuvo_error) return VM_ERROR_RUNTIME;           \
                vm->tope -= 2;                                               \
                empujar(vm, _r);                                             \
                break;                                                       \
            }                                                                \
            /* Overflow: fallback a path BIG con mp_int temporales. */       \
            Valor _b = sacar(vm);                                            \
            Valor _a = sacar(vm);                                            \
            mp_int *_ma = evaluador_nuevo_mp();                              \
            mp_int *_mb = evaluador_nuevo_mp();                              \
            mp_int *_rmp = evaluador_nuevo_mp();                             \
            if (!_ma || !_mb || !_rmp) {                                     \
                evaluador_liberar_mp(_ma); evaluador_liberar_mp(_mb);        \
                evaluador_liberar_mp(_rmp);                                  \
                valor_destruir(&_a); valor_destruir(&_b);                    \
                VM_ERROR("memoria insuficiente");                            \
                return VM_ERROR_RUNTIME;                                     \
            }                                                                \
            mp_set_i64(_ma, _a64); mp_set_i64(_mb, _b64);                    \
            if (MP_OP(_ma, _mb, _rmp) != MP_OKAY) {                          \
                evaluador_liberar_mp(_ma); evaluador_liberar_mp(_mb);        \
                evaluador_liberar_mp(_rmp);                                  \
                valor_destruir(&_a); valor_destruir(&_b);                    \
                VM_ERROR("fallo en operacion entera");                       \
                return VM_ERROR_RUNTIME;                                     \
            }                                                                \
            evaluador_liberar_mp(_ma); evaluador_liberar_mp(_mb);            \
            valor_destruir(&_a); valor_destruir(&_b);                        \
            empujar(vm, valor_entero_de_mp_normalizado(_rmp));               \
            break;                                                           \
        }                                                                    \
        if (_pa->tipo == VAL_ENTERO && _pb->tipo == VAL_ENTERO) {            \
            Valor _b = sacar(vm);                                            \
            Valor _a = sacar(vm);                                            \
            mp_int *_r = evaluador_nuevo_mp();                               \
            if (!_r || MP_OP(_a.como.entero, _b.como.entero, _r) != MP_OKAY) {\
                evaluador_liberar_mp(_r);                                    \
                valor_destruir(&_a); valor_destruir(&_b);                    \
                VM_ERROR("memoria insuficiente");                            \
                return VM_ERROR_RUNTIME;                                     \
            }                                                                \
            valor_destruir(&_a); valor_destruir(&_b);                        \
            empujar(vm, valor_entero_de_mp_normalizado(_r));                 \
            break;                                                           \
        }                                                                    \
        /* Mezcla SMALL/BIG o tipo no-entero: degradar al slow path. */      \
        uint8_t *_codigo = (uint8_t *)frame->chunk->codigo;                  \
        _codigo[(int)(opcode_addr - _codigo)] = (uint8_t)(BASE_OP);          \
        frame->ip = opcode_addr;                                             \
    } while (0)

/*
 * Comparaciones IC (B9 v0.11): camino rápido i64 si ambos SMALL,
 * mp_cmp si ambos BIG, degrade en mezclas.
 */
#define BIN_INT_INT_CMP(BASE_OP, COND)                                      \
    do {                                                                     \
        const uint8_t *opcode_addr = frame->ip - 1;                          \
        Valor *_pb = vm->tope - 1;                                           \
        Valor *_pa = vm->tope - 2;                                           \
        if (_pa->tipo == VAL_ENTERO_SMALL && _pb->tipo == VAL_ENTERO_SMALL) {\
            int64_t _a64 = _pa->como.entero_small;                           \
            int64_t _b64 = _pb->como.entero_small;                           \
            int _cmp = (_a64 < _b64) ? -1 : (_a64 > _b64) ? 1 : 0;           \
            vm->tope -= 2;                                                   \
            empujar(vm, valor_booleano(COND));                               \
            break;                                                           \
        }                                                                    \
        if (_pa->tipo == VAL_ENTERO && _pb->tipo == VAL_ENTERO) {            \
            Valor _b = sacar(vm);                                            \
            Valor _a = sacar(vm);                                            \
            int _mp = mp_cmp(_a.como.entero, _b.como.entero);                \
            int _cmp = (_mp == MP_LT) ? -1 : (_mp == MP_GT) ? 1 : 0;         \
            valor_destruir(&_a); valor_destruir(&_b);                        \
            empujar(vm, valor_booleano(COND));                               \
            break;                                                           \
        }                                                                    \
        uint8_t *_codigo = (uint8_t *)frame->chunk->codigo;                  \
        _codigo[(int)(opcode_addr - _codigo)] = (uint8_t)(BASE_OP);          \
        frame->ip = opcode_addr;                                             \
    } while (0)

/* ──────────────────────────────────────────────────────────────────
 * Helpers de OP_LLAMAR (F10).
 *
 * Cada uno ejecuta el body correspondiente al tipo de callee, con la
 * pila/frame ya validados por el llamador (slow path o variante
 * especializada). Devuelven VM_OK en éxito o VM_ERROR_RUNTIME tras
 * setear vm->error.
 *
 * Para BC/CLASE/METODO_LIGADO el helper crea un nuevo CallFrame y
 * actualiza `*frame_inout` para que el dispatch loop continúe sobre
 * el frame nuevo.
 * ────────────────────────────────────────────────────────────────── */

static ResultadoVM ejecutar_llamar_nativa(VM *vm, CallFrame *frame,
                                            Valor *base_nuevo, uint8_t n_args) {
    int linea = linea_actual_frame(frame);
    Valor *args = base_nuevo + 1;
    Valor r = base_nuevo->como.nativa.fn(&vm->error, n_args, args, linea, 0);
    if (vm->error.tuvo_error) {
        valor_destruir(&r);
        return VM_ERROR_RUNTIME;
    }
    for (int i = 0; i < n_args; i++) {
        Valor v = *(--vm->tope);
        valor_destruir(&v);
    }
    Valor cv = *(--vm->tope);
    valor_destruir(&cv);
    empujar(vm, r);
    return VM_OK;
}

/* Helper interno para reportar error con printf-style desde un helper. */
static void llamar_set_error(VM *vm, CallFrame *frame, const char *fmt, ...) {
    vm->error.tuvo_error = true;
    vm->error.linea = linea_actual_frame(frame);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(vm->error.mensaje, sizeof(vm->error.mensaje), fmt, ap);
    va_end(ap);
}

static ResultadoVM ejecutar_llamar_bc(VM *vm, CallFrame **frame_inout,
                                       Valor *base_nuevo, uint8_t n_args) {
    CallFrame *frame = *frame_inout;
    Closure *cl = base_nuevo->como.closure;
    FuncionBC *fn = cl->plantilla;
    if (n_args != fn->aridad) {
        llamar_set_error(vm, frame,
            "ErrorDeTipo: %.*s() esperaba %d argumentos, recibio %d",
            fn->longitud_nombre, fn->nombre, fn->aridad, n_args);
        return VM_ERROR_RUNTIME;
    }
    if (vm->n_frames >= VM_FRAMES_MAX) {
        llamar_set_error(vm, frame,
            "desbordamiento de pila de llamadas (>%d frames)", VM_FRAMES_MAX);
        return VM_ERROR_RUNTIME;
    }
    CallFrame *nf = &vm->frames[vm->n_frames++];
    nf->chunk = &fn->chunk;
    nf->ip = fn->chunk.codigo;
    nf->base_pila = base_nuevo;
    nf->closure = cl;
    nf->es_constructor = false;
    nf->modulo_en_carga = NULL;
    nf->globales_pre_modulo = NULL;
    nf->chunk_modulo = NULL;
    if (cl->globales_definicion != NULL
        && cl->globales_definicion != vm->globales) {
        nf->globales_pre_llamada = vm->globales;
        vm->globales = cl->globales_definicion;
    } else {
        nf->globales_pre_llamada = NULL;
    }
    nf->modulo_binding_name = NULL;
    nf->modulo_binding_len = 0;
    nf->desde_import = false;
    *frame_inout = nf;
    return VM_OK;
}

static ResultadoVM ejecutar_llamar_clase(VM *vm, CallFrame **frame_inout,
                                          Valor *base_nuevo, uint8_t n_args) {
    CallFrame *frame = *frame_inout;
    Clase *cl_class = base_nuevo->como.clase;
    Instancia *inst = instancia_nueva(cl_class);
    if (!inst) {
        llamar_set_error(vm, frame, "memoria insuficiente al crear instancia");
        return VM_ERROR_RUNTIME;
    }
    Valor clave_init = valor_cadena_referencia("__iniciar__", 11);
    Valor met_v;
    bool tiene_init = dicc_obtener(cl_class->metodos, &clave_init, &met_v);

    if (!tiene_init) {
        if (n_args != 0) {
            instancia_liberar(inst);
            llamar_set_error(vm, frame,
                "ErrorDeTipo: %.*s() no acepta argumentos (sin __iniciar__)",
                cl_class->longitud_nombre, cl_class->nombre);
            return VM_ERROR_RUNTIME;
        }
        Valor cv = *(--vm->tope);
        valor_destruir(&cv);
        empujar(vm, valor_instancia(inst));
        return VM_OK;
    }

    if (met_v.tipo != VAL_FUNCION_BC) {
        valor_destruir(&met_v);
        instancia_liberar(inst);
        llamar_set_error(vm, frame,
            "estado interno corrupto: __iniciar__ no es closure");
        return VM_ERROR_RUNTIME;
    }
    Closure *cl = met_v.como.closure;
    FuncionBC *fn = cl->plantilla;
    if (n_args + 1 != fn->aridad) {
        valor_destruir(&met_v);
        instancia_liberar(inst);
        llamar_set_error(vm, frame,
            "ErrorDeTipo: %.*s() esperaba %d argumentos, recibio %d",
            cl_class->longitud_nombre, cl_class->nombre,
            fn->aridad - 1, n_args);
        return VM_ERROR_RUNTIME;
    }
    if (vm->n_frames >= VM_FRAMES_MAX) {
        valor_destruir(&met_v);
        instancia_liberar(inst);
        llamar_set_error(vm, frame,
            "desbordamiento de pila de llamadas (>%d frames)", VM_FRAMES_MAX);
        return VM_ERROR_RUNTIME;
    }
    if (vm->tope - vm->pila >= VM_PILA_MAX) {
        valor_destruir(&met_v);
        instancia_liberar(inst);
        llamar_set_error(vm, frame, "Desbordamiento de pila");
        return VM_ERROR_RUNTIME;
    }
    /* Insertar la instancia como receptor: shift de args y reemplazar
       callee con la closure. */
    if (n_args > 0) {
        memmove(base_nuevo + 2, base_nuevo + 1,
                sizeof(Valor) * (size_t)n_args);
    }
    vm->tope++;
    Valor old = *base_nuevo;
    *base_nuevo = met_v;
    base_nuevo[1] = valor_instancia(inst);
    valor_destruir(&old);

    CallFrame *nf = &vm->frames[vm->n_frames++];
    nf->chunk = &fn->chunk;
    nf->ip = fn->chunk.codigo;
    nf->base_pila = base_nuevo;
    nf->closure = cl;
    nf->es_constructor = true;
    nf->modulo_en_carga = NULL;
    nf->globales_pre_modulo = NULL;
    nf->chunk_modulo = NULL;
    if (cl->globales_definicion != NULL
        && cl->globales_definicion != vm->globales) {
        nf->globales_pre_llamada = vm->globales;
        vm->globales = cl->globales_definicion;
    } else {
        nf->globales_pre_llamada = NULL;
    }
    nf->modulo_binding_name = NULL;
    nf->modulo_binding_len = 0;
    nf->desde_import = false;
    *frame_inout = nf;
    return VM_OK;
}

static ResultadoVM ejecutar_llamar_metodo_ligado(VM *vm, CallFrame **frame_inout,
                                                  Valor *base_nuevo,
                                                  uint8_t n_args) {
    CallFrame *frame = *frame_inout;
    MetodoLigado *bm = base_nuevo->como.metodo_ligado;
    Closure *cl = bm->metodo;
    FuncionBC *fn = cl->plantilla;
    if (n_args + 1 != fn->aridad) {
        llamar_set_error(vm, frame,
            "ErrorDeTipo: %.*s() esperaba %d argumentos, recibio %d",
            fn->longitud_nombre, fn->nombre, fn->aridad - 1, n_args);
        return VM_ERROR_RUNTIME;
    }
    if (vm->n_frames >= VM_FRAMES_MAX) {
        llamar_set_error(vm, frame,
            "desbordamiento de pila de llamadas (>%d frames)", VM_FRAMES_MAX);
        return VM_ERROR_RUNTIME;
    }
    if (vm->tope - vm->pila >= VM_PILA_MAX) {
        llamar_set_error(vm, frame, "Desbordamiento de pila");
        return VM_ERROR_RUNTIME;
    }
    if (n_args > 0) {
        memmove(base_nuevo + 2, base_nuevo + 1,
                sizeof(Valor) * (size_t)n_args);
    }
    vm->tope++;
    closure_retener(cl);
    Valor receptor = valor_clonar(&bm->receptor);
    Valor bound_old = *base_nuevo;
    *base_nuevo = valor_closure(cl);
    base_nuevo[1] = receptor;
    valor_destruir(&bound_old);

    CallFrame *nf = &vm->frames[vm->n_frames++];
    nf->chunk = &fn->chunk;
    nf->ip = fn->chunk.codigo;
    nf->base_pila = base_nuevo;
    nf->closure = cl;
    nf->es_constructor = false;
    nf->modulo_en_carga = NULL;
    nf->globales_pre_modulo = NULL;
    nf->chunk_modulo = NULL;
    if (cl->globales_definicion != NULL
        && cl->globales_definicion != vm->globales) {
        nf->globales_pre_llamada = vm->globales;
        vm->globales = cl->globales_definicion;
    } else {
        nf->globales_pre_llamada = NULL;
    }
    nf->modulo_binding_name = NULL;
    nf->modulo_binding_len = 0;
    nf->desde_import = false;
    *frame_inout = nf;
    return VM_OK;
}

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
                    frame->modulo_binding_name = NULL;
                    frame->modulo_binding_len = 0;
                frame->desde_import = false;

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

            /* ─── Aritmética y comparaciones binarias (slow path) ─── */
            case OP_SUMAR: case OP_RESTAR: case OP_MULTIPLICAR:
            case OP_DIVIDIR: case OP_DIVIDIR_ENTERO: case OP_MODULO:
            case OP_POTENCIA:
            case OP_IGUAL: case OP_DISTINTO:
            case OP_MENOR: case OP_MENOR_IGUAL:
            case OP_MAYOR: case OP_MAYOR_IGUAL:
            case OP_ES: case OP_EN: {
                const uint8_t *opcode_addr = frame->ip - 1;
                int linea = linea_actual_frame(frame);
                /* Capturar tipos antes de sacar (evaluador_aplicar_binario
                   destruye los operandos). v0.11 sesión 5: el IC ahora
                   acepta SMALL+SMALL inline, así que promovemos siempre
                   que ambos sean enteros (incluyendo SMALL). */
                bool ambos_int = (valor_es_entero(&vm->tope[-1])
                                  && valor_es_entero(&vm->tope[-2]));
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
                /* F10: si ambos eran enteros y el op tiene variante
                   INT_INT, promover. */
                if (ambos_int) {
                    OpCode promote = (OpCode)op;
                    switch (op) {
                        case OP_SUMAR:        promote = OP_SUMAR_INT_INT; break;
                        case OP_RESTAR:       promote = OP_RESTAR_INT_INT; break;
                        case OP_MULTIPLICAR:  promote = OP_MULTIPLICAR_INT_INT; break;
                        case OP_MENOR:        promote = OP_MENOR_INT_INT; break;
                        case OP_MENOR_IGUAL:  promote = OP_MENOR_IGUAL_INT_INT; break;
                        case OP_MAYOR:        promote = OP_MAYOR_INT_INT; break;
                        case OP_MAYOR_IGUAL:  promote = OP_MAYOR_IGUAL_INT_INT; break;
                        default: break;
                    }
                    if ((uint8_t)promote != op) {
                        uint8_t *codigo = (uint8_t *)frame->chunk->codigo;
                        codigo[(int)(opcode_addr - codigo)] = (uint8_t)promote;
                    }
                }
                break;
            }
            case OP_SUMAR_INT_INT:        BIN_INT_INT_ARITH(OP_SUMAR, mp_add, TT_MAS); break;
            case OP_RESTAR_INT_INT:       BIN_INT_INT_ARITH(OP_RESTAR, mp_sub, TT_MENOS); break;
            case OP_MULTIPLICAR_INT_INT:  BIN_INT_INT_ARITH(OP_MULTIPLICAR, mp_mul, TT_ASTERISCO); break;
            case OP_MENOR_INT_INT:        BIN_INT_INT_CMP(OP_MENOR, _cmp == MP_LT); break;
            case OP_MENOR_IGUAL_INT_INT:  BIN_INT_INT_CMP(OP_MENOR_IGUAL, _cmp != MP_GT); break;
            case OP_MAYOR_INT_INT:        BIN_INT_INT_CMP(OP_MAYOR, _cmp == MP_GT); break;
            case OP_MAYOR_IGUAL_INT_INT:  BIN_INT_INT_CMP(OP_MAYOR_IGUAL, _cmp != MP_LT); break;

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
                    /* Liberar el chunk del módulo. */
                    if (frame->chunk_modulo) {
                        chunk_destruir(frame->chunk_modulo);
                        free(frame->chunk_modulo);
                    }
                    /* Cachear siempre por nombre real. */
                    Valor val_cache = valor_modulo(mod);
                    modulo_retener(mod);
                    Valor clave_cache = valor_cadena_duplicar(
                        mod->nombre, mod->longitud_nombre);
                    dicc_asignar(vm->cache_modulos, clave_cache, val_cache);

                    if (frame->desde_import) {
                        /* v0.9.1: `desde X importar Y, Z` — push módulo
                           al stack como "valor de retorno" para que el
                           código siguiente lea atributos. */
                        valor_destruir(&r);
                        r = valor_modulo(mod);
                        /* mod ya tiene refcount=2 (cache + esto); ok. */
                    } else {
                        /* `importar X [como Y]` — registrar binding global. */
                        const char *binding_name = frame->modulo_binding_name
                            ? frame->modulo_binding_name : mod->nombre;
                        int binding_len = frame->modulo_binding_name
                            ? frame->modulo_binding_len : mod->longitud_nombre;
                        Valor clave_global = valor_cadena_duplicar(
                            binding_name, binding_len);
                        Valor val_global = valor_modulo(mod);
                        /* mod ya tiene refcount=2 (cache + 1 implícita); usamos
                           la implícita para esta global, no añadimos retain. */
                        dicc_asignar(vm->globales, clave_global, val_global);
                        valor_destruir(&r);
                        r = valor_nulo();
                    }
                    /* Liberar el binding_name buffer del frame. */
                    if (frame->modulo_binding_name) {
                        free(frame->modulo_binding_name);
                        frame->modulo_binding_name = NULL;
                    }
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
                /* v0.9.1: dos operandos.
                 *   name_idx     → cadena con el nombre real del módulo
                 *                  (puede tener `.` para subsegmentos).
                 *   binding_idx  → cadena con el nombre de la global del
                 *                  importador (alias o último segmento).
                 * Para `importar X` ambos son `X`.
                 * Para `importar X como Y` son [X, Y].
                 * Para `importar X.Y como Z` son [X.Y, Z]. */
                uint8_t name_idx = LEER_BYTE();
                uint8_t binding_idx = LEER_BYTE();
                const Valor *nombre = &frame->chunk->constantes[name_idx];
                const Valor *binding = &frame->chunk->constantes[binding_idx];
                if (nombre->tipo != VAL_CADENA || binding->tipo != VAL_CADENA) {
                    VM_ERROR("estado interno corrupto: operandos de OP_IMPORTAR no son cadenas");
                    return VM_ERROR_RUNTIME;
                }
                /* 1. Cache hit: solo asignar la global con el binding. */
                Valor cached;
                if (dicc_obtener(vm->cache_modulos, nombre, &cached)) {
                    Valor clave = valor_clonar(binding);
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
                /* Guardar el nombre de binding (heap-dup) para usarlo
                   al finalizar el módulo en OP_RETORNAR. */
                int blen = binding->como.cadena.longitud;
                fr_mod->modulo_binding_name = (char *)malloc((size_t)blen + 1);
                if (fr_mod->modulo_binding_name) {
                    memcpy(fr_mod->modulo_binding_name,
                           binding->como.cadena.texto, (size_t)blen);
                    fr_mod->modulo_binding_name[blen] = '\0';
                }
                fr_mod->modulo_binding_len = blen;
                fr_mod->desde_import = false;
                /* Cambiar a las globales del módulo. */
                vm->globales = globales_modulo;
                frame = fr_mod;
                break;
            }

            case OP_IMPORTAR_PARA_DESDE: {
                /* Como OP_IMPORTAR pero al finalizar deja el módulo en
                   el tope del stack en lugar de registrarlo como global.
                   Usado por `desde X importar Y, Z` para extraer
                   atributos sin contaminar el namespace del importador
                   con el nombre del módulo. */
                uint8_t name_idx = LEER_BYTE();
                const Valor *nombre = &frame->chunk->constantes[name_idx];
                if (nombre->tipo != VAL_CADENA) {
                    VM_ERROR("estado interno corrupto: nombre de modulo no es cadena");
                    return VM_ERROR_RUNTIME;
                }
                /* Cache hit: solo empujar la copia. */
                Valor cached;
                if (dicc_obtener(vm->cache_modulos, nombre, &cached)) {
                    empujar(vm, cached);
                    break;
                }
                /* Cache miss: cargar archivo y compilar. */
                Chunk *ch_mod = cargar_modulo_desde_archivo(
                    nombre->como.cadena.texto, nombre->como.cadena.longitud);
                if (!ch_mod) {
                    VM_ERROR("ErrorDeImportacion: no se pudo cargar el modulo '%.*s' (archivo no encontrado o invalido)",
                             nombre->como.cadena.longitud, nombre->como.cadena.texto);
                    return VM_ERROR_RUNTIME;
                }
                Modulo *mod = modulo_nuevo(nombre->como.cadena.texto,
                                             nombre->como.cadena.longitud);
                if (!mod) {
                    chunk_destruir(ch_mod); free(ch_mod);
                    VM_ERROR("memoria insuficiente al crear modulo");
                    return VM_ERROR_RUNTIME;
                }
                Diccionario *globales_modulo = dicc_nuevo();
                if (!globales_modulo) {
                    modulo_liberar(mod);
                    chunk_destruir(ch_mod); free(ch_mod);
                    VM_ERROR("memoria insuficiente al crear globales del modulo");
                    return VM_ERROR_RUNTIME;
                }
                nativos_registrar_dicc(globales_modulo);
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
                fr_mod->modulo_binding_name = NULL;
                fr_mod->modulo_binding_len = 0;
                fr_mod->desde_import = true;   /* clave: pushea al stack al finalizar */
                vm->globales = globales_modulo;
                frame = fr_mod;
                break;
            }

            case OP_DUP: {
                if (vm->tope == vm->pila) {
                    VM_ERROR("OP_DUP sobre stack vacio");
                    return VM_ERROR_RUNTIME;
                }
                empujar(vm, valor_clonar(&vm->tope[-1]));
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
            case OP_OBTENER_GLOBAL_CACHE: {
                /*
                 * Fast path quickened (F10). Layout 6 bytes:
                 *   [opcode][name_idx][ver_hi][ver_lo][slot_hi][slot_lo]
                 *
                 * Si los 16 bits bajos de vm->globales->version coinciden
                 * con cache_ver Y el slot sigue ocupado, leemos
                 * entradas[slot_idx] directamente — sin hashing, sin
                 * probing, sin clonar la clave. Miss → degradar el
                 * opcode a OP_OBTENER_GLOBAL y rebobinar ip para que
                 * la siguiente iteración del loop ejecute el slow path.
                 */
                const uint8_t *opcode_addr = frame->ip - 1;
                uint8_t name_idx = LEER_BYTE();
                (void)name_idx;  /* solo lo lee el slow path */
                uint8_t v_hi = LEER_BYTE();
                uint8_t v_lo = LEER_BYTE();
                uint16_t cached_ver = ((uint16_t)v_hi << 8) | (uint16_t)v_lo;
                uint8_t s_hi = LEER_BYTE();
                uint8_t s_lo = LEER_BYTE();
                uint16_t cached_slot = ((uint16_t)s_hi << 8) | (uint16_t)s_lo;

                Diccionario *d = vm->globales;
                if ((uint16_t)d->version == cached_ver
                    && cached_slot < (uint16_t)d->capacidad
                    && d->entradas[cached_slot].ocupada) {
                    empujar(vm, valor_clonar(&d->entradas[cached_slot].valor));
                    break;
                }

                /* Miss: degradar el opcode y rebobinar ip. La próxima
                   iteración leerá OP_OBTENER_GLOBAL y rellenará el cache. */
                uint8_t *codigo = (uint8_t *)frame->chunk->codigo;
                int op_offset = (int)(opcode_addr - codigo);
                codigo[op_offset] = (uint8_t)OP_OBTENER_GLOBAL;
                frame->ip = opcode_addr;
                break;
            }
            case OP_OBTENER_GLOBAL: {
                /*
                 * Slow path. Layout 6 bytes idéntico al CACHE — pero
                 * los 4 bytes de cache se leen como zero la primera vez
                 * y los rellenamos al final, promoviendo el opcode.
                 */
                const uint8_t *opcode_addr = frame->ip - 1;
                uint8_t idx = LEER_BYTE();
                /* Saltar los 4 bytes de cache reservados — los rellenamos
                   abajo si la búsqueda tiene éxito y el slot cabe en u16. */
                frame->ip += 4;
                const Valor *nombre = &frame->chunk->constantes[idx];
                Valor v;
                int slot_idx;
                if (!dicc_obtener_y_slot(vm->globales, nombre, &v, &slot_idx)) {
                    vm->error.tuvo_error = true;
                    vm->error.linea = linea_actual_frame(frame);
                    snprintf(vm->error.mensaje, sizeof(vm->error.mensaje),
                        "ErrorDeNombre: nombre '%.*s' no esta definido",
                        nombre->como.cadena.longitud,
                        nombre->como.cadena.texto);
                    return VM_ERROR_RUNTIME;
                }
                /*
                 * Rellenar cache y promover a OP_OBTENER_GLOBAL_CACHE si
                 * el slot_idx cabe en u16. Capacidades >65535 (= >32k
                 * globales) son irrealistas pero no rompen — solo nos
                 * quedamos en slow path para esa entrada.
                 */
                if (slot_idx >= 0 && slot_idx <= UINT16_MAX) {
                    uint8_t *codigo = (uint8_t *)frame->chunk->codigo;
                    int op_offset = (int)(opcode_addr - codigo);
                    uint16_t ver = (uint16_t)vm->globales->version;
                    codigo[op_offset]     = (uint8_t)OP_OBTENER_GLOBAL_CACHE;
                    /* codigo[op_offset+1] = name_idx — ya está bien. */
                    codigo[op_offset + 2] = (uint8_t)((ver >> 8) & 0xff);
                    codigo[op_offset + 3] = (uint8_t)(ver & 0xff);
                    codigo[op_offset + 4] = (uint8_t)((slot_idx >> 8) & 0xff);
                    codigo[op_offset + 5] = (uint8_t)(slot_idx & 0xff);
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
                    if (!valor_es_entero(&key) && key.tipo != VAL_BOOLEANO) {
                        VM_ERROR("ErrorDeTipo: indice de lista debe ser entero, no '%s'",
                                 valor_nombre_tipo(&key));
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    Lista *l = obj.como.lista;
                    long i;
                    if (key.tipo == VAL_BOOLEANO) i = key.como.booleano ? 1 : 0;
                    else {
                        int64_t i64;
                        if (!valor_entero_a_i64(&key, &i64)) i = LONG_MAX;
                        else { i = (long)i64; if ((int64_t)i != i64) i = LONG_MAX; }
                    }
                    if (i < 0) i += l->cuenta;
                    if (i < 0 || i >= l->cuenta) {
                        VM_ERROR("ErrorDeIndice: indice %ld fuera de rango (lista de %d)",
                                 i, l->cuenta);
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    r = valor_clonar(&l->elementos[i]);
                } else if (obj.tipo == VAL_TUPLA) {
                    if (!valor_es_entero(&key) && key.tipo != VAL_BOOLEANO) {
                        VM_ERROR("ErrorDeTipo: indice de tupla debe ser entero");
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    Tupla *t = obj.como.tupla;
                    long i;
                    if (key.tipo == VAL_BOOLEANO) i = key.como.booleano ? 1 : 0;
                    else {
                        int64_t i64;
                        if (!valor_entero_a_i64(&key, &i64)) i = LONG_MAX;
                        else { i = (long)i64; if ((int64_t)i != i64) i = LONG_MAX; }
                    }
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
                } else if (obj.tipo == VAL_CADENA) {
                    /* v0.9.1: indexación UTF-8 sobre cadenas.
                       Devuelve una cadena de 1 carácter. Índices
                       negativos cuentan desde el final.  */
                    if (!valor_es_entero(&key) && key.tipo != VAL_BOOLEANO) {
                        VM_ERROR("ErrorDeTipo: indice de cadena debe ser entero, no '%s'",
                                 valor_nombre_tipo(&key));
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    long i;
                    if (key.tipo == VAL_BOOLEANO) i = key.como.booleano ? 1 : 0;
                    else {
                        int64_t i64;
                        if (!valor_entero_a_i64(&key, &i64)) i = LONG_MAX;
                        else { i = (long)i64; if ((int64_t)i != i64) i = LONG_MAX; }
                    }

                    int len_bytes = obj.como.cadena.longitud;
                    const char *texto = obj.como.cadena.texto;

                    /* Si i < 0, contar el número total de caracteres
                       para resolverlo a positivo. */
                    if (i < 0) {
                        int n_chars = 0;
                        int p = 0;
                        while (p < len_bytes) {
                            utf8proc_int32_t cp;
                            utf8proc_ssize_t cons = utf8proc_iterate(
                                (const utf8proc_uint8_t *)(texto + p),
                                len_bytes - p, &cp);
                            if (cons <= 0) break;
                            p += (int)cons; n_chars++;
                        }
                        i += n_chars;
                    }
                    if (i < 0) {
                        VM_ERROR("ErrorDeIndice: indice fuera de rango para cadena");
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }

                    /* Avanzar i caracteres. */
                    int p = 0;
                    long count = 0;
                    while (p < len_bytes && count < i) {
                        utf8proc_int32_t cp;
                        utf8proc_ssize_t cons = utf8proc_iterate(
                            (const utf8proc_uint8_t *)(texto + p),
                            len_bytes - p, &cp);
                        if (cons <= 0) break;
                        p += (int)cons; count++;
                    }
                    if (count < i || p >= len_bytes) {
                        VM_ERROR("ErrorDeIndice: indice %ld fuera de rango (cadena)", i);
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    utf8proc_int32_t cp;
                    utf8proc_ssize_t cons = utf8proc_iterate(
                        (const utf8proc_uint8_t *)(texto + p),
                        len_bytes - p, &cp);
                    if (cons <= 0) {
                        VM_ERROR("cadena UTF-8 invalida");
                        valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    r = valor_cadena_duplicar(texto + p, (int)cons);
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
                    if (!valor_es_entero(&key) && key.tipo != VAL_BOOLEANO) {
                        VM_ERROR("ErrorDeTipo: indice de lista debe ser entero");
                        valor_destruir(&valor); valor_destruir(&key); valor_destruir(&obj);
                        return VM_ERROR_RUNTIME;
                    }
                    Lista *l = obj.como.lista;
                    long i;
                    if (key.tipo == VAL_BOOLEANO) i = key.como.booleano ? 1 : 0;
                    else {
                        int64_t i64;
                        if (!valor_entero_a_i64(&key, &i64)) i = LONG_MAX;
                        else { i = (long)i64; if ((int64_t)i != i64) i = LONG_MAX; }
                    }
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
                    if (!valor_es_entero(&paso_v) && paso_v.tipo != VAL_BOOLEANO) {
                        VM_ERROR("ErrorDeTipo: paso de rebanada debe ser entero");
                        valor_destruir(&obj); valor_destruir(&inicio_v);
                        valor_destruir(&fin_v); valor_destruir(&paso_v);
                        return VM_ERROR_RUNTIME;
                    }
                    if (paso_v.tipo == VAL_BOOLEANO) {
                        paso = paso_v.como.booleano ? 1 : 0;
                    } else {
                        int64_t i64 = 0;
                        (void)valor_entero_a_i64(&paso_v, &i64);
                        paso = (long)i64;
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
                } else if (valor_es_entero(&inicio_v) || inicio_v.tipo == VAL_BOOLEANO) {
                    if (inicio_v.tipo == VAL_BOOLEANO) {
                        inicio = inicio_v.como.booleano ? 1 : 0;
                    } else {
                        int64_t i64 = 0;
                        (void)valor_entero_a_i64(&inicio_v, &i64);
                        inicio = (long)i64;
                    }
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
                } else if (valor_es_entero(&fin_v) || fin_v.tipo == VAL_BOOLEANO) {
                    if (fin_v.tipo == VAL_BOOLEANO) {
                        fin = fin_v.como.booleano ? 1 : 0;
                    } else {
                        int64_t i64 = 0;
                        (void)valor_entero_a_i64(&fin_v, &i64);
                        fin = (long)i64;
                    }
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
            case OP_FORMATO_F: {
                /* Convierte el TOS a su representación cadena estilo
                   `imprimir` y empuja el resultado. Para enteros bignum
                   se dimensiona el buffer con `mp_radix_size`; el resto
                   cabe holgadamente en 4096 bytes. */
                Valor v = sacar(vm);
                Valor r;
                if (v.tipo == VAL_CADENA) {
                    /* `cadena` → `cadena`: copia profunda (dueña). */
                    r = valor_cadena_duplicar(v.como.cadena.texto,
                                                v.como.cadena.longitud);
                } else if (v.tipo == VAL_ENTERO) {
                    int tam = 0;
                    if (mp_radix_size(v.como.entero, 10, &tam) != MP_OKAY) {
                        valor_destruir(&v);
                        VM_ERROR("memoria insuficiente al formatear entero");
                        return VM_ERROR_RUNTIME;
                    }
                    char *buf = (char *)malloc((size_t)tam);
                    if (!buf) {
                        valor_destruir(&v);
                        VM_ERROR("memoria insuficiente al formatear entero");
                        return VM_ERROR_RUNTIME;
                    }
                    size_t escritos;
                    if (mp_to_radix(v.como.entero, buf, (size_t)tam,
                                      &escritos, 10) != MP_OKAY) {
                        free(buf);
                        valor_destruir(&v);
                        VM_ERROR("error al convertir entero a cadena");
                        return VM_ERROR_RUNTIME;
                    }
                    int n = (int)escritos - 1;
                    if (n < 0) n = 0;
                    r = valor_cadena_duplicar(buf, n);
                    free(buf);
                } else {
                    char buffer[4096];
                    int n = valor_a_cadena(&v, buffer, sizeof(buffer));
                    r = valor_cadena_duplicar(buffer, n);
                }
                valor_destruir(&v);
                if (r.tipo == VAL_NULO) {
                    VM_ERROR("memoria insuficiente al formatear f-cadena");
                    return VM_ERROR_RUNTIME;
                }
                empujar(vm, r);
                break;
            }

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
            case OP_OBTENER_ATRIBUTO_INSTANCIA: {
                /*
                 * Fast path quickened (F10). Layout 6 bytes idéntico al
                 * slow:
                 *   [opcode][name_idx][clase_hash u16][slot_idx u16]
                 *
                 * Verifica que obj es VAL_INSTANCIA, low16(clase) coincide,
                 * el slot está ocupado y la clave guardada es la
                 * esperada. La verificación de clave (cadena memcmp) es
                 * crítica para correctness: instancias de la misma clase
                 * pueden tener atributos distintos si fueron mutadas
                 * dinámicamente.
                 */
                const uint8_t *opcode_addr = frame->ip - 1;
                uint8_t name_idx = LEER_BYTE();
                uint8_t ch_hi = LEER_BYTE();
                uint8_t ch_lo = LEER_BYTE();
                uint16_t cached_clase_hash = ((uint16_t)ch_hi << 8)
                                              | (uint16_t)ch_lo;
                uint8_t s_hi = LEER_BYTE();
                uint8_t s_lo = LEER_BYTE();
                uint16_t cached_slot = ((uint16_t)s_hi << 8) | (uint16_t)s_lo;
                Valor *p_obj = vm->tope - 1;
                if (p_obj->tipo == VAL_INSTANCIA) {
                    Instancia *inst = p_obj->como.instancia;
                    uint16_t live_clase_hash =
                        (uint16_t)((uintptr_t)inst->clase & 0xFFFF);
                    if (live_clase_hash == cached_clase_hash) {
                        Diccionario *attrs = inst->atributos;
                        if (cached_slot < attrs->capacidad
                            && attrs->entradas[cached_slot].ocupada) {
                            Valor *expected =
                                &frame->chunk->constantes[name_idx];
                            if (valor_iguales(
                                    &attrs->entradas[cached_slot].clave,
                                    expected)) {
                                Valor v = valor_clonar(
                                    &attrs->entradas[cached_slot].valor);
                                Valor old = *(--vm->tope);
                                valor_destruir(&old);
                                empujar(vm, v);
                                break;
                            }
                        }
                    }
                }
                /* Miss: degradar a OP_OBTENER_ATRIBUTO y rebobinar. */
                {
                    uint8_t *codigo = (uint8_t *)frame->chunk->codigo;
                    codigo[(int)(opcode_addr - codigo)] =
                        (uint8_t)OP_OBTENER_ATRIBUTO;
                    frame->ip = opcode_addr;
                }
                break;
            }
            case OP_OBTENER_ATRIBUTO: {
                const uint8_t *opcode_addr = frame->ip - 1;
                uint8_t idx = LEER_BYTE();
                /* Saltar 4 bytes de cache reservados — los rellenamos
                   abajo si encontramos el atributo en instancia.atributos. */
                frame->ip += 4;
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
                int slot_idx;
                if (dicc_obtener_y_slot(obj.como.instancia->atributos,
                                          nombre, &v, &slot_idx)) {
                    /* F10: rellenar cache y promover si slot cabe en u16. */
                    if (slot_idx >= 0 && slot_idx <= UINT16_MAX) {
                        uint8_t *codigo = (uint8_t *)frame->chunk->codigo;
                        int op_offset = (int)(opcode_addr - codigo);
                        uint16_t ch = (uint16_t)((uintptr_t)
                            obj.como.instancia->clase & 0xFFFF);
                        codigo[op_offset]     = (uint8_t)OP_OBTENER_ATRIBUTO_INSTANCIA;
                        codigo[op_offset + 2] = (uint8_t)((ch >> 8) & 0xff);
                        codigo[op_offset + 3] = (uint8_t)(ch & 0xff);
                        codigo[op_offset + 4] = (uint8_t)((slot_idx >> 8) & 0xff);
                        codigo[op_offset + 5] = (uint8_t)(slot_idx & 0xff);
                    }
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
                    frame->modulo_binding_name = NULL;
                    frame->modulo_binding_len = 0;
                frame->desde_import = false;
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
            /*
             * F10: variantes especializadas de OP_LLAMAR.
             *
             * Cada *_<TIPO> verifica que el callee es del tipo esperado
             * y, si sí, ejecuta el helper. Si el tipo cambió (sitio
             * polimórfico, raro), reescribe el opcode a OP_LLAMAR
             * (slow path) y rebobina ip para reejecución.
             */
            case OP_LLAMAR_NATIVA: {
                const uint8_t *opcode_addr = frame->ip - 1;
                uint8_t n_args = LEER_BYTE();
                Valor *base_nuevo = vm->tope - n_args - 1;
                if (base_nuevo->tipo != VAL_NATIVA) {
                    DEGRADAR_LLAMAR();
                    break;
                }
                if (ejecutar_llamar_nativa(vm, frame, base_nuevo, n_args)
                    != VM_OK) {
                    return VM_ERROR_RUNTIME;
                }
                break;
            }
            case OP_LLAMAR_BC: {
                const uint8_t *opcode_addr = frame->ip - 1;
                uint8_t n_args = LEER_BYTE();
                Valor *base_nuevo = vm->tope - n_args - 1;
                if (base_nuevo->tipo != VAL_FUNCION_BC) {
                    DEGRADAR_LLAMAR();
                    break;
                }
                if (ejecutar_llamar_bc(vm, &frame, base_nuevo, n_args)
                    != VM_OK) {
                    return VM_ERROR_RUNTIME;
                }
                break;
            }
            case OP_LLAMAR_CLASE: {
                const uint8_t *opcode_addr = frame->ip - 1;
                uint8_t n_args = LEER_BYTE();
                Valor *base_nuevo = vm->tope - n_args - 1;
                if (base_nuevo->tipo != VAL_CLASE) {
                    DEGRADAR_LLAMAR();
                    break;
                }
                if (ejecutar_llamar_clase(vm, &frame, base_nuevo, n_args)
                    != VM_OK) {
                    return VM_ERROR_RUNTIME;
                }
                break;
            }
            case OP_LLAMAR_METODO_LIGADO: {
                const uint8_t *opcode_addr = frame->ip - 1;
                uint8_t n_args = LEER_BYTE();
                Valor *base_nuevo = vm->tope - n_args - 1;
                if (base_nuevo->tipo != VAL_METODO_LIGADO) {
                    DEGRADAR_LLAMAR();
                    break;
                }
                if (ejecutar_llamar_metodo_ligado(vm, &frame, base_nuevo,
                                                    n_args) != VM_OK) {
                    return VM_ERROR_RUNTIME;
                }
                break;
            }
            case OP_LLAMAR: {
                /*
                 * Slow path. Despacha por tipo y, tras éxito, promueve
                 * el opcode a la variante especializada para que los
                 * próximos hits del mismo site bypaseen este switch.
                 *
                 * Capturamos `caller_codigo` ANTES de invocar los
                 * helpers porque BC/CLASE/METODO_LIGADO empujan un
                 * frame nuevo y cambian `frame->chunk` por el callee.
                 * La promoción debe escribirse en el chunk del CALLER.
                 */
                const uint8_t *opcode_addr = frame->ip - 1;
                uint8_t *caller_codigo = (uint8_t *)frame->chunk->codigo;
                uint8_t n_args = LEER_BYTE();
                Valor *base_nuevo = vm->tope - n_args - 1;
                Valor callee = *base_nuevo;
                OpCode promote = OP_LLAMAR;

                switch (callee.tipo) {
                    case VAL_NATIVA:
                        if (ejecutar_llamar_nativa(vm, frame, base_nuevo,
                                                     n_args) != VM_OK)
                            return VM_ERROR_RUNTIME;
                        promote = OP_LLAMAR_NATIVA;
                        break;
                    case VAL_FUNCION_BC:
                        if (ejecutar_llamar_bc(vm, &frame, base_nuevo,
                                                 n_args) != VM_OK)
                            return VM_ERROR_RUNTIME;
                        promote = OP_LLAMAR_BC;
                        break;
                    case VAL_CLASE:
                        if (ejecutar_llamar_clase(vm, &frame, base_nuevo,
                                                    n_args) != VM_OK)
                            return VM_ERROR_RUNTIME;
                        promote = OP_LLAMAR_CLASE;
                        break;
                    case VAL_METODO_LIGADO:
                        if (ejecutar_llamar_metodo_ligado(vm, &frame,
                                base_nuevo, n_args) != VM_OK)
                            return VM_ERROR_RUNTIME;
                        promote = OP_LLAMAR_METODO_LIGADO;
                        break;
                    default:
                        VM_ERROR("ErrorDeTipo: '%s' no es invocable",
                                 valor_nombre_tipo(&callee));
                        return VM_ERROR_RUNTIME;
                }
                caller_codigo[(int)(opcode_addr - caller_codigo)] = (uint8_t)promote;
                break;
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
