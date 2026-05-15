/*
 * Tests de iteradores lazy con `__siguiente__` (v1.43).
 *
 * `__iterar__` puede devolver una instancia con `__siguiente__`. La VM
 * la despacha en cada paso del `para` vía sub-VM síncrono — mismo
 * mecanismo que v1.42 usa para `__hash__`/`__igual__`. El fin de
 * iteración se señaliza con `lanzar ErrorDeIteracion`; la VM la
 * atrapa internamente.
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

#define CONTADOR \
    "clase Contador:\n" \
    "  funcion __iniciar__(yo, ini, tope):\n" \
    "    yo.i = ini\n" \
    "    yo.tope = tope\n" \
    "  fin funcion\n" \
    "  funcion __iterar__(yo):\n" \
    "    retornar yo\n" \
    "  fin funcion\n" \
    "  funcion __siguiente__(yo):\n" \
    "    si yo.i >= yo.tope:\n" \
    "      lanzar ErrorDeIteracion()\n" \
    "    fin si\n" \
    "    v = yo.i\n" \
    "    yo.i = yo.i + 1\n" \
    "    retornar v\n" \
    "  fin funcion\n" \
    "fin clase\n"

static void test_contador_basico(void) {
    verificar_var(CONTADOR
                  "total = 0\n"
                  "para n en Contador(0, 5):\n"
                  "  total = total + n\n"
                  "fin para\n",
                  "total", "10");
}

static void test_contador_vacio(void) {
    /* tope == ini: cero iteraciones. */
    verificar_var(CONTADOR
                  "veces = 0\n"
                  "para n en Contador(7, 7):\n"
                  "  veces = veces + 1\n"
                  "fin para\n",
                  "veces", "0");
}

static void test_instancia_directa_sin_iterar(void) {
    /* Sin `__iterar__`, la instancia que define solo `__siguiente__`
       puede iterarse directamente. */
    verificar_var("clase Tres:\n"
                  "  funcion __iniciar__(yo):\n"
                  "    yo.n = 0\n"
                  "  fin funcion\n"
                  "  funcion __siguiente__(yo):\n"
                  "    yo.n = yo.n + 1\n"
                  "    si yo.n > 3:\n"
                  "      lanzar ErrorDeIteracion()\n"
                  "    fin si\n"
                  "    retornar yo.n\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "total = 0\n"
                  "para v en Tres():\n"
                  "  total = total + v\n"
                  "fin para\n",
                  "total", "6");
}

static void test_iterador_distinto_de_la_coleccion(void) {
    /* Colección y su iterador como clases separadas. */
    verificar_var("clase ListaIter:\n"
                  "  funcion __iniciar__(yo, datos):\n"
                  "    yo.datos = datos\n"
                  "    yo.i = 0\n"
                  "  fin funcion\n"
                  "  funcion __siguiente__(yo):\n"
                  "    si yo.i >= longitud(yo.datos):\n"
                  "      lanzar ErrorDeIteracion()\n"
                  "    fin si\n"
                  "    v = yo.datos[yo.i]\n"
                  "    yo.i = yo.i + 1\n"
                  "    retornar v\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "clase MiLista:\n"
                  "  funcion __iniciar__(yo, xs):\n"
                  "    yo.xs = xs\n"
                  "  fin funcion\n"
                  "  funcion __iterar__(yo):\n"
                  "    retornar ListaIter(yo.xs)\n"
                  "  fin funcion\n"
                  "fin clase\n"
                  "total = 0\n"
                  "para x en MiLista([10, 20, 30]):\n"
                  "  total = total + x\n"
                  "fin para\n",
                  "total", "60");
}

static void test_romper_dentro_del_bucle(void) {
    /* `romper` desde el cuerpo termina el bucle sin agotar el iterador. */
    verificar_var(CONTADOR
                  "vistos = 0\n"
                  "para n en Contador(0, 100):\n"
                  "  si n == 3:\n"
                  "    romper\n"
                  "  fin si\n"
                  "  vistos = vistos + 1\n"
                  "fin para\n",
                  "vistos", "3");
}

static void test_error_iteracion_atrapable_explicito(void) {
    /* El usuario puede atrapar ErrorDeIteracion manualmente. */
    verificar_var("intentar:\n"
                  "  lanzar ErrorDeIteracion(\"x\")\n"
                  "atrapar ErrorDeIteracion como e:\n"
                  "  r = \"caught\"\n"
                  "fin intentar\n",
                  "r", "caught");
}

static void test_iter_consume_por_pasos(void) {
    /* Dos `para` sobre el MISMO iterador instance: la segunda
       comienza donde se quedó la primera (sin nueva llamada a
       __iterar__). */
    verificar_var(CONTADOR
                  "c = Contador(0, 5)\n"
                  "primero = 0\n"
                  "para n en c:\n"
                  "  primero = primero + 1\n"
                  "  si primero == 2:\n"
                  "    romper\n"
                  "  fin si\n"
                  "fin para\n"
                  "segundo = 0\n"
                  "para n en c:\n"
                  "  segundo = segundo + 1\n"
                  "fin para\n"
                  "total = primero * 100 + segundo\n",
                  "total", "203");   /* 2 + 3 restantes */
}

int main(void) {
    test_contador_basico();
    test_contador_vacio();
    test_instancia_directa_sin_iterar();
    test_iterador_distinto_de_la_coleccion();
    test_romper_dentro_del_bucle();
    test_error_iteracion_atrapable_explicito();
    test_iter_consume_por_pasos();

    if (fallos == 0) {
        printf("iter_lazy: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "iter_lazy: %d fallo(s)\n", fallos);
    return 1;
}
