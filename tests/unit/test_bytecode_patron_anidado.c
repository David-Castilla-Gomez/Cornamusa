/*
 * Tests de sub-patrones complejos en PATRON_TIPO args y PATRON_DICC
 * (v1.180). Cierra las limitaciones documentadas en v1.178 y v1.179.
 *
 * Antes:
 *   - Foo(a=(1, 2)) → "sub-patron en 'Foo(...)' debe ser BIND..."
 *   - {"k": Foo(a)} → "sub-patron en patron dict debe ser BIND..."
 *
 * Ahora: cualquier sub-patron es valido, incluyendo recursion arbitraria.
 *
 * Implementacion: refactor de int*indices a PathSegmento* que soporta
 * tres tipos de segmento: PATH_NUM (indice), PATH_ATTR (atributo),
 * PATH_CLAVE (clave de dict). emitir_navegar aplica cada uno.
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
        "test_patron_anidado_out.txt";
#else
        "/tmp/test_patron_anidado_out.txt";
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
    /* Tupla anidada en arg de tipo */
    {
        char out[512];
        ejecutar_capturando(
            "clase Datos:\n"
            "    funcion __iniciar__(yo, pos, color):\n"
            "        yo.pos = pos\n"
            "        yo.color = color\n"
            "    fin funcion\n"
            "fin clase\n"
            "coincidir Datos((1, 2), \"rojo\"):\n"
            "    cuando Datos(pos=(a, b), color=c):\n"
            "        imprimir(a, b, c)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2 rojo") != NULL, "tupla_en_tipo");
    }

    /* Tipo anidado en arg de tipo */
    {
        char out[512];
        ejecutar_capturando(
            "clase Caja:\n"
            "    funcion __iniciar__(yo, dato):\n"
            "        yo.dato = dato\n"
            "    fin funcion\n"
            "fin clase\n"
            "clase Num:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "fin clase\n"
            "coincidir Caja(Num(42)):\n"
            "    cuando Caja(dato=Num(n)):\n"
            "        imprimir(n)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "tipo_en_tipo");
    }

    /* Tupla en valor de dict pattern */
    {
        char out[512];
        ejecutar_capturando(
            "coincidir {\"par\": (1, 2), \"extra\": \"x\"}:\n"
            "    cuando {\"par\": (a, b)}:\n"
            "        imprimir(a, b)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2") != NULL, "tupla_en_dict");
    }

    /* Dict en valor de dict pattern */
    {
        char out[512];
        ejecutar_capturando(
            "coincidir {\"k\": {\"sub\": \"valor\"}}:\n"
            "    cuando {\"k\": {\"sub\": v}}:\n"
            "        imprimir(v)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "valor") != NULL, "dict_en_dict");
    }

    /* Tipo en valor de dict pattern */
    {
        char out[512];
        ejecutar_capturando(
            "clase Num:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "fin clase\n"
            "coincidir {\"obj\": Num(99)}:\n"
            "    cuando {\"obj\": Num(n)}:\n"
            "        imprimir(n)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "99") != NULL, "tipo_en_dict");
    }

    /* Lista anidada en arg */
    {
        char out[512];
        ejecutar_capturando(
            "clase Caja:\n"
            "    funcion __iniciar__(yo, contenidos):\n"
            "        yo.contenidos = contenidos\n"
            "    fin funcion\n"
            "fin clase\n"
            "coincidir Caja([1, 2, 3]):\n"
            "    cuando Caja(contenidos=[primero, *resto]):\n"
            "        imprimir(primero, resto)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 [2, 3]") != NULL, "lista_en_tipo");
    }

    /* Literal anidado en arg de tipo */
    {
        char out[512];
        ejecutar_capturando(
            "clase Caja:\n"
            "    funcion __iniciar__(yo, x):\n"
            "        yo.x = x\n"
            "    fin funcion\n"
            "fin clase\n"
            "coincidir Caja((0, 0)):\n"
            "    cuando Caja(x=(0, 0)):\n"
            "        imprimir(\"origen\")\n"
            "    cuando Caja(x=(a, b)):\n"
            "        imprimir(\"otro\", a, b)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "origen") != NULL, "literal_anidado");
    }

    /* Combinacion compleja: dict con tupla con tipo */
    {
        char out[512];
        ejecutar_capturando(
            "clase Color:\n"
            "    funcion __iniciar__(yo, nombre):\n"
            "        yo.nombre = nombre\n"
            "    fin funcion\n"
            "fin clase\n"
            "coincidir {\"datos\": (10, 20, Color(\"rojo\"))}:\n"
            "    cuando {\"datos\": (ax, bx, Color(nombre))}:\n"
            "        imprimir(ax, bx, nombre)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10 20 rojo") != NULL, "combinacion");
    }

    /* No-match con sub-patron complejo */
    {
        char out[512];
        ejecutar_capturando(
            "clase Caja:\n"
            "    funcion __iniciar__(yo, x):\n"
            "        yo.x = x\n"
            "    fin funcion\n"
            "fin clase\n"
            "coincidir Caja(5):\n"
            "    cuando Caja(x=(a, b)):\n"
            "        imprimir(\"tupla\")\n"
            "    cuando Caja(x=n):\n"
            "        imprimir(\"escalar\", n)\n"
            "fin coincidir\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "escalar 5") != NULL, "no_match_complejo");
    }

    if (fallos == 0) {
        printf("patron_anidado: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "patron_anidado: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
