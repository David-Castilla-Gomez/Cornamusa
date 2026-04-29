/*
 * Tests del recolector ejecutándose desde código Cornamusa (v0.8.1).
 *
 * Cubre:
 *   - Built-in `recolectar()` ejecuta un ciclo de mark-sweep.
 *   - Aridad incorrecta es rechazada con error claro.
 *   - Trigger automático del GC durante la ejecución (deferred a
 *     frontera de opcode) no rompe programas en uso normal.
 *   - Ciclos refcount creados desde Cornamusa se rompen via
 *     `recolectar()`.
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "compilador.h"
#include "lexer.h"
#include "memoria.h"
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

/* ───── recolectar() ───── */

static void test_recolectar_devuelve_entero(void) {
    /* Sin nada que liberar, recolectar devuelve 0. */
    verificar_var(
        "z = recolectar()",
        "z", "0");
}

static void test_recolectar_aridad(void) {
    /* recolectar() no acepta argumentos. */
    verificar_error(
        "recolectar(1)",
        "no acepta argumentos");
}

static void test_recolectar_libera_ciclo(void) {
    /* Crear un ciclo entre dos diccionarios y verificar que recolectar
       libera al menos los 2 dicc. */
    verificar_var(
        "a = {}\n"
        "b = {}\n"
        "a[\"b\"] = b\n"
        "b[\"a\"] = a\n"
        "a = nulo\n"
        "b = nulo\n"
        "z = recolectar() >= 2",
        "z", "verdadero");
}

static void test_recolectar_libera_ciclo_clases(void) {
    /* Misma idea con clases: dos instancias mutuamente referenciadas. */
    verificar_var(
        "clase Nodo:\n"
        "    funcion __iniciar__(yo, v):\n"
        "        yo.valor = v\n"
        "    fin funcion\n"
        "fin clase\n"
        "a = Nodo(\"A\")\n"
        "b = Nodo(\"B\")\n"
        "a.par = b\n"
        "b.par = a\n"
        "a = nulo\n"
        "b = nulo\n"
        "z = recolectar() >= 4",   /* 2 instancias + 2 dicc atributos */
        "z", "verdadero");
}

static void test_recolectar_no_toca_vivos(void) {
    /* recolectar mantiene los objetos accesibles desde raíces. */
    verificar_var(
        "v = [1, 2, 3]\n"
        "n = recolectar()\n"
        "z = v[0] + v[1] + v[2]",
        "z", "6");
}

/* ───── Trigger automático bajo carga ───── */

static void test_carga_pesada_no_explota(void) {
    /* Programa que aloca muchas listas/dicc en un bucle. Bajo
       --gc-stress (que activa trigger en cada alocación) este test
       exigirá que cada allocation sea segura.
       Sin --gc-stress, el trigger automático puede o no dispararse
       según el umbral. En cualquier caso, el resultado debe ser
       correcto. */
    verificar_var(
        "total = 0\n"
        "para i en rango(50):\n"
        "    l = [i, i + 1, i + 2]\n"
        "    total = total + l[0]\n"
        "fin para\n"
        "z = total",
        "z", "1225");
}

static void test_metodos_en_bucle(void) {
    /* Crear instancias en un bucle ejercita class instantiation +
       __iniciar__ + métodos repetidamente. */
    verificar_var(
        "clase Contador:\n"
        "    funcion __iniciar__(yo, n):\n"
        "        yo.n = n\n"
        "    fin funcion\n"
        "    funcion duplicar(yo):\n"
        "        retornar yo.n * 2\n"
        "    fin funcion\n"
        "fin clase\n"
        "suma = 0\n"
        "para i en rango(20):\n"
        "    c = Contador(i)\n"
        "    suma = suma + c.duplicar()\n"
        "fin para\n"
        "z = suma",
        "z", "380");   /* 2 * (0+1+...+19) = 2 * 190 = 380 */
}

int main(void) {
    test_recolectar_devuelve_entero();
    test_recolectar_aridad();
    test_recolectar_libera_ciclo();
    test_recolectar_libera_ciclo_clases();
    test_recolectar_no_toca_vivos();
    test_carga_pesada_no_explota();
    test_metodos_en_bucle();

    if (fallos == 0) {
        printf("OK: todos los tests del GC end-to-end pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
