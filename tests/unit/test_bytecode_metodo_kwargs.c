/*
 * Tests de kwargs en metodos de clase (v1.143).
 *
 * Asimetria que cerro v1.142 era *args/**kw en la signatura del
 * metodo. Quedaba la otra: kwargs en el SITIO DE LLAMADA
 * (`obj.m(x=1, y=2)`). El path ejecutar_llamar_kw aceptaba
 * VAL_FUNCION_BC y VAL_CLASE (constructor con kwargs) pero
 * rechazaba VAL_METODO_LIGADO con:
 *   ErrorDeTipo: keyword args solo soportados para funciones
 *   bytecode (no 'metodo')
 *
 * v1.143: ejecutar_llamar_kw detecta VAL_METODO_LIGADO igual que
 * detecta VAL_CLASE — extrae la closure y el receptor del
 * MetodoLigado, hace shift de args+kwargs, inserta receptor como
 * pos0 (yo), reemplaza callee con la closure y cae al path BC
 * normal que ya sabe procesar kwargs/defaults/variadicos.
 *
 * Combinado con v1.142 (variadicos en el lado de la firma), todos
 * los patrones idiomaticos de Python para metodos funcionan:
 *   - posicionales por nombre (`obj.m(a=1, b=2)`)
 *   - defaults completados via kwarg (`obj.m(b=20)`)
 *   - `**kw` absorbiendo extras
 *   - mezcla `*args, **kw` que captura todo
 *   - decoradores forwardeando `*args, **kw`
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
        "test_metodo_kw_out.txt";
#else
        "/tmp/test_metodo_kw_out.txt";
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
    /* Posicionales pasados por nombre */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion m(yo, a, b):\n"
            "        imprimir(a, b)\n"
            "    fin funcion\n"
            "fin clase\n"
            "c = C()\n"
            "c.m(a=1, b=2)\n"
            "c.m(b=20, a=10)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2") != NULL, "kw_orden_natural");
        AFIRMAR(strstr(out, "10 20") != NULL, "kw_orden_invertido");
    }

    /* Defaults + kwargs (caso comun de configuracion) */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion saludar(yo, nombre=\"amigo\", saludo=\"Hola\"):\n"
            "        imprimir(saludo + \", \" + nombre)\n"
            "    fin funcion\n"
            "fin clase\n"
            "c = C()\n"
            "c.saludar()\n"
            "c.saludar(nombre=\"David\")\n"
            "c.saludar(saludo=\"Buenas\")\n"
            "c.saludar(saludo=\"Buenas\", nombre=\"David\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "Hola, amigo") != NULL, "defaults_solo");
        AFIRMAR(strstr(out, "Hola, David") != NULL, "kw_un_arg");
        AFIRMAR(strstr(out, "Buenas, amigo") != NULL, "kw_otro_arg");
        AFIRMAR(strstr(out, "Buenas, David") != NULL, "kw_ambos");
    }

    /* `**kw` absorbe kwargs no declarados */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion absorber(yo, **kw):\n"
            "        imprimir(longitud(kw))\n"
            "    fin funcion\n"
            "fin clase\n"
            "c = C()\n"
            "c.absorber(uno=1, dos=2, tres=3)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "kw_absorbe");
    }

    /* Mezcla *args + **kw en metodo */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion mezcla(yo, a, b, *resto, **kw):\n"
            "        imprimir(a, b, resto, longitud(kw))\n"
            "    fin funcion\n"
            "fin clase\n"
            "c = C()\n"
            "c.mezcla(1, 2)\n"
            "c.mezcla(1, 2, 3, 4)\n"
            "c.mezcla(1, 2, 3, 4, opcion=\"x\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2 () 0") != NULL, "mezcla_solo_fijos");
        AFIRMAR(strstr(out, "1 2 (3, 4) 0") != NULL, "mezcla_con_resto");
        AFIRMAR(strstr(out, "1 2 (3, 4) 1") != NULL, "mezcla_completa");
    }

    /* Decorador que reenvia `*args, **kw` a metodo decorado */
    {
        char out[256];
        ejecutar_capturando(
            "funcion deco(f):\n"
            "    funcion envuelto(*args, **kw):\n"
            "        imprimir(\"antes\")\n"
            "        r = f(*args, **kw)\n"
            "        imprimir(\"despues\")\n"
            "        retornar r\n"
            "    fin funcion\n"
            "    retornar envuelto\n"
            "fin funcion\n"
            "clase Foo:\n"
            "    @deco\n"
            "    funcion saludar(yo, nombre, mensaje=\"Hola\"):\n"
            "        imprimir(mensaje + \", \" + nombre)\n"
            "    fin funcion\n"
            "fin clase\n"
            "Foo().saludar(\"David\", mensaje=\"Hey\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "antes") != NULL, "deco_antes");
        AFIRMAR(strstr(out, "Hey, David") != NULL, "deco_mensaje");
        AFIRMAR(strstr(out, "despues") != NULL, "deco_despues");
    }

    /* Error: kwarg duplicado con posicional */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion m(yo, a):\n"
            "        imprimir(a)\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    C().m(1, a=2)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err dup\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err dup") != NULL, "kwarg_duplicado");
    }

    /* Error: kwarg desconocido */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion m(yo, a):\n"
            "        imprimir(a)\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    C().m(b=2)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err keyword\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err keyword") != NULL, "kwarg_desconocido");
    }

    /* Regresion: posicionales clasicos siguen funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "clase C:\n"
            "    funcion m(yo, a, b):\n"
            "        imprimir(a, b)\n"
            "    fin funcion\n"
            "fin clase\n"
            "C().m(1, 2)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2") != NULL, "regr_posicional");
    }

    /* Regresion: constructor con kwargs sigue funcionando (v1.121) */
    {
        char out[256];
        ejecutar_capturando(
            "clase Persona:\n"
            "    funcion __iniciar__(yo, nombre, edad=0):\n"
            "        yo.nombre = nombre\n"
            "        yo.edad = edad\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = Persona(nombre=\"David\", edad=42)\n"
            "imprimir(p.nombre, p.edad)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "David 42") != NULL, "regr_constructor_kw");
    }

    if (fallos == 0) {
        printf("metodo_kw: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "metodo_kw: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
