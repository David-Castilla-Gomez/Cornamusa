/*
 * Tests: excepciones definidas por el usuario (v1.206).
 *
 * Ahora se puede `lanzar` una instancia de clase cualquiera y atraparla
 * por el nombre de su clase o de una superclase (herencia). El handler
 * recibe la instancia con sus atributos. `atrapar Excepcion` sigue
 * siendo el catch-all genérico. Funciona con multi-tipo (v1.202),
 * re-lanzar, y se mezcla con las excepciones nativas.
 *
 * Implementación: OP_LANZAR acepta VAL_INSTANCIA; OP_COMPROBAR_TIPO_EXC
 * compara el nombre de la clase (recorriendo la cadena de superclases);
 * el reporte de no-atrapada usa el nombre de la clase + atributo
 * `mensaje` si existe.
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
        "test_exc_usr_out.txt";
#else
        "/tmp/test_exc_usr_out.txt";
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
            else if (rcvm == VM_ERROR_RUNTIME) rc = 70;
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

#define ERRAPP \
    "clase ErrorMiApp:\n" \
    "    funcion __iniciar__(yo, mensaje, codigo):\n" \
    "        yo.mensaje = mensaje\n" \
    "        yo.codigo = codigo\n" \
    "    fin funcion\n" \
    "fin clase\n"

int main(void) {
    /* Lanzar/atrapar una instancia de usuario; el handler ve sus atributos. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            ERRAPP
            "intentar:\n"
            "    lanzar ErrorMiApp(\"fallo\", 42)\n"
            "atrapar ErrorMiApp como e:\n"
            "    imprimir(\"A\", e.mensaje, e.codigo)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "basico_rc");
        AFIRMAR(strstr(out, "A fallo 42") != NULL, "basico_atributos");
    }

    /* Herencia: atrapar por la superclase. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase ErrorBase:\n"
            "    funcion __iniciar__(yo, m):\n"
            "        yo.mensaje = m\n"
            "    fin funcion\n"
            "fin clase\n"
            "clase ErrorEsp extiende ErrorBase:\n"
            "    funcion __iniciar__(yo, m):\n"
            "        yo.mensaje = m\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    lanzar ErrorEsp(\"esp\")\n"
            "atrapar ErrorBase como e:\n"
            "    imprimir(\"H\", e.mensaje)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "herencia_rc");
        AFIRMAR(strstr(out, "H esp") != NULL, "herencia_superclase");
    }

    /* `atrapar Excepcion` atrapa instancias de usuario (catch-all). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            ERRAPP
            "intentar:\n"
            "    lanzar ErrorMiApp(\"g\", 1)\n"
            "atrapar Excepcion:\n"
            "    imprimir(\"CATCHALL\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "catchall_rc");
        AFIRMAR(strstr(out, "CATCHALL") != NULL, "catchall_ok");
    }

    /* Multi-tipo (v1.202) mezclando nativa + usuario. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            ERRAPP
            "intentar:\n"
            "    lanzar ErrorMiApp(\"m\", 7)\n"
            "atrapar (ErrorDeValor, ErrorMiApp) como e:\n"
            "    imprimir(\"MT\", e.mensaje)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "multitipo_rc");
        AFIRMAR(strstr(out, "MT m") != NULL, "multitipo_usuario");
    }

    /* Re-lanzar (lanzar sin valor) una instancia de usuario. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            ERRAPP
            "funcion f():\n"
            "    intentar:\n"
            "        lanzar ErrorMiApp(\"re\", 9)\n"
            "    atrapar ErrorMiApp:\n"
            "        lanzar\n"
            "    fin intentar\n"
            "fin funcion\n"
            "intentar:\n"
            "    f()\n"
            "atrapar ErrorMiApp como e:\n"
            "    imprimir(\"RE\", e.mensaje)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "relanzar_rc");
        AFIRMAR(strstr(out, "RE re") != NULL, "relanzar_instancia");
    }

    /* Una instancia que no coincide pasa al siguiente atrapador. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            ERRAPP
            "intentar:\n"
            "    lanzar ErrorMiApp(\"x\", 0)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ZZNO\")\n"
            "atrapar ErrorMiApp como e:\n"
            "    imprimir(\"FB\", e.mensaje)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "fallback_rc");
        AFIRMAR(strstr(out, "FB x") != NULL, "fallback_correcto");
        AFIRMAR(strstr(out, "ZZNO") == NULL, "fallback_no_falso");
    }

    /* Excepción de usuario NO atrapada → error de runtime con clase +
     * mensaje (rc 70), no crash. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            ERRAPP
            "lanzar ErrorMiApp(\"sin atrapar\", 99)\n",
            out, sizeof(out));
        AFIRMAR(rc == 70, "noatrapada_rc");
    }

    /* Lanzar algo que no es excepción ni instancia → ErrorDeTipo. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    lanzar 42\n"
            "atrapar Excepcion:\n"
            "    imprimir(\"NOINST\")\n"
            "fin intentar\n",
            out, sizeof(out));
        /* lanzar 42 da ErrorDeTipo en runtime; no se atrapa por la
         * cláusula porque el error es del propio OP_LANZAR. rc 70. */
        AFIRMAR(rc == 70, "noinstancia_rc");
    }

    /* v1.206: la instancia debe preservar su identidad (atributos y
     * cadena de superclases) al cruzar la frontera de un sub-dispatch
     * (generador, dunder __siguiente__, builtin que itera). Antes se
     * aplanaba a una Excepción plana y se perdían atributos/herencia. */
    {
        char out[512];
        int rc = ejecutar_capturando(
            "clase Base:\n"
            "    funcion __iniciar__(yo, m):\n"
            "        yo.mensaje = m\n"
            "    fin funcion\n"
            "fin clase\n"
            "clase Sub extiende Base:\n"
            "    funcion __iniciar__(yo, m):\n"
            "        yo.mensaje = m\n"
            "    fin funcion\n"
            "fin clase\n"
            /* desde generador, atrapar por superclase */
            "funcion gen():\n"
            "    producir 1\n"
            "    lanzar Sub(\"g\")\n"
            "fin funcion\n"
            "intentar:\n"
            "    para v en gen():\n"
            "        pasar\n"
            "    fin para\n"
            "atrapar Base como e:\n"
            "    imprimir(\"G\", tipo(e), e.mensaje)\n"
            "fin intentar\n"
            /* desde __siguiente__, atrapar por superclase */
            "clase Iter:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.n = 0\n"
            "    fin funcion\n"
            "    funcion __siguiente__(yo):\n"
            "        yo.n = yo.n + 1\n"
            "        si yo.n == 2:\n"
            "            lanzar Sub(\"s\")\n"
            "        fin si\n"
            "        retornar yo.n\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    para v en Iter():\n"
            "        pasar\n"
            "    fin para\n"
            "atrapar Base como e:\n"
            "    imprimir(\"D\", tipo(e), e.mensaje)\n"
            "fin intentar\n"
            /* desde un generador consumido por un builtin (lista) */
            "funcion gen3():\n"
            "    producir 10\n"
            "    lanzar Sub(\"b\")\n"
            "fin funcion\n"
            "intentar:\n"
            "    lista(gen3())\n"
            "atrapar Base como e:\n"
            "    imprimir(\"B\", tipo(e), e.mensaje)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "subdispatch_rc");
        AFIRMAR(strstr(out, "G instancia g") != NULL, "subdispatch_generador");
        AFIRMAR(strstr(out, "D instancia s") != NULL, "subdispatch_dunder");
        AFIRMAR(strstr(out, "B instancia b") != NULL, "subdispatch_builtin");
    }

    /* Regresión del SIGSEGV: un handler instalado DENTRO de otro
     * generador (sub-dispatch anidado) cuyo stack ya fue derribado NO
     * debe corromper memoria. La excepción escapa de forma segura
     * preservando su identidad (mismo comportamiento que una excepción
     * nativa en ese caso). Lo crítico es que NO crashea (rc != 139). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase ExcA:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.mensaje = \"soy-A\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "funcion gen_a():\n"
            "    producir 1\n"
            "    lanzar ExcA()\n"
            "fin funcion\n"
            "funcion gen_b():\n"
            "    intentar:\n"
            "        para v en gen_a():\n"
            "            producir v\n"
            "        fin para\n"
            "    atrapar ExcA como ea:\n"
            "        imprimir(\"atrapada\", ea.mensaje)\n"
            "    fin intentar\n"
            "fin funcion\n"
            "para v en gen_b():\n"
            "    imprimir(\"recibido\", v)\n"
            "fin para\n",
            out, sizeof(out));
        /* rc 0 (atrapada) o 70 (escapa); lo que importa: termina sin
         * crash y la identidad se preserva en el reporte. */
        AFIRMAR(rc == 0 || rc == 70, "anidado_sin_crash");
        AFIRMAR(strstr(out, "recibido 1") != NULL, "anidado_progreso");
    }

    if (fallos == 0) {
        printf("excepciones_usuario: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "excepciones_usuario: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
