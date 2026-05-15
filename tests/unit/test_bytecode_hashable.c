/*
 * Tests de las instancias hashables introducidas en v1.42:
 *
 *   - `__hash__(yo)` invocado por `hash_valor` (vía hook).
 *   - `__igual__(yo, otro)` invocado por `valor_iguales` (vía hook).
 *   - Cache perezoso en `Instancia.cache_hash` (despacho una vez).
 *   - Mecanismo de sub-VM síncrono: pushes frame, corre dispatch hasta
 *     OP_RETORNAR del dunder, retorna el valor.
 *   - Handler_techo evita que excepciones del dunder se desenrosquen
 *     más allá del sub-VM (propagación se hace vía bandera one-shot).
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

static const char *ejecutar(const char *fuente, const char *nombre_var,
                              const char **error_out) {
    static char buffer[4096];

    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (!prog || p.tuvo_error) {
        if (error_out) *error_out = "<error de parseo>";
        arena_destruir(&a);
        return NULL;
    }

    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, prog, n)) {
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", c.error.mensaje);
            *error_out = errbuf;
        }
        chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }

    VM vm; vm_iniciar(&vm);
    Valor resultado = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &resultado);
    if (rc != VM_OK) {
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", vm.error.mensaje);
            *error_out = errbuf;
        }
        valor_destruir(&resultado);
        vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }

    Valor nombre = valor_cadena_referencia(nombre_var, (int)strlen(nombre_var));
    Valor v;
    if (!dicc_obtener(vm.globales, &nombre, &v)) {
        if (error_out) *error_out = "<variable no encontrada>";
        valor_destruir(&resultado);
        vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    valor_a_cadena(&v, buffer, sizeof(buffer));
    valor_destruir(&v);
    valor_destruir(&resultado);
    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
    if (error_out) *error_out = NULL;
    return buffer;
}

static void verificar_var(const char *fuente, const char *var,
                           const char *esperado) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, var, &err);
    if (!res) {
        fprintf(stderr, "FALLO: %s\n  -> error: %s\n", fuente,
                err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO: %s\n  -> %s=%s (esperaba %s)\n",
                fuente, var, res, esperado);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, "_n", &err);
    if (res) {
        fprintf(stderr, "FALLO: '%s' debia dar error pero ejecuto\n", fuente);
        fallos++;
        return;
    }
    if (substring && (!err || !strstr(err, substring))) {
        fprintf(stderr, "FALLO: error %s no contiene '%s'\n",
                err ? err : "<vacio>", substring);
        fallos++;
    }
}

#define DEF_PUNTO                                              \
    "clase P:\n"                                                \
    "  funcion __iniciar__(yo, a, b):\n"                        \
    "    yo.a = a\n"                                            \
    "    yo.b = b\n"                                            \
    "  fin funcion\n"                                           \
    "  funcion __hash__(yo):\n"                                 \
    "    retornar yo.a * 31 + yo.b\n"                           \
    "  fin funcion\n"                                           \
    "  funcion __igual__(yo, otro):\n"                          \
    "    retornar yo.a == otro.a y yo.b == otro.b\n"            \
    "  fin funcion\n"                                           \
    "fin clase\n"

/* ─── __hash__ + __igual__ básicos ─── */

static void test_dict_dedupe_por_valor(void) {
    /* Dos instancias con mismo __hash__ y __igual__ → mismo slot. */
    verificar_var(DEF_PUNTO
                  "d = {}\n"
                  "d[P(3, 4)] = \"primero\"\n"
                  "d[P(3, 4)] = \"sobreescrito\"\n"
                  "n = longitud(d)\n",
                  "n", "1");
    verificar_var(DEF_PUNTO
                  "d = {}\n"
                  "d[P(3, 4)] = \"a\"\n"
                  "d[P(3, 4)] = \"b\"\n"
                  "r = d[P(3, 4)]\n",
                  "r", "b");
}

static void test_dict_distintos_por_valor(void) {
    /* Instancias con valores distintos quedan como entradas separadas. */
    verificar_var(DEF_PUNTO
                  "d = {}\n"
                  "d[P(1, 2)] = \"a\"\n"
                  "d[P(3, 4)] = \"b\"\n"
                  "d[P(5, 6)] = \"c\"\n"
                  "n = longitud(d)\n",
                  "n", "3");
}

static void test_conjunto_dedupe(void) {
    verificar_var(DEF_PUNTO
                  "s = {P(1, 1), P(1, 1), P(2, 2)}\n"
                  "n = longitud(s)\n",
                  "n", "2");
}

static void test_membership_en_conjunto(void) {
    verificar_var(DEF_PUNTO
                  "s = {P(1, 1), P(2, 2), P(3, 3)}\n"
                  "r = P(2, 2) en s\n",
                  "r", "verdadero");
    verificar_var(DEF_PUNTO
                  "s = {P(1, 1), P(2, 2)}\n"
                  "r = P(9, 9) en s\n",
                  "r", "falso");
}

static void test_membership_en_dict(void) {
    verificar_var(DEF_PUNTO
                  "d = {P(1, 1): \"x\", P(2, 2): \"y\"}\n"
                  "r = P(1, 1) en d\n",
                  "r", "verdadero");
}

/* ─── Sin __hash__: identidad ─── */

static void test_sin_hash_es_identidad(void) {
    /* Sin __hash__, dos instancias distintas son claves distintas. */
    verificar_var("clase Sin:\n"
                  "  pasar\n"
                  "fin clase\n"
                  "a = Sin()\n"
                  "b = Sin()\n"
                  "d = {a: 1, b: 2}\n"
                  "n = longitud(d)\n",
                  "n", "2");
    /* La misma instancia sí encuentra su slot. */
    verificar_var("clase Sin:\n"
                  "  pasar\n"
                  "fin clase\n"
                  "a = Sin()\n"
                  "d = {a: 1}\n"
                  "r = d[a]\n",
                  "r", "1");
}

/* ─── Built-ins agregar/quitar/en para set/dict ─── */

static void test_agregar_quitar_conjunto(void) {
    verificar_var(DEF_PUNTO
                  "s = conjunto()\n"
                  "agregar(s, P(1, 1))\n"
                  "agregar(s, P(2, 2))\n"
                  "agregar(s, P(1, 1))\n"
                  "n = longitud(s)\n",
                  "n", "2");
    verificar_var(DEF_PUNTO
                  "s = {P(1, 1), P(2, 2), P(3, 3)}\n"
                  "quitar(s, P(2, 2))\n"
                  "r = P(2, 2) en s\n",
                  "r", "falso");
}

static void test_quitar_de_dict(void) {
    verificar_var(DEF_PUNTO
                  "d = {P(1, 1): \"a\", P(2, 2): \"b\"}\n"
                  "quitar(d, P(1, 1))\n"
                  "r = longitud(d)\n",
                  "r", "1");
}

/* ─── Errores: __hash__ lanzar, __hash__ retorno no entero ─── */

static void test_hash_lanzar_se_atrapa(void) {
    /* `lanzar` dentro de __hash__ se propaga al `intentar/atrapar` del
       caller. El handler_techo del sub-VM impide que escape sin
       control el C-stack. */
    verificar_var("clase Bad:\n"
                  "  funcion __hash__(yo):\n"
                  "    lanzar ErrorDeValor(\"no\")\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "atrapado = falso\n"
                  "intentar:\n"
                  "  d = {Bad(): 1}\n"
                  "atrapar ErrorDeValor como e:\n"
                  "  atrapado = verdadero\n"
                  "fin intentar\n",
                  "atrapado", "verdadero");
}

static void test_hash_no_entero_es_error(void) {
    verificar_error("clase Mal:\n"
                    "  funcion __hash__(yo):\n"
                    "    retornar \"no soy entero\"\n"
                    "  fin funcion\n"
                    "fin clase\n"
                    "_n = {Mal(): 1}\n",
                    "__hash__ debe retornar entero");
}

/* ─── Cache: __hash__ se llama una vez por instancia ─── */

static void test_hash_cacheado(void) {
    /* Incrementamos un contador en __hash__. Lo usamos varias veces
       para la misma instancia. Solo debería ejecutarse 1 vez gracias
       al cache. */
    verificar_var("contador = [0]\n"
                  "clase Contado:\n"
                  "  funcion __hash__(yo):\n"
                  "    contador[0] = contador[0] + 1\n"
                  "    retornar 42\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "p = Contado()\n"
                  "d = {}\n"
                  "d[p] = \"a\"\n"
                  "_ = p en d\n"
                  "_ = d[p]\n"
                  "n = contador[0]\n",
                  "n", "1");
}

/* ─── Recursión dentro del dunder ─── */

static void test_dunder_recursivo(void) {
    /* El dunder puede llamar a otra función. Aquí __hash__ delega en
       un método auxiliar. Verifica que el sub-VM maneja bien frames
       anidados. */
    verificar_var("clase Q:\n"
                  "  funcion __iniciar__(yo, n):\n"
                  "    yo.n = n\n"
                  "  fin funcion\n"
                  "  funcion calcular(yo):\n"
                  "    retornar yo.n * 7\n"
                  "  fin funcion\n"
                  "  funcion __hash__(yo):\n"
                  "    retornar yo.calcular()\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "d = {Q(3): \"x\"}\n"
                  "n = longitud(d)\n",
                  "n", "1");
}

int main(void) {
    test_dict_dedupe_por_valor();
    test_dict_distintos_por_valor();
    test_conjunto_dedupe();
    test_membership_en_conjunto();
    test_membership_en_dict();
    test_sin_hash_es_identidad();
    test_agregar_quitar_conjunto();
    test_quitar_de_dict();
    test_hash_lanzar_se_atrapa();
    test_hash_no_entero_es_error();
    test_hash_cacheado();
    test_dunder_recursivo();

    if (fallos == 0) {
        printf("hashable: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "hashable: %d fallo(s)\n", fallos);
    return 1;
}
