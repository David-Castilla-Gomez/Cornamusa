/*
 * Tests de `@clasemetodo` (v1.85).
 *
 * Verifica:
 *   - `Clase.metodo()` inyecta la clase como primer arg.
 *   - `instancia.metodo()` también lo inyecta (la clase de la instancia).
 *   - Polimorfismo: `Hijo.metodo()` recibe Hijo, no Base.
 *   - Caller pasa N args al `@clasemetodo` → método recibe (cls + N args).
 *   - clasemetodo(no_callable) lanza ErrorDeTipo.
 *   - Constructor alternativo polimórfico: `cls(...)` crea instancia
 *     de la subclase correcta.
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
        "test_cm_out.txt";
#else
        "/tmp/test_cm_out.txt";
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
    /* Test 1: Clase.clasemetodo(args) — cls es la clase. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase Foo:\n"
            "    @clasemetodo\n"
            "    funcion crear(cls, n):\n"
            "        retornar cls(n * 10)\n"
            "    fin funcion\n"
            "    funcion __iniciar__(yo, v):\n"
            "        yo.v = v\n"
            "    fin funcion\n"
            "fin clase\n"
            "f = Foo.crear(5)\n"
            "imprimir(f.v)\n", out, sizeof(out));
        AFIRMAR(rc == 0, "clase_cm_ejecuta");
        AFIRMAR(strstr(out, "50") != NULL, "clase_cm_resultado");
    }

    /* Test 2: instancia.clasemetodo(args) — cls es la clase de la instancia. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase Foo:\n"
            "    @clasemetodo\n"
            "    funcion crear(cls, n):\n"
            "        retornar cls(n + 100)\n"
            "    fin funcion\n"
            "    funcion __iniciar__(yo, v):\n"
            "        yo.v = v\n"
            "    fin funcion\n"
            "fin clase\n"
            "i = Foo(1)\n"
            "f = i.crear(5)\n"
            "imprimir(f.v)\n", out, sizeof(out));
        AFIRMAR(rc == 0, "inst_cm_ejecuta");
        AFIRMAR(strstr(out, "105") != NULL, "inst_cm_resultado");
    }

    /* Test 3: polimorfismo con herencia — cls es Hijo, no Base. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase Base:\n"
            "    @clasemetodo\n"
            "    funcion crear(cls, n):\n"
            "        retornar cls(n * 10)\n"
            "    fin funcion\n"
            "    funcion __iniciar__(yo, v):\n"
            "        yo.v = v\n"
            "    fin funcion\n"
            "fin clase\n"
            "clase Hijo extiende Base:\n"
            "    funcion __iniciar__(yo, v):\n"
            "        yo.v = v + 1000\n"   /* marker para detectar quien creo */
            "    fin funcion\n"
            "fin clase\n"
            "b = Base.crear(5)\n"
            "h = Hijo.crear(7)\n"
            "imprimir(b.v)\n"
            "imprimir(h.v)\n", out, sizeof(out));
        AFIRMAR(rc == 0, "polimorfismo_ejecuta");
        AFIRMAR(strstr(out, "50") != NULL, "polimorfismo_base");
        AFIRMAR(strstr(out, "1070") != NULL, "polimorfismo_hijo");
    }

    /* Test 4: 0 args (solo cls). */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    @clasemetodo\n"
            "    funcion saludo(cls):\n"
            "        retornar \"hola desde\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(M.saludo())\n", out, sizeof(out));
        AFIRMAR(rc == 0, "cero_args_ejecuta");
        AFIRMAR(strstr(out, "hola desde") != NULL, "cero_args_resultado");
    }

    /* Test 5: clasemetodo(no_callable) lanza. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    e = clasemetodo(42)\n"
            "    imprimir(\"BUG\")\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(\"rechazado\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(rc == 0, "no_callable_ejecuta");
        AFIRMAR(strstr(out, "rechazado") != NULL, "no_callable_rechazado");
    }

    /* Test 6: clasemetodo puede acceder a metodos estaticos via cls. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    @estaticometodo\n"
            "    funcion helper(n):\n"
            "        retornar n * 2\n"
            "    fin funcion\n"
            "    @clasemetodo\n"
            "    funcion combinar(cls, n):\n"
            "        retornar cls.helper(n) + 1\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(M.combinar(5))\n", out, sizeof(out));
        AFIRMAR(rc == 0, "cm_accede_estatico");
        AFIRMAR(strstr(out, "11") != NULL, "cm_resultado_combinado");
    }

    if (fallos == 0) {
        printf("clasemetodo: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "clasemetodo: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
