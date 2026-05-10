/*
 * Tests del dunder `__iterar__` (v1.12).
 *
 * Verifica que `para x en obj` funciona cuando `obj` es VAL_INSTANCIA
 * y su clase define `__iterar__`. El dunder debe retornar un iterable
 * nativo (lista, tupla, conjunto, dicc, rango, cadena); ese valor se
 * itera con la maquinaria existente.
 *
 * Cubre:
 *   - Lista, tupla, rango, cadena retornados por __iterar__.
 *   - Encadenamiento: __iterar__ de una clase retorna otra instancia
 *     con __iterar__ (la VM dispatcha hasta encontrar iterable nativo).
 *   - Errores: clase sin __iterar__, __iterar__ que retorna no-iterable,
 *     __iterar__ que lanza excepción (debe ser atrapable).
 */

#include <stdio.h>
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

static const char *ejecutar(const char *fuente, const char *nombre_var,
                              const char **error_out) {
    static char buffer[4096];
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **prog = parser_parsear_programa(&p, &n);
    if (!prog || p.tuvo_error) {
        if (error_out) *error_out = "<error de parseo>";
        arena_destruir(&a);
        return NULL;
    }
    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, prog, n)) {
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", c.error.mensaje);
            *error_out = errbuf;
        }
        chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    VM vm; vm_iniciar(&vm);
    Valor resultado = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &resultado);
    if (rc != VM_OK) {
        if (error_out) {
            static char errbuf[512];
            snprintf(errbuf, sizeof(errbuf), "%s", vm.error.mensaje);
            *error_out = errbuf;
        }
        valor_destruir(&resultado);
        vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    Valor nombre = valor_cadena_referencia(nombre_var, (int)strlen(nombre_var));
    Valor v;
    if (!dicc_obtener(vm.globales, &nombre, &v)) {
        if (error_out) *error_out = "<variable no encontrada>";
        valor_destruir(&resultado);
        vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }
    valor_a_cadena(&v, buffer, sizeof(buffer));
    valor_destruir(&v);
    valor_destruir(&resultado);
    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
    if (error_out) *error_out = NULL;
    return buffer;
}

static void verificar_var(const char *fuente, const char *var,
                           const char *esperado) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, var, &err);
    if (!res) {
        fprintf(stderr, "FALLO: %s\n  -> error: %s\n", fuente,
                err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO: %s\n  -> %s=%s (esperaba %s)\n",
                fuente, var, res, esperado);
        fallos++;
    }
}

/* Plantillas de fuente reutilizadas. */
static const char *PILA_DECL =
    "clase Pila:\n"
    "  funcion __iniciar__(yo):\n"
    "    yo.items = []\n"
    "  fin funcion\n"
    "  funcion meter(yo, val):\n"
    "    agregar(yo.items, val)\n"
    "  fin funcion\n"
    "  funcion __iterar__(yo):\n"
    "    res = []\n"
    "    i = longitud(yo.items) - 1\n"
    "    mientras i >= 0:\n"
    "      agregar(res, yo.items[i])\n"
    "      i = i - 1\n"
    "    fin mientras\n"
    "    retornar res\n"
    "  fin funcion\n"
    "fin clase\n";

/* ───── Casos básicos ───── */

static void test_iterar_lista(void) {
    /* Pila vacía → bucle no entra. */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p = Pila()\n"
        "p.meter(1)\n"
        "p.meter(2)\n"
        "p.meter(3)\n"
        "salida = []\n"
        "para v en p:\n"
        "  agregar(salida, v)\n"
        "fin para\n"
        "x = salida",
        PILA_DECL);
    verificar_var(fuente, "x", "[3, 2, 1]");
}

static void test_iterar_lista_vacia(void) {
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p = Pila()\n"
        "_n = 0\n"
        "para v en p:\n"
        "  _n = _n + 1\n"
        "fin para\n"
        "x = _n",
        PILA_DECL);
    verificar_var(fuente, "x", "0");
}

static void test_iterar_tupla(void) {
    /* __iterar__ retornando tupla. */
    verificar_var(
        "clase Caja:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 10\n"
        "  fin funcion\n"
        "  funcion __iterar__(yo):\n"
        "    retornar (1, 2, 3)\n"
        "  fin funcion\n"
        "fin clase\n"
        "salida = []\n"
        "para v en Caja():\n"
        "  agregar(salida, v)\n"
        "fin para\n"
        "x = salida",
        "x", "[1, 2, 3]");
}

static void test_iterar_rango(void) {
    /* __iterar__ retornando rango. */
    verificar_var(
        "clase R:\n"
        "  funcion __iniciar__(yo, n):\n"
        "    yo.n = n\n"
        "  fin funcion\n"
        "  funcion __iterar__(yo):\n"
        "    retornar rango(yo.n)\n"
        "  fin funcion\n"
        "fin clase\n"
        "salida = []\n"
        "para v en R(4):\n"
        "  agregar(salida, v)\n"
        "fin para\n"
        "x = salida",
        "x", "[0, 1, 2, 3]");
}

static void test_iterar_cadena(void) {
    /* __iterar__ retornando cadena → itera code points. */
    verificar_var(
        "clase Letras:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 0\n"
        "  fin funcion\n"
        "  funcion __iterar__(yo):\n"
        "    retornar \"ab\"\n"
        "  fin funcion\n"
        "fin clase\n"
        "salida = []\n"
        "para c en Letras():\n"
        "  agregar(salida, c)\n"
        "fin para\n"
        "x = salida",
        "x", "[\"a\", \"b\"]");
}

/* ───── Encadenamiento ───── */

static void test_iterar_encadenado(void) {
    /* Una clase cuyo __iterar__ retorna otra instancia con __iterar__.
       La VM dispatcha hasta encontrar iterable nativo. */
    verificar_var(
        "clase Hojas:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 0\n"
        "  fin funcion\n"
        "  funcion __iterar__(yo):\n"
        "    retornar [10, 20]\n"
        "  fin funcion\n"
        "fin clase\n"
        "clase Wrapper:\n"
        "  funcion __iniciar__(yo, h):\n"
        "    yo.h = h\n"
        "  fin funcion\n"
        "  funcion __iterar__(yo):\n"
        "    retornar yo.h\n"  /* Otra instancia con __iterar__ */
        "  fin funcion\n"
        "fin clase\n"
        "salida = []\n"
        "para v en Wrapper(Hojas()):\n"
        "  agregar(salida, v)\n"
        "fin para\n"
        "x = salida",
        "x", "[10, 20]");
}

/* ───── Errores ───── */

static void test_iterar_clase_sin_dunder(void) {
    /* Clase sin __iterar__ → ErrorDeTipo claro, atrapable. */
    verificar_var(
        "clase Vacia:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "fin clase\n"
        "_msg = \"\"\n"
        "intentar:\n"
        "  para v en Vacia():\n"
        "    _msg = \"no debe pasar\"\n"
        "  fin para\n"
        "atrapar ErrorDeTipo como e:\n"
        "  _msg = \"atrapado\"\n"
        "fin intentar\n"
        "x = _msg",
        "x", "atrapado");
}

static void test_iterar_dunder_lanza(void) {
    /* __iterar__ que lanza una excepción debe propagarse atrapable. */
    verificar_var(
        "clase Mal:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "  funcion __iterar__(yo):\n"
        "    lanzar ErrorDeValor(\"intencional\")\n"
        "  fin funcion\n"
        "fin clase\n"
        "_msg = \"\"\n"
        "intentar:\n"
        "  para v en Mal():\n"
        "    _msg = \"no debe pasar\"\n"
        "  fin para\n"
        "atrapar ErrorDeValor como e:\n"
        "  _msg = \"atrapado\"\n"
        "fin intentar\n"
        "x = _msg",
        "x", "atrapado");
}

static void test_iterar_retorna_no_iterable(void) {
    /* __iterar__ que retorna un entero → ErrorDeTipo. */
    verificar_var(
        "clase MalRet:\n"
        "  funcion __iniciar__(yo):\n"
        "    yo.x = 1\n"
        "  fin funcion\n"
        "  funcion __iterar__(yo):\n"
        "    retornar 42\n"
        "  fin funcion\n"
        "fin clase\n"
        "_msg = \"\"\n"
        "intentar:\n"
        "  para v en MalRet():\n"
        "    _msg = \"no debe pasar\"\n"
        "  fin para\n"
        "atrapar ErrorDeTipo como e:\n"
        "  _msg = \"atrapado\"\n"
        "fin intentar\n"
        "x = _msg",
        "x", "atrapado");
}

/* ───── Iteración del propio cuerpo ───── */

static void test_iterar_modificar_durante(void) {
    /* `__iterar__` materializa al inicio — modificar después no afecta
       al bucle (la lista materializada es snapshot). */
    char fuente[2048];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p = Pila()\n"
        "p.meter(1)\n"
        "p.meter(2)\n"
        "salida = []\n"
        "para v en p:\n"
        "  agregar(salida, v)\n"
        "  p.meter(99)\n"  /* No debería aparecer en el bucle */
        "fin para\n"
        "x = salida",
        PILA_DECL);
    verificar_var(fuente, "x", "[2, 1]");
}

static void test_iterar_anidado(void) {
    /* Dos pilas, bucle anidado: cada uno tiene su propio iter. */
    char fuente[3072];
    snprintf(fuente, sizeof(fuente),
        "%s"
        "p1 = Pila()\n"
        "p1.meter(1)\n"
        "p1.meter(2)\n"
        "p2 = Pila()\n"
        "p2.meter(10)\n"
        "p2.meter(20)\n"
        "salida = []\n"
        "para a en p1:\n"
        "  para b en p2:\n"
        "    agregar(salida, a * 100 + b)\n"
        "  fin para\n"
        "fin para\n"
        "x = salida",
        PILA_DECL);
    /* p1 LIFO: 2, 1; p2 LIFO: 20, 10. Producto cartesiano:
       220, 210, 120, 110. */
    verificar_var(fuente, "x", "[220, 210, 120, 110]");
}

/* ───── Compatibilidad: tipos nativos siguen funcionando ───── */

static void test_iter_lista_nativa(void) {
    verificar_var(
        "salida = []\n"
        "para v en [1, 2, 3]:\n"
        "  agregar(salida, v * 2)\n"
        "fin para\n"
        "x = salida",
        "x", "[2, 4, 6]");
}

static void test_iter_rango_nativo(void) {
    verificar_var(
        "_t = 0\n"
        "para i en rango(5):\n"
        "  _t = _t + i\n"
        "fin para\n"
        "x = _t",
        "x", "10");
}

int main(void) {
    test_iterar_lista();
    test_iterar_lista_vacia();
    test_iterar_tupla();
    test_iterar_rango();
    test_iterar_cadena();

    test_iterar_encadenado();

    test_iterar_clase_sin_dunder();
    test_iterar_dunder_lanza();
    test_iterar_retorna_no_iterable();

    test_iterar_modificar_durante();
    test_iterar_anidado();

    test_iter_lista_nativa();
    test_iter_rango_nativo();

    if (fallos == 0) {
        printf("iteradores: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "iteradores: %d fallo(s)\n", fallos);
    return 1;
}
