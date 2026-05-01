#ifndef CORNAMUSA_VALOR_H
#define CORNAMUSA_VALOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "memoria.h"
#include "tommath.h"

/*
 * Representación de valores en runtime.
 *
 * En esta versión (Fase 4 v0.4.0, decisión B3):
 *   - Enteros son bignum boxed con libtommath (precisión arbitraria
 *     desde día 1, sin overflow).
 *   - Decimales son IEEE 754 double.
 *   - Booleanos, nulo, cadena son simples.
 *   - Funciones definidas por el usuario y nativas son punteros.
 *
 * En Fase 6 con la VM bytecode, esta representación se transforma en
 * tagged i63 + bignum (probable NaN-boxing). Toda la semántica
 * exterior es la misma — solo cambia performance.
 *
 * Las cadenas almacenan puntero al buffer fuente sin copiar mientras
 * el AST esté vivo. Operaciones que producen cadenas nuevas (como
 * concatenación) alocan en heap. Esta sesión 1 no implementa esas
 * operaciones todavía.
 *
 * Sin GC en Fase 4-5: el cliente llama `valor_destruir` cuando un
 * Valor sale de scope. El Environment es responsable de liberar sus
 * Valores al destruirse.
 */

/* Forward decls. `Sent` vive en ast.h pero no queremos incluirlo aquí
   para no crear un ciclo (ast.h ya incluye lexer.h y arena.h). */
struct Sent;
struct Entorno;
struct Evaluador;
struct EvalError;

typedef enum {
    VAL_NULO,
    VAL_BOOLEANO,
    /*
     * Enteros — DOS representaciones desde v0.11 (decisión B9):
     *
     *   VAL_ENTERO_SMALL: int64_t inline en `como.entero_small`. Para
     *     enteros que caben en [-2^62, 2^62). Sin allocación.
     *   VAL_ENTERO:       mp_int * en `como.entero`. Para enteros más
     *     grandes (o resultados intermedios que se demoten cuando
     *     pasen por una operación que los normalice).
     *
     * NUNCA se accede a `como.entero` sin haber verificado primero que
     * `v.tipo == VAL_ENTERO`. Para "es algún entero" usar el helper
     * `valor_es_entero(&v)`. Para extraer el valor numérico usar
     * `valor_entero_a_i64` o `valor_entero_a_mp_int`. Acceso directo
     * está restringido a valor.c y a hot paths del IC en vm.c.
     */
    VAL_ENTERO,
    VAL_ENTERO_SMALL,
    VAL_DECIMAL,       /* double IEEE 754 */
    VAL_CADENA,        /* texto UTF-8, ref al buffer fuente o heap */
    VAL_FUNCION,       /* función definida por el usuario (tree-walking) */
    VAL_NATIVA,        /* función nativa (built-in en C) */
    VAL_RANGO,         /* iterable rango(inicio, fin, paso) */
    VAL_LISTA,         /* array dinámico de Valor con refcount */
    VAL_DICCIONARIO,   /* tabla hash de Valor → Valor con refcount */
    VAL_CONJUNTO,      /* tabla hash de Valor sin valor con refcount */
    VAL_TUPLA,         /* secuencia inmutable de Valor con refcount */
    VAL_FUNCION_BC,    /* closure ejecutable (plantilla + upvalues) */
    VAL_PLANTILLA_BC,  /* plantilla de función (en constant pool, sin upvalues) */
    VAL_ITERADOR,      /* iterador interno (uso VM-only para `para`) */
    VAL_EXCEPCION,     /* excepción runtime con clase + mensaje */
    VAL_CLASE,         /* clase definida por el usuario (Fase 8) */
    VAL_INSTANCIA,     /* instancia de una clase (Fase 8) */
    VAL_METODO_LIGADO, /* método con receptor ligado (Fase 8 S2) */
    VAL_MODULO,        /* módulo cargado via `importar` (Fase 9) */
} TipoValor;

/* Forward decls de tipos coleccion. La definición completa va después
   del typedef de Valor. */
typedef struct Lista Lista;
typedef struct Diccionario Diccionario;
typedef struct Conjunto Conjunto;
typedef struct Tupla Tupla;
typedef struct FuncionBC FuncionBC;
typedef struct Iterador Iterador;
typedef struct Closure Closure;
typedef struct Upvalue Upvalue;
typedef struct Excepcion Excepcion;
typedef struct Clase Clase;
typedef struct Instancia Instancia;
typedef struct MetodoLigado MetodoLigado;
typedef struct Modulo Modulo;

/*
 * Firma de una función nativa (puntero a función C). Recibe un
 * `EvalError *` para reportar errores de runtime (preserva el primer
 * error ya activo si lo hubiese), los argumentos ya evaluados (el
 * llamador conserva ownership y los destruye al finalizar la
 * llamada) y la posición del call-site. Devuelve el Valor resultado
 * del que el llamador toma posesión.
 *
 * Refactor en F6 S6: antes recibía `Evaluador *ev`. Ahora `EvalError *`
 * hace que las nativas puedan invocarse tanto desde el evaluador
 * tree-walking (`&ev->error`) como desde la VM bytecode (`&vm->error`)
 * sin acoplarlas a una de las dos.
 */
typedef struct Valor (*FnNativa)(struct EvalError *err, int n_args,
                                  struct Valor *args,
                                  int linea, int columna);

typedef struct Valor {
    TipoValor tipo;
    /*
     * Para VAL_CADENA: si dueño_cadena=true, valor.cadena.texto fue
     * alocado con malloc y debe liberarse en valor_destruir.
     * Si false, apunta al buffer fuente (no se libera).
     */
    bool dueno_cadena;
    union {
        bool booleano;
        mp_int *entero;            /* VAL_ENTERO: malloc'd; liberado en valor_destruir */
        int64_t entero_small;      /* VAL_ENTERO_SMALL: inline (B9, v0.11) */
        double decimal;
        struct {
            const char *texto;     /* UTF-8, NO terminado en \0 necesariamente */
            int longitud;          /* en bytes */
        } cadena;
        /*
         * VAL_FUNCION: referencia a un nodo SENT_FUNCION del AST + el
         * entorno donde se definió. Sin closures (decisión B2) el
         * entorno_definicion es siempre el global; se reserva el campo
         * para Fase 6+ con closures verdaderas.
         *
         * `def` es propiedad del Arena del parser — la función Valor
         * NO toma posesión, simplemente referencia. Por eso clonar es
         * trivial (copia la struct) y destruir es un no-op.
         */
        struct {
            const struct Sent *def;
            struct Entorno *entorno_definicion;
        } funcion;
        /*
         * VAL_NATIVA: nombre + puntero a función C. Estructura inline
         * para ahorrar una indirección. La función nativa típicamente
         * se registra una sola vez y vive todo el programa.
         */
        struct {
            const char *nombre;
            FnNativa fn;
        } nativa;
        /*
         * VAL_RANGO: tres mp_int* dueños (mismo patrón que VAL_ENTERO).
         * Itera de `inicio` a `fin` (excluido) con `paso`. paso > 0 va
         * ascendente, paso < 0 descendente. paso == 0 es ilegal y se
         * detecta al construir.
         */
        struct {
            mp_int *inicio;
            mp_int *fin;
            mp_int *paso;
        } rango;
        Lista *lista;       /* refcount; ver Lista más abajo */
        Diccionario *dicc;  /* refcount; ver Diccionario más abajo */
        Conjunto *conjunto; /* refcount */
        Tupla *tupla;       /* refcount, inmutable */
        Closure *closure;   /* refcount; función compilada a bytecode con upvalues */
        FuncionBC *plantilla; /* refcount; plantilla en constant pool */
        Iterador *iterador; /* uso VM-only; vida corta en stack */
        Excepcion *excepcion; /* refcount; excepción runtime */
        Clase *clase;       /* refcount; clase definida por el usuario */
        Instancia *instancia; /* refcount; instancia de una clase */
        MetodoLigado *metodo_ligado; /* refcount; método con receptor */
        Modulo *modulo;     /* refcount; módulo cargado */
    } como;
} Valor;

/*
 * Lista mutable con referencia compartida (decisión Fase 5):
 *   x = [1, 2, 3]
 *   y = x         # mismo objeto, no copia
 *   y[0] = 99     # x[0] también es 99
 *
 * Sin GC todavía (Fase 7) implementamos refcount manual:
 *   - `valor_clonar(v)` con v VAL_LISTA hace `lista_retener` (++ref).
 *   - `valor_destruir(v)` con v VAL_LISTA hace `lista_liberar` (--ref;
 *     libera la lista entera si refcount llega a 0).
 *
 * Esto NO cubre ciclos (lista que se contiene a sí misma): se filtran
 * memoria. Es aceptable hasta Fase 7, que añadirá mark-sweep real.
 */
struct Lista {
    GCObject obj;           /* Fase 7 S1: header GC; debe ser el primer campo. */
    Valor *elementos;       /* array contiguo de Valores; cada slot dueño */
    int cuenta;
    int capacidad;
    int refcount;
};

Lista *lista_nueva(int capacidad_inicial);
void lista_retener(Lista *l);
void lista_liberar(Lista *l);

/* Añade un elemento al final, transfiriendo posesión. Devuelve true si OK. */
bool lista_agregar(Lista *l, Valor v);

/* Devuelve un puntero al slot interno (para lectura/escritura). NULL si
   el índice está fuera de rango. NO clona el valor. */
Valor *lista_obtener_ref(Lista *l, int indice);

/* Reemplaza el elemento en `indice`, destruyendo el anterior y tomando
   posesión del nuevo. Devuelve false si índice fuera de rango. */
bool lista_asignar(Lista *l, int indice, Valor v);

/*
 * Diccionario: mapa Valor → Valor implementado como tabla hash con
 * probing lineal (estilo clox cap. 20). Capacidad potencia de 2,
 * factor de carga 0.75. Comparte la misma estrategia de refcount
 * manual que Lista (ver doc arriba).
 *
 * Solo tipos hashables como clave: nulo, booleano, entero, decimal,
 * cadena, función. NO admite lista/diccionario/conjunto como clave.
 * Si `a == b` entonces `hash(a) == hash(b)` para que `dicc[1.0]` y
 * `dicc[1]` accedan al mismo slot.
 */
typedef struct EntradaDicc {
    Valor clave;
    Valor valor;
    bool ocupada;
} EntradaDicc;

struct Diccionario {
    GCObject obj;           /* Fase 7 S2: header GC; primer campo. */
    EntradaDicc *entradas;
    int cuenta;
    int capacidad;
    int refcount;
    /*
     * Contador monotónico que se incrementa en cada cambio
     * estructural (inserción de clave nueva, borrado, redimensionado).
     * NO se incrementa al sobreescribir el valor de una clave existente
     * — deliberado: caches del IC (F10) conservan su validez para
     * variables que cambian de valor sin cambiar de slot.
     *
     * Lo consume el inline cache de OP_OBTENER_GLOBAL_CACHE: el cache
     * slot guarda los 16 bits bajos de esta versión vista en el último
     * acierto, y compara en cada hit. Wraparound a 65k mutaciones
     * causa colisión teórica de versión + key_match tras rehash con
     * probabilidad ≈1/2^32 — irrelevante.
     */
    uint64_t version;
};

Diccionario *dicc_nuevo(void);
void dicc_retener(Diccionario *d);
void dicc_liberar(Diccionario *d);

/* Comprueba si un Valor puede usarse como clave. Listas/dicc/conjunto
   no son hashables. */
bool valor_es_hashable(const Valor *v);

/* Inserta o actualiza. Toma posesión de `clave` y `valor`. Devuelve
   true si OK; false si OOM o clave no hashable. */
bool dicc_asignar(Diccionario *d, Valor clave, Valor valor);

/* Busca por clave. Si existe, copia un clon del valor en `*out` y
   devuelve true. Si no, deja `*out` sin tocar y devuelve false. */
bool dicc_obtener(const Diccionario *d, const Valor *clave, Valor *out);

/*
 * Como dicc_obtener pero además devuelve el índice del slot en
 * `entradas` donde se encontró la clave. Lo usa el inline cache de
 * `OP_OBTENER_GLOBAL` (F10) para que el siguiente acierto pueda leer
 * `entradas[slot_idx]` directamente sin re-buscar. El slot_idx es
 * válido mientras `d->version` no cambie.
 */
bool dicc_obtener_y_slot(const Diccionario *d, const Valor *clave,
                          Valor *out, int *out_slot_idx);

/* Devuelve true si la clave está presente. */
bool dicc_contiene(const Diccionario *d, const Valor *clave);

/* Elimina la entrada y devuelve el valor en `*out` (con ownership).
   Devuelve false si la clave no estaba. */
bool dicc_quitar(Diccionario *d, const Valor *clave, Valor *out);

/*
 * Conjunto: hash set sobre Valor. Solo elementos hashables. Comparte
 * la estrategia de probing lineal y refcount con Diccionario.
 */
typedef struct EntradaConjunto {
    Valor elemento;
    bool ocupada;
} EntradaConjunto;

struct Conjunto {
    GCObject obj;           /* Fase 7 S2: header GC; primer campo. */
    EntradaConjunto *entradas;
    int cuenta;
    int capacidad;
    int refcount;
};

Conjunto *conj_nuevo(void);
void conj_retener(Conjunto *c);
void conj_liberar(Conjunto *c);

/* Añade `v` al conjunto. Toma posesión. Si ya estaba, libera el
   nuevo elemento (mantiene el original). Devuelve true salvo OOM. */
bool conj_agregar(Conjunto *c, Valor v);

bool conj_contiene(const Conjunto *c, const Valor *v);

/* Elimina un elemento. Devuelve true si estaba. */
bool conj_quitar(Conjunto *c, const Valor *v);

Valor valor_conjunto(Conjunto *c);

/*
 * Tupla: secuencia inmutable de Valor. Como toda la lista de
 * elementos se fija al construirla (parser), reservamos exactamente
 * `cuenta` slots. Cualquier "modificación" requiere construir una
 * tupla nueva. Hashable si todos los elementos son hashables.
 */
struct Tupla {
    GCObject obj;           /* Fase 7 S2: header GC; primer campo. */
    Valor *elementos;
    int cuenta;
    int refcount;
};

Tupla *tupla_nueva(int cuenta);   /* aloca elementos sin inicializar */
void tupla_retener(Tupla *t);
void tupla_liberar(Tupla *t);

Valor valor_tupla(Tupla *t);

/*
 * Iterador VM-only (Fase 6 / v0.6.1).
 *
 * Cornamusa no expone este tipo al usuario — es la representación
 * intermedia que la VM bytecode usa para implementar `para X en Y`.
 * Vive solo en la pila de la VM y nunca se hashea, asigna a globales
 * ni clona profundamente.
 *
 * Estado:
 *   - `iterable`: copia con refcount del iterable original (lista,
 *     tupla, diccionario, conjunto, cadena, rango).
 *   - `cursor`: posición en bytes (cadena), índice (lista/tupla/rango)
 *     o slot interno (diccionario/conjunto).
 */
struct Iterador {
    GCObject obj;           /* Fase 7 S2: header GC; primer campo. */
    Valor iterable;
    int cursor;
};

bool valor_es_iterable(const Valor *v);

Iterador *iter_nuevo(const Valor *iterable);
void iter_destruir(Iterador *it);

/* Obtiene el siguiente valor (con ownership). Devuelve true si quedaba
   uno; false si se agotó (en cuyo caso `*out` queda en VAL_NULO). */
bool iter_siguiente(Iterador *it, Valor *out);

Valor valor_iterador(Iterador *it);

/*
 * Excepción runtime: clase (cadena) + mensaje (cadena). v0.6.3 usa
 * un modelo simple de pares cadena→cadena; cuando lleguen las clases
 * (Fase 8) se reemplazará por instancias de la clase `Excepcion`.
 */
struct Excepcion {
    GCObject obj;           /* Fase 7 S2: header GC; primer campo. */
    char *clase;          /* heap-duplicated; se libera con la struct */
    int longitud_clase;
    char *mensaje;        /* heap-duplicated */
    int longitud_mensaje;
    int refcount;
};

Excepcion *excepcion_nueva(const char *clase, int len_clase,
                            const char *mensaje, int len_mensaje);
void excepcion_retener(Excepcion *e);
void excepcion_liberar(Excepcion *e);

Valor valor_excepcion(Excepcion *e);

/*
 * Clase definida por el usuario (Fase 8 v0.7.0).
 *
 * Una `Clase` se construye en runtime al ejecutar `clase Foo: ... fin clase`.
 * Tiene un nombre y (a partir de F8 S2) una tabla de métodos. En la
 * sesión 1 los métodos están vacíos: la clase solo permite crear
 * instancias con atributos asignables desde fuera (`obj.x = 1`).
 *
 * `metodos` es siempre un Diccionario válido (cadena → VAL_FUNCION_BC),
 * pero en S1 se construye vacío.
 */
struct Clase {
    GCObject obj;             /* Fase 7 S2: header GC; primer campo. */
    char *nombre;             /* heap-duplicated; se libera con la struct */
    int longitud_nombre;
    Diccionario *metodos;     /* dicc cadena → VAL_FUNCION_BC; poblado por OP_METODO */
    /* Superclase opcional (Fase 8 S4 v0.7.0). Solo herencia simple en v0.7.0;
       el parser admite múltiples supers pero el compilador rechaza más de una.
       Los métodos heredados se copian en `metodos` al ejecutar OP_HEREDAR
       (no walking de la cadena en cada lookup). */
    Clase *superclase;        /* refcount; NULL si no hereda */
    int refcount;
};

Clase *clase_nueva(const char *nombre, int len_nombre);
void clase_retener(Clase *c);
void clase_liberar(Clase *c);

Valor valor_clase(Clase *c);

/*
 * Busca un método por nombre en la tabla `metodos` de la clase. Útil
 * para invocar dunders (`__sumar__`, `__cadena__`, ...) y constructor
 * (`__iniciar__`). NO sube por la cadena de superclases — los métodos
 * heredados se copian al `metodos` propio en `OP_HEREDAR`.
 *
 * Devuelve el `Closure *` del método si existe, NULL si no o si la
 * entrada no es VAL_FUNCION_BC (caso patológico).
 *
 * El refcount NO se incrementa — el llamador debe `closure_retener`
 * si guarda la referencia más allá de la vida de la clase.
 */
Closure *clase_obtener_metodo(const Clase *cl, const char *nombre, int len);

/*
 * Instancia de una clase (Fase 8 v0.7.0).
 *
 * Mantiene una referencia compartida a su `Clase` (refcount) y un
 * Diccionario propio de atributos modificables. La identidad de
 * instancia es por puntero (`a is b` solo si misma struct).
 *
 * En esta sesión los atributos se asignan exclusivamente con
 * `obj.attr = valor` desde código de usuario; no hay constructor
 * `__iniciar__` todavía.
 */
struct Instancia {
    GCObject obj;             /* Fase 7 S2: header GC; primer campo. */
    Clase *clase;             /* referencia compartida (refcount) */
    Diccionario *atributos;   /* dicc cadena → Valor */
    int refcount;
};

Instancia *instancia_nueva(Clase *c);
void instancia_retener(Instancia *i);
void instancia_liberar(Instancia *i);

Valor valor_instancia(Instancia *i);

/*
 * Método ligado: asocia un Closure (el método compilado de la clase)
 * con un receptor concreto (la instancia sobre la que se invoca). Se
 * crea cada vez que se accede a `instancia.metodo` (cuando `metodo`
 * está en `clase.metodos` y no como atributo de instancia).
 *
 * Al llamarlo (`OP_LLAMAR` con un `VAL_METODO_LIGADO`), la VM inserta
 * el receptor como primer argumento del frame, de modo que el primer
 * parámetro de la función (convencionalmente `yo`) lo recibe sin que
 * el llamador lo escriba.
 */
struct MetodoLigado {
    GCObject obj;         /* Fase 7 S2: header GC; primer campo. */
    Valor receptor;       /* normalmente VAL_INSTANCIA; ownership con refcount */
    Closure *metodo;      /* refcount compartido con la clase */
    int refcount;
};

MetodoLigado *metodo_ligado_nuevo(const Valor *receptor, Closure *metodo);
void metodo_ligado_retener(MetodoLigado *m);
void metodo_ligado_liberar(MetodoLigado *m);

Valor valor_metodo_ligado(MetodoLigado *m);

/*
 * Módulo cargado via `importar` (Fase 9).
 *
 * Un módulo encapsula:
 *   - nombre: cadena heap-duplicada (la usada en la sentencia importar).
 *   - atributos: Diccionario propio que el módulo construyó al
 *     ejecutarse (sus globales). Se accede via `modulo.atributo` con
 *     OP_OBTENER_ATRIBUTO (que despacha sobre VAL_MODULO igual que
 *     sobre VAL_INSTANCIA, mirando este Diccionario).
 *
 * El módulo es propietario de su Diccionario y lo libera al destruirse.
 */
struct Modulo {
    GCObject obj;
    char *nombre;
    int longitud_nombre;
    Diccionario *atributos;
    int refcount;
};

Modulo *modulo_nuevo(const char *nombre, int len_nombre);
void modulo_retener(Modulo *m);
void modulo_liberar(Modulo *m);

Valor valor_modulo(Modulo *m);

/* ──────────────────────────────────────────────────────────────────
 * Constructores
 * ────────────────────────────────────────────────────────────────── */

Valor valor_nulo(void);
Valor valor_booleano(bool v);
Valor valor_decimal(double v);

/*
 * Construye un VAL_ENTERO desde un literal de fuente Cornamusa
 * (ej. "42", "0xff", "0b1010", "1_000_000"). Devuelve el Valor con
 * mp_int recién alocado. Si el lexema no es válido, devuelve nulo.
 */
Valor valor_entero_de_lexema(const char *lexema, int longitud);

/*
 * Construye un VAL_ENTERO desde un long signed simple. Útil para
 * inicializar contadores, índices y constantes en runtime.
 *
 * v0.11: delega en `valor_entero_de_i64` y por tanto puede devolver
 * VAL_ENTERO_SMALL si `v` cabe en el rango SMALL.
 */
Valor valor_entero_de_long(long v);

/* ──────────────────────────────────────────────────────────────────
 * Small-int tagging (decisión B9, v0.11).
 *
 * API canónica para construir, inspeccionar y consumir enteros de
 * Cornamusa. Cualquier código FUERA de valor.c y de los hot paths del
 * IC en vm.c debe usar estos helpers; el acceso directo a
 * `v.como.entero` o `v.como.entero_small` está restringido.
 * ────────────────────────────────────────────────────────────────── */

/*
 * Rango de SMALL. Reservamos 1 bit de margen respecto a int64_t para
 * que la suma `a + b` con ambos en rango SMALL nunca cause UB en C
 * (cabe en int64_t sin overflow). Tras la suma se comprueba si el
 * resultado cabe de nuevo en SMALL; si no, se promueve a BIG.
 */
#define CORNAMUSA_SMALL_INT_MAX  ((int64_t)0x3FFFFFFFFFFFFFFFLL)  /* 2^62 - 1 */
#define CORNAMUSA_SMALL_INT_MIN  (-CORNAMUSA_SMALL_INT_MAX - 1)   /* -2^62 */

/* True si v es VAL_ENTERO o VAL_ENTERO_SMALL. Sustituye el patrón
   `v->tipo == VAL_ENTERO` que era válido en v0.10 pero no v0.11. */
static inline bool valor_es_entero(const Valor *v) {
    return v->tipo == VAL_ENTERO || v->tipo == VAL_ENTERO_SMALL;
}

/* Si v es entero (SMALL o BIG) y el valor cabe en int64_t, escribe
   en *out y devuelve true. False si no es entero o el BIG no cabe.
   No toma posesión, no muta v. */
bool valor_entero_a_i64(const Valor *v, int64_t *out);

/* Devuelve un mp_int* válido apuntando al valor numérico de v.
 *
 *   - Si v es VAL_ENTERO (BIG): devuelve v->como.entero, *propio = false.
 *     El cliente NO debe liberarlo.
 *   - Si v es VAL_ENTERO_SMALL: aloca un mp_int temporal con el valor,
 *     *propio = true. El cliente DEBE liberar con `evaluador_liberar_mp`
 *     tras usarlo.
 *
 * Devuelve NULL si v no es entero o si la alocación temporal falla. */
mp_int *valor_entero_a_mp_int(const Valor *v, bool *propio);

/* Constructor canónico desde int64_t. Si n cabe en [SMALL_INT_MIN,
   SMALL_INT_MAX] devuelve VAL_ENTERO_SMALL inline. Si no, aloca un
   mp_int y devuelve VAL_ENTERO. */
Valor valor_entero_de_i64(int64_t n);

/* Constructor desde mp_int *, tomando posesión. Si el valor cabe en
   SMALL, libera el mp_int y devuelve VAL_ENTERO_SMALL. Si no, lo
   envuelve directamente en VAL_ENTERO. Útil tras una operación
   bignum cuyo resultado puede normalizar a SMALL.
   El llamador NUNCA debe usar `m` tras esta llamada (puede haber sido
   liberado). */
Valor valor_entero_de_mp_normalizado(mp_int *m);

/*
 * Construye un VAL_DECIMAL desde un literal Cornamusa
 * (ej. "3.14", "1.5e-3"). Acepta `_` como separador.
 */
Valor valor_decimal_de_lexema(const char *lexema, int longitud);

/*
 * Construye una cadena que apunta al buffer fuente sin copiar.
 * No se libera en valor_destruir.
 */
Valor valor_cadena_referencia(const char *texto, int longitud);

/*
 * Construye una cadena duplicando el contenido en heap.
 * Se libera en valor_destruir.
 */
Valor valor_cadena_duplicar(const char *texto, int longitud);

/*
 * Construye una cadena copiando un slice y procesando los escapes
 * canónicos (`\n`, `\t`, `\r`, `\0`, `\\`, `\"`, `\'`). Para escapes
 * no reconocidos copia el carácter siguiente literal.
 *
 * Usado por:
 *   - El compilador y el evaluador para `EXPR_LITERAL_CADENA` (con el
 *     lexema sin las comillas).
 *   - Las partes literales de `EXPR_LITERAL_F_CADENA`.
 *
 * Devuelve VAL_NULO ante OOM. El llamador convierte VAL_NULO en un
 * error contextual.
 */
Valor valor_cadena_desde_escapes(const char *src, int srclen);

/*
 * Construye un VAL_FUNCION referenciando un SENT_FUNCION del AST y un
 * entorno de definición. El Valor NO toma posesión — el AST y el
 * entorno deben vivir mientras la función se use.
 */
Valor valor_funcion(const struct Sent *def, struct Entorno *entorno_def);

/*
 * Construye un VAL_NATIVA con nombre estático y puntero a la función C.
 */
Valor valor_nativa(const char *nombre, FnNativa fn);

/*
 * Construye un VAL_RANGO con valores ya en `long`. Útil para `rango(n)`
 * con n pequeño y para tests. Toma posesión interna (aloca tres mp_int).
 */
Valor valor_rango_de_longs(long inicio, long fin, long paso);

/*
 * Construye un VAL_RANGO transferiendo posesión de tres `mp_int *` ya
 * inicializados. Útil cuando los argumentos son bignum.
 */
Valor valor_rango_de_mp(mp_int *inicio, mp_int *fin, mp_int *paso);

/*
 * Construye un VAL_LISTA tomando posesión del refcount del cliente.
 * Si el cliente sigue usando la lista debe llamar `lista_retener`
 * primero.
 */
Valor valor_lista(Lista *l);

/*
 * Construye un VAL_DICCIONARIO tomando posesión del refcount.
 */
Valor valor_diccionario(Diccionario *d);

/*
 * Variante "repr" de la conversión a cadena: añade comillas a las
 * cadenas y formatea las listas anidadas usando repr para sus
 * elementos. Útil al imprimir el contenido de una lista.
 */
int valor_a_repr(const Valor *v, char *buffer, int capacidad);

/* ──────────────────────────────────────────────────────────────────
 * Destrucción y copia
 * ────────────────────────────────────────────────────────────────── */

/*
 * Libera la memoria que el valor posee (mp_int para enteros, buffer
 * para cadenas con dueño, etc.). Idempotente: tras la llamada el
 * Valor queda en estado VAL_NULO seguro de descartar.
 */
void valor_destruir(Valor *v);

/*
 * Crea una copia profunda del Valor: si tiene mp_int, asigna uno
 * nuevo con el mismo valor; si tiene cadena con dueño, duplica el
 * buffer; etc. Tras la copia, el original y la copia son indepen-
 * dientes y deben destruirse por separado.
 */
Valor valor_clonar(const Valor *v);

/* ──────────────────────────────────────────────────────────────────
 * Inspección
 * ────────────────────────────────────────────────────────────────── */

/*
 * Imprime el valor en `salida` en formato tipo Python
 * (ej. 42, 3.14, "hola", verdadero, nulo). Para enteros grandes usa
 * todos los dígitos (sin notación científica).
 */
void valor_imprimir(const Valor *v, FILE *salida);

/*
 * Versión a buffer. Devuelve el número de bytes escritos. Trunca
 * silenciosamente si el buffer es insuficiente.
 */
int valor_a_cadena(const Valor *v, char *buffer, int capacidad);

/*
 * Variante con asignación dinámica: convierte `v` a su representación
 * cadena estilo `imprimir` y devuelve un Valor cadena dueño con el
 * resultado completo, sin truncado para tipos comunes (entero bignum,
 * cadena). Para colecciones grandes escala el buffer hasta cap máximo
 * configurado (16 MB) — más allá trunca silenciosamente como
 * `valor_a_cadena`.
 *
 * Devuelve VAL_NULO ante OOM. Idéntico a `cadena(x)` salvo que no
 * propaga errores de runtime — el llamador convierte VAL_NULO en
 * error contextual.
 */
Valor valor_a_cadena_alocada(const Valor *v);

/*
 * Devuelve el nombre del tipo en castellano (para `tipo()` built-in
 * y mensajes de error). Cadena estática, no liberar.
 *   "entero", "decimal", "cadena", "booleano", "nulo", "funcion", "rango"
 */
const char *valor_nombre_tipo(const Valor *v);

/*
 * Verdadez (truthiness) según ESPEC §6.2: nulo y falso son falsos;
 * 0, 0.0, "" y colecciones vacías son falsos; el resto verdadero.
 */
bool valor_es_verdadero(const Valor *v);

/*
 * Igualdad estructural según ESPEC §6.3 (operador `==`).
 *   - nulo == nulo → verdadero.
 *   - booleano == booleano: comparación directa.
 *   - entero == entero: comparación de bignum.
 *   - decimal == decimal: comparación de double.
 *   - entero == decimal: convertir decimal a fracción exacta y comparar.
 *   - cadena == cadena: comparación byte-a-byte.
 *   - tipos distintos no comparables → falso (sin error).
 */
bool valor_iguales(const Valor *a, const Valor *b);

#endif /* CORNAMUSA_VALOR_H */
