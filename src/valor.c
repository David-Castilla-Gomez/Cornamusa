#include "valor.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "chunk.h"   /* FuncionBC: definición completa para refcount */
#include "tommath.h"
#include "utf8proc.h" /* iteración UTF-8 en cadenas */

/* ──────────────────────────────────────────────────────────────────
 * Helpers internos
 * ────────────────────────────────────────────────────────────────── */

/*
 * Aloca un mp_int en heap. mp_int es la struct de libtommath; su
 * `mp_init` aloca también un array interno. Si falla, devolvemos NULL.
 */
static mp_int *nuevo_mp_int(void) {
    mp_int *m = (mp_int *)malloc(sizeof(mp_int));
    if (m == NULL) return NULL;
    if (mp_init(m) != MP_OKAY) {
        free(m);
        return NULL;
    }
    return m;
}

/*
 * Copia un literal numérico al heap eliminando guiones bajos (que
 * libtommath no acepta) y descartando prefijo de base si lo hay.
 * Devuelve la cadena resultante (alocada con malloc) o NULL si OOM.
 *
 * `base_out` recibe la base detectada (10, 16, 8, 2).
 */
static char *normalizar_literal_entero(const char *lexema, int longitud,
                                        int *base_out) {
    int base = 10;
    int inicio = 0;
    /* Detectar prefijos: 0x/0X, 0o/0O, 0b/0B. */
    if (longitud >= 2 && lexema[0] == '0') {
        char p = lexema[1];
        if (p == 'x' || p == 'X') { base = 16; inicio = 2; }
        else if (p == 'o' || p == 'O') { base = 8; inicio = 2; }
        else if (p == 'b' || p == 'B') { base = 2; inicio = 2; }
    }
    *base_out = base;

    char *limpio = (char *)malloc((size_t)(longitud - inicio) + 1);
    if (limpio == NULL) return NULL;
    int j = 0;
    for (int i = inicio; i < longitud; i++) {
        if (lexema[i] != '_') {
            limpio[j++] = lexema[i];
        }
    }
    limpio[j] = '\0';
    return limpio;
}

/* Para decimales: copia eliminando '_'. Sin prefijos. */
static char *normalizar_literal_decimal(const char *lexema, int longitud) {
    char *limpio = (char *)malloc((size_t)longitud + 1);
    if (limpio == NULL) return NULL;
    int j = 0;
    for (int i = 0; i < longitud; i++) {
        if (lexema[i] != '_') {
            limpio[j++] = lexema[i];
        }
    }
    limpio[j] = '\0';
    return limpio;
}

/* ──────────────────────────────────────────────────────────────────
 * Constructores
 * ────────────────────────────────────────────────────────────────── */

Valor valor_nulo(void) {
    Valor v;
    v.tipo = VAL_NULO;
    v.dueno_cadena = false;
    return v;
}

Valor valor_booleano(bool b) {
    Valor v;
    v.tipo = VAL_BOOLEANO;
    v.dueno_cadena = false;
    v.como.booleano = b;
    return v;
}

Valor valor_decimal(double d) {
    Valor v;
    v.tipo = VAL_DECIMAL;
    v.dueno_cadena = false;
    v.como.decimal = d;
    return v;
}

Valor valor_entero_de_lexema(const char *lexema, int longitud) {
    int base;
    char *limpio = normalizar_literal_entero(lexema, longitud, &base);
    if (limpio == NULL) return valor_nulo();

    mp_int *m = nuevo_mp_int();
    if (m == NULL) { free(limpio); return valor_nulo(); }

    if (mp_read_radix(m, limpio, base) != MP_OKAY) {
        mp_clear(m);
        free(m);
        free(limpio);
        return valor_nulo();
    }
    free(limpio);

    /* v0.11 (B9): normalizar a SMALL si cabe — los literales en código
       fuente son típicamente pequeños. */
    return valor_entero_de_mp_normalizado(m);
}

Valor valor_entero_de_long(long n) {
    /* Delega en valor_entero_de_i64 para que pase por la política de
       SMALL/BIG (B9, v0.11). */
    return valor_entero_de_i64((int64_t)n);
}

/* ──────────────────────────────────────────────────────────────────
 * Small-int tagging — helpers públicos (B9, v0.11.0).
 *
 * `valor_entero_de_i64(n)` produce SMALL si n cabe en
 * [SMALL_INT_MIN, SMALL_INT_MAX], BIG en caso contrario.
 * `valor_entero_de_mp_normalizado(m)` toma posesión de m y lo demote
 * a SMALL si su valor cabe.
 * ────────────────────────────────────────────────────────────────── */

bool valor_entero_a_i64(const Valor *v, int64_t *out) {
    if (!v || !out) return false;
    if (v->tipo == VAL_ENTERO_SMALL) {
        *out = v->como.entero_small;
        return true;
    }
    if (v->tipo == VAL_ENTERO) {
        /* mp_count_bits devuelve la magnitud del bignum (sin signo).
           `< 64` significa magnitud ≤ 63 bits, que cubre el rango de
           int64 EXCEPTO INT64_MIN (cuya magnitud es exactamente 64).
           INT64_MIN queda excluido a propósito: como SMALL_INT_MIN es
           -2^62, no perdemos nada útil y evitamos casos especiales. */
        mp_int *m = v->como.entero;
        if (mp_count_bits(m) < 64) {
            *out = mp_get_i64(m);
            return true;
        }
        return false;
    }
    return false;
}

mp_int *valor_entero_a_mp_int(const Valor *v, bool *propio) {
    if (!v || !propio) return NULL;
    /* Inicializar *propio antes de cualquier return path para que el
       caller no lea memoria sin inicializar si la alocación falla. */
    *propio = false;
    if (v->tipo == VAL_ENTERO) {
        return v->como.entero;
    }
    if (v->tipo == VAL_ENTERO_SMALL) {
        mp_int *m = nuevo_mp_int();
        if (!m) return NULL;
        mp_set_i64(m, v->como.entero_small);
        *propio = true;
        return m;
    }
    return NULL;
}

Valor valor_entero_de_i64(int64_t n) {
    /* Si n cabe en el rango SMALL, devolvemos VAL_ENTERO_SMALL inline
       sin alocar. Si no, BIG. */
    if (n >= CORNAMUSA_SMALL_INT_MIN && n <= CORNAMUSA_SMALL_INT_MAX) {
        Valor v;
        v.tipo = VAL_ENTERO_SMALL;
        v.dueno_cadena = false;
        v.como.entero_small = n;
        return v;
    }
    mp_int *m = nuevo_mp_int();
    if (m == NULL) return valor_nulo();
    mp_set_i64(m, n);
    Valor v;
    v.tipo = VAL_ENTERO;
    v.dueno_cadena = false;
    v.como.entero = m;
    return v;
}

Valor valor_entero_de_mp_normalizado(mp_int *m) {
    /* Normaliza a SMALL si el valor cabe. Toma posesión de m y lo
       libera si demote ocurre. */
    if (!m) return valor_nulo();
    /* Si cabe en 63 bits (signo + magnitud), demote. mp_count_bits
       cuenta la magnitud en bits. */
    if (mp_count_bits(m) <= 62) {
        int64_t n = mp_get_i64(m);
        if (n >= CORNAMUSA_SMALL_INT_MIN && n <= CORNAMUSA_SMALL_INT_MAX) {
            mp_clear(m);
            free(m);
            Valor v;
            v.tipo = VAL_ENTERO_SMALL;
            v.dueno_cadena = false;
            v.como.entero_small = n;
            return v;
        }
    }
    Valor v;
    v.tipo = VAL_ENTERO;
    v.dueno_cadena = false;
    v.como.entero = m;
    return v;
}

Valor valor_decimal_de_lexema(const char *lexema, int longitud) {
    char *limpio = normalizar_literal_decimal(lexema, longitud);
    if (limpio == NULL) return valor_nulo();
    double d = strtod(limpio, NULL);
    free(limpio);
    return valor_decimal(d);
}

Valor valor_cadena_referencia(const char *texto, int longitud) {
    Valor v;
    v.tipo = VAL_CADENA;
    v.dueno_cadena = false;
    v.como.cadena.texto = texto;
    v.como.cadena.longitud = longitud;
    return v;
}

Valor valor_cadena_duplicar(const char *texto, int longitud) {
    char *copia = (char *)malloc((size_t)longitud + 1);
    if (copia == NULL) return valor_nulo();
    if (longitud > 0) memcpy(copia, texto, (size_t)longitud);
    copia[longitud] = '\0';
    Valor v;
    v.tipo = VAL_CADENA;
    v.dueno_cadena = true;
    v.como.cadena.texto = copia;
    v.como.cadena.longitud = longitud;
    return v;
}

Valor valor_cadena_desde_escapes(const char *src, int srclen) {
    if (srclen <= 0) return valor_cadena_duplicar("", 0);
    char *buf = (char *)malloc((size_t)srclen + 1);
    if (!buf) return valor_nulo();
    int j = 0;
    for (int i = 0; i < srclen; i++) {
        char c = src[i];
        if (c == '\\' && i + 1 < srclen) {
            char nx = src[++i];
            switch (nx) {
                case 'n': buf[j++] = '\n'; break;
                case 't': buf[j++] = '\t'; break;
                case 'r': buf[j++] = '\r'; break;
                case '0': buf[j++] = '\0'; break;
                case '\\': buf[j++] = '\\'; break;
                case '\'': buf[j++] = '\''; break;
                case '"': buf[j++] = '"'; break;
                default: buf[j++] = nx; break;
            }
        } else {
            buf[j++] = c;
        }
    }
    buf[j] = '\0';
    Valor v;
    v.tipo = VAL_CADENA;
    v.dueno_cadena = true;
    v.como.cadena.texto = buf;
    v.como.cadena.longitud = j;
    return v;
}

Valor valor_funcion(const struct Sent *def, struct Entorno *entorno_def) {
    Valor v;
    v.tipo = VAL_FUNCION;
    v.dueno_cadena = false;
    v.como.funcion.def = def;
    v.como.funcion.entorno_definicion = entorno_def;
    return v;
}

Valor valor_nativa(const char *nombre, FnNativa fn) {
    Valor v;
    v.tipo = VAL_NATIVA;
    v.dueno_cadena = false;
    v.como.nativa.nombre = nombre;
    v.como.nativa.fn = fn;
    return v;
}

Valor valor_rango_de_longs(long inicio, long fin, long paso) {
    mp_int *mi = nuevo_mp_int();
    mp_int *mf = nuevo_mp_int();
    mp_int *mp = nuevo_mp_int();
    if (!mi || !mf || !mp) {
        if (mi) { mp_clear(mi); free(mi); }
        if (mf) { mp_clear(mf); free(mf); }
        if (mp) { mp_clear(mp); free(mp); }
        return valor_nulo();
    }
    mp_set_l(mi, inicio);
    mp_set_l(mf, fin);
    mp_set_l(mp, paso);
    Valor v;
    v.tipo = VAL_RANGO;
    v.dueno_cadena = false;
    v.como.rango.inicio = mi;
    v.como.rango.fin = mf;
    v.como.rango.paso = mp;
    return v;
}

Valor valor_rango_de_mp(mp_int *inicio, mp_int *fin, mp_int *paso) {
    Valor v;
    v.tipo = VAL_RANGO;
    v.dueno_cadena = false;
    v.como.rango.inicio = inicio;
    v.como.rango.fin = fin;
    v.como.rango.paso = paso;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Lista — gestión de refcount y operaciones básicas
 * ────────────────────────────────────────────────────────────────── */

#define LISTA_CAPACIDAD_MIN 4

Lista *lista_nueva(int capacidad_inicial) {
    /* Fase 7 S1: pasamos por gc_alocar para que el GC pueda rastrear.
       Si no hay Memoria instalada (tests low-level), gc_alocar cae a
       malloc puro sin rastreo y todo sigue funcionando. */
    Lista *l = (Lista *)gc_alocar(sizeof(Lista), GC_TIPO_LISTA);
    if (!l) return NULL;
    int cap = capacidad_inicial < LISTA_CAPACIDAD_MIN
            ? LISTA_CAPACIDAD_MIN : capacidad_inicial;
    l->elementos = (Valor *)malloc(sizeof(Valor) * (size_t)cap);
    if (!l->elementos) { free(l); return NULL; }
    l->cuenta = 0;
    l->capacidad = cap;
    l->refcount = 1;
    return l;
}

void lista_retener(Lista *l) {
    if (l) l->refcount++;
}

void lista_liberar(Lista *l) {
    if (!l) return;
    l->refcount--;
    if (l->refcount > 0) return;
    /* Refcount llegó a 0: destruir todos los elementos y la propia lista. */
    for (int i = 0; i < l->cuenta; i++) {
        valor_destruir(&l->elementos[i]);
    }
    free(l->elementos);
    /* Fase 7 S1: desenlazar del rastreo del GC antes de liberar. */
    gc_desenlazar(&l->obj);
    free(l);
}

bool lista_agregar(Lista *l, Valor v) {
    if (!l) { valor_destruir(&v); return false; }
    if (l->cuenta == l->capacidad) {
        int nueva_cap = l->capacidad * 2;
        Valor *nuevo = (Valor *)realloc(l->elementos,
            sizeof(Valor) * (size_t)nueva_cap);
        if (!nuevo) { valor_destruir(&v); return false; }
        l->elementos = nuevo;
        l->capacidad = nueva_cap;
    }
    l->elementos[l->cuenta++] = v;
    return true;
}

Valor *lista_obtener_ref(Lista *l, int indice) {
    if (!l || indice < 0 || indice >= l->cuenta) return NULL;
    return &l->elementos[indice];
}

bool lista_asignar(Lista *l, int indice, Valor v) {
    if (!l || indice < 0 || indice >= l->cuenta) {
        valor_destruir(&v);
        return false;
    }
    valor_destruir(&l->elementos[indice]);
    l->elementos[indice] = v;
    return true;
}

Valor valor_lista(Lista *l) {
    Valor v;
    v.tipo = VAL_LISTA;
    v.dueno_cadena = false;
    v.como.lista = l;
    return v;
}

Valor valor_diccionario(Diccionario *d) {
    Valor v;
    v.tipo = VAL_DICCIONARIO;
    v.dueno_cadena = false;
    v.como.dicc = d;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Diccionario — tabla hash con probing lineal y refcount
 * ────────────────────────────────────────────────────────────────── */

#include <math.h>
/* Nota: math.h ya incluido más arriba — duplicado idempotente. */

#define DICC_CAPACIDAD_INICIAL 8
#define DICC_FACTOR_CARGA_NUM 3
#define DICC_FACTOR_CARGA_DEN 4   /* 0.75 */

static uint64_t fnv1a_64(const uint8_t *data, size_t len) {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* Convierte un valor numérico (booleano/entero/decimal) a int64 si
 * cabe sin pérdida. Devuelve true si la conversión es exacta.
 *
 * v0.11.4: corregido el rango BIG. Antes aceptaba solo magnitud ≤ 62
 * bits (rango SMALL); ahora acepta hasta < 64 bits (rango int64
 * completo excepto INT64_MIN). Esto cierra una incoherencia de hash:
 * un BIG en la banda 2^62..2^63 daba un hash distinto de un DECIMAL
 * con el mismo valor numérico, porque el DECIMAL pasaba por el path
 * rápido i64 (rango ±9.2e18) y el BIG no. Ahora ambos convergen al
 * mismo path i64 → mismo hash. (Tech-debt #6 de revisión post-v0.11.) */
static bool valor_a_int64_si_cabe(const Valor *v, int64_t *out) {
    if (v->tipo == VAL_BOOLEANO) { *out = v->como.booleano ? 1 : 0; return true; }
    if (v->tipo == VAL_ENTERO_SMALL) { *out = v->como.entero_small; return true; }
    if (v->tipo == VAL_ENTERO) {
        if (mp_count_bits(v->como.entero) >= 64) return false;
        *out = (int64_t)mp_get_i64(v->como.entero);
        return true;
    }
    if (v->tipo == VAL_DECIMAL) {
        double d = v->como.decimal;
        /* NaN/Inf: !(d > -inf && d < inf) los excluye sin usar isfinite()
           (que en algunas glibc tiene un macro float-promoting). */
        if (!(d > -1e308 && d < 1e308)) return false;
        if (floor(d) != d) return false;
        if (d < -9.2e18 || d > 9.2e18) return false;
        *out = (int64_t)d;
        return true;
    }
    return false;
}

/*
 * Hash de un Valor. Garantiza que valores numéricamente iguales
 * (entero/booleano/decimal con mismo valor matemático) hashean igual,
 * conservando la invariante `a == b ⇒ hash(a) == hash(b)`.
 */
/* v1.42: hooks registrados por la VM para despachar __hash__/__igual__
   desde valor.c sin tener visibilidad de la VM struct. */
static void *g_vm_ctx = NULL;
static ValorHashDunderHook g_hash_hook = NULL;
static ValorIgualesDunderHook g_iguales_hook = NULL;
/* Bandera one-shot: el hook la pone a true al retornar
   DUNDER_HOOK_ERROR; el OP de la VM la consulta tras dicc_/conj_
   para propagar el error vía RAISE_OR_DIE. */
static bool g_dunder_hubo_error = false;

void valor_set_hooks(void *vm_ctx,
                     ValorHashDunderHook hash_hook,
                     ValorIgualesDunderHook iguales_hook) {
    g_vm_ctx = vm_ctx;
    g_hash_hook = hash_hook;
    g_iguales_hook = iguales_hook;
}

bool valor_dunder_hubo_error_y_limpiar(void) {
    bool e = g_dunder_hubo_error;
    g_dunder_hubo_error = false;
    return e;
}

static uint64_t hash_valor(const Valor *v) {
    /* Camino rápido para numéricos que caben en int64. */
    int64_t n;
    if (valor_a_int64_si_cabe(v, &n)) {
        return fnv1a_64((const uint8_t *)&n, sizeof(n));
    }
    switch (v->tipo) {
        case VAL_NULO:
            return 0xdeadbeefULL;
        case VAL_ENTERO: {
            /* Bignum: hashear los dígitos + el signo. */
            uint64_t h = 14695981039346656037ULL;
            int used = v->como.entero->used;
            for (int i = 0; i < used; i++) {
                mp_digit d = v->como.entero->dp[i];
                for (size_t j = 0; j < sizeof(d); j++) {
                    h ^= (uint8_t)(d >> (j * 8));
                    h *= 1099511628211ULL;
                }
            }
            if (mp_isneg(v->como.entero) == MP_YES) h ^= 0x8000000000000000ULL;
            return h;
        }
        case VAL_DECIMAL: {
            double d = v->como.decimal;
            uint64_t bits;
            memcpy(&bits, &d, sizeof(bits));
            return bits;
        }
        case VAL_CADENA:
            return fnv1a_64((const uint8_t *)v->como.cadena.texto,
                            (size_t)v->como.cadena.longitud);
        case VAL_FUNCION:
            return fnv1a_64((const uint8_t *)&v->como.funcion.def,
                            sizeof(v->como.funcion.def));
        case VAL_NATIVA:
            return fnv1a_64((const uint8_t *)&v->como.nativa.fn,
                            sizeof(v->como.nativa.fn));
        case VAL_FUNCION_BC:
            return fnv1a_64((const uint8_t *)&v->como.closure,
                            sizeof(v->como.closure));
        case VAL_TUPLA: {
            /* Hash combinando los hashes de cada elemento (estilo
               Python `tuplehash`). */
            uint64_t h = 14695981039346656037ULL;
            const Tupla *t = v->como.tupla;
            for (int i = 0; i < t->cuenta; i++) {
                uint64_t eh = hash_valor(&t->elementos[i]);
                h ^= eh;
                h *= 1099511628211ULL;
            }
            return h;
        }
        case VAL_INSTANCIA: {
            /* v1.42: si la clase define `__hash__`, despachamos el
               dunder (vía hook hacia la VM) la primera vez y cacheamos.
               Sin dunder caemos a hash por identidad (puntero). En caso
               de error del dunder, marcamos la bandera one-shot para
               que el OP de la VM la consulte y propague. */
            Instancia *inst = v->como.instancia;
            if (inst->cache_hash_valido) {
                return inst->cache_hash;
            }
            if (g_hash_hook && g_vm_ctx) {
                uint64_t h;
                ValorDunderHookResultado r = g_hash_hook(g_vm_ctx, inst, &h);
                if (r == DUNDER_HOOK_OK) {
                    inst->cache_hash = h;
                    inst->cache_hash_valido = true;
                    return h;
                }
                if (r == DUNDER_HOOK_ERROR) {
                    g_dunder_hubo_error = true;
                    /* Devolvemos un hash deterministico (identidad) para
                       que el dicc_/conj_ no se confunda; el caller
                       verá el flag y abortará. */
                }
                /* DUNDER_HOOK_NO_DUNDER → caemos a identidad. */
            }
            /* Identidad: hash del puntero. Cachear para estabilidad
               (Python: id-based hash es estable durante la vida del
               objeto). */
            uint64_t h = fnv1a_64((const uint8_t *)&inst, sizeof(inst));
            inst->cache_hash = h;
            inst->cache_hash_valido = true;
            return h;
        }
        default:
            return 0;  /* unhashable; el llamador debe rechazar antes */
    }
}

bool valor_es_hashable(const Valor *v) {
    if (v == NULL) return false;
    switch (v->tipo) {
        case VAL_LISTA:
        case VAL_DICCIONARIO:
        case VAL_CONJUNTO:
        case VAL_RANGO:
        case VAL_ITERADOR:
        case VAL_PLANTILLA_BC:
        case VAL_EXCEPCION:
        case VAL_METODO_LIGADO:
        case VAL_MODULO:
        case VAL_GENERADOR:
            return false;
        case VAL_CLASE:
        case VAL_INSTANCIA:
            /* v1.42: instancias siempre son hashables — por identidad
               por defecto, o por `__hash__` si la clase lo define. Las
               clases también son hashables por identidad (útil como
               valores en cache, etc.). */
            return true;
        case VAL_TUPLA:
            /* Tupla es hashable solo si todos sus elementos lo son. */
            for (int i = 0; i < v->como.tupla->cuenta; i++) {
                if (!valor_es_hashable(&v->como.tupla->elementos[i])) {
                    return false;
                }
            }
            return true;
        default:
            return true;
    }
}

/*
 * Busca el slot adecuado para `clave` en `entradas`. Devuelve el slot
 * que la contiene o el primer slot vacío en la cadena de probing.
 * Asume tabla NO completamente llena.
 */
static EntradaDicc *dicc_buscar_slot(EntradaDicc *entradas, int capacidad,
                                      const Valor *clave) {
    uint64_t hash = hash_valor(clave);
    int indice = (int)(hash & (uint64_t)(capacidad - 1));
    for (;;) {
        EntradaDicc *e = &entradas[indice];
        if (!e->ocupada) return e;
        if (valor_iguales(&e->clave, clave)) return e;
        indice = (indice + 1) & (capacidad - 1);
    }
}

static bool dicc_redimensionar(Diccionario *d, int nueva_cap) {
    EntradaDicc *nuevas = (EntradaDicc *)calloc((size_t)nueva_cap,
        sizeof(EntradaDicc));
    if (!nuevas) return false;
    /* v1.20: nuevo array de orden con la misma capacidad. */
    int *nuevo_orden = (int *)malloc(sizeof(int) * (size_t)nueva_cap);
    if (!nuevo_orden) { free(nuevas); return false; }

    /* Iteramos por el orden de inserción viejo para construir el
       nuevo orden con los nuevos slot indices. Cada clave se reubica
       a su nueva posición y registramos ese slot en `nuevo_orden`. */
    for (int i = 0; i < d->cuenta; i++) {
        int slot_viejo = d->orden_insercion[i];
        EntradaDicc *src = &d->entradas[slot_viejo];
        EntradaDicc *dst = dicc_buscar_slot(nuevas, nueva_cap, &src->clave);
        *dst = *src;
        nuevo_orden[i] = (int)(dst - nuevas);
    }
    free(d->entradas);
    free(d->orden_insercion);
    d->entradas = nuevas;
    d->capacidad = nueva_cap;
    d->orden_insercion = nuevo_orden;
    d->orden_capacidad = nueva_cap;
    /* Cualquier cache de slot_idx queda invalidada — todas las
       posiciones cambiaron por el rehash. */
    d->version++;
    return true;
}

Diccionario *dicc_nuevo(void) {
    Diccionario *d = (Diccionario *)gc_alocar(sizeof(Diccionario), GC_TIPO_DICCIONARIO);
    if (!d) return NULL;
    d->entradas = (EntradaDicc *)calloc(DICC_CAPACIDAD_INICIAL,
        sizeof(EntradaDicc));
    if (!d->entradas) { gc_desenlazar(&d->obj); free(d); return NULL; }
    d->cuenta = 0;
    d->capacidad = DICC_CAPACIDAD_INICIAL;
    d->refcount = 1;
    d->version = 0;
    /* v1.20: orden de inserción con la misma capacidad que el hash. */
    d->orden_insercion = (int *)malloc(sizeof(int) * DICC_CAPACIDAD_INICIAL);
    if (!d->orden_insercion) {
        free(d->entradas);
        gc_desenlazar(&d->obj); free(d);
        return NULL;
    }
    d->orden_capacidad = DICC_CAPACIDAD_INICIAL;
    return d;
}

void dicc_retener(Diccionario *d) {
    if (d) d->refcount++;
}

void dicc_liberar(Diccionario *d) {
    if (!d) return;
    d->refcount--;
    if (d->refcount > 0) return;
    for (int i = 0; i < d->capacidad; i++) {
        if (d->entradas[i].ocupada) {
            valor_destruir(&d->entradas[i].clave);
            valor_destruir(&d->entradas[i].valor);
        }
    }
    free(d->entradas);
    free(d->orden_insercion);  /* v1.20 */
    gc_desenlazar(&d->obj);
    free(d);
}

bool dicc_asignar(Diccionario *d, Valor clave, Valor valor) {
    if (!d || !valor_es_hashable(&clave)) {
        valor_destruir(&clave); valor_destruir(&valor);
        return false;
    }
    if ((d->cuenta + 1) * DICC_FACTOR_CARGA_DEN
        > d->capacidad * DICC_FACTOR_CARGA_NUM) {
        if (!dicc_redimensionar(d, d->capacidad * 2)) {
            valor_destruir(&clave); valor_destruir(&valor);
            return false;
        }
    }
    EntradaDicc *slot = dicc_buscar_slot(d->entradas, d->capacidad, &clave);
    if (slot->ocupada) {
        valor_destruir(&slot->clave);
        valor_destruir(&slot->valor);
        /* Sobreescritura: NO se incrementa version, NO se modifica el
           orden de inserción. El slot_idx cacheado sigue apuntando a
           la misma posición. */
    } else {
        slot->ocupada = true;
        d->cuenta++;
        /* Inserción nueva: bumpear version y registrar en el orden. */
        d->version++;
        /* v1.20: el orden_capacidad se mantiene == capacidad por la
           lógica de dicc_redimensionar. Aseguramos espacio. */
        if (d->cuenta > d->orden_capacidad) {
            int nueva_orden_cap = d->orden_capacidad * 2;
            if (nueva_orden_cap < d->cuenta) nueva_orden_cap = d->cuenta;
            int *nuevo = (int *)realloc(d->orden_insercion,
                sizeof(int) * (size_t)nueva_orden_cap);
            if (!nuevo) {
                /* OOM: revertir slot y propagar error. */
                slot->ocupada = false;
                d->cuenta--;
                valor_destruir(&clave); valor_destruir(&valor);
                return false;
            }
            d->orden_insercion = nuevo;
            d->orden_capacidad = nueva_orden_cap;
        }
        d->orden_insercion[d->cuenta - 1] = (int)(slot - d->entradas);
    }
    slot->clave = clave;
    slot->valor = valor;
    return true;
}

bool dicc_obtener(const Diccionario *d, const Valor *clave, Valor *out) {
    if (!d || !valor_es_hashable(clave)) return false;
    EntradaDicc *slot = dicc_buscar_slot(d->entradas, d->capacidad, clave);
    if (!slot->ocupada) return false;
    *out = valor_clonar(&slot->valor);
    return true;
}

bool dicc_obtener_y_slot(const Diccionario *d, const Valor *clave,
                          Valor *out, int *out_slot_idx) {
    if (!d || !valor_es_hashable(clave)) return false;
    EntradaDicc *slot = dicc_buscar_slot(d->entradas, d->capacidad, clave);
    if (!slot->ocupada) return false;
    *out = valor_clonar(&slot->valor);
    *out_slot_idx = (int)(slot - d->entradas);
    return true;
}

bool dicc_contiene(const Diccionario *d, const Valor *clave) {
    if (!d || !valor_es_hashable(clave)) return false;
    EntradaDicc *slot = dicc_buscar_slot(d->entradas, d->capacidad, clave);
    return slot->ocupada;
}

/* Helper interno v1.20: encuentra la posición de `slot_idx` en
   `orden_insercion` y la reemplaza por `nuevo_slot_idx` (in place,
   sin shift). Usado para actualizar el orden cuando una clave se
   reubica durante el re-hashing tras borrado. */
static void dicc_actualizar_slot_en_orden(Diccionario *d,
                                            int slot_idx_viejo,
                                            int slot_idx_nuevo) {
    for (int i = 0; i < d->cuenta; i++) {
        if (d->orden_insercion[i] == slot_idx_viejo) {
            d->orden_insercion[i] = slot_idx_nuevo;
            return;
        }
    }
}

bool dicc_quitar(Diccionario *d, const Valor *clave, Valor *out) {
    if (!d || !valor_es_hashable(clave)) return false;
    EntradaDicc *slot = dicc_buscar_slot(d->entradas, d->capacidad, clave);
    if (!slot->ocupada) return false;
    /* Borrado invalida slot cacheado de esta clave Y de otras claves
       que tras la reinserción del cluster cambian de posición. */
    d->version++;
    int slot_borrado = (int)(slot - d->entradas);

    valor_destruir(&slot->clave);
    *out = slot->valor;
    slot->ocupada = false;
    slot->clave = valor_nulo();
    slot->valor = valor_nulo();
    d->cuenta--;

    /* v1.20: quitar slot_borrado del orden de inserción (shift left). */
    for (int i = 0; i < d->cuenta + 1; i++) {
        if (d->orden_insercion[i] == slot_borrado) {
            for (int k = i; k < d->cuenta; k++) {
                d->orden_insercion[k] = d->orden_insercion[k + 1];
            }
            break;
        }
    }

    /* Reinsertar slots subsiguientes manteniendo el orden. Cada uno
       puede reubicarse a una posición distinta (incluso al
       slot_borrado); actualizamos `orden_insercion` con el nuevo
       slot_idx. */
    int j = (slot_borrado + 1) & (d->capacidad - 1);
    while (d->entradas[j].ocupada) {
        EntradaDicc copia = d->entradas[j];
        d->entradas[j].ocupada = false;
        d->entradas[j].clave = valor_nulo();
        d->entradas[j].valor = valor_nulo();
        int slot_viejo = j;
        /* Re-buscar slot para `copia.clave` con el hueco abierto.
           No usamos dicc_asignar porque éste tocaría el orden. */
        EntradaDicc *dst = dicc_buscar_slot(d->entradas, d->capacidad,
                                              &copia.clave);
        *dst = copia;
        dst->ocupada = true;
        int slot_nuevo = (int)(dst - d->entradas);
        if (slot_nuevo != slot_viejo) {
            dicc_actualizar_slot_en_orden(d, slot_viejo, slot_nuevo);
        }
        j = (j + 1) & (d->capacidad - 1);
    }
    return true;
}

/* ──────────────────────────────────────────────────────────────────
 * Conjunto — hash set sobre Valor
 * ────────────────────────────────────────────────────────────────── */

static EntradaConjunto *conj_buscar_slot(EntradaConjunto *entradas,
                                          int capacidad, const Valor *v) {
    uint64_t hash = hash_valor(v);
    int indice = (int)(hash & (uint64_t)(capacidad - 1));
    for (;;) {
        EntradaConjunto *e = &entradas[indice];
        if (!e->ocupada) return e;
        if (valor_iguales(&e->elemento, v)) return e;
        indice = (indice + 1) & (capacidad - 1);
    }
}

static bool conj_redimensionar(Conjunto *c, int nueva_cap) {
    EntradaConjunto *nuevas = (EntradaConjunto *)calloc((size_t)nueva_cap,
        sizeof(EntradaConjunto));
    if (!nuevas) return false;
    for (int i = 0; i < c->capacidad; i++) {
        EntradaConjunto *src = &c->entradas[i];
        if (!src->ocupada) continue;
        EntradaConjunto *dst = conj_buscar_slot(nuevas, nueva_cap, &src->elemento);
        *dst = *src;
    }
    free(c->entradas);
    c->entradas = nuevas;
    c->capacidad = nueva_cap;
    return true;
}

Conjunto *conj_nuevo(void) {
    Conjunto *c = (Conjunto *)gc_alocar(sizeof(Conjunto), GC_TIPO_CONJUNTO);
    if (!c) return NULL;
    c->entradas = (EntradaConjunto *)calloc(DICC_CAPACIDAD_INICIAL,
        sizeof(EntradaConjunto));
    if (!c->entradas) { gc_desenlazar(&c->obj); free(c); return NULL; }
    c->cuenta = 0;
    c->capacidad = DICC_CAPACIDAD_INICIAL;
    c->refcount = 1;
    return c;
}

void conj_retener(Conjunto *c) { if (c) c->refcount++; }

void conj_liberar(Conjunto *c) {
    if (!c) return;
    c->refcount--;
    if (c->refcount > 0) return;
    for (int i = 0; i < c->capacidad; i++) {
        if (c->entradas[i].ocupada) {
            valor_destruir(&c->entradas[i].elemento);
        }
    }
    free(c->entradas);
    gc_desenlazar(&c->obj);
    free(c);
}

bool conj_agregar(Conjunto *c, Valor v) {
    if (!c || !valor_es_hashable(&v)) {
        valor_destruir(&v);
        return false;
    }
    if ((c->cuenta + 1) * DICC_FACTOR_CARGA_DEN
        > c->capacidad * DICC_FACTOR_CARGA_NUM) {
        if (!conj_redimensionar(c, c->capacidad * 2)) {
            valor_destruir(&v);
            return false;
        }
    }
    EntradaConjunto *slot = conj_buscar_slot(c->entradas, c->capacidad, &v);
    if (slot->ocupada) {
        /* Ya estaba: descartamos el nuevo elemento (el original queda). */
        valor_destruir(&v);
        return true;
    }
    slot->elemento = v;
    slot->ocupada = true;
    c->cuenta++;
    return true;
}

bool conj_contiene(const Conjunto *c, const Valor *v) {
    if (!c || !valor_es_hashable(v)) return false;
    EntradaConjunto *slot = conj_buscar_slot(c->entradas, c->capacidad, v);
    return slot->ocupada;
}

bool conj_quitar(Conjunto *c, const Valor *v) {
    if (!c || !valor_es_hashable(v)) return false;
    EntradaConjunto *slot = conj_buscar_slot(c->entradas, c->capacidad, v);
    if (!slot->ocupada) return false;
    valor_destruir(&slot->elemento);
    slot->elemento = valor_nulo();
    slot->ocupada = false;
    c->cuenta--;

    int j = (int)((slot - c->entradas) + 1) & (c->capacidad - 1);
    while (c->entradas[j].ocupada) {
        Valor copia = c->entradas[j].elemento;
        c->entradas[j].elemento = valor_nulo();
        c->entradas[j].ocupada = false;
        c->cuenta--;
        conj_agregar(c, copia);
        j = (j + 1) & (c->capacidad - 1);
    }
    return true;
}

Valor valor_conjunto(Conjunto *c) {
    Valor v;
    v.tipo = VAL_CONJUNTO;
    v.dueno_cadena = false;
    v.como.conjunto = c;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Tupla — secuencia inmutable
 * ────────────────────────────────────────────────────────────────── */

Tupla *tupla_nueva(int cuenta) {
    Tupla *t = (Tupla *)gc_alocar(sizeof(Tupla), GC_TIPO_TUPLA);
    if (!t) return NULL;
    if (cuenta > 0) {
        t->elementos = (Valor *)malloc(sizeof(Valor) * (size_t)cuenta);
        if (!t->elementos) { gc_desenlazar(&t->obj); free(t); return NULL; }
    } else {
        t->elementos = NULL;
    }
    t->cuenta = cuenta;
    t->refcount = 1;
    return t;
}

void tupla_retener(Tupla *t) { if (t) t->refcount++; }

void tupla_liberar(Tupla *t) {
    if (!t) return;
    t->refcount--;
    if (t->refcount > 0) return;
    for (int i = 0; i < t->cuenta; i++) {
        valor_destruir(&t->elementos[i]);
    }
    free(t->elementos);
    gc_desenlazar(&t->obj);
    free(t);
}

Valor valor_tupla(Tupla *t) {
    Valor v;
    v.tipo = VAL_TUPLA;
    v.dueno_cadena = false;
    v.como.tupla = t;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Iterador (uso VM-only)
 * ────────────────────────────────────────────────────────────────── */

bool valor_es_iterable(const Valor *v) {
    if (v == NULL) return false;
    switch (v->tipo) {
        case VAL_CADENA:
        case VAL_LISTA:
        case VAL_TUPLA:
        case VAL_DICCIONARIO:
        case VAL_CONJUNTO:
        case VAL_RANGO:
        case VAL_GENERADOR:  /* v1.31 */
            return true;
        default:
            return false;
    }
}

Iterador *iter_nuevo(const Valor *iterable) {
    Iterador *it = (Iterador *)gc_alocar(sizeof(Iterador), GC_TIPO_ITERADOR);
    if (!it) return NULL;
    it->iterable = valor_clonar(iterable);  /* refcount o copia según tipo */
    it->cursor = 0;
    return it;
}

void iter_destruir(Iterador *it) {
    if (!it) return;
    valor_destruir(&it->iterable);
    gc_desenlazar(&it->obj);
    free(it);
}

/* ──────────────────────────────────────────────────────────────────
 * Generador (v1.31)
 * ────────────────────────────────────────────────────────────────── */

Generador *generador_nuevo(Closure *cl) {
    Generador *g = (Generador *)gc_alocar(sizeof(Generador), GC_TIPO_GENERADOR);
    if (!g) return NULL;
    closure_retener(cl);
    g->closure = cl;
    g->stack_buf = NULL;
    g->stack_n = 0;
    g->stack_cap = 0;
    g->ip_offset = 0;
    g->agotado = false;
    g->ejecutando = false;
    g->refcount = 1;
    return g;
}

void generador_retener(Generador *g) {
    if (g) g->refcount++;
}

void generador_liberar(Generador *g) {
    if (!g) return;
    g->refcount--;
    if (g->refcount > 0) return;
    closure_liberar(g->closure);
    if (g->stack_buf) {
        for (int i = 0; i < g->stack_n; i++) {
            valor_destruir(&g->stack_buf[i]);
        }
        free(g->stack_buf);
    }
    gc_desenlazar(&g->obj);
    free(g);
}

Valor valor_generador(Generador *g) {
    Valor v;
    v.tipo = VAL_GENERADOR;
    v.dueno_cadena = false;
    v.como.generador = g;
    return v;
}

bool iter_siguiente(Iterador *it, Valor *out) {
    if (!it) { *out = valor_nulo(); return false; }
    const Valor *iter = &it->iterable;

    switch (iter->tipo) {
        case VAL_LISTA: {
            const Lista *l = iter->como.lista;
            if (it->cursor >= l->cuenta) { *out = valor_nulo(); return false; }
            *out = valor_clonar(&l->elementos[it->cursor]);
            it->cursor++;
            return true;
        }
        case VAL_TUPLA: {
            const Tupla *t = iter->como.tupla;
            if (it->cursor >= t->cuenta) { *out = valor_nulo(); return false; }
            *out = valor_clonar(&t->elementos[it->cursor]);
            it->cursor++;
            return true;
        }
        case VAL_CADENA: {
            int len = iter->como.cadena.longitud;
            if (it->cursor >= len) { *out = valor_nulo(); return false; }
            const char *texto = iter->como.cadena.texto;
            utf8proc_int32_t cp;
            utf8proc_ssize_t consumido = utf8proc_iterate(
                (const utf8proc_uint8_t *)(texto + it->cursor),
                (utf8proc_ssize_t)(len - it->cursor), &cp);
            if (consumido <= 0) { *out = valor_nulo(); return false; }
            *out = valor_cadena_duplicar(texto + it->cursor, (int)consumido);
            it->cursor += (int)consumido;
            return true;
        }
        case VAL_DICCIONARIO: {
            /* v1.20: yields claves en orden de INSERCIÓN. `cursor` es
               el índice en `orden_insercion[]`, no en `entradas[]`. */
            const Diccionario *d = iter->como.dicc;
            if (it->cursor >= d->cuenta) { *out = valor_nulo(); return false; }
            int slot = d->orden_insercion[it->cursor];
            *out = valor_clonar(&d->entradas[slot].clave);
            it->cursor++;
            return true;
        }
        case VAL_CONJUNTO: {
            const Conjunto *c = iter->como.conjunto;
            while (it->cursor < c->capacidad) {
                if (c->entradas[it->cursor].ocupada) {
                    *out = valor_clonar(&c->entradas[it->cursor].elemento);
                    it->cursor++;
                    return true;
                }
                it->cursor++;
            }
            *out = valor_nulo();
            return false;
        }
        case VAL_RANGO: {
            /*
             * Camino rápido int64 (v0.11.2): si inicio, fin y paso
             * caben todos en int64, calculamos `inicio + cursor*paso`
             * directamente sin alocar mp_int. Cubre el caso típico
             * `para i en rango(N)` con N moderado, donde cada paso
             * antes hacía 4 mp_init + 1 malloc + 5 mp_clear.
             *
             * Detecta overflow durante mult/add con __builtin_*_overflow
             * (GCC/Clang); en MSVC fallback con cota int31 (suficiente
             * para rangos típicos hasta unos 10^9 elementos). Si overflow
             * o si alguno no cabe en int64, cae al path bignum.
             */
            int64_t inicio_i64 = 0, fin_i64 = 0, paso_i64 = 0;
            bool fits_inicio = mp_count_bits(iter->como.rango.inicio) < 64;
            bool fits_fin    = mp_count_bits(iter->como.rango.fin) < 64;
            bool fits_paso   = mp_count_bits(iter->como.rango.paso) < 64;
            if (fits_inicio && fits_fin && fits_paso) {
                inicio_i64 = mp_get_i64(iter->como.rango.inicio);
                fin_i64    = mp_get_i64(iter->como.rango.fin);
                paso_i64   = mp_get_i64(iter->como.rango.paso);
                int64_t delta_i64, actual_i64;
#if defined(__GNUC__) || defined(__clang__)
                if (__builtin_mul_overflow((int64_t)it->cursor, paso_i64, &delta_i64)
                    || __builtin_add_overflow(inicio_i64, delta_i64, &actual_i64)) {
                    goto rango_bignum;
                }
#else
                /* MSVC fallback: si ambos cursor y paso caben en int31,
                   la mult cabe en int62 sin overflow. */
                if (it->cursor > INT32_MAX
                    || paso_i64 < INT32_MIN || paso_i64 > INT32_MAX
                    || inicio_i64 < CORNAMUSA_SMALL_INT_MIN
                    || inicio_i64 > CORNAMUSA_SMALL_INT_MAX) {
                    goto rango_bignum;
                }
                delta_i64 = (int64_t)it->cursor * paso_i64;
                actual_i64 = inicio_i64 + delta_i64;
#endif
                bool paso_neg = paso_i64 < 0;
                bool sigue = paso_neg ? (actual_i64 > fin_i64)
                                       : (actual_i64 < fin_i64);
                if (!sigue) {
                    *out = valor_nulo();
                    return false;
                }
                *out = valor_entero_de_i64(actual_i64);
                it->cursor++;
                return true;
            }
        rango_bignum:
            ; /* Etiqueta — el path bignum sigue abajo. */
            /* Path bignum: calcular `inicio + cursor*paso`. */
            mp_int actual;
            if (mp_init(&actual) != MP_OKAY) {
                *out = valor_nulo();
                return false;
            }
            if (mp_copy(iter->como.rango.inicio, &actual) != MP_OKAY) {
                mp_clear(&actual);
                *out = valor_nulo();
                return false;
            }
            if (it->cursor > 0) {
                mp_int delta, cur_mp;
                if (mp_init_multi(&delta, &cur_mp, NULL) != MP_OKAY) {
                    mp_clear(&actual);
                    *out = valor_nulo();
                    return false;
                }
                mp_set_l(&cur_mp, (long)it->cursor);
                mp_err r1 = mp_mul(&cur_mp, iter->como.rango.paso, &delta);
                mp_err r2 = mp_add(&actual, &delta, &actual);
                (void)r1; (void)r2;
                mp_clear_multi(&delta, &cur_mp, NULL);
            }
            int cmp = mp_cmp(&actual, iter->como.rango.fin);
            bool paso_neg = (mp_isneg(iter->como.rango.paso) == MP_YES);
            bool sigue = paso_neg ? (cmp == MP_GT) : (cmp == MP_LT);
            if (!sigue) {
                mp_clear(&actual);
                *out = valor_nulo();
                return false;
            }
            mp_int *resultado = (mp_int *)malloc(sizeof(mp_int));
            if (!resultado || mp_init(resultado) != MP_OKAY
                           || mp_copy(&actual, resultado) != MP_OKAY) {
                if (resultado) { mp_clear(resultado); free(resultado); }
                mp_clear(&actual);
                *out = valor_nulo();
                return false;
            }
            mp_clear(&actual);
            *out = valor_entero_de_mp_normalizado(resultado);
            it->cursor++;
            return true;
        }
        default:
            *out = valor_nulo();
            return false;
    }
}

Valor valor_iterador(Iterador *it) {
    Valor v;
    v.tipo = VAL_ITERADOR;
    v.dueno_cadena = false;
    v.como.iterador = it;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Excepción
 * ────────────────────────────────────────────────────────────────── */

Excepcion *excepcion_nueva(const char *clase, int len_clase,
                            const char *mensaje, int len_mensaje) {
    Excepcion *e = (Excepcion *)gc_alocar(sizeof(Excepcion), GC_TIPO_EXCEPCION);
    if (!e) return NULL;
    char *cls = (char *)malloc((size_t)len_clase + 1);
    char *msg = (char *)malloc((size_t)len_mensaje + 1);
    if (!cls || !msg) {
        free(cls); free(msg);
        gc_desenlazar(&e->obj); free(e);
        return NULL;
    }
    if (len_clase > 0) memcpy(cls, clase, (size_t)len_clase);
    cls[len_clase] = '\0';
    if (len_mensaje > 0) memcpy(msg, mensaje, (size_t)len_mensaje);
    msg[len_mensaje] = '\0';
    e->clase = cls;
    e->longitud_clase = len_clase;
    e->mensaje = msg;
    e->longitud_mensaje = len_mensaje;
    e->refcount = 1;
    return e;
}

void excepcion_retener(Excepcion *e) { if (e) e->refcount++; }

void excepcion_liberar(Excepcion *e) {
    if (!e) return;
    e->refcount--;
    if (e->refcount > 0) return;
    free(e->clase);
    free(e->mensaje);
    gc_desenlazar(&e->obj);
    free(e);
}

Valor valor_excepcion(Excepcion *e) {
    Valor v;
    v.tipo = VAL_EXCEPCION;
    v.dueno_cadena = false;
    v.como.excepcion = e;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Clase / Instancia (Fase 8 v0.7.0)
 * ────────────────────────────────────────────────────────────────── */

Clase *clase_nueva(const char *nombre, int len_nombre) {
    Clase *c = (Clase *)gc_alocar(sizeof(Clase), GC_TIPO_CLASE);
    if (!c) return NULL;
    char *copia = (char *)malloc((size_t)len_nombre + 1);
    if (!copia) { gc_desenlazar(&c->obj); free(c); return NULL; }
    if (len_nombre > 0) memcpy(copia, nombre, (size_t)len_nombre);
    copia[len_nombre] = '\0';
    Diccionario *met = dicc_nuevo();
    if (!met) { free(copia); gc_desenlazar(&c->obj); free(c); return NULL; }
    c->nombre = copia;
    c->longitud_nombre = len_nombre;
    c->metodos = met;
    c->superclase = NULL;
    c->refcount = 1;
    return c;
}

void clase_retener(Clase *c) { if (c) c->refcount++; }

void clase_liberar(Clase *c) {
    if (!c) return;
    c->refcount--;
    if (c->refcount > 0) return;
    dicc_liberar(c->metodos);
    if (c->superclase) clase_liberar(c->superclase);
    free(c->nombre);
    gc_desenlazar(&c->obj);
    free(c);
}

Valor valor_clase(Clase *c) {
    Valor v;
    v.tipo = VAL_CLASE;
    v.dueno_cadena = false;
    v.como.clase = c;
    return v;
}

Closure *clase_obtener_metodo(const Clase *cl, const char *nombre, int len) {
    if (!cl || !cl->metodos || !nombre || len <= 0) return NULL;
    Valor clave = valor_cadena_referencia(nombre, len);
    Valor metodo;
    if (!dicc_obtener(cl->metodos, &clave, &metodo)) return NULL;
    if (metodo.tipo != VAL_FUNCION_BC) {
        valor_destruir(&metodo);
        return NULL;
    }
    Closure *c = metodo.como.closure;
    /* `dicc_obtener` devuelve un clon con refcount +1. Como el contrato
       de este helper es "no-owning" y `cl->metodos` mantiene la closure
       viva mientras la clase exista, descartamos el incremento extra. */
    valor_destruir(&metodo);
    return c;
}

Instancia *instancia_nueva(Clase *c) {
    if (!c) return NULL;
    Instancia *i = (Instancia *)gc_alocar(sizeof(Instancia), GC_TIPO_INSTANCIA);
    if (!i) return NULL;
    Diccionario *atr = dicc_nuevo();
    if (!atr) { gc_desenlazar(&i->obj); free(i); return NULL; }
    clase_retener(c);
    i->clase = c;
    i->atributos = atr;
    i->refcount = 1;
    i->cache_hash = 0;             /* v1.42: lazy, poblado al primer uso */
    i->cache_hash_valido = false;
    return i;
}

void instancia_retener(Instancia *i) { if (i) i->refcount++; }

void instancia_liberar(Instancia *i) {
    if (!i) return;
    i->refcount--;
    if (i->refcount > 0) return;
    dicc_liberar(i->atributos);
    clase_liberar(i->clase);
    gc_desenlazar(&i->obj);
    free(i);
}

Valor valor_instancia(Instancia *i) {
    Valor v;
    v.tipo = VAL_INSTANCIA;
    v.dueno_cadena = false;
    v.como.instancia = i;
    return v;
}

MetodoLigado *metodo_ligado_nuevo(const Valor *receptor, Closure *metodo) {
    if (!metodo) return NULL;
    MetodoLigado *m = (MetodoLigado *)gc_alocar(sizeof(MetodoLigado),
                                                  GC_TIPO_METODO_LIGADO);
    if (!m) return NULL;
    m->receptor = valor_clonar(receptor);
    closure_retener(metodo);
    m->metodo = metodo;
    m->refcount = 1;
    return m;
}

void metodo_ligado_retener(MetodoLigado *m) { if (m) m->refcount++; }

void metodo_ligado_liberar(MetodoLigado *m) {
    if (!m) return;
    m->refcount--;
    if (m->refcount > 0) return;
    valor_destruir(&m->receptor);
    closure_liberar(m->metodo);
    gc_desenlazar(&m->obj);
    free(m);
}

Valor valor_metodo_ligado(MetodoLigado *m) {
    Valor v;
    v.tipo = VAL_METODO_LIGADO;
    v.dueno_cadena = false;
    v.como.metodo_ligado = m;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Módulo (Fase 9 v0.9.0)
 * ────────────────────────────────────────────────────────────────── */

Modulo *modulo_nuevo(const char *nombre, int len_nombre) {
    Modulo *m = (Modulo *)gc_alocar(sizeof(Modulo), GC_TIPO_MODULO);
    if (!m) return NULL;
    char *copia = (char *)malloc((size_t)len_nombre + 1);
    if (!copia) { gc_desenlazar(&m->obj); free(m); return NULL; }
    if (len_nombre > 0) memcpy(copia, nombre, (size_t)len_nombre);
    copia[len_nombre] = '\0';
    Diccionario *atr = dicc_nuevo();
    if (!atr) { free(copia); gc_desenlazar(&m->obj); free(m); return NULL; }
    m->nombre = copia;
    m->longitud_nombre = len_nombre;
    m->atributos = atr;
    m->refcount = 1;
    return m;
}

void modulo_retener(Modulo *m) { if (m) m->refcount++; }

void modulo_liberar(Modulo *m) {
    if (!m) return;
    m->refcount--;
    if (m->refcount > 0) return;
    dicc_liberar(m->atributos);
    free(m->nombre);
    gc_desenlazar(&m->obj);
    free(m);
}

Valor valor_modulo(Modulo *m) {
    Valor v;
    v.tipo = VAL_MODULO;
    v.dueno_cadena = false;
    v.como.modulo = m;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Destrucción y copia
 * ────────────────────────────────────────────────────────────────── */

void valor_destruir(Valor *v) {
    if (v == NULL) return;
    switch (v->tipo) {
        case VAL_ENTERO_SMALL:
            /* Inline: nada que liberar. */
            break;
        case VAL_ENTERO:
            if (v->como.entero) {
                mp_clear(v->como.entero);
                free(v->como.entero);
                v->como.entero = NULL;
            }
            break;
        case VAL_CADENA:
            if (v->dueno_cadena && v->como.cadena.texto) {
                /* cast para deshacer const — sabemos que la asignamos
                   nosotros con malloc cuando dueno_cadena==true. */
                free((char *)v->como.cadena.texto);
                v->como.cadena.texto = NULL;
                v->dueno_cadena = false;
            }
            break;
        case VAL_RANGO:
            if (v->como.rango.inicio) {
                mp_clear(v->como.rango.inicio); free(v->como.rango.inicio);
            }
            if (v->como.rango.fin) {
                mp_clear(v->como.rango.fin); free(v->como.rango.fin);
            }
            if (v->como.rango.paso) {
                mp_clear(v->como.rango.paso); free(v->como.rango.paso);
            }
            v->como.rango.inicio = NULL;
            v->como.rango.fin = NULL;
            v->como.rango.paso = NULL;
            break;
        case VAL_LISTA:
            lista_liberar(v->como.lista);
            v->como.lista = NULL;
            break;
        case VAL_DICCIONARIO:
            dicc_liberar(v->como.dicc);
            v->como.dicc = NULL;
            break;
        case VAL_CONJUNTO:
            conj_liberar(v->como.conjunto);
            v->como.conjunto = NULL;
            break;
        case VAL_TUPLA:
            tupla_liberar(v->como.tupla);
            v->como.tupla = NULL;
            break;
        case VAL_FUNCION_BC:
            closure_liberar(v->como.closure);
            v->como.closure = NULL;
            break;
        case VAL_PLANTILLA_BC:
            funcion_bc_liberar(v->como.plantilla);
            v->como.plantilla = NULL;
            break;
        case VAL_ITERADOR:
            iter_destruir(v->como.iterador);
            v->como.iterador = NULL;
            break;
        case VAL_EXCEPCION:
            excepcion_liberar(v->como.excepcion);
            v->como.excepcion = NULL;
            break;
        case VAL_CLASE:
            clase_liberar(v->como.clase);
            v->como.clase = NULL;
            break;
        case VAL_INSTANCIA:
            instancia_liberar(v->como.instancia);
            v->como.instancia = NULL;
            break;
        case VAL_METODO_LIGADO:
            metodo_ligado_liberar(v->como.metodo_ligado);
            v->como.metodo_ligado = NULL;
            break;
        case VAL_MODULO:
            modulo_liberar(v->como.modulo);
            v->como.modulo = NULL;
            break;
        case VAL_GENERADOR:
            generador_liberar(v->como.generador);
            v->como.generador = NULL;
            break;
        default:
            break;
    }
    v->tipo = VAL_NULO;
}

Valor valor_clonar(const Valor *v) {
    if (v == NULL) return valor_nulo();
    switch (v->tipo) {
        case VAL_NULO:      return valor_nulo();
        case VAL_BOOLEANO:  return valor_booleano(v->como.booleano);
        case VAL_DECIMAL:   return valor_decimal(v->como.decimal);
        case VAL_ENTERO_SMALL: {
            /* SMALL es inline; copia trivial de la unión. */
            Valor c;
            c.tipo = VAL_ENTERO_SMALL;
            c.dueno_cadena = false;
            c.como.entero_small = v->como.entero_small;
            return c;
        }
        case VAL_ENTERO: {
            mp_int *m = nuevo_mp_int();
            if (m == NULL) return valor_nulo();
            if (mp_copy(v->como.entero, m) != MP_OKAY) {
                mp_clear(m);
                free(m);
                return valor_nulo();
            }
            Valor c;
            c.tipo = VAL_ENTERO;
            c.dueno_cadena = false;
            c.como.entero = m;
            return c;
        }
        case VAL_CADENA:
            /* Si la fuente original es referencia, mantenemos referencia.
               Si es dueño, duplicamos para que la copia tenga la suya. */
            if (v->dueno_cadena) {
                return valor_cadena_duplicar(v->como.cadena.texto,
                                              v->como.cadena.longitud);
            }
            return valor_cadena_referencia(v->como.cadena.texto,
                                            v->como.cadena.longitud);
        case VAL_FUNCION:
        case VAL_NATIVA: {
            /* Funciones se comparten por valor inmutable: la struct
               apunta a recursos externos (AST, función C estática) que
               no se duplican. Por eso clonar es copiar la struct. */
            Valor c = *v;
            return c;
        }
        case VAL_RANGO: {
            mp_int *mi = nuevo_mp_int();
            mp_int *mf = nuevo_mp_int();
            mp_int *mp = nuevo_mp_int();
            if (!mi || !mf || !mp) {
                if (mi) { mp_clear(mi); free(mi); }
                if (mf) { mp_clear(mf); free(mf); }
                if (mp) { mp_clear(mp); free(mp); }
                return valor_nulo();
            }
            if (mp_copy(v->como.rango.inicio, mi) != MP_OKAY
             || mp_copy(v->como.rango.fin, mf) != MP_OKAY
             || mp_copy(v->como.rango.paso, mp) != MP_OKAY) {
                mp_clear(mi); free(mi);
                mp_clear(mf); free(mf);
                mp_clear(mp); free(mp);
                return valor_nulo();
            }
            return valor_rango_de_mp(mi, mf, mp);
        }
        case VAL_LISTA: {
            /* Refcount semantics: clonar = compartir referencia. La
               mutación en una "copia" se ve en el original (semántica
               Python). Para copia profunda hay que iterar y construir
               una lista nueva — no es lo que hace `valor_clonar`. */
            lista_retener(v->como.lista);
            return valor_lista(v->como.lista);
        }
        case VAL_DICCIONARIO:
            dicc_retener(v->como.dicc);
            return valor_diccionario(v->como.dicc);
        case VAL_CONJUNTO:
            conj_retener(v->como.conjunto);
            return valor_conjunto(v->como.conjunto);
        case VAL_TUPLA:
            tupla_retener(v->como.tupla);
            return valor_tupla(v->como.tupla);
        case VAL_FUNCION_BC:
            closure_retener(v->como.closure);
            return valor_closure(v->como.closure);
        case VAL_PLANTILLA_BC:
            funcion_bc_retener(v->como.plantilla);
            return valor_plantilla(v->como.plantilla);
        case VAL_ITERADOR:
            /* Iteradores no son clonables (son uso interno de la VM
               con vida corta). Devolvemos nulo si alguien lo intenta. */
            return valor_nulo();
        case VAL_EXCEPCION:
            excepcion_retener(v->como.excepcion);
            return valor_excepcion(v->como.excepcion);
        case VAL_CLASE:
            clase_retener(v->como.clase);
            return valor_clase(v->como.clase);
        case VAL_INSTANCIA:
            instancia_retener(v->como.instancia);
            return valor_instancia(v->como.instancia);
        case VAL_METODO_LIGADO:
            metodo_ligado_retener(v->como.metodo_ligado);
            return valor_metodo_ligado(v->como.metodo_ligado);
        case VAL_MODULO:
            modulo_retener(v->como.modulo);
            return valor_modulo(v->como.modulo);
        case VAL_GENERADOR:
            generador_retener(v->como.generador);
            return valor_generador(v->como.generador);
    }
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * Inspección
 * ────────────────────────────────────────────────────────────────── */

void valor_imprimir(const Valor *v, FILE *out) {
    char buffer[1024];
    valor_a_cadena(v, buffer, sizeof(buffer));
    fputs(buffer, out);
}

/* v1.45: parsea un format spec estilo Python. Devuelve true si OK. */
typedef struct {
    char fill;
    char align;       /* '<', '>', '^', o 0 = default */
    int  width;
    int  precision;   /* -1 = sin precisión explícita */
    char type;        /* 'd','f','e','x','X','b','s', o 0 = implícito */
    bool cero_padding; /* '0' antes del ancho → relleno con 0 alineado a der */
} FmtSpec;

static bool fmt_spec_parsear(const char *spec, int spec_len, FmtSpec *out,
                              char *err, int err_cap) {
    out->fill = ' ';
    out->align = 0;
    out->width = 0;
    out->precision = -1;
    out->type = 0;
    out->cero_padding = false;
    int i = 0;
    /* [fill][align] */
    if (spec_len >= 2
        && (spec[1] == '<' || spec[1] == '>' || spec[1] == '^')) {
        out->fill = spec[0];
        out->align = spec[1];
        i = 2;
    } else if (spec_len >= 1
        && (spec[0] == '<' || spec[0] == '>' || spec[0] == '^')) {
        out->align = spec[0];
        i = 1;
    }
    /* '0' indica zero-padding (Python: implica `>` y fill='0') */
    if (i < spec_len && spec[i] == '0' && out->align == 0) {
        out->cero_padding = true;
        out->fill = '0';
        out->align = '>';
        i++;
    }
    /* width */
    while (i < spec_len && spec[i] >= '0' && spec[i] <= '9') {
        out->width = out->width * 10 + (spec[i] - '0');
        i++;
    }
    /* .precision */
    if (i < spec_len && spec[i] == '.') {
        i++;
        out->precision = 0;
        while (i < spec_len && spec[i] >= '0' && spec[i] <= '9') {
            out->precision = out->precision * 10 + (spec[i] - '0');
            i++;
        }
    }
    /* type */
    if (i < spec_len) {
        char t = spec[i];
        if (t == 'd' || t == 'f' || t == 'e' || t == 'x' || t == 'X'
            || t == 'b' || t == 's') {
            out->type = t;
            i++;
        } else {
            if (err) snprintf(err, (size_t)err_cap,
                "ErrorDeValor: tipo de formato '%c' no soportado", t);
            return false;
        }
    }
    if (i != spec_len) {
        if (err) snprintf(err, (size_t)err_cap,
            "ErrorDeValor: spec de formato invalido");
        return false;
    }
    return true;
}

/* Aplica padding/align/fill a `cuerpo` (longitud `cuerpo_len`) según
   spec. Si el cuerpo ya cabe en `width`, lo devuelve tal cual.
   Aloca un buffer en heap; el caller libera. Devuelve len escrito. */
static char *fmt_aplicar_padding(const char *cuerpo, int cuerpo_len,
                                   const FmtSpec *spec, int *out_len) {
    if (cuerpo_len >= spec->width) {
        char *r = (char *)malloc((size_t)cuerpo_len);
        if (!r) return NULL;
        memcpy(r, cuerpo, (size_t)cuerpo_len);
        *out_len = cuerpo_len;
        return r;
    }
    int total = spec->width;
    char *r = (char *)malloc((size_t)total);
    if (!r) return NULL;
    int padding = total - cuerpo_len;
    char align = spec->align ? spec->align : '<';
    if (align == '<') {
        memcpy(r, cuerpo, (size_t)cuerpo_len);
        memset(r + cuerpo_len, spec->fill, (size_t)padding);
    } else if (align == '>') {
        memset(r, spec->fill, (size_t)padding);
        memcpy(r + padding, cuerpo, (size_t)cuerpo_len);
    } else {  /* '^' */
        int pad_izq = padding / 2;
        int pad_der = padding - pad_izq;
        memset(r, spec->fill, (size_t)pad_izq);
        memcpy(r + pad_izq, cuerpo, (size_t)cuerpo_len);
        memset(r + pad_izq + cuerpo_len, spec->fill, (size_t)pad_der);
    }
    *out_len = total;
    return r;
}

/* Convierte un valor entero (SMALL o BIG) a cadena en la base dada,
   con prefijo de signo `-` si es negativo. Aloca buffer; caller libera. */
static char *fmt_entero_a_str(const Valor *v, int base, bool may,
                                int *out_len) {
    /* SMALL path: usa int64. */
    if (v->tipo == VAL_ENTERO_SMALL) {
        int64_t n = v->como.entero_small;
        char buf[80];
        int len = 0;
        bool neg = n < 0;
        uint64_t u;
        if (neg) u = (uint64_t)(-(n + 1)) + 1; else u = (uint64_t)n;
        if (u == 0) { buf[len++] = '0'; }
        while (u > 0) {
            int d = (int)(u % (uint64_t)base);
            char c;
            if (d < 10) c = (char)('0' + d);
            else c = (char)((may ? 'A' : 'a') + (d - 10));
            buf[len++] = c;
            u /= (uint64_t)base;
        }
        if (neg) buf[len++] = '-';
        /* Reverse. */
        char *out = (char *)malloc((size_t)len);
        if (!out) return NULL;
        for (int i = 0; i < len; i++) out[i] = buf[len - 1 - i];
        *out_len = len;
        return out;
    }
    /* BIG path: libtommath. */
    if (v->tipo == VAL_ENTERO) {
        int tam = 0;
        if (mp_radix_size(v->como.entero, base, &tam) != MP_OKAY) return NULL;
        char *buf = (char *)malloc((size_t)tam);
        if (!buf) return NULL;
        size_t escritos;
        if (mp_to_radix(v->como.entero, buf, (size_t)tam, &escritos, base)
            != MP_OKAY) {
            free(buf);
            return NULL;
        }
        int n = (int)escritos - 1;
        if (n < 0) n = 0;
        if (may && base > 10) {
            for (int i = 0; i < n; i++) {
                if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 32;
            }
        }
        *out_len = n;
        return buf;
    }
    /* Booleano como entero. */
    if (v->tipo == VAL_BOOLEANO) {
        char *out = (char *)malloc(1);
        if (!out) return NULL;
        out[0] = v->como.booleano ? '1' : '0';
        *out_len = 1;
        return out;
    }
    return NULL;
}

Valor valor_formatear_con_spec(const Valor *v, const char *spec,
                                 int spec_len,
                                 char *err_buffer, int err_cap) {
    FmtSpec fs;
    if (!fmt_spec_parsear(spec, spec_len, &fs, err_buffer, err_cap)) {
        return valor_nulo();
    }
    char *cuerpo = NULL;
    int cuerpo_len = 0;

    /* Resolver tipo implícito si no hay explícito. */
    char type = fs.type;
    if (type == 0) {
        if (v->tipo == VAL_DECIMAL) type = 'f';
        else if (valor_es_entero(v) || v->tipo == VAL_BOOLEANO) type = 'd';
        else type = 's';
    }

    /* Producir cuerpo según el tipo. */
    if (type == 'd') {
        if (!valor_es_entero(v) && v->tipo != VAL_BOOLEANO) {
            /* Aceptar VAL_DECIMAL convirtiendo a entero (truncado)? Python
               rechaza con TypeError; nosotros también para evitar perder
               información silenciosamente. */
            if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
                "ErrorDeTipo: formato 'd' requiere entero, no '%s'",
                valor_nombre_tipo(v));
            return valor_nulo();
        }
        cuerpo = fmt_entero_a_str(v, 10, false, &cuerpo_len);
    } else if (type == 'x' || type == 'X') {
        if (!valor_es_entero(v) && v->tipo != VAL_BOOLEANO) {
            if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
                "ErrorDeTipo: formato '%c' requiere entero, no '%s'",
                type, valor_nombre_tipo(v));
            return valor_nulo();
        }
        cuerpo = fmt_entero_a_str(v, 16, type == 'X', &cuerpo_len);
    } else if (type == 'b') {
        if (!valor_es_entero(v) && v->tipo != VAL_BOOLEANO) {
            if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
                "ErrorDeTipo: formato 'b' requiere entero, no '%s'",
                valor_nombre_tipo(v));
            return valor_nulo();
        }
        cuerpo = fmt_entero_a_str(v, 2, false, &cuerpo_len);
    } else if (type == 'f' || type == 'e') {
        double d;
        if (v->tipo == VAL_DECIMAL) d = v->como.decimal;
        else if (v->tipo == VAL_ENTERO_SMALL) d = (double)v->como.entero_small;
        else if (v->tipo == VAL_ENTERO) {
            /* mp_int → double, posiblemente con pérdida. */
            int tam = 0;
            if (mp_radix_size(v->como.entero, 10, &tam) != MP_OKAY) {
                if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
                    "ErrorInterno: mp_radix_size fallo");
                return valor_nulo();
            }
            char *buf = (char *)malloc((size_t)tam);
            if (!buf) {
                if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
                    "memoria insuficiente");
                return valor_nulo();
            }
            size_t escritos;
            mp_to_radix(v->como.entero, buf, (size_t)tam, &escritos, 10);
            d = strtod(buf, NULL);
            free(buf);
        } else if (v->tipo == VAL_BOOLEANO) d = v->como.booleano ? 1.0 : 0.0;
        else {
            if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
                "ErrorDeTipo: formato '%c' requiere numero, no '%s'",
                type, valor_nombre_tipo(v));
            return valor_nulo();
        }
        int prec = (fs.precision >= 0) ? fs.precision : 6;
        char tmp[128];
        int n;
        if (type == 'f') n = snprintf(tmp, sizeof(tmp), "%.*f", prec, d);
        else             n = snprintf(tmp, sizeof(tmp), "%.*e", prec, d);
        if (n < 0) n = 0;
        if (n >= (int)sizeof(tmp)) n = (int)sizeof(tmp) - 1;
        cuerpo = (char *)malloc((size_t)n);
        if (!cuerpo) {
            if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
                "memoria insuficiente");
            return valor_nulo();
        }
        memcpy(cuerpo, tmp, (size_t)n);
        cuerpo_len = n;
    } else if (type == 's') {
        /* Coerción canónica a cadena. Sin invocar __cadena__ (no
           tenemos acceso a la VM aquí). */
        Valor s = valor_a_cadena_alocada(v);
        if (s.tipo != VAL_CADENA) {
            if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
                "memoria insuficiente al stringificar");
            return valor_nulo();
        }
        int len = s.como.cadena.longitud;
        if (fs.precision >= 0 && fs.precision < len) len = fs.precision;
        cuerpo = (char *)malloc((size_t)len);
        if (!cuerpo) {
            valor_destruir(&s);
            return valor_nulo();
        }
        memcpy(cuerpo, s.como.cadena.texto, (size_t)len);
        cuerpo_len = len;
        valor_destruir(&s);
    } else {
        if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
            "ErrorInterno: tipo de formato no manejado: %c", type);
        return valor_nulo();
    }
    if (!cuerpo) {
        if (err_buffer && err_buffer[0] == '\0')
            snprintf(err_buffer, (size_t)err_cap,
                "memoria insuficiente al formatear");
        return valor_nulo();
    }

    /* Default de alineación según el tipo (Python):
       - numéricos (`d f e x X b`): derecha
       - cadenas (`s`, sin tipo): izquierda */
    if (fs.align == 0) {
        if (type == 'd' || type == 'f' || type == 'e'
            || type == 'x' || type == 'X' || type == 'b') {
            fs.align = '>';
        } else {
            fs.align = '<';
        }
    }

    /* Aplicar padding/align/width. */
    int final_len = 0;
    char *final_buf = fmt_aplicar_padding(cuerpo, cuerpo_len, &fs, &final_len);
    free(cuerpo);
    if (!final_buf) {
        if (err_buffer) snprintf(err_buffer, (size_t)err_cap,
            "memoria insuficiente al rellenar");
        return valor_nulo();
    }
    Valor r = valor_cadena_duplicar(final_buf, final_len);
    free(final_buf);
    return r;
}

Valor valor_a_cadena_alocada(const Valor *v) {
    if (v == NULL) return valor_cadena_duplicar("nulo", 4);

    /* Cadena: copia profunda directa (sin pasar por formato). */
    if (v->tipo == VAL_CADENA) {
        return valor_cadena_duplicar(v->como.cadena.texto,
                                       v->como.cadena.longitud);
    }
    /* Entero bignum: dimensionar exactamente con mp_radix_size. */
    if (v->tipo == VAL_ENTERO) {
        int tam = 0;
        if (mp_radix_size(v->como.entero, 10, &tam) != MP_OKAY) {
            return valor_nulo();
        }
        char *buf = (char *)malloc((size_t)tam);
        if (!buf) return valor_nulo();
        size_t escritos;
        if (mp_to_radix(v->como.entero, buf, (size_t)tam, &escritos, 10) != MP_OKAY) {
            free(buf);
            return valor_nulo();
        }
        int n = (int)escritos - 1;
        if (n < 0) n = 0;
        Valor r = valor_cadena_duplicar(buf, n);
        free(buf);
        return r;
    }
    /* Resto: snprintf con buffer escalable.
     *
     * `valor_a_cadena` trunca silenciosamente si la repr no cabe.
     * Detectamos saturación heurísticamente: si los bytes escritos
     * llegan al límite del buffer (con margen de 4 bytes para
     * cubrir el truncado de listas/dicc, que rompen al `cap - 2`),
     * doblamos. Cap máximo 16 MB — beyond eso aceptamos el truncado
     * silencioso del comportamiento legacy. */
    int cap = 1024;
    char *buf = NULL;
    int n = 0;
    const int CAP_MAX = 16 * 1024 * 1024;
    while (cap <= CAP_MAX) {
        char *nuevo = (char *)realloc(buf, (size_t)cap);
        if (!nuevo) { free(buf); return valor_nulo(); }
        buf = nuevo;
        n = valor_a_cadena(v, buf, cap);
        /* Si quedan al menos 4 bytes de margen, asumimos que la
           representación cabe completa. */
        if (n + 4 < cap) break;
        if (cap == CAP_MAX) break;
        long siguiente = (long)cap * 2;
        cap = (siguiente > CAP_MAX) ? CAP_MAX : (int)siguiente;
    }
    Valor r = valor_cadena_duplicar(buf, n);
    free(buf);
    return r;
}

int valor_a_cadena(const Valor *v, char *buffer, int capacidad) {
    if (v == NULL || capacidad <= 0) return 0;
    int n = 0;

    switch (v->tipo) {
        case VAL_NULO:
            n = snprintf(buffer, (size_t)capacidad, "nulo");
            break;
        case VAL_BOOLEANO:
            n = snprintf(buffer, (size_t)capacidad, "%s",
                v->como.booleano ? "verdadero" : "falso");
            break;
        case VAL_DECIMAL:
            /* %g usa formato corto eligiendo entre fijo y científica.
               Para integer-valued floats, añadimos .0 para distinguir
               de enteros (estilo Python). */
            n = snprintf(buffer, (size_t)capacidad, "%g", v->como.decimal);
            if (n > 0 && n < capacidad
                && strchr(buffer, '.') == NULL
                && strchr(buffer, 'e') == NULL
                && strchr(buffer, 'n') == NULL /* nan */
                && strchr(buffer, 'i') == NULL /* inf */) {
                /* Valor entero como float: añadir ".0". */
                if (n + 2 < capacidad) {
                    buffer[n++] = '.';
                    buffer[n++] = '0';
                    buffer[n] = '\0';
                }
            }
            break;
        case VAL_ENTERO_SMALL:
            n = snprintf(buffer, (size_t)capacidad, "%lld",
                         (long long)v->como.entero_small);
            break;
        case VAL_ENTERO: {
            /* mp_radix_size devuelve el espacio necesario incluyendo
               '\0'. Usamos un buffer temporal si el resultado no cabe. */
            int tam = 0;
            if (mp_radix_size(v->como.entero, 10, &tam) != MP_OKAY) {
                n = snprintf(buffer, (size_t)capacidad, "<error>");
                break;
            }
            if (tam <= capacidad) {
                size_t escritos;
                if (mp_to_radix(v->como.entero, buffer, (size_t)capacidad,
                                &escritos, 10) != MP_OKAY) {
                    n = snprintf(buffer, (size_t)capacidad, "<error>");
                } else {
                    /* mp_to_radix incluye el '\0' en `escritos`. La
                       convención de valor_a_cadena es bytes escritos
                       SIN contar el terminador. */
                    n = (int)escritos - 1;
                    if (n < 0) n = 0;
                }
            } else {
                /* No cabe — escribir tantos dígitos como podamos.
                   Casos así son raros en uso normal. */
                n = snprintf(buffer, (size_t)capacidad, "<entero grande>");
            }
            break;
        }
        case VAL_CADENA: {
            /* Imprimir sin comillas (representación tipo print, no repr). */
            int longitud = v->como.cadena.longitud;
            if (longitud >= capacidad) longitud = capacidad - 1;
            if (longitud > 0) memcpy(buffer, v->como.cadena.texto,
                                      (size_t)longitud);
            buffer[longitud] = '\0';
            n = longitud;
            break;
        }
        case VAL_FUNCION:
            n = snprintf(buffer, (size_t)capacidad, "<funcion>");
            break;
        case VAL_NATIVA:
            n = snprintf(buffer, (size_t)capacidad, "<funcion %s>",
                v->como.nativa.nombre ? v->como.nativa.nombre : "nativa");
            break;
        case VAL_RANGO: {
            char ai[64], fi[64], pa[64];
            size_t esc;
            ai[0] = fi[0] = pa[0] = '?'; ai[1] = fi[1] = pa[1] = '\0';
            mp_err r1 = mp_to_radix(v->como.rango.inicio, ai, sizeof(ai), &esc, 10);
            mp_err r2 = mp_to_radix(v->como.rango.fin,    fi, sizeof(fi), &esc, 10);
            mp_err r3 = mp_to_radix(v->como.rango.paso,   pa, sizeof(pa), &esc, 10);
            (void)r1; (void)r2; (void)r3;
            n = snprintf(buffer, (size_t)capacidad,
                "rango(%s, %s, %s)", ai, fi, pa);
            break;
        }
        case VAL_LISTA: {
            /* Formato Python-like: [a, b, c] usando repr para los
               elementos (cadenas con comillas, listas anidadas
               idem). Buffer grande para soportar listas largas;
               truncamos limpiamente si llegamos al límite. */
            int escritos = snprintf(buffer, (size_t)capacidad, "[");
            if (escritos < 0 || escritos >= capacidad) { n = capacidad - 1; break; }
            int n_elem = v->como.lista->cuenta;
            for (int i = 0; i < n_elem; i++) {
                if (escritos + 2 >= capacidad) break;
                if (i > 0) {
                    buffer[escritos++] = ',';
                    buffer[escritos++] = ' ';
                }
                int restante = capacidad - escritos;
                int e = valor_a_repr(&v->como.lista->elementos[i],
                                      buffer + escritos, restante);
                escritos += e;
            }
            if (escritos < capacidad - 1) buffer[escritos++] = ']';
            buffer[escritos < capacidad ? escritos : capacidad - 1] = '\0';
            n = escritos < capacidad ? escritos : capacidad - 1;
            break;
        }
        case VAL_DICCIONARIO: {
            int escritos = snprintf(buffer, (size_t)capacidad, "{");
            if (escritos < 0 || escritos >= capacidad) { n = capacidad - 1; break; }
            const Diccionario *d = v->como.dicc;
            /* v1.20: iterar por orden de inserción. */
            for (int idx = 0; idx < d->cuenta; idx++) {
                int slot = d->orden_insercion[idx];
                const EntradaDicc *e = &d->entradas[slot];
                if (escritos + 4 >= capacidad) break;
                if (idx > 0) {
                    buffer[escritos++] = ',';
                    buffer[escritos++] = ' ';
                }
                int restante = capacidad - escritos;
                int wk = valor_a_repr(&e->clave, buffer + escritos, restante);
                escritos += wk;
                if (escritos + 3 >= capacidad) break;
                buffer[escritos++] = ':';
                buffer[escritos++] = ' ';
                restante = capacidad - escritos;
                int wv = valor_a_repr(&e->valor, buffer + escritos, restante);
                escritos += wv;
            }
            if (escritos < capacidad - 1) buffer[escritos++] = '}';
            buffer[escritos < capacidad ? escritos : capacidad - 1] = '\0';
            n = escritos < capacidad ? escritos : capacidad - 1;
            break;
        }
        case VAL_CONJUNTO: {
            const Conjunto *c = v->como.conjunto;
            /* Conjunto vacío: imprimir como `conjunto()` para distinguir
               de `{}` (diccionario vacío). Conjuntos no vacíos se
               representan como `{a, b, c}`. */
            if (c->cuenta == 0) {
                n = snprintf(buffer, (size_t)capacidad, "conjunto()");
                break;
            }
            int escritos = snprintf(buffer, (size_t)capacidad, "{");
            if (escritos < 0 || escritos >= capacidad) { n = capacidad - 1; break; }
            int impreso = 0;
            for (int i = 0; i < c->capacidad; i++) {
                if (!c->entradas[i].ocupada) continue;
                if (escritos + 2 >= capacidad) break;
                if (impreso > 0) {
                    buffer[escritos++] = ',';
                    buffer[escritos++] = ' ';
                }
                int restante = capacidad - escritos;
                int we = valor_a_repr(&c->entradas[i].elemento,
                                       buffer + escritos, restante);
                escritos += we;
                impreso++;
            }
            if (escritos < capacidad - 1) buffer[escritos++] = '}';
            buffer[escritos < capacidad ? escritos : capacidad - 1] = '\0';
            n = escritos < capacidad ? escritos : capacidad - 1;
            break;
        }
        case VAL_FUNCION_BC: {
            const Closure *cl = v->como.closure;
            const FuncionBC *f = cl->plantilla;
            n = snprintf(buffer, (size_t)capacidad, "<funcion %.*s>",
                f->longitud_nombre, f->nombre);
            break;
        }
        case VAL_PLANTILLA_BC: {
            const FuncionBC *f = v->como.plantilla;
            n = snprintf(buffer, (size_t)capacidad, "<plantilla %.*s>",
                f->longitud_nombre, f->nombre);
            break;
        }
        case VAL_ITERADOR:
            n = snprintf(buffer, (size_t)capacidad, "<iterador>");
            break;
        case VAL_EXCEPCION: {
            const Excepcion *e = v->como.excepcion;
            n = snprintf(buffer, (size_t)capacidad, "%.*s: %.*s",
                e->longitud_clase, e->clase,
                e->longitud_mensaje, e->mensaje);
            break;
        }
        case VAL_CLASE: {
            const Clase *c = v->como.clase;
            n = snprintf(buffer, (size_t)capacidad, "<clase %.*s>",
                c->longitud_nombre, c->nombre);
            break;
        }
        case VAL_INSTANCIA: {
            const Instancia *i = v->como.instancia;
            n = snprintf(buffer, (size_t)capacidad, "<instancia de %.*s>",
                i->clase->longitud_nombre, i->clase->nombre);
            break;
        }
        case VAL_METODO_LIGADO: {
            const MetodoLigado *m = v->como.metodo_ligado;
            const FuncionBC *fn = m->metodo->plantilla;
            n = snprintf(buffer, (size_t)capacidad, "<metodo %.*s>",
                fn->longitud_nombre, fn->nombre);
            break;
        }
        case VAL_MODULO: {
            const Modulo *m = v->como.modulo;
            n = snprintf(buffer, (size_t)capacidad, "<modulo %.*s>",
                m->longitud_nombre, m->nombre);
            break;
        }
        case VAL_GENERADOR: {
            const Generador *g = v->como.generador;
            const FuncionBC *fn = g->closure ? g->closure->plantilla : NULL;
            if (fn) {
                n = snprintf(buffer, (size_t)capacidad, "<generador %.*s%s>",
                    fn->longitud_nombre, fn->nombre,
                    g->agotado ? " agotado" : "");
            } else {
                n = snprintf(buffer, (size_t)capacidad, "<generador>");
            }
            break;
        }
        case VAL_TUPLA: {
            const Tupla *t = v->como.tupla;
            int escritos = snprintf(buffer, (size_t)capacidad, "(");
            if (escritos < 0 || escritos >= capacidad) { n = capacidad - 1; break; }
            for (int i = 0; i < t->cuenta; i++) {
                if (escritos + 2 >= capacidad) break;
                if (i > 0) {
                    buffer[escritos++] = ',';
                    buffer[escritos++] = ' ';
                }
                int restante = capacidad - escritos;
                int we = valor_a_repr(&t->elementos[i],
                                       buffer + escritos, restante);
                escritos += we;
            }
            /* Tupla de un elemento se imprime con coma final: (x,) */
            if (t->cuenta == 1 && escritos + 1 < capacidad) {
                buffer[escritos++] = ',';
            }
            if (escritos < capacidad - 1) buffer[escritos++] = ')';
            buffer[escritos < capacidad ? escritos : capacidad - 1] = '\0';
            n = escritos < capacidad ? escritos : capacidad - 1;
            break;
        }
    }

    if (n < 0) n = 0;
    if (n >= capacidad) n = capacidad - 1;
    buffer[n] = '\0';
    return n;
}

const char *valor_nombre_tipo(const Valor *v) {
    if (v == NULL) return "nulo";
    switch (v->tipo) {
        case VAL_NULO:      return "nulo";
        case VAL_BOOLEANO:  return "booleano";
        case VAL_ENTERO_SMALL:
        case VAL_ENTERO:    return "entero";
        case VAL_DECIMAL:   return "decimal";
        case VAL_CADENA:    return "cadena";
        case VAL_FUNCION:   return "funcion";
        case VAL_NATIVA:    return "funcion";  /* mismas semánticas externas */
        case VAL_RANGO:     return "rango";
        case VAL_LISTA:     return "lista";
        case VAL_DICCIONARIO: return "diccionario";
        case VAL_CONJUNTO:    return "conjunto";
        case VAL_TUPLA:       return "tupla";
        case VAL_FUNCION_BC:  return "funcion";
        case VAL_PLANTILLA_BC: return "plantilla";
        case VAL_ITERADOR:    return "iterador";
        case VAL_EXCEPCION:   return "excepcion";
        case VAL_CLASE:       return "clase";
        case VAL_INSTANCIA:   return "instancia";
        case VAL_METODO_LIGADO: return "funcion";  /* visible como funcion */
        case VAL_MODULO:        return "modulo";
        case VAL_GENERADOR:     return "generador";
    }
    return "desconocido";
}

/*
 * Variante "repr" de la conversión a cadena: añade comillas a las
 * cadenas. El resto de tipos comparte el formato de `valor_a_cadena`.
 * Usado al imprimir colecciones, donde queremos ver `[1, "hola"]` y
 * no `[1, hola]`.
 */
int valor_a_repr(const Valor *v, char *buffer, int capacidad) {
    if (v == NULL || capacidad <= 0) return 0;
    if (v->tipo == VAL_CADENA) {
        int n = snprintf(buffer, (size_t)capacidad, "\"%.*s\"",
            v->como.cadena.longitud, v->como.cadena.texto);
        if (n < 0) n = 0;
        if (n >= capacidad) n = capacidad - 1;
        buffer[n] = '\0';
        return n;
    }
    return valor_a_cadena(v, buffer, capacidad);
}

bool valor_es_verdadero(const Valor *v) {
    if (v == NULL) return false;
    switch (v->tipo) {
        case VAL_NULO:      return false;
        case VAL_BOOLEANO:  return v->como.booleano;
        case VAL_ENTERO_SMALL: return v->como.entero_small != 0;
        case VAL_ENTERO:    return mp_iszero(v->como.entero) == MP_NO;
        case VAL_DECIMAL:   return v->como.decimal != 0.0;
        case VAL_CADENA:    return v->como.cadena.longitud > 0;
        case VAL_FUNCION:
        case VAL_NATIVA:    return true;
        case VAL_RANGO: {
            /* Truthy si la iteración produciría al menos un elemento. */
            int cmp_ini_fin = mp_cmp(v->como.rango.inicio, v->como.rango.fin);
            bool paso_neg = (mp_isneg(v->como.rango.paso) == MP_YES);
            if (paso_neg) return cmp_ini_fin == MP_GT;
            return cmp_ini_fin == MP_LT;
        }
        case VAL_LISTA:
            return v->como.lista && v->como.lista->cuenta > 0;
        case VAL_DICCIONARIO:
            return v->como.dicc && v->como.dicc->cuenta > 0;
        case VAL_CONJUNTO:
            return v->como.conjunto && v->como.conjunto->cuenta > 0;
        case VAL_TUPLA:
            return v->como.tupla && v->como.tupla->cuenta > 0;
        case VAL_FUNCION_BC:
            return v->como.closure != NULL;
        case VAL_PLANTILLA_BC:
            return v->como.plantilla != NULL;
        case VAL_ITERADOR:
            return v->como.iterador != NULL;
        case VAL_EXCEPCION:
            return v->como.excepcion != NULL;
        case VAL_CLASE:
            return v->como.clase != NULL;
        case VAL_INSTANCIA:
            return v->como.instancia != NULL;
        case VAL_METODO_LIGADO:
            return v->como.metodo_ligado != NULL;
        case VAL_MODULO:
            return v->como.modulo != NULL;
        case VAL_GENERADOR:
            return v->como.generador != NULL && !v->como.generador->agotado;
    }
    return false;
}

bool valor_iguales(const Valor *a, const Valor *b) {
    if (a == NULL || b == NULL) return a == b;

    /*
     * Numéricos cross-tag (B9, v0.11): cualquier mezcla de
     * SMALL/BIG/BOOLEANO/DECIMAL se normaliza y compara aquí
     * arriba. Esto reemplaza las múltiples coerciones individuales
     * que había antes y unifica el comportamiento para que
     * SMALL(5) == BIG(5) == 5.0 == True (con 1).
     */
    bool a_num = valor_es_entero(a) || a->tipo == VAL_BOOLEANO
                 || a->tipo == VAL_DECIMAL;
    bool b_num = valor_es_entero(b) || b->tipo == VAL_BOOLEANO
                 || b->tipo == VAL_DECIMAL;
    if (a_num && b_num) {
        /* Si alguno es decimal, comparamos en double (precisión
           limitada para BIG grandes — semántica heredada de Python). */
        if (a->tipo == VAL_DECIMAL || b->tipo == VAL_DECIMAL) {
            double ad, bd;
            if (a->tipo == VAL_DECIMAL) ad = a->como.decimal;
            else if (a->tipo == VAL_BOOLEANO) ad = a->como.booleano ? 1.0 : 0.0;
            else if (a->tipo == VAL_ENTERO_SMALL) ad = (double)a->como.entero_small;
            else ad = mp_get_double(a->como.entero);
            if (b->tipo == VAL_DECIMAL) bd = b->como.decimal;
            else if (b->tipo == VAL_BOOLEANO) bd = b->como.booleano ? 1.0 : 0.0;
            else if (b->tipo == VAL_ENTERO_SMALL) bd = (double)b->como.entero_small;
            else bd = mp_get_double(b->como.entero);
            return ad == bd;
        }
        /* Ambos son enteros (incluido bool). Si ambos caben en i64,
           comparación inline; si no, promote a mp_int y usa mp_cmp. */
        int64_t ai, bi;
        bool a_fits = valor_a_int64_si_cabe(a, &ai);
        bool b_fits = valor_a_int64_si_cabe(b, &bi);
        if (a_fits && b_fits) return ai == bi;
        /* Al menos uno es BIG fuera del rango i64 — ambos deben ser
           BIG para tener chance de igualdad. */
        if (a->tipo != VAL_ENTERO || b->tipo != VAL_ENTERO) return false;
        return mp_cmp(a->como.entero, b->como.entero) == MP_EQ;
    }

    if (a->tipo != b->tipo) return false;

    switch (a->tipo) {
        case VAL_NULO:      return true;
        case VAL_BOOLEANO:  return a->como.booleano == b->como.booleano;
        case VAL_DECIMAL:   return a->como.decimal == b->como.decimal;
        case VAL_ENTERO_SMALL:
            return a->como.entero_small == b->como.entero_small;
        case VAL_ENTERO:    return mp_cmp(a->como.entero, b->como.entero) == MP_EQ;
        case VAL_CADENA:
            if (a->como.cadena.longitud != b->como.cadena.longitud) return false;
            return memcmp(a->como.cadena.texto, b->como.cadena.texto,
                          (size_t)a->como.cadena.longitud) == 0;
        case VAL_FUNCION:
            return a->como.funcion.def == b->como.funcion.def;
        case VAL_NATIVA:
            return a->como.nativa.fn == b->como.nativa.fn;
        case VAL_RANGO:
            return mp_cmp(a->como.rango.inicio, b->como.rango.inicio) == MP_EQ
                && mp_cmp(a->como.rango.fin,    b->como.rango.fin)    == MP_EQ
                && mp_cmp(a->como.rango.paso,   b->como.rango.paso)   == MP_EQ;
        case VAL_LISTA: {
            const Lista *la = a->como.lista;
            const Lista *lb = b->como.lista;
            if (la == lb) return true;        /* mismo objeto */
            if (la->cuenta != lb->cuenta) return false;
            for (int i = 0; i < la->cuenta; i++) {
                if (!valor_iguales(&la->elementos[i], &lb->elementos[i])) {
                    return false;
                }
            }
            return true;
        }
        case VAL_DICCIONARIO: {
            const Diccionario *da = a->como.dicc;
            const Diccionario *db = b->como.dicc;
            if (da == db) return true;
            if (da->cuenta != db->cuenta) return false;
            for (int i = 0; i < da->capacidad; i++) {
                const EntradaDicc *e = &da->entradas[i];
                if (!e->ocupada) continue;
                Valor otro;
                if (!dicc_obtener(db, &e->clave, &otro)) return false;
                bool igual = valor_iguales(&e->valor, &otro);
                valor_destruir(&otro);
                if (!igual) return false;
            }
            return true;
        }
        case VAL_CONJUNTO: {
            const Conjunto *ca = a->como.conjunto;
            const Conjunto *cb = b->como.conjunto;
            if (ca == cb) return true;
            if (ca->cuenta != cb->cuenta) return false;
            for (int i = 0; i < ca->capacidad; i++) {
                if (!ca->entradas[i].ocupada) continue;
                if (!conj_contiene(cb, &ca->entradas[i].elemento)) return false;
            }
            return true;
        }
        case VAL_TUPLA: {
            const Tupla *ta = a->como.tupla;
            const Tupla *tb = b->como.tupla;
            if (ta == tb) return true;
            if (ta->cuenta != tb->cuenta) return false;
            for (int i = 0; i < ta->cuenta; i++) {
                if (!valor_iguales(&ta->elementos[i], &tb->elementos[i])) {
                    return false;
                }
            }
            return true;
        }
        case VAL_FUNCION_BC:
            return a->como.closure == b->como.closure;
        case VAL_PLANTILLA_BC:
            return a->como.plantilla == b->como.plantilla;
        case VAL_ITERADOR:
            return a->como.iterador == b->como.iterador;
        case VAL_EXCEPCION:
            return a->como.excepcion == b->como.excepcion;
        case VAL_CLASE:
            return a->como.clase == b->como.clase;
        case VAL_INSTANCIA: {
            /* v1.42: si la clase de `a` define `__igual__`, despachamos
               el dunder. Identidad como fallback. En error, marcar
               flag para que el caller propague. */
            if (a->como.instancia == b->como.instancia) return true;
            if (g_iguales_hook && g_vm_ctx) {
                bool resultado;
                ValorDunderHookResultado r = g_iguales_hook(
                    g_vm_ctx, a->como.instancia, b, &resultado);
                if (r == DUNDER_HOOK_OK) return resultado;
                if (r == DUNDER_HOOK_ERROR) g_dunder_hubo_error = true;
            }
            return false;   /* identidad ya fallida arriba */
        }
        case VAL_METODO_LIGADO:
            return a->como.metodo_ligado == b->como.metodo_ligado;
        case VAL_MODULO:
            return a->como.modulo == b->como.modulo;
        case VAL_GENERADOR:
            return a->como.generador == b->como.generador;
    }
    return false;
}
