/*
 * Tests de `@estaticometodo` (v1.84).
 *
 * Verifica:
 *   - Método estático invocable via `Clase.metodo` (NUEVO en v1.84).
 *   - Método estático invocable via `instancia.metodo` sin inyectar `yo`.
 *   - El método estático SÍ recibe sus argumentos normales.
 *   - estaticometodo(no_callable) lanza ErrorDeTipo.
 *   - Acceso a método NO estático via `Clase.X` devuelve la closure
 *     no ligada (caller debe pasar `yo`).
 *   - Acceso a atributo inexistente de la clase lanza ErrorDeAtributo.
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
        "test_estatico_out.txt";
#else
        "/tmp/test_estatico_out.txt";
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
    /* Test 1: Clase.metodo_estatico via clase directamente. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    @estaticometodo\n"
            "    funcion duplicar(n):\n"
            "        retornar n * 2\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(M.duplicar(7))\n", out, sizeof(out));
        AFIRMAR(rc == 0, "clase_dot_metodo_ejecuta");
        AFIRMAR(strstr(out, "14") != NULL, "clase_dot_resultado");
    }

    /* Test 2: instancia.metodo_estatico no inyecta yo. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    @estaticometodo\n"
            "    funcion suma(a, b):\n"
            "        retornar a + b\n"
            "    fin funcion\n"
            "fin clase\n"
            "i = M()\n"
            "imprimir(i.suma(3, 4))\n", out, sizeof(out));
        AFIRMAR(rc == 0, "inst_dot_estatico_ejecuta");
        AFIRMAR(strstr(out, "7") != NULL, "inst_dot_estatico_resultado");
    }

    /* Test 3: el estaticometodo recibe args normales (1 arg, no yo+1). */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    @estaticometodo\n"
            "    funcion identidad(x):\n"
            "        retornar x\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(M.identidad(\"hola\"))\n", out, sizeof(out));
        AFIRMAR(rc == 0, "args_pasan_normal");
        AFIRMAR(strstr(out, "hola") != NULL, "args_resultado");
    }

    /* Test 4: estaticometodo(no_callable) lanza. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    e = estaticometodo(42)\n"
            "    imprimir(\"BUG\")\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(\"rechazado\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(rc == 0, "no_callable_ejecuta");
        AFIRMAR(strstr(out, "rechazado") != NULL, "no_callable_rechazado");
    }

    /* Test 5: metodo NO estatico accedido via Clase.X devuelve closure
     * no ligada — caller debe pasar yo manualmente. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "    funcion duplicar_n(yo):\n"
            "        retornar yo.n * 2\n"
            "    fin funcion\n"
            "fin clase\n"
            "i = M(7)\n"
            "# acceso via Clase, pasamos yo explicito\n"
            "fn = M.duplicar_n\n"
            "imprimir(fn(i))\n", out, sizeof(out));
        AFIRMAR(rc == 0, "clase_dot_metodo_normal_ejecuta");
        AFIRMAR(strstr(out, "14") != NULL, "clase_dot_metodo_normal_resultado");
    }

    /* Test 6: atributo inexistente de clase lanza ErrorDeAtributo. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase M:\n"
            "    funcion bar(yo):\n"
            "        retornar 1\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    _ = M.no_existe\n"
            "    imprimir(\"BUG\")\n"
            "atrapar Excepcion como e:\n"
            "    imprimir(\"atrapado\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(rc == 0, "atributo_inexistente_ejecuta");
        AFIRMAR(strstr(out, "atrapado") != NULL, "atributo_inexistente_atrapado");
    }

    /* Test 7: estaticometodo accede a metodos estaticos de la misma
     * clase via Clase.X (patron constructor alternativo). */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase Punto:\n"
            "    funcion __iniciar__(yo, x, b):\n"
            "        yo.x = x\n"
            "        yo.b = b\n"
            "    fin funcion\n"
            "    @estaticometodo\n"
            "    funcion origen():\n"
            "        retornar Punto(0, 0)\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"({yo.x}, {yo.b})\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = Punto.origen()\n"
            "imprimir(p)\n", out, sizeof(out));
        AFIRMAR(rc == 0, "constructor_alternativo");
        AFIRMAR(strstr(out, "(0, 0)") != NULL, "constructor_resultado");
    }

    if (fallos == 0) {
        printf("estaticometodo: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "estaticometodo: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
