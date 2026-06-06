/*
 * Tests de `*args` y `**kw` en metodos de clase (v1.142).
 *
 * Pre v1.142, las funciones libres soportaban variadicos pero los
 * metodos NO — el dispatch en ejecutar_llamar_metodo_ligado hacia
 * una verificacion estricta `n_args + 1 != fn->aridad`. Las clases
 * con `funcion m(yo, *args):` fallaban con
 *   ErrorDeTipo: m() esperaba 1 argumentos, recibio N
 * incluso aunque la funcion BC tuviera `fn->tiene_estrella = true`.
 *
 * Este bug bloquea el idioma comun `__salir__(yo, *_)` para
 * context managers (descubierto en v1.141).
 *
 * v1.142: replica en ejecutar_llamar_metodo_ligado la lógica de
 * variadicos que ya existe en ejecutar_llamar_bc (linea 1035+):
 * computar n_fijos, validar minimo, recoger sobrantes en tupla
 * para *resto, empujar dict vacio para **kw.
 *
 * Limitacion: kwargs en metodos pasan por otro path
 * (ejecutar_llamar_kw) y siguen sin soportarse — error claro
 * "keyword args solo soportados para funciones bytecode". Eso es
 * un bug separado.
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
        "test_metodo_var_out.txt";
#else
        "/tmp/test_metodo_var_out.txt";
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
    /* `*args` solo, sin args fijos extras */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion m(yo, *args):\n"
            "        imprimir(args)\n"
            "    fin funcion\n"
            "fin clase\n"
            "C().m()\n"
            "C().m(1, 2, 3)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "()") != NULL, "args_vacio");
        AFIRMAR(strstr(out, "(1, 2, 3)") != NULL, "args_3");
    }

    /* `*args` con fijos antes */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion m(yo, a, b, *resto):\n"
            "        imprimir(a, b, resto)\n"
            "    fin funcion\n"
            "fin clase\n"
            "C().m(1, 2)\n"
            "C().m(10, 20, 30, 40)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2 ()") != NULL, "fijos_solo");
        AFIRMAR(strstr(out, "10 20 (30, 40)") != NULL, "fijos_con_extras");
    }

    /* Idiom `*_` (el caso original que motivó el bug) */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion salir(yo, *_):\n"
            "        imprimir(\"ok\")\n"
            "    fin funcion\n"
            "fin clase\n"
            "C().salir(1, 2, 3)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "ignorar_args");
    }

    /* Compatible con `con` (caso de uso real de v1.141)
     * Uso `*resto` en lugar de `*_` por claridad de la salida. */
    {
        char out[256];
        ejecutar_capturando(
            "clase Recurso:\n"
            "    funcion __entrar__(yo):\n"
            "        imprimir(\"entrar\")\n"
            "        retornar yo\n"
            "    fin funcion\n"
            "    funcion __salir__(yo, *resto):\n"
            "        imprimir(\"salir args=\" + cadena(resto))\n"
            "    fin funcion\n"
            "fin clase\n"
            "con Recurso() como r:\n"
            "    imprimir(\"cuerpo\")\n"
            "fin con\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "entrar") != NULL, "con_entrar");
        AFIRMAR(strstr(out, "cuerpo") != NULL, "con_cuerpo");
        AFIRMAR(strstr(out, "salir args=(nulo, nulo, nulo)") != NULL,
                "con_salir_args");
    }

    /* `**kw` solo (sin args extras posicionales) */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion m(yo, **kw):\n"
            "        imprimir(\"kw type=\" + tipo(kw))\n"
            "    fin funcion\n"
            "fin clase\n"
            "C().m()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "diccionario") != NULL, "kw_dict_vacio");
    }

    /* `*args` + `**kw` juntos */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion m(yo, *args, **kw):\n"
            "        imprimir(args, \"-\", tipo(kw))\n"
            "    fin funcion\n"
            "fin clase\n"
            "C().m()\n"
            "C().m(1, 2)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "() - diccionario") != NULL, "args_kw_vacios");
        AFIRMAR(strstr(out, "(1, 2) - diccionario") != NULL,
                "args_kw_con_args");
    }

    /* Regresion: funciones libres con *args siguen funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f(*args):\n"
            "    imprimir(args)\n"
            "fin funcion\n"
            "f()\n"
            "f(1, 2, 3)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "()") != NULL, "regr_libre_vacio");
        AFIRMAR(strstr(out, "(1, 2, 3)") != NULL, "regr_libre_3");
    }

    /* Regresion: metodos con aridad fija sin variadicos */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion m(yo, x):\n"
            "        imprimir(x)\n"
            "    fin funcion\n"
            "fin clase\n"
            "C().m(42)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "regr_metodo_fijo");
    }

    if (fallos == 0) {
        printf("metodo_var: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "metodo_var: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
