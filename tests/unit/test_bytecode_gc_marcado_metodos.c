/*
 * Regresión del marcado GC de Propiedad / MetodoEstatico / MetodoDeClase
 * (v1.201).
 *
 * Bug (use-after-free, mismo patrón que el de VAL_GENERADOR en v1.200):
 * los envoltorios `@propiedad`, `@estaticometodo` y `@clasemetodo` son
 * GC-alocados (sujetos al sweep) y viven dentro del diccionario de
 * métodos de la clase. `gc_marcar_valor` NO tenía sus cases, así que al
 * marcar el dict de la clase (vía GC_TIPO_CLASE → dict → cada valor)
 * caían al `default` y NO se marcaban. El sweep los liberaba (ignora el
 * refcount); un acceso POSTERIOR (`obj.propiedad`, `Clase.estatico`)
 * leía el envoltorio liberado → segfault una vez la memoria se reusaba.
 *
 * Fix: cases VAL_PROPIEDAD / VAL_METODO_ESTATICO / VAL_METODO_DE_CLASE
 * en gc_marcar_valor.
 *
 * El repro fuerza el reuso de memoria con allocations entre el GC y el
 * re-acceso; antes del fix segfaultea (RC!=0), después es correcto.
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

static int ejecutar_capturando(const char *fuente, char *out_buf, int out_cap) {
    const char *tmpfile =
#ifdef _WIN32
        "test_gc_metodos_out.txt";
#else
        "/tmp/test_gc_metodos_out.txt";
#endif
    if (!freopen(tmpfile, "w+", stdout)) return -1;

    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    int rc = -1;
    if (!p.tuvo_error) {
        Chunk chunk; chunk_iniciar(&chunk);
        Compilador c; compilador_iniciar(&c, &chunk);
        if (compilador_compilar_programa(&c, sents, n)) {
            VM vm; vm_iniciar(&vm);
            Valor r = valor_nulo();
            ResultadoVM rcvm = vm_ejecutar(&vm, &chunk, &r);
            valor_destruir(&r);
            vm_destruir(&vm);
            if (rcvm == VM_OK) rc = 0;
        }
        chunk_destruir(&chunk);
    }
    arena_destruir(&a);

    fflush(stdout);
#ifdef _WIN32
    freopen("CON", "w", stdout);
#else
    freopen("/dev/tty", "w", stdout);
#endif

    FILE *f = fopen(tmpfile, "r");
    if (f) {
        int leido = (int)fread(out_buf, 1, (size_t)(out_cap - 1), f);
        out_buf[leido] = '\0';
        fclose(f);
        remove(tmpfile);
    } else {
        out_buf[0] = '\0';
    }
    return rc;
}

int main(void) {
    /* @estaticometodo: re-acceso tras GC + reuso de memoria. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    @estaticometodo\n"
            "    funcion dup(n):\n"
            "        retornar n * 2\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(M.dup(1))\n"
            "recolectar()\n"
            "basura = []\n"
            "para i en rango(3000):\n"
            "    basura.añadir([i, i + 1, i + 2])\n"
            "fin para\n"
            "imprimir(M.dup(2))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "estatico_rc");
        AFIRMAR(strstr(out, "2\n") != NULL, "estatico_primero");
        AFIRMAR(strstr(out, "4") != NULL, "estatico_tras_gc");
    }

    /* @clasemetodo: re-acceso tras GC + reuso. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    @clasemetodo\n"
            "    funcion crear(cls, x):\n"
            "        retornar x + 1\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(M.crear(10))\n"
            "recolectar()\n"
            "basura = []\n"
            "para i en rango(3000):\n"
            "    basura.añadir([i, i + 1, i + 2])\n"
            "fin para\n"
            "imprimir(M.crear(20))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "clasemetodo_rc");
        AFIRMAR(strstr(out, "11") != NULL, "clasemetodo_primero");
        AFIRMAR(strstr(out, "21") != NULL, "clasemetodo_tras_gc");
    }

    /* @propiedad: re-acceso desde instancia tras GC + reuso. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    @propiedad\n"
            "    funcion valor(yo):\n"
            "        retornar 99\n"
            "    fin funcion\n"
            "fin clase\n"
            "obj = M()\n"
            "imprimir(obj.valor)\n"
            "recolectar()\n"
            "basura = []\n"
            "para i en rango(3000):\n"
            "    basura.añadir([i, i + 1, i + 2])\n"
            "fin para\n"
            "imprimir(obj.valor)\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "propiedad_rc");
        AFIRMAR(strstr(out, "99\n99") != NULL, "propiedad_doble");
    }

    if (fallos == 0) {
        printf("gc_marcado_metodos: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "gc_marcado_metodos: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
