/*
 * Tests del bytecode con clases — v0.7.0 Fase 8 sesión 1.
 *
 * Cubre:
 *   - Definición de una clase con cuerpo `pasar`.
 *   - Instanciación: `Foo()` produce VAL_INSTANCIA.
 *   - Asignación de atributos sobre instancia: `obj.x = 1`.
 *   - Lectura de atributos: `obj.x`.
 *   - tipo() devuelve "instancia" / "clase".
 *   - Atributo no presente: ErrorDeAtributo.
 *   - Asignar atributo a no-instancia: ErrorDeTipo.
 *   - Llamar la clase con argumentos: rechazado en S1.
 *   - Métodos en cuerpo: rechazado en S1 con error claro.
 *   - Herencia: rechazada en S1.
 *
 * En F8 S2 se añadirán métodos y `yo`. En S3 dunders. En S4 herencia.
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
    static char buffer[2048];
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 16384);
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
    if (nombre_var == NULL) {
        buffer[0] = '\0';
    } else {
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
    }
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
        fprintf(stderr, "FALLO en programa:\n%s\n  error: %s\n",
                fuente, err ? err : "<desconocido>");
        fallos++;
        return;
    }
    if (strcmp(res, esperado) != 0) {
        fprintf(stderr, "FALLO en programa:\n%s\n  esperaba %s=%s\n  obtuvo: %s\n",
                fuente, var, esperado, res);
        fallos++;
    }
}

static void verificar_error(const char *fuente, const char *substring) {
    const char *err = NULL;
    const char *res = ejecutar(fuente, NULL, &err);
    if (res) {
        fprintf(stderr, "FALLO: programa debería dar error pero ejecutó:\n%s\n",
                fuente);
        fallos++;
        return;
    }
    if (!err || !strstr(err, substring)) {
        fprintf(stderr, "FALLO: '%s' dio '%s' pero se esperaba '%s'\n",
                fuente, err ? err : "<null>", substring);
        fallos++;
    }
}

/* ───── Definición de clase y tipo ───── */

static void test_definir_clase(void) {
    /* La clase definida sin cuerpo (solo `pasar`) se registra como global. */
    verificar_var(
        "clase Punto:\n"
        "    pasar\n"
        "fin clase\n"
        "x = Punto",
        "x", "<clase Punto>");

    /* tipo(Punto) devuelve "clase". */
    verificar_var(
        "clase Foo:\n"
        "    pasar\n"
        "fin clase\n"
        "x = tipo(Foo)",
        "x", "clase");
}

/* ───── Instanciación ───── */

static void test_instanciar(void) {
    /* Llamar la clase devuelve una instancia. */
    verificar_var(
        "clase Punto:\n"
        "    pasar\n"
        "fin clase\n"
        "p = Punto()\n"
        "x = p",
        "x", "<instancia de Punto>");

    /* tipo(instancia) → "instancia". */
    verificar_var(
        "clase Foo:\n"
        "    pasar\n"
        "fin clase\n"
        "x = tipo(Foo())",
        "x", "instancia");
}

/* ───── Atributos: lectura y escritura ───── */

static void test_atributos(void) {
    /* Asignar y leer un atributo. */
    verificar_var(
        "clase Punto:\n"
        "    pasar\n"
        "fin clase\n"
        "p = Punto()\n"
        "p.coordx = 10\n"
        "p.coordy = 20\n"
        "z = p.coordx + p.coordy",
        "z", "30");

    /* Cadenas y otros tipos como atributos. */
    verificar_var(
        "clase Persona:\n"
        "    pasar\n"
        "fin clase\n"
        "p = Persona()\n"
        "p.nombre = \"Ada\"\n"
        "x = p.nombre",
        "x", "Ada");

    /* Mutación: una instancia compartida ve cambios. */
    verificar_var(
        "clase Caja:\n"
        "    pasar\n"
        "fin clase\n"
        "a = Caja()\n"
        "b = a\n"
        "a.contenido = 42\n"
        "x = b.contenido",
        "x", "42");

    /* Sobrescribir un atributo existente. */
    verificar_var(
        "clase C:\n"
        "    pasar\n"
        "fin clase\n"
        "c = C()\n"
        "c.v = 1\n"
        "c.v = 2\n"
        "x = c.v",
        "x", "2");
}

/* ───── Errores ───── */

static void test_errores_runtime(void) {
    /* Leer un atributo que no existe. */
    verificar_error(
        "clase Foo:\n"
        "    pasar\n"
        "fin clase\n"
        "f = Foo()\n"
        "x = f.no_existe",
        "ErrorDeAtributo");

    /* Asignar atributo a algo que no es instancia. */
    verificar_error(
        "x = 5\n"
        "x.atr = 1",
        "no admite asignacion de atributos");

    /* Leer atributo de algo que no es instancia. */
    verificar_error(
        "x = 5\n"
        "z = x.atr",
        "no tiene atributos accesibles");

    /* Llamar una clase sin __iniciar__ con argumentos: rechazado. */
    verificar_error(
        "clase Foo:\n"
        "    pasar\n"
        "fin clase\n"
        "f = Foo(1, 2)",
        "no acepta argumentos");
}

static void test_errores_compilacion(void) {
    /* Herencia múltiple: error claro (solo simple en v0.7.0). */
    const char *err = NULL;
    const char *res = ejecutar(
        "clase A:\n"
        "    pasar\n"
        "fin clase\n"
        "clase B:\n"
        "    pasar\n"
        "fin clase\n"
        "clase C extiende A, B:\n"
        "    pasar\n"
        "fin clase\n",
        NULL, &err);
    if (res || !err || !strstr(err, "herencia multiple")) {
        fprintf(stderr, "FALLO: herencia multiple no dio el error esperado\n");
        fprintf(stderr, "  obtuvo: %s\n", err ? err : "<null>");
        fallos++;
    }

    /* Heredar de algo que no es clase: error en runtime. */
    err = NULL;
    res = ejecutar(
        "x = 5\n"
        "clase Bad extiende x:\n"
        "    pasar\n"
        "fin clase\n",
        NULL, &err);
    if (res || !err || !strstr(err, "solo se puede heredar")) {
        fprintf(stderr, "FALLO: heredar de no-clase no dio el error esperado\n");
        fprintf(stderr, "  obtuvo: %s\n", err ? err : "<null>");
        fallos++;
    }
}

/* ───── Herencia simple (Fase 8 sesión 4) ───── */

static void test_herencia_metodos_heredados(void) {
    /* Hijo hereda métodos de Padre sin redefinir. */
    verificar_var(
        "clase Animal:\n"
        "    funcion saludar(yo):\n"
        "        retornar \"hola\"\n"
        "    fin funcion\n"
        "fin clase\n"
        "clase Perro extiende Animal:\n"
        "    pasar\n"
        "fin clase\n"
        "p = Perro()\n"
        "z = p.saludar()",
        "z", "hola");
}

static void test_herencia_override(void) {
    /* Hijo sobrescribe método del padre. */
    verificar_var(
        "clase Animal:\n"
        "    funcion sonido(yo):\n"
        "        retornar \"???\"\n"
        "    fin funcion\n"
        "fin clase\n"
        "clase Perro extiende Animal:\n"
        "    funcion sonido(yo):\n"
        "        retornar \"guau\"\n"
        "    fin funcion\n"
        "fin clase\n"
        "z = Perro().sonido()",
        "z", "guau");

    /* El padre conserva su método original. */
    verificar_var(
        "clase Animal:\n"
        "    funcion sonido(yo):\n"
        "        retornar \"???\"\n"
        "    fin funcion\n"
        "fin clase\n"
        "clase Perro extiende Animal:\n"
        "    funcion sonido(yo):\n"
        "        retornar \"guau\"\n"
        "    fin funcion\n"
        "fin clase\n"
        "z = Animal().sonido()",
        "z", "???");
}

static void test_herencia_constructor_heredado(void) {
    /* Hijo hereda __iniciar__ del padre. */
    verificar_var(
        "clase Animal:\n"
        "    funcion __iniciar__(yo, nombre):\n"
        "        yo.nombre = nombre\n"
        "    fin funcion\n"
        "fin clase\n"
        "clase Perro extiende Animal:\n"
        "    pasar\n"
        "fin clase\n"
        "p = Perro(\"Rex\")\n"
        "z = p.nombre",
        "z", "Rex");
}

static void test_herencia_polimorfismo(void) {
    /* Diferentes subclases dispatchean a su propio método. */
    verificar_var(
        "clase Forma:\n"
        "    funcion area(yo):\n"
        "        retornar 0\n"
        "    fin funcion\n"
        "fin clase\n"
        "clase Cuadrado extiende Forma:\n"
        "    funcion __iniciar__(yo, lado):\n"
        "        yo.lado = lado\n"
        "    fin funcion\n"
        "    funcion area(yo):\n"
        "        retornar yo.lado * yo.lado\n"
        "    fin funcion\n"
        "fin clase\n"
        "clase Circulo extiende Forma:\n"
        "    funcion __iniciar__(yo, radio):\n"
        "        yo.radio = radio\n"
        "    fin funcion\n"
        "    funcion area(yo):\n"
        "        retornar yo.radio * yo.radio * 3\n"
        "    fin funcion\n"
        "fin clase\n"
        "c = Cuadrado(5)\n"
        "z = c.area()",
        "z", "25");
}

static void test_herencia_metodos_y_override(void) {
    /* Combinación: el hijo override una y hereda otra. */
    verificar_var(
        "clase Padre:\n"
        "    funcion uno(yo):\n"
        "        retornar 1\n"
        "    fin funcion\n"
        "    funcion dos(yo):\n"
        "        retornar 2\n"
        "    fin funcion\n"
        "fin clase\n"
        "clase Hijo extiende Padre:\n"
        "    funcion uno(yo):\n"
        "        retornar 100\n"
        "    fin funcion\n"
        "fin clase\n"
        "h = Hijo()\n"
        "z = h.uno() + h.dos()",
        "z", "102");
}

/* ───── Métodos (Fase 8 sesión 2) ───── */

static void test_metodo_simple(void) {
    /* Método sin argumentos extra: solo `yo`. */
    verificar_var(
        "clase Saludador:\n"
        "    funcion saludar(yo):\n"
        "        retornar 7\n"
        "    fin funcion\n"
        "fin clase\n"
        "s = Saludador()\n"
        "z = s.saludar()",
        "z", "7");
}

static void test_metodo_con_args(void) {
    /* Método con argumentos extra. */
    verificar_var(
        "clase Calc:\n"
        "    funcion suma(yo, a, b):\n"
        "        retornar a + b\n"
        "    fin funcion\n"
        "fin clase\n"
        "c = Calc()\n"
        "z = c.suma(3, 4)",
        "z", "7");
}

static void test_metodo_usa_yo(void) {
    /* El método accede a atributos de la instancia via `yo`. */
    verificar_var(
        "clase Contador:\n"
        "    funcion incrementar(yo):\n"
        "        yo.n = yo.n + 1\n"
        "    fin funcion\n"
        "    funcion valor(yo):\n"
        "        retornar yo.n\n"
        "    fin funcion\n"
        "fin clase\n"
        "c = Contador()\n"
        "c.n = 0\n"
        "c.incrementar()\n"
        "c.incrementar()\n"
        "c.incrementar()\n"
        "z = c.valor()",
        "z", "3");
}

static void test_metodo_retorna_yo(void) {
    /* Método encadenable: retorna `yo`. */
    verificar_var(
        "clase Caja:\n"
        "    funcion poner(yo, v):\n"
        "        yo.v = v\n"
        "        retornar yo\n"
        "    fin funcion\n"
        "fin clase\n"
        "c = Caja()\n"
        "z = c.poner(42).v",
        "z", "42");
}

static void test_metodo_aridad(void) {
    /* Mensaje de error reporta la aridad sin el receptor. */
    verificar_error(
        "clase C:\n"
        "    funcion m(yo, a, b):\n"
        "        retornar a + b\n"
        "    fin funcion\n"
        "fin clase\n"
        "C().m(1)",
        "esperaba 2 argumentos, recibio 1");
}

static void test_atributo_sombrea_metodo(void) {
    /* Atributo de instancia con el mismo nombre tiene prioridad. */
    verificar_var(
        "clase Foo:\n"
        "    funcion m(yo):\n"
        "        retornar 1\n"
        "    fin funcion\n"
        "fin clase\n"
        "f = Foo()\n"
        "f.m = 99\n"
        "z = f.m",
        "z", "99");
}

static void test_metodo_ligado_es_funcion(void) {
    /* tipo() de un metodo ligado lo reporta como `funcion`. */
    verificar_var(
        "clase Foo:\n"
        "    funcion m(yo):\n"
        "        retornar 1\n"
        "    fin funcion\n"
        "fin clase\n"
        "f = Foo()\n"
        "z = tipo(f.m)",
        "z", "funcion");
}

/* ───── Atributos como funciones ───── */

static void test_atributo_funcion(void) {
    /* Un atributo puede ser una función (almacenada). En F8 S2 se
       convertirá en método con auto-bind, pero en S1 se llama
       explícitamente. */
    verificar_var(
        "funcion saluda():\n"
        "    retornar 7\n"
        "fin funcion\n"
        "clase Caja:\n"
        "    pasar\n"
        "fin clase\n"
        "c = Caja()\n"
        "c.fn = saluda\n"
        "x = c.fn()",
        "x", "7");
}

/* ───── __iniciar__ (constructor) — Fase 8 sesión 3 ───── */

static void test_constructor_simple(void) {
    /* __iniciar__ se ejecuta al instanciar y puede asignar atributos. */
    verificar_var(
        "clase Punto:\n"
        "    funcion __iniciar__(yo, a, b):\n"
        "        yo.coordx = a\n"
        "        yo.coordy = b\n"
        "    fin funcion\n"
        "fin clase\n"
        "p = Punto(3, 4)\n"
        "z = p.coordx + p.coordy",
        "z", "7");
}

static void test_constructor_sin_args(void) {
    /* __iniciar__ con solo `yo` (sin args extra). */
    verificar_var(
        "clase Caja:\n"
        "    funcion __iniciar__(yo):\n"
        "        yo.contenido = 42\n"
        "    fin funcion\n"
        "fin clase\n"
        "c = Caja()\n"
        "z = c.contenido",
        "z", "42");
}

static void test_constructor_devuelve_instancia(void) {
    /* Aunque __iniciar__ no devuelva nada (o devuelva otra cosa), la
       llamada `Foo()` siempre devuelve la instancia. */
    verificar_var(
        "clase Foo:\n"
        "    funcion __iniciar__(yo):\n"
        "        yo.x = 1\n"
        "        retornar 999\n"
        "    fin funcion\n"
        "fin clase\n"
        "f = Foo()\n"
        "z = f.x",
        "z", "1");

    verificar_var(
        "clase Foo:\n"
        "    funcion __iniciar__(yo):\n"
        "        yo.x = 1\n"
        "        retornar 999\n"
        "    fin funcion\n"
        "fin clase\n"
        "x = tipo(Foo())",
        "x", "instancia");
}

static void test_constructor_aridad_error(void) {
    /* Pasar pocos args: error claro. */
    verificar_error(
        "clase Foo:\n"
        "    funcion __iniciar__(yo, a, b):\n"
        "        pasar\n"
        "    fin funcion\n"
        "fin clase\n"
        "Foo(1)",
        "esperaba 2 argumentos, recibio 1");
}

static void test_constructor_y_metodos(void) {
    /* Combinación realista: constructor + métodos sobre los atributos
       inicializados. */
    verificar_var(
        "clase Contador:\n"
        "    funcion __iniciar__(yo, inicial):\n"
        "        yo.n = inicial\n"
        "    fin funcion\n"
        "    funcion incrementar(yo):\n"
        "        yo.n = yo.n + 1\n"
        "        retornar yo\n"
        "    fin funcion\n"
        "    funcion valor(yo):\n"
        "        retornar yo.n\n"
        "    fin funcion\n"
        "fin clase\n"
        "c = Contador(10)\n"
        "c.incrementar().incrementar().incrementar()\n"
        "z = c.valor()",
        "z", "13");
}

/* ───── Identidad de instancias ───── */

static void test_identidad(void) {
    /* Dos instancias diferentes no son iguales por `is`. */
    verificar_var(
        "clase Foo:\n"
        "    pasar\n"
        "fin clase\n"
        "a = Foo()\n"
        "b = Foo()\n"
        "x = a es b",
        "x", "falso");

    /* Misma referencia: `is` verdadero. */
    verificar_var(
        "clase Foo:\n"
        "    pasar\n"
        "fin clase\n"
        "a = Foo()\n"
        "b = a\n"
        "x = a es b",
        "x", "verdadero");
}

int main(void) {
    test_definir_clase();
    test_instanciar();
    test_atributos();
    test_errores_runtime();
    test_errores_compilacion();
    test_metodo_simple();
    test_metodo_con_args();
    test_metodo_usa_yo();
    test_metodo_retorna_yo();
    test_metodo_aridad();
    test_atributo_sombrea_metodo();
    test_metodo_ligado_es_funcion();
    test_constructor_simple();
    test_constructor_sin_args();
    test_constructor_devuelve_instancia();
    test_constructor_aridad_error();
    test_constructor_y_metodos();
    test_herencia_metodos_heredados();
    test_herencia_override();
    test_herencia_constructor_heredado();
    test_herencia_polimorfismo();
    test_herencia_metodos_y_override();
    test_atributo_funcion();
    test_identidad();

    if (fallos == 0) {
        printf("OK: todos los tests del bytecode con clases pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
