/*
 * Tests del mensaje de error en kwargs de constructor (v1.124).
 *
 * Antes:
 *   Persona(nombre="Ana", edad=30, profesion="ing")
 *   -> ErrorDeTipo: __iniciar__() no acepta keyword 'profesion'
 *
 * Ahora:
 *   Persona(nombre="Ana", edad=30, profesion="ing")
 *   -> ErrorDeTipo: Persona() no acepta keyword 'profesion'
 *
 * Causa: ejecutar_llamar_kw (src/vm.c) transformaba VAL_CLASE en su
 * closure __iniciar__ y luego los snprintf de error usaban
 * fn->longitud_nombre/fn->nombre que apuntaban a __iniciar__. Fix:
 * capturar nombre/longitud_nombre de la clase ANTES de la transformacion
 * (variables err_nombre/err_long_nombre) y usarlos en los errores.
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
        "test_kwerr_out.txt";
#else
        "/tmp/test_kwerr_out.txt";
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

#define CLASE_PERSONA                                                          \
    "clase Persona:\n"                                                          \
    "    funcion __iniciar__(yo, nombre, edad, ciudad=\"Madrid\"):\n"           \
    "        yo.nombre = nombre\n"                                              \
    "        yo.edad = edad\n"                                                  \
    "    fin funcion\n"                                                         \
    "fin clase\n"

int main(void) {
    /* Kwarg duplicado: mensaje debe decir Persona() */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PERSONA
            "intentar:\n"
            "    Persona(\"Ana\", nombre=\"Otro\", edad=30)\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(e)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Persona()") != NULL, "duplicado_dice_Persona");
        AFIRMAR(strstr(out, "__iniciar__") == NULL, "duplicado_no_init");
        AFIRMAR(strstr(out, "'nombre'") != NULL, "duplicado_menciona_clave");
    }

    /* Kwarg desconocido */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PERSONA
            "intentar:\n"
            "    Persona(nombre=\"Ana\", edad=30, profesion=\"ing\")\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(e)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Persona()") != NULL, "kw_dice_Persona");
        AFIRMAR(strstr(out, "__iniciar__") == NULL, "kw_no_init");
        AFIRMAR(strstr(out, "'profesion'") != NULL, "kw_menciona_clave");
    }

    /* Falta argumento */
    {
        char out[512];
        ejecutar_capturando(
            CLASE_PERSONA
            "intentar:\n"
            "    Persona(nombre=\"Ana\")\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(e)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Persona()") != NULL, "falta_dice_Persona");
        AFIRMAR(strstr(out, "__iniciar__") == NULL, "falta_no_init");
        AFIRMAR(strstr(out, "'edad'") != NULL, "falta_menciona_param");
    }

    /* Para FUNCIONES (no clase), el mensaje sigue diciendo el nombre de
     * la funcion — no debe cambiar a algo raro. */
    {
        char out[512];
        ejecutar_capturando(
            "funcion saludar(nombre, edad):\n"
            "    imprimir(nombre)\n"
            "fin funcion\n"
            "intentar:\n"
            "    saludar(nombre=\"Ana\", edad=30, extra=1)\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(e)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "saludar()") != NULL, "fn_dice_su_nombre");
    }

    if (fallos == 0) {
        printf("kwargs_clase_err: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "kwargs_clase_err: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
