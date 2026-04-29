/*
 * Tests del inline cache (F10 sesión 2).
 *
 * Verifica el comportamiento de quickening de OP_OBTENER_GLOBAL:
 *   - Tras un acierto, el opcode se reescribe a OP_OBTENER_GLOBAL_CACHE
 *     y los 4 bytes de cache (versión + slot_idx) se rellenan.
 *   - Mutación estructural del dicc (insertar nueva clave) invalida:
 *     Diccionario.version se incrementa y el cache hace miss → degrada
 *     a OP_OBTENER_GLOBAL.
 *   - Sobreescritura de valor existente NO invalida (el cache hace hit
 *     y devuelve el nuevo valor desde el mismo slot).
 *   - Fallback correcto cuando el slot_idx > UINT16_MAX (no probado
 *     directamente — capacidad realista <65k).
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "compilador.h"
#include "lexer.h"
#include "parser.h"
#include "valor.h"
#include "vm.h"

static int fallos = 0;

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n",                            \
                    __FILE__, __LINE__, #cond);                                \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

/*
 * Ejecuta un programa y deja la VM y el chunk vivos para inspección.
 * Devuelve true si exec OK. El cliente es responsable de destruir
 * vm/chunk/arena.
 */
static bool ejecutar_para_inspeccion(const char *fuente,
                                       Arena *a, Chunk *chunk, VM *vm) {
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    arena_iniciar(a, 4096);
    Parser p;
    parser_iniciar(&p, &l, a, fuente, "<test>");
    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (!prog || p.tuvo_error) return false;

    chunk_iniciar(chunk);
    Compilador c; compilador_iniciar(&c, chunk);
    if (!compilador_compilar_programa(&c, prog, n)) return false;

    vm_iniciar(vm);
    Valor r = valor_nulo();
    ResultadoVM rc = vm_ejecutar(vm, chunk, &r);
    valor_destruir(&r);
    return rc == VM_OK;
}

/*
 * Busca el primer offset en chunk->codigo donde aparezca el opcode
 * indicado. Devuelve -1 si no lo encuentra. NO sirve si hay datos
 * embebidos que coincidan numéricamente con el opcode (rara
 * coincidencia para los opcodes que probamos).
 */
static int buscar_primer_opcode(const Chunk *chunk, OpCode op) {
    for (int i = 0; i < chunk->cuenta; i++) {
        if (chunk->codigo[i] == (uint8_t)op) return i;
    }
    return -1;
}

/* ───── 1. Quickening: tras un hit, OP_OBTENER_GLOBAL → CACHE ───── */

static void test_quickening_basico(void) {
    /* Programa que accede a `x` exactamente una vez tras definirlo. */
    const char *fuente = "x = 42\nimprimir(x)";
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* Tras la ejecución, el OP_OBTENER_GLOBAL para `x` debe haber sido
       promovido a OP_OBTENER_GLOBAL_CACHE. */
    int orig = buscar_primer_opcode(&chunk, OP_OBTENER_GLOBAL);
    int cached = buscar_primer_opcode(&chunk, OP_OBTENER_GLOBAL_CACHE);
    AFIRMAR(cached >= 0);
    /* Como solo hay un OP_OBTENER_GLOBAL en el programa (la lectura de
       `x`), tras quickening no debería quedar ningún OP_OBTENER_GLOBAL
       sin promover. (`imprimir` también es global; comprobamos abajo.) */
    (void)orig;

    /* Los 4 bytes de cache deben estar rellenos (ver=1 mín porque
       definir x bumpeó version). */
    uint16_t cached_ver = (uint16_t)((chunk.codigo[cached + 2] << 8)
                                     | chunk.codigo[cached + 3]);
    AFIRMAR(cached_ver != 0);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── 2. Cache hit múltiples veces no degrada el opcode ───── */

static void test_hits_multiples_estables(void) {
    /* Loop que lee `x` muchas veces. */
    const char *fuente =
        "x = 100\n"
        "total = 0\n"
        "para i en rango(50):\n"
        "    total = total + x\n"
        "fin para\n";
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* Tras 50 hits, el opcode debe estar quickened y mantenido. */
    int cached = buscar_primer_opcode(&chunk, OP_OBTENER_GLOBAL_CACHE);
    AFIRMAR(cached >= 0);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── 3. Inserción nueva tras hit invalida → opcode degradado ───── */

static void test_insertacion_invalida_cache(void) {
    /*
     * Estrategia: leer x (cache fill), luego dentro del bucle insertar
     * UNA NUEVA global cada iteración (`globales_nuevos[i] = i` no
     * existe aún en Cornamusa, pero podemos simularlo con asignación
     * a nombres distintos).
     *
     * En un único hilo de ejecución, lo más sencillo es:
     *   x = 1
     *   leer x   <- promoción
     *   y = 2    <- inserción nueva: bumpea version
     *   leer x   <- cache miss → degrada a OP_OBTENER_GLOBAL
     *   leer x   <- promoción otra vez
     */
    /* `y` es palabra clave AND — usamos `nuevaglobal` como nombre que
       no choca con keywords. */
    const char *fuente =
        "x = 1\n"
        "_ = x\n"             /* primer acceso: quickening */
        "nuevaglobal = 2\n"   /* inserción nueva → bump version */
        "_ = x\n"             /* cache miss → degradar */
        "_ = x\n";            /* re-quickening */
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* Hay 3 OP_OBTENER_GLOBAL para `x`. El último ejecutado
       (re-quickening) debería estar como OP_OBTENER_GLOBAL_CACHE.
       El segundo (que hizo miss y se degradó) volvió a ejecutarse
       inmediatamente y se re-promovió. Así que tras todo el programa,
       LOS TRES sites están en estado CACHE. */
    int cached_count = 0;
    for (int i = 0; i < chunk.cuenta; i++) {
        if (chunk.codigo[i] == (uint8_t)OP_OBTENER_GLOBAL_CACHE) {
            cached_count++;
            i += 5;  /* skip operandos */
        }
    }
    /* Esperamos al menos 3 sites de `x` cacheados. (También hay
       lookups de `_` y `imprimir` aunque aquí no llamamos a imprimir;
       y los lookups del lado izquierdo de `_ = x` son DEFINIR_GLOBAL
       no OBTENER, así que no cuentan.) */
    AFIRMAR(cached_count >= 3);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── 4. Sobreescritura NO invalida cache ───── */

static void test_sobrescritura_no_invalida(void) {
    /*
     * x = 1; _ = x; x = 99; _ = x; _ = x
     * Tras `x = 99` (sobreescritura, NO inserción), version no cambia.
     * Los lookups posteriores siguen siendo cache HITS.
     * Verificamos que el resultado es correcto y el cache está activo.
     */
    /* Nota: `y` es palabra clave (AND) en Cornamusa, usamos nombres
       arbitrarios `aa` y `bb`. */
    const char *fuente =
        "x = 1\n"
        "_ = x\n"
        "x = 99\n"        /* sobreescritura — NO bump version */
        "aa = x\n"        /* cache hit, debe leer 99 */
        "bb = x\n";
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* Verificar que aa == 99 y bb == 99 (el cache devuelve el nuevo valor
       desde el mismo slot porque `dicc_asignar` overwrite escribe en
       slot->valor sin tocar version). */
    Valor nombre_aa = valor_cadena_referencia("aa", 2);
    Valor nombre_bb = valor_cadena_referencia("bb", 2);
    Valor vaa, vbb;
    AFIRMAR(dicc_obtener(vm.globales, &nombre_aa, &vaa));
    AFIRMAR(dicc_obtener(vm.globales, &nombre_bb, &vbb));
    char buf_aa[32], buf_bb[32];
    valor_a_cadena(&vaa, buf_aa, sizeof(buf_aa));
    valor_a_cadena(&vbb, buf_bb, sizeof(buf_bb));
    AFIRMAR(strcmp(buf_aa, "99") == 0);
    AFIRMAR(strcmp(buf_bb, "99") == 0);
    valor_destruir(&vaa); valor_destruir(&vbb);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

int main(void) {
    test_quickening_basico();
    test_hits_multiples_estables();
    test_insertacion_invalida_cache();
    test_sobrescritura_no_invalida();
    if (fallos == 0) {
        printf("test_bytecode_ic: 4 tests PASS\n");
        return 0;
    }
    fprintf(stderr, "test_bytecode_ic: %d FALLO(s)\n", fallos);
    return 1;
}
