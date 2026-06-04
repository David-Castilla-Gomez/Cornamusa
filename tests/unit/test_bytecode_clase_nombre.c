/*
 * Tests de atributos sinteticos de VAL_CLASE (fase 3).
 *
 * Antes de v1.122, `tipo(yo).__nombre__` daba dos errores en cascada:
 * (1) `tipo(yo)` devuelve la cadena "instancia", (2) la cadena no
 * tiene atributos. Para nombre de clase de una instancia hacian falta
 * las nativas globales `nombre_clase()` y `clase_de()`, pero el corpus
 * pedagogico (examples/07_clases_herencia) usaba notacion de atributo.
 *
 * v1.122: VAL_CLASE expone `.nombre` y `.__nombre__` como atributos
 * sinteticos en OP_OBTENER_ATRIBUTO. La forma idiomatica para
 * polimorfismo de __cadena__ es ahora:
 *
 *     f"{nombre_clase(yo)}(...)"        # forma directa
 *     f"{clase_de(yo).nombre}(...)"     # composicion via atributo
 *
 * `tipo()` sigue devolviendo la cadena del tipo (cambio incompatible
 * descartado tras detectar 8 regresiones de tests existentes).
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
        "test_cls_name_out.txt";
#else
        "/tmp/test_cls_name_out.txt";
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
    /* Clase.nombre devuelve el identificador exacto */
    {
        char out[256];
        ejecutar_capturando(
            "clase Persona:\n"
            "    pasar\n"
            "fin clase\n"
            "imprimir(Persona.nombre)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Persona") != NULL, "clase_nombre_directo");
    }

    /* Clase.__nombre__ idem */
    {
        char out[256];
        ejecutar_capturando(
            "clase Foo:\n"
            "    pasar\n"
            "fin clase\n"
            "imprimir(Foo.__nombre__)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Foo") != NULL, "clase_dunder_nombre");
    }

    /* clase_de(instancia).nombre - el camino del corpus */
    {
        char out[256];
        ejecutar_capturando(
            "clase Punto:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = Punto()\n"
            "imprimir(clase_de(p).nombre)\n"
            "imprimir(clase_de(p).__nombre__)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Punto") != NULL, "clase_de_inst_nombre");
    }

    /* nombre_clase(instancia) - la forma corta */
    {
        char out[256];
        ejecutar_capturando(
            "clase Bar:\n"
            "    pasar\n"
            "fin clase\n"
            "imprimir(nombre_clase(Bar()))\n"
            "imprimir(nombre_clase(Bar))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Bar") != NULL, "nombre_clase_inst_y_clase");
    }

    /* Polimorfismo via clase_de(yo).nombre dentro de __cadena__ */
    {
        char out[256];
        ejecutar_capturando(
            "clase A:\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"soy un {clase_de(yo).nombre}\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "clase B extiende A:\n"
            "    pasar\n"
            "fin clase\n"
            "imprimir(A())\n"
            "imprimir(B())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "soy un A") != NULL, "polimorfismo_A");
        AFIRMAR(strstr(out, "soy un B") != NULL, "polimorfismo_B");
    }

    /* tipo(instancia) sigue siendo la cadena "instancia" (no se cambia) */
    {
        char out[256];
        ejecutar_capturando(
            "clase X:\n"
            "    pasar\n"
            "fin clase\n"
            "imprimir(tipo(X()))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "instancia") != NULL, "tipo_inst_legacy");
    }

    /* clase_de sobre no-instancia devuelve nulo */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(clase_de(42))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "nulo") != NULL, "clase_de_no_instancia");
    }

    if (fallos == 0) {
        printf("clase_nombre: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "clase_nombre: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
