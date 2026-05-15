/*
 * Tests del despacho de los dunders de coerción introducidos en v1.41:
 *   - `__repr__`     — invocado por el built-in `repr(obj)`.
 *   - `__booleano__` — invocado por OP_NO y OP_SALTAR_SI_FALSO (`si`,
 *                       `mientras`, `y`/`o`, `no`).
 *
 * El despacho sigue el patrón establecido por `__cadena__` (v1.2):
 * cuando el operando es VAL_INSTANCIA y la clase define el dunder, la
 * VM empuja un frame para ejecutarlo. Si no, fallback a la semántica
 * por defecto (`valor_a_repr` para repr; `valor_es_verdadero` para
 * booleano — instancia siempre verdadera).
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

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, "_n", &err);
    if (res) {
        fprintf(stderr, "FALLO: '%s' debia dar error pero ejecuto\n", fuente);
        fallos++;
        return;
    }
    if (substring && (!err || !strstr(err, substring))) {
        fprintf(stderr, "FALLO: error %s no contiene '%s'\n",
                err ? err : "<vacio>", substring);
        fallos++;
    }
}

/* ─── __repr__ ─── */

static void test_repr_dunder_basico(void) {
    /* La clase define __repr__: `repr(obj)` invoca el dunder. */
    const char *src =
        "clase P:\n"
        "  funcion __iniciar__(yo, n): yo.n = n\n"
        "  funcion __repr__(yo): retornar \"P(\" + cadena(yo.n) + \")\"\n"
        "fin clase\n"
        "r = repr(P(7))\n";
    verificar_var(src, "r", "P(7)");
}

static void test_repr_sin_dunder_fallback(void) {
    /* Sin __repr__, repr() cae a `valor_a_repr` — representación
       canónica. Para un entero es la cifra; para una cadena es la
       cadena entre comillas. */
    verificar_var("r = repr(42)", "r", "42");
    verificar_var("r = repr(\"hola\")", "r", "\"hola\"");
    verificar_var("r = repr([1, 2])", "r", "[1, 2]");
}

static void test_repr_distinto_de_cadena(void) {
    /* __repr__ y __cadena__ pueden devolver cosas distintas. */
    const char *src =
        "clase P:\n"
        "  funcion __iniciar__(yo, n): yo.n = n\n"
        "  funcion __cadena__(yo): retornar cadena(yo.n)\n"
        "  funcion __repr__(yo): retornar \"P(\" + cadena(yo.n) + \")\"\n"
        "fin clase\n"
        "p = P(5)\n"
        "s = cadena(p)\n"
        "r = repr(p)\n";
    verificar_var(src, "s", "5");
    verificar_var(src, "r", "P(5)");
}

static void test_repr_retorno_no_cadena(void) {
    /* __repr__ debe retornar cadena — OP_ASEGURAR_CADENA lo valida. */
    verificar_error(
        "clase P:\n"
        "  funcion __repr__(yo):\n"
        "    retornar 42\n"
        "  fin funcion\n"
        "fin clase\n"
        "_n = repr(P())\n",
        "se esperaba cadena");
}

/* ─── __booleano__ ─── */

static void test_booleano_si_falso(void) {
    /* __booleano__ devuelve falso → rama `sino` se toma. */
    const char *src =
        "clase Vacia:\n"
        "  funcion __booleano__(yo): retornar falso\n"
        "fin clase\n"
        "v = Vacia()\n"
        "si v:\n"
        "  r = \"si\"\n"
        "sino:\n"
        "  r = \"no\"\n"
        "fin si\n";
    verificar_var(src, "r", "no");
}

static void test_booleano_si_verdadero(void) {
    const char *src =
        "clase Llena:\n"
        "  funcion __booleano__(yo): retornar verdadero\n"
        "fin clase\n"
        "v = Llena()\n"
        "si v:\n"
        "  r = \"si\"\n"
        "sino:\n"
        "  r = \"no\"\n"
        "fin si\n";
    verificar_var(src, "r", "si");
}

static void test_booleano_no_unario(void) {
    /* `no obj` invoca __booleano__ y niega. */
    const char *src =
        "clase Vacia:\n"
        "  funcion __booleano__(yo): retornar falso\n"
        "fin clase\n"
        "r = no Vacia()\n";
    verificar_var(src, "r", "verdadero");
}

static void test_booleano_sin_dunder_es_verdadero(void) {
    /* Sin __booleano__, una instancia es siempre verdadera. */
    const char *src =
        "clase X:\n"
        "  pasar\n"
        "fin clase\n"
        "si X():\n"
        "  r = \"si\"\n"
        "sino:\n"
        "  r = \"no\"\n"
        "fin si\n";
    verificar_var(src, "r", "si");
}

static void test_booleano_en_mientras(void) {
    /* __booleano__ se consulta en cada iteración de `mientras`. */
    const char *src =
        "clase Contador:\n"
        "  funcion __iniciar__(yo, n): yo.n = n\n"
        "  funcion __booleano__(yo): retornar yo.n > 0\n"
        "fin clase\n"
        "c = Contador(3)\n"
        "total = 0\n"
        "mientras c:\n"
        "  total = total + c.n\n"
        "  c.n = c.n - 1\n"
        "fin mientras\n";
    verificar_var(src, "total", "6");
}

static void test_booleano_y_corto_circuito(void) {
    /* `a y b` invoca __booleano__(a). Si falso, devuelve a (sin
       evaluar b). Si verdadero, devuelve b. La marca global registra
       si `efecto()` se ejecutó. */
    const char *src =
        "clase Vacia:\n"
        "  funcion __booleano__(yo): retornar falso\n"
        "fin clase\n"
        "v = Vacia()\n"
        "marca = [0]\n"
        "funcion efecto():\n"
        "  marca[0] = 99\n"
        "  retornar verdadero\n"
        "fin funcion\n"
        "r = v y efecto()\n"
        "saltado = marca[0]\n";
    /* `v y efecto()`: v es falsy → corto-circuito, no llama efecto.
       Resultado = v (instancia Vacia). `marca[0]` queda en 0. */
    verificar_var(src, "saltado", "0");
}

static void test_booleano_o_corto_circuito(void) {
    /* `a o b`: si a verdadero, retorna a; si falso, evalúa b. */
    const char *src =
        "clase Vacia:\n"
        "  funcion __booleano__(yo): retornar falso\n"
        "fin clase\n"
        "v = Vacia()\n"
        "r = v o 99\n";
    verificar_var(src, "r", "99");
}

static void test_booleano_repr_combinados(void) {
    /* Una clase puede definir AMBOS dunders y se invocan según
       contexto. */
    const char *src =
        "clase Optional:\n"
        "  funcion __iniciar__(yo, x): yo.x = x\n"
        "  funcion __repr__(yo): retornar \"Opt(\" + cadena(yo.x) + \")\"\n"
        "  funcion __booleano__(yo): retornar yo.x != nulo\n"
        "fin clase\n"
        "a = Optional(5)\n"
        "b = Optional(nulo)\n"
        "ra = repr(a)\n"
        "rb = repr(b)\n"
        "ba = booleano(0)\n"  /* fallback path para no-instancia */
        "si a:\n"
        "  da = \"si\"\n"
        "sino:\n"
        "  da = \"no\"\n"
        "fin si\n"
        "si b:\n"
        "  db = \"si\"\n"
        "sino:\n"
        "  db = \"no\"\n"
        "fin si\n";
    verificar_var(src, "ra", "Opt(5)");
    verificar_var(src, "rb", "Opt(nulo)");
    verificar_var(src, "da", "si");
    verificar_var(src, "db", "no");
}

int main(void) {
    test_repr_dunder_basico();
    test_repr_sin_dunder_fallback();
    test_repr_distinto_de_cadena();
    test_repr_retorno_no_cadena();

    test_booleano_si_falso();
    test_booleano_si_verdadero();
    test_booleano_no_unario();
    test_booleano_sin_dunder_es_verdadero();
    test_booleano_en_mientras();
    test_booleano_y_corto_circuito();
    test_booleano_o_corto_circuito();
    test_booleano_repr_combinados();

    if (fallos == 0) {
        printf("dunders_coercion: todos los tests pasan\n");
        return 0;
    }
    fprintf(stderr, "dunders_coercion: %d fallo(s)\n", fallos);
    return 1;
}
