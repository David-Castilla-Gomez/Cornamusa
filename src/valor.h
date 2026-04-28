#ifndef CORNAMUSA_VALOR_H
#define CORNAMUSA_VALOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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

typedef enum {
    VAL_NULO,
    VAL_BOOLEANO,
    VAL_ENTERO,        /* bignum boxed (mp_int *) */
    VAL_DECIMAL,       /* double IEEE 754 */
    VAL_CADENA,        /* texto UTF-8, ref al buffer fuente o heap */
    VAL_FUNCION,       /* función definida por el usuario */
    VAL_NATIVA,        /* función nativa (built-in en C) */
    VAL_RANGO,         /* iterable rango(inicio, fin, paso) */
} TipoValor;

/*
 * Firma de una función nativa (puntero a función C). Definida con
 * `struct Valor` porque el typedef `Valor` no existe todavía dentro
 * de su propia definición. Recibe argumentos ya evaluados (el
 * llamador conserva ownership y los destruye al finalizar la
 * llamada) y la posición del call-site. Devuelve el Valor resultado
 * del que el llamador toma posesión.
 */
typedef struct Valor (*FnNativa)(struct Evaluador *ev, int n_args,
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
        mp_int *entero;            /* malloc'd; liberado en valor_destruir */
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
    } como;
} Valor;

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
 */
Valor valor_entero_de_long(long v);

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
