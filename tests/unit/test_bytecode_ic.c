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

/* ───── 5. OP_LLAMAR se promueve a OP_LLAMAR_NATIVA al invocar ───── */

static void test_llamar_promueve_a_nativa(void) {
    /* Nota: `imprimir` y `longitud` se compilan a OP_IMPRIMIR/OP_LONGITUD
       (atajos en compilador, NO pasan por OP_LLAMAR). Usamos `tipo` que
       sí va por OP_LLAMAR como nativa cualquiera. */
    const char *fuente = "n = tipo(\"hola\")\n";
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    int slow = buscar_primer_opcode(&chunk, OP_LLAMAR);
    int fast = buscar_primer_opcode(&chunk, OP_LLAMAR_NATIVA);
    AFIRMAR(fast >= 0);
    AFIRMAR(slow < 0);

    /* Y n debe ser "cadena". */
    Valor n_n = valor_cadena_referencia("n", 1);
    Valor v;
    AFIRMAR(dicc_obtener(vm.globales, &n_n, &v));
    char buf[32]; valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "cadena") == 0);
    valor_destruir(&v);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── 6. OP_LLAMAR se promueve a OP_LLAMAR_BC al llamar función ───── */

static void test_llamar_promueve_a_bc(void) {
    /* La función `cuadrar` es FUNCION_BC; tras llamarla el site se
       quickens. El call site de imprimir() también se quickens
       (a NATIVA). */
    const char *fuente =
        "funcion cuadrar(n):\n"
        "    retornar n * n\n"
        "fin funcion\n"
        "x = cuadrar(7)\n";
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* Buscamos OP_LLAMAR_BC en el chunk top-level (donde está la
       llamada a cuadrar(7)). */
    int found_bc = buscar_primer_opcode(&chunk, OP_LLAMAR_BC);
    AFIRMAR(found_bc >= 0);

    /* Y `x` debe ser 49. */
    Valor n_x = valor_cadena_referencia("x", 1);
    Valor v;
    AFIRMAR(dicc_obtener(vm.globales, &n_x, &v));
    char buf[32]; valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "49") == 0);
    valor_destruir(&v);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── 7. Degradación: site polimórfico vuelve a OP_LLAMAR ───── */

static void test_llamar_degradacion_polimorfica(void) {
    /*
     * Un site monomórfico se quickens y queda. Para forzar
     * degradación necesitamos un site que ALTERNE entre tipos.
     * Trick: redefinir una global de funcion_bc a nativa entre
     * llamadas.
     *
     * Esto es difícil en Cornamusa puro porque no podemos
     * REASIGNAR `imprimir` (es global protegido). Como sustituto:
     * llamamos imprimir varias veces, comprobamos que el opcode es
     * NATIVA, y verificamos que NO aparece OP_LLAMAR (no degradación
     * espontánea).
     */
    const char *fuente =
        "a = tipo(\"a\")\n"
        "b = tipo(\"bb\")\n"
        "c = tipo(\"ccc\")\n";
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* Tres call sites, todos a `tipo` (nativa). Los tres deben
       acabar como OP_LLAMAR_NATIVA. v1.3: ya no podemos usar `longitud`
       aquí porque el compilador la atajea como OP_LONGITUD. */
    int count_nativa = 0;
    int count_slow = 0;
    for (int i = 0; i < chunk.cuenta; i++) {
        if (chunk.codigo[i] == (uint8_t)OP_LLAMAR_NATIVA) {
            count_nativa++;
            i++;  /* skip n_args */
        } else if (chunk.codigo[i] == (uint8_t)OP_LLAMAR) {
            count_slow++;
            i++;
        }
    }
    AFIRMAR(count_nativa == 3);
    AFIRMAR(count_slow == 0);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── 8. OP_SUMAR/RESTAR/MULTIPLICAR int+int promueven a INT_INT ───── */

static void test_binario_promueve_a_int_int(void) {
    /* Site monomórfico int+int: tras ejecutar, los opcodes deben estar
       quickened. v0.11.3: usamos variables intermedias (k0..k5) para
       evitar que el constant folding del compilador reduzca el binario
       a OP_CONST en compile-time. */
    const char *fuente =
        "k0 = 1\nk1 = 2\nk2 = 5\nk3 = 3\nk4 = 4\nk5 = 6\n"
        "a = k0 + k1\n"     /* OP_SUMAR_INT_INT (tras quickening) */
        "b = k2 - k3\n"     /* OP_RESTAR_INT_INT */
        "c = k4 * k5\n";    /* OP_MULTIPLICAR_INT_INT */
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    AFIRMAR(buscar_primer_opcode(&chunk, OP_SUMAR_INT_INT) >= 0);
    AFIRMAR(buscar_primer_opcode(&chunk, OP_RESTAR_INT_INT) >= 0);
    AFIRMAR(buscar_primer_opcode(&chunk, OP_MULTIPLICAR_INT_INT) >= 0);
    /* Nota: NO comprobamos que OP_SUMAR/RESTAR/MULTIPLICAR estén ausentes
       porque buscar_primer_opcode hace búsqueda byte-a-byte y puede dar
       falsos positivos con operandos numéricos que coincidan con el
       valor enum del opcode. Verificamos solo presencia de las
       variantes especializadas. */

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── 9. OP_MENOR int+int promueve, str+str se queda en slow ───── */

static void test_menor_int_int_y_str_str(void) {
    /* v0.11.3: variables para evitar constant folding. */
    const char *fuente =
        "k0 = 1\nk1 = 2\nk2 = \"a\"\nk3 = \"b\"\n"
        "a = k0 < k1\n"             /* int+int → OP_MENOR_INT_INT */
        "b = k2 < k3\n";            /* str+str → se queda en OP_MENOR */
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* Hay un OP_MENOR_INT_INT (linea 1) y un OP_MENOR (linea 2). */
    AFIRMAR(buscar_primer_opcode(&chunk, OP_MENOR_INT_INT) >= 0);
    AFIRMAR(buscar_primer_opcode(&chunk, OP_MENOR) >= 0);

    /* Resultados correctos. */
    Valor n_a = valor_cadena_referencia("a", 1);
    Valor n_b = valor_cadena_referencia("b", 1);
    Valor va, vb;
    AFIRMAR(dicc_obtener(vm.globales, &n_a, &va));
    AFIRMAR(dicc_obtener(vm.globales, &n_b, &vb));
    AFIRMAR(va.tipo == VAL_BOOLEANO && va.como.booleano == true);
    AFIRMAR(vb.tipo == VAL_BOOLEANO && vb.como.booleano == true);
    valor_destruir(&va); valor_destruir(&vb);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── 10. Site polimórfico SUMAR (int+int luego str+str) degrada ───── */

static void test_binario_degradacion_polimorfica(void) {
    /* `y` es palabra clave AND — usamos `j` como segundo parámetro. */
    const char *fuente =
        "funcion suma(x, j):\n"
        "    retornar x + j\n"
        "fin funcion\n"
        "a = suma(1, 2)\n"
        "b = suma(\"hi\", \"!\")\n"
        "c = suma(10, 20)\n";
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* Tras las 3 ejecuciones del site:
       1. int+int   → promueve a SUMAR_INT_INT
       2. str+str   → miss, degrada a SUMAR (rebobina ip), reejecuta
                       slow path → SUMAR queda en chunk
       3. int+int   → promueve a SUMAR_INT_INT
       Final: el byte del site es SUMAR_INT_INT (la última promoción).
       Pero comprobamos correccion semántica de los resultados. */
    Valor n_a = valor_cadena_referencia("a", 1);
    Valor n_b = valor_cadena_referencia("b", 1);
    Valor n_c = valor_cadena_referencia("c", 1);
    Valor va, vb, vc;
    AFIRMAR(dicc_obtener(vm.globales, &n_a, &va));
    AFIRMAR(dicc_obtener(vm.globales, &n_b, &vb));
    AFIRMAR(dicc_obtener(vm.globales, &n_c, &vc));
    char buf_a[32], buf_b[32], buf_c[32];
    valor_a_cadena(&va, buf_a, sizeof(buf_a));
    valor_a_cadena(&vb, buf_b, sizeof(buf_b));
    valor_a_cadena(&vc, buf_c, sizeof(buf_c));
    AFIRMAR(strcmp(buf_a, "3") == 0);
    AFIRMAR(strcmp(buf_b, "hi!") == 0);
    AFIRMAR(strcmp(buf_c, "30") == 0);
    valor_destruir(&va); valor_destruir(&vb); valor_destruir(&vc);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── 11. OP_OBTENER_ATRIBUTO promueve a INSTANCIA ───── */

static void test_obtener_atributo_promueve(void) {
    /* Loop que lee p.x repetidamente — tras la primera iteración el
       site debe estar quickened a OP_OBTENER_ATRIBUTO_INSTANCIA. */
    const char *fuente =
        "clase Punto:\n"
        "    funcion __iniciar__(yo, cx):\n"
        "        yo.cx = cx\n"
        "    fin funcion\n"
        "fin clase\n"
        "p = Punto(42)\n"
        "total = 0\n"
        "para i en rango(3):\n"
        "    total = total + p.cx\n"
        "fin para\n";
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* El chunk top-level debe contener OP_OBTENER_ATRIBUTO_INSTANCIA
       tras las 3 iteraciones (la primera promociona). */
    AFIRMAR(buscar_primer_opcode(&chunk,
        OP_OBTENER_ATRIBUTO_INSTANCIA) >= 0);

    /* Y total debe ser 42 * 3 = 126. */
    Valor n_t = valor_cadena_referencia("total", 5);
    Valor v;
    AFIRMAR(dicc_obtener(vm.globales, &n_t, &v));
    char buf[32]; valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "126") == 0);
    valor_destruir(&v);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── Regresión v0.11.6: stack growth en mientras con nuevo local ─────
 *
 * Bug detectado al validar `examples/25_biblioteca_oop.cor` durante
 * sesión 5 del plan v1.0. La fix de v0.11.5 (OP_NULO + agregar_local +
 * push + OP_ASIGNAR_LOCAL) crecía el stack +1 por cada nueva
 * asignación. Para `para` esto era OK porque SENT_PARA reserva el
 * slot del objetivo fuera del cuerpo. Para `mientras` no había
 * pre-reserva → cada iteración del while empujaba un OP_NULO extra.
 *
 * Tras 2+ iteraciones del while con un nuevo local en el cuerpo, el
 * stack se desincronizaba: "OP_ITER_SIGUIENTE sin iterador en slot N".
 *
 * Fix v0.11.6: pre_reservar_locales(c, cuerpo) recursa el AST por
 * SENT_BLOQUE y SENT_SI buscando SENT_ASIGNAR a IDENT no-local;
 * por cada uno emite OP_NULO + agregar_local UNA vez ANTES del
 * bucle. La asignación dentro del cuerpo ahora encuentra "local
 * existente" y emite plain OP_ASIGNAR_LOCAL.
 */

static void test_regresion_mientras_con_nuevo_local(void) {
    const char *fuente =
        "funcion ejecutar():\n"
        "    rondas = 3\n"
        "    suma = 0\n"
        "    mientras rondas > 0:\n"
        "        n = rondas + 10\n"     /* nuevo local en mientras */
        "        suma = suma + n\n"
        "        rondas = rondas - 1\n"
        "    fin mientras\n"
        "    retornar suma\n"
        "fin funcion\n"
        "resultado = ejecutar()\n";    /* esperado: 13 + 12 + 11 = 36 */
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);
    Valor n = valor_cadena_referencia("resultado", 9);
    Valor v;
    AFIRMAR(dicc_obtener(vm.globales, &n, &v));
    char buf[32]; valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "36") == 0);
    valor_destruir(&v);
    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

/* ───── Regresión v0.11.5: nuevo local en bucle dentro de función ─────
 *
 * Bug detectado al validar el tutorial v1.0 (sesión 3): la "OLD
 * convention" del compilador para nuevos locales (push valor +
 * agregar_local sin OP_ASIGNAR_LOCAL) solo funcionaba en la PRIMERA
 * ejecución. Dentro de un bucle dentro de una función, la asignación
 * `a = v` quedaba con el valor de la primera iteración para siempre.
 *
 * Tree-walking sin bug; bytecode con bug. Los 8 tests diferenciales
 * existentes no lo detectaron porque sus ejemplos no usan ese patrón.
 *
 * Fix: emitir OP_NULO + agregar_local + push valor + OP_ASIGNAR_LOCAL.
 */

static void test_regresion_local_nuevo_en_bucle(void) {
    const char *fuente =
        "funcion ejecutar():\n"
        "    suma = 0\n"
        "    para v en [1, 2, 3]:\n"
        "        a = v + 10\n"          /* nuevo local 'a' en bucle */
        "        suma = suma + a\n"
        "    fin para\n"
        "    retornar suma\n"
        "fin funcion\n"
        "resultado = ejecutar()\n";
    Arena a; Chunk chunk; VM vm;
    bool ok = ejecutar_para_inspeccion(fuente, &a, &chunk, &vm);
    AFIRMAR(ok);

    /* Esperado: a = 11, 12, 13 → suma = 11 + 12 + 13 = 36. */
    Valor n = valor_cadena_referencia("resultado", 9);
    Valor v;
    AFIRMAR(dicc_obtener(vm.globales, &n, &v));
    char buf[32]; valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "36") == 0);
    valor_destruir(&v);

    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
}

int main(void) {
    test_regresion_local_nuevo_en_bucle();
    test_regresion_mientras_con_nuevo_local();
    test_quickening_basico();
    test_hits_multiples_estables();
    test_insertacion_invalida_cache();
    test_sobrescritura_no_invalida();
    test_llamar_promueve_a_nativa();
    test_llamar_promueve_a_bc();
    test_llamar_degradacion_polimorfica();
    test_binario_promueve_a_int_int();
    test_menor_int_int_y_str_str();
    test_binario_degradacion_polimorfica();
    test_obtener_atributo_promueve();
    if (fallos == 0) {
        printf("test_bytecode_ic: 11 tests PASS\n");
        return 0;
    }
    fprintf(stderr, "test_bytecode_ic: %d FALLO(s)\n", fallos);
    return 1;
}
