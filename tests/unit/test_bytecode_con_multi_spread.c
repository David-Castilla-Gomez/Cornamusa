/*
 * Tests de v1.46: multi-recurso en `con` y combinación de `*args` con
 * `**kwargs` en la misma llamada.
 *
 * `con A, B:` se desazucara a `con A: con B:` (anidados). Entra en
 * orden A→B; sale en orden inverso B→A (LIFO).
 *
 * Combinar `*args` + kwarg + `**dict` en una llamada: construye una
 * lista runtime con los posicionales (incluido el `*spread`) y un
 * dict con kwargs/`**dict`, los pasa juntos vía
 * OP_LLAMAR_SPREAD_KW_DICT.
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

#define DEF_R \
    "clase R:\n" \
    "  funcion __iniciar__(yo, nombre):\n" \
    "    yo.nombre = nombre\n" \
    "    yo.evento = []\n" \
    "  fin funcion\n" \
    "  funcion __entrar__(yo):\n" \
    "    agregar(orden, 'entra ' + yo.nombre)\n" \
    "    retornar yo\n" \
    "  fin funcion\n" \
    "  funcion __salir__(yo, _t, _v, _b):\n" \
    "    agregar(orden, 'sale ' + yo.nombre)\n" \
    "  fin funcion\n" \
    "fin clase\n"

#define DEF_DESTINO \
    "funcion destino(a, b, c=0, d=0):\n" \
    "  retornar f\"a={a} b={b} c={c} d={d}\"\n" \
    "fin funcion\n"

/* ─── Multi-recurso `con` ─── */

static void test_con_dos_recursos_orden_LIFO(void) {
    /* Dos recursos: entra a, b; sale b, a. */
    verificar_var(DEF_R
                  "orden = []\n"
                  "con R(\"a\") como a, R(\"b\") como b:\n"
                  "  agregar(orden, \"dentro\")\n"
                  "fin con\n"
                  "r = cadena(orden)\n",
                  "r",
                  "[\"entra a\", \"entra b\", \"dentro\", \"sale b\", \"sale a\"]");
}

static void test_con_tres_recursos(void) {
    verificar_var(DEF_R
                  "orden = []\n"
                  "con R(\"a\"), R(\"b\"), R(\"c\"):\n"
                  "  agregar(orden, \"medio\")\n"
                  "fin con\n"
                  "r = cadena(orden)\n",
                  "r",
                  "[\"entra a\", \"entra b\", \"entra c\", \"medio\", \"sale c\", \"sale b\", \"sale a\"]");
}

static void test_con_excepcion_libera_en_orden_inverso(void) {
    /* Si el cuerpo lanza, __salir__ se ejecuta de adentro hacia
       afuera incluso ante excepción. */
    verificar_var(DEF_R
                  "orden = []\n"
                  "intentar:\n"
                  "  con R(\"a\"), R(\"b\"):\n"
                  "    lanzar ErrorDeValor(\"boom\")\n"
                  "  fin con\n"
                  "atrapar ErrorDeValor como e:\n"
                  "  agregar(orden, \"atrapado\")\n"
                  "fin intentar\n"
                  "r = cadena(orden)\n",
                  "r",
                  "[\"entra a\", \"entra b\", \"sale b\", \"sale a\", \"atrapado\"]");
}

static void test_con_mezcla_con_y_sin_alias(void) {
    /* Algunos recursos con alias, otros sin. */
    verificar_var(DEF_R
                  "orden = []\n"
                  "con R(\"x\"), R(\"y\") como yy:\n"
                  "  agregar(orden, \"yy=\" + yy.nombre)\n"
                  "fin con\n"
                  "r = cadena(orden)\n",
                  "r",
                  "[\"entra x\", \"entra y\", \"yy=y\", \"sale y\", \"sale x\"]");
}

/* ─── Combinar *args + **kwargs ─── */

static void test_spread_combinado_con_kwarg(void) {
    verificar_var(DEF_DESTINO
                  "r = destino(*[1, 2], c=10)\n",
                  "r", "a=1 b=2 c=10 d=0");
}

static void test_spread_combinado_con_dspread(void) {
    verificar_var(DEF_DESTINO
                  "r = destino(*[1, 2], **{\"c\": 7, \"d\": 8})\n",
                  "r", "a=1 b=2 c=7 d=8");
}

static void test_spread_combinado_con_kwarg_y_dspread(void) {
    verificar_var(DEF_DESTINO
                  "r = destino(*[1], 2, c=3, **{\"d\": 4})\n",
                  "r", "a=1 b=2 c=3 d=4");
}

static void test_wrapper_generico(void) {
    /* El patrón típico: `funcion wrap(f, *args, **kw): retornar f(*args, **kw)`. */
    verificar_var(DEF_DESTINO
                  "funcion wrap(f, *args, **kw):\n"
                  "  retornar f(*args, **kw)\n"
                  "fin funcion\n"
                  "r = wrap(destino, 1, 2, c=9, d=10)\n",
                  "r", "a=1 b=2 c=9 d=10");
}

static void test_wrapper_solo_spread(void) {
    /* También funciona el caso donde kw está vacío. */
    verificar_var(DEF_DESTINO
                  "funcion wrap(f, *args, **kw):\n"
                  "  retornar f(*args, **kw)\n"
                  "fin funcion\n"
                  "r = wrap(destino, *[10, 20], **{\"c\": 30})\n",
                  "r", "a=10 b=20 c=30 d=0");
}

int main(void) {
    test_con_dos_recursos_orden_LIFO();
    test_con_tres_recursos();
    test_con_excepcion_libera_en_orden_inverso();
    test_con_mezcla_con_y_sin_alias();

    test_spread_combinado_con_kwarg();
    test_spread_combinado_con_dspread();
    test_spread_combinado_con_kwarg_y_dspread();
    test_wrapper_generico();
    test_wrapper_solo_spread();

    if (fallos == 0) {
        printf("con_multi_spread: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "con_multi_spread: %d fallo(s)\n", fallos);
    return 1;
}
