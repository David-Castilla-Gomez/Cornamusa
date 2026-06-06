/*
 * Tests de `con` con `__salir__` recibiendo 3 argumentos (v1.141).
 *
 * Antes: el desugar de `con` invocaba `__salir__()` sin argumentos.
 * La firma esperada en Python es `__salir__(yo, tipo_exc, valor_exc,
 * traza)`. Las clases que declaran los 4 parametros fallaban con
 * ErrorDeTipo "esperaba 3 argumentos, recibio 0".
 *
 * v1.141: el desugar pasa 3 args nulos (tipo_exc, valor_exc, traza)
 * a `__salir__`. Por ahora son siempre nulos — no se introspecciona
 * la excepcion. La pieza pendiente es exponer la info real al
 * dunder; ese upgrade requiere acceso al frame de excepcion y queda
 * para una release futura.
 *
 * Multi-recurso (`con A, B:`) ya estaba en v1.46; ahora todos los
 * __salir__ de la cadena reciben tambien los 3 nulos.
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
        "test_con_salir_out.txt";
#else
        "/tmp/test_con_salir_out.txt";
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

static const char *CLASE_R =
    "clase Recurso:\n"
    "    funcion __iniciar__(yo, nombre):\n"
    "        yo.nombre = nombre\n"
    "    fin funcion\n"
    "    funcion __entrar__(yo):\n"
    "        imprimir(f\"entrar {yo.nombre}\")\n"
    "        retornar yo\n"
    "    fin funcion\n"
    "    funcion __salir__(yo, tipo_exc, valor, traza):\n"
    "        imprimir(f\"salir {yo.nombre} tipo={tipo_exc}\")\n"
    "    fin funcion\n"
    "fin clase\n";

int main(void) {
    /* `con` simple con __salir__ de 4 parametros */
    {
        char out[512];
        char fuente[2048];
        snprintf(fuente, sizeof(fuente),
            "%s"
            "con Recurso(\"A\") como a:\n"
            "    imprimir(f\"cuerpo {a.nombre}\")\n"
            "fin con\n",
            CLASE_R);
        ejecutar_capturando(fuente, out, sizeof(out));
        AFIRMAR(strstr(out, "entrar A") != NULL, "entrar");
        AFIRMAR(strstr(out, "cuerpo A") != NULL, "cuerpo");
        AFIRMAR(strstr(out, "salir A tipo=nulo") != NULL, "salir_nulos");
    }

    /* `con` sin alias */
    {
        char out[512];
        char fuente[2048];
        snprintf(fuente, sizeof(fuente),
            "%s"
            "con Recurso(\"Z\"):\n"
            "    imprimir(\"cuerpo sin alias\")\n"
            "fin con\n",
            CLASE_R);
        ejecutar_capturando(fuente, out, sizeof(out));
        AFIRMAR(strstr(out, "salir Z tipo=nulo") != NULL, "sin_alias");
    }

    /* Multi-recurso: ambos __salir__ reciben 3 args. Orden LIFO. */
    {
        char out[512];
        char fuente[2048];
        snprintf(fuente, sizeof(fuente),
            "%s"
            "con Recurso(\"X\") como x, Recurso(\"Y\") como yy:\n"
            "    imprimir(f\"cuerpo {x.nombre} {yy.nombre}\")\n"
            "fin con\n",
            CLASE_R);
        ejecutar_capturando(fuente, out, sizeof(out));
        AFIRMAR(strstr(out, "entrar X") != NULL, "multi_entrar_x");
        AFIRMAR(strstr(out, "entrar Y") != NULL, "multi_entrar_y");
        AFIRMAR(strstr(out, "cuerpo X Y") != NULL, "multi_cuerpo");
        AFIRMAR(strstr(out, "salir Y tipo=nulo") != NULL, "multi_salir_y");
        AFIRMAR(strstr(out, "salir X tipo=nulo") != NULL, "multi_salir_x");
        /* Salir en orden LIFO: Y antes que X. */
        const char *sy = strstr(out, "salir Y");
        const char *sx = strstr(out, "salir X");
        AFIRMAR(sy != NULL && sx != NULL && sy < sx, "multi_orden_lifo");
    }

    /* `con` cuyo cuerpo lanza excepcion: __salir__ se ejecuta y
     * la excepcion se propaga al atrapar de fuera. */
    {
        char out[512];
        char fuente[2048];
        snprintf(fuente, sizeof(fuente),
            "%s"
            "intentar:\n"
            "    con Recurso(\"Boom\") como b:\n"
            "        imprimir(\"antes\")\n"
            "        lanzar ErrorDeValor(\"estallo\")\n"
            "    fin con\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"atrapada\")\n"
            "fin intentar\n",
            CLASE_R);
        ejecutar_capturando(fuente, out, sizeof(out));
        AFIRMAR(strstr(out, "antes") != NULL, "exc_antes");
        /* __salir__ se ejecuta antes de que la excepcion se propague. */
        AFIRMAR(strstr(out, "salir Boom") != NULL, "exc_salir_se_ejecuta");
        AFIRMAR(strstr(out, "atrapada") != NULL, "exc_propagada");
    }

    /* __salir__ con firma exacta (4 params) no falla por aridad. */
    {
        char out[256];
        ejecutar_capturando(
            "clase Estricto:\n"
            "    funcion __entrar__(yo):\n"
            "        retornar yo\n"
            "    fin funcion\n"
            "    funcion __salir__(yo, tipo, val, tb):\n"
            "        imprimir(f\"got tipo={tipo} val={val} tb={tb}\")\n"
            "    fin funcion\n"
            "fin clase\n"
            "con Estricto():\n"
            "    pasar\n"
            "fin con\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "got tipo=nulo val=nulo tb=nulo") != NULL,
                "firma_estricta");
    }

    if (fallos == 0) {
        printf("con_salir: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "con_salir: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
