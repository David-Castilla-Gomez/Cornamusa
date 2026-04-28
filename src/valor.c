#include "valor.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "tommath.h"

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

    Valor v;
    v.tipo = VAL_ENTERO;
    v.dueno_cadena = false;
    v.como.entero = m;
    return v;
}

Valor valor_entero_de_long(long n) {
    mp_int *m = nuevo_mp_int();
    if (m == NULL) return valor_nulo();
    mp_set_l(m, n);
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
    Lista *l = (Lista *)malloc(sizeof(Lista));
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

/* ──────────────────────────────────────────────────────────────────
 * Destrucción y copia
 * ────────────────────────────────────────────────────────────────── */

void valor_destruir(Valor *v) {
    if (v == NULL) return;
    switch (v->tipo) {
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
        case VAL_ENTERO:    return "entero";
        case VAL_DECIMAL:   return "decimal";
        case VAL_CADENA:    return "cadena";
        case VAL_FUNCION:   return "funcion";
        case VAL_NATIVA:    return "funcion";  /* mismas semánticas externas */
        case VAL_RANGO:     return "rango";
        case VAL_LISTA:     return "lista";
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
    }
    return false;
}

bool valor_iguales(const Valor *a, const Valor *b) {
    if (a == NULL || b == NULL) return a == b;

    /* Caso especial: entero == decimal compara matemáticamente. */
    if (a->tipo == VAL_ENTERO && b->tipo == VAL_DECIMAL) {
        /* Convertir entero a double y comparar. Pierde precisión para
           enteros grandes, pero es la semántica de Python. */
        return mp_get_double(a->como.entero) == b->como.decimal;
    }
    if (a->tipo == VAL_DECIMAL && b->tipo == VAL_ENTERO) {
        return a->como.decimal == mp_get_double(b->como.entero);
    }

    /* Booleanos se comparan como enteros con otros tipos numéricos
       (Python: True == 1 == 1.0). */
    if (a->tipo == VAL_BOOLEANO
        && (b->tipo == VAL_ENTERO || b->tipo == VAL_DECIMAL)) {
        long ai = a->como.booleano ? 1 : 0;
        if (b->tipo == VAL_DECIMAL) return (double)ai == b->como.decimal;
        mp_int tmp;
        if (mp_init(&tmp) != MP_OKAY) return false;
        mp_set_l(&tmp, ai);
        bool igual = mp_cmp(&tmp, b->como.entero) == MP_EQ;
        mp_clear(&tmp);
        return igual;
    }
    if (b->tipo == VAL_BOOLEANO
        && (a->tipo == VAL_ENTERO || a->tipo == VAL_DECIMAL)) {
        long bi = b->como.booleano ? 1 : 0;
        if (a->tipo == VAL_DECIMAL) return a->como.decimal == (double)bi;
        mp_int tmp;
        if (mp_init(&tmp) != MP_OKAY) return false;
        mp_set_l(&tmp, bi);
        bool igual = mp_cmp(a->como.entero, &tmp) == MP_EQ;
        mp_clear(&tmp);
        return igual;
    }

    if (a->tipo != b->tipo) return false;

    switch (a->tipo) {
        case VAL_NULO:      return true;
        case VAL_BOOLEANO:  return a->como.booleano == b->como.booleano;
        case VAL_DECIMAL:   return a->como.decimal == b->como.decimal;
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
    }
    return false;
}
