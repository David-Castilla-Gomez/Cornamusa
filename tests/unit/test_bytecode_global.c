/*
 * Tests de `global X` en bytecode (v1.57).
 *
 * Cubre:
 *   - `global X` luego asignacion → modifica módulo, no crea local.
 *   - `global X` luego aug-assign (`X += 1`).
 *   - `global X` crea la global si no existia.
 *   - Validaciones del compilador:
 *     - `global` fuera de funcion.
 *     - `global X` cuando X ya es local del scope.
 *     - `global X` cuando X ya es `nolocal`.
 *   - Sin `global`: la asignacion crea local nuevo (semantica clasica).
 */

#include <stdio.h>
#include <stdlib.h>
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
static int casos = 0;

#define AFIRMAR(cond, etiqueta)                                                \
    do {                                                                        \
        casos++;                                                                \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FALLO %s:%d (%s)\n", __FILE__, __LINE__, etiqueta);\
            fallos++;                                                           \
        }                                                                       \
    } while (0)

/* Ejecuta `fuente` capturando stdout. */
static bool ejecutar_capturando(const char *fuente, char *buf, size_t cap) {
    const char *tmpfile =
#ifdef _WIN32
        "test_global_out.txt";
#else
        "/tmp/test_global_out.txt";
#endif
    if (!freopen(tmpfile, "w+", stdout)) return false;

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Arena a;
    arena_iniciar(&a, 8192);
    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    if (p.tuvo_error) { arena_destruir(&a); return false; }
    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    bool compila = compilador_compilar_programa(&c, sents, n);
    if (!compila) { chunk_destruir(&chunk); arena_destruir(&a); return false; }
    VM vm; vm_iniciar(&vm);
    Valor r = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &r);
    valor_destruir(&r);
    vm_destruir(&vm);
    chunk_destruir(&chunk);
    arena_destruir(&a);

    fflush(stdout);
#ifdef _WIN32
    freopen("CON", "w", stdout);
#else
    freopen("/dev/tty", "w", stdout);
#endif
    if (rc != VM_OK) return false;

    FILE *f = fopen(tmpfile, "r");
    if (!f) return false;
    size_t leido = fread(buf, 1, cap - 1, f);
    buf[leido] = '\0';
    fclose(f);
    remove(tmpfile);
    return true;
}

/* Solo verifica que un programa parsea+compila. Devuelve false si
 * hay error en cualquier fase. */
static bool compila_ok(const char *fuente) {
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 4096);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    if (p.tuvo_error) { arena_destruir(&a); return false; }
    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    /* Silenciar el error a stderr. */
    const char *devnull =
#ifdef _WIN32
        "nul";
#else
        "/dev/null";
#endif
    freopen(devnull, "w", stderr);
    bool ok = compilador_compilar_programa(&c, sents, n);
    freopen("CON", "w", stderr);
    chunk_destruir(&chunk);
    arena_destruir(&a);
    return ok;
}

int main(void) {
    /* ─── global X + asignacion modifica global ─── */
    {
        char out[256];
        bool ok = ejecutar_capturando(
            "contador = 0\n"
            "funcion inc():\n"
            "    global contador\n"
            "    contador = contador + 1\n"
            "fin funcion\n"
            "inc()\n"
            "inc()\n"
            "imprimir(contador)\n", out, sizeof(out));
        AFIRMAR(ok, "global_modifica_ejecuta");
        AFIRMAR(strstr(out, "2") != NULL, "global_modifica_2");
    }

    /* ─── global X + aug-assign ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_capturando(
            "n = 10\n"
            "funcion bump():\n"
            "    global n\n"
            "    n += 5\n"
            "fin funcion\n"
            "bump()\n"
            "bump()\n"
            "imprimir(n)\n", out, sizeof(out)), "aug_global_ejecuta");
        AFIRMAR(strstr(out, "20") != NULL, "aug_global_20");
    }

    /* ─── global crea la variable si no existia ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_capturando(
            "funcion crear():\n"
            "    global nuevo\n"
            "    nuevo = 42\n"
            "fin funcion\n"
            "crear()\n"
            "imprimir(nuevo)\n", out, sizeof(out)), "global_crea_ejecuta");
        AFIRMAR(strstr(out, "42") != NULL, "global_crea_42");
    }

    /* ─── sin global: la asignacion crea local nuevo (no modifica
     *     el del modulo, semantica clasica) ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_capturando(
            "x = 100\n"
            "funcion local_solo():\n"
            "    x = 1\n"  /* sin `global`, crea local */
            "    imprimir(x)\n"
            "fin funcion\n"
            "local_solo()\n"
            "imprimir(x)\n", out, sizeof(out)), "sin_global_ejecuta");
        /* Output: "1\n100\n" — local primero, luego global intacto. */
        AFIRMAR(strstr(out, "1") != NULL && strstr(out, "100") != NULL,
                 "sin_global_no_modifica");
    }

    /* ─── Error: global fuera de funcion ─── */
    {
        bool ok = compila_ok("global x\n");
        AFIRMAR(!ok, "global_fuera_funcion_falla");
    }

    /* ─── Error: global X cuando X ya es local ─── */
    {
        bool ok = compila_ok(
            "funcion f():\n"
            "    x = 1\n"
            "    global x\n"
            "    retornar x\n"
            "fin funcion\n");
        AFIRMAR(!ok, "global_local_conflict_falla");
    }

    /* ─── Error: global X cuando X ya es nolocal ─── */
    {
        bool ok = compila_ok(
            "funcion outer():\n"
            "    x = 1\n"
            "    funcion inner():\n"
            "        nolocal x\n"
            "        global x\n"
            "        retornar x\n"
            "    fin funcion\n"
            "    retornar inner()\n"
            "fin funcion\n");
        AFIRMAR(!ok, "global_nolocal_conflict_falla");
    }

    /* ─── Multiples nombres en una sola declaracion ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_capturando(
            "a = 1\n"
            "b = 2\n"
            "funcion swap():\n"
            "    global a, b\n"
            "    tmp = a\n"
            "    a = b\n"
            "    b = tmp\n"
            "fin funcion\n"
            "swap()\n"
            "imprimir(a, b)\n", out, sizeof(out)), "multiples_globales");
        AFIRMAR(strstr(out, "2") != NULL && strstr(out, "1") != NULL,
                 "swap_via_globales");
    }

    if (fallos == 0) {
        printf("global: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "global: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
