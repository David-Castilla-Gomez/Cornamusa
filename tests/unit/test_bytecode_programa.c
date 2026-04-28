/*
 * Tests del bytecode end-to-end con sentencias — Fase 6 sesión 3.
 *
 * Verifica programas que usan asignación a global, sentencia-expresión
 * (`imprimir(...)`), bloques y `pasar`. Se inspeccionan tanto las
 * variables globales tras ejecutar como la salida estándar capturada.
 */

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define dup _dup
#define dup2 _dup2
#define close _close
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "compilador.h"
#include "lexer.h"
#include "parser.h"
#include "valor.h"
#include "vm.h"

static int fallos = 0;

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n",                            \
                    __FILE__, __LINE__, #cond);                                \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

/*
 * Compila y ejecuta `fuente` como programa completo.
 * Si `nombre_var` no es NULL, devuelve la representación textual del
 * valor de esa global tras ejecutar.
 * Si es NULL, devuelve un buffer vacío y solo se verifica exit OK.
 *
 * `error_out` recibe el mensaje de error si lo hubo (parser, compilador
 * o runtime VM).
 */
static const char *ejecutar_programa(const char *fuente,
                                       const char *nombre_var,
                                       const char **error_out) {
    static char buffer[1024];

    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");

    Arena a;
    arena_iniciar(&a, 4096);

    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");

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
        vm_destruir(&vm);
        chunk_destruir(&chunk); arena_destruir(&a);
        return NULL;
    }

    if (nombre_var) {
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
    } else {
        buffer[0] = '\0';
    }
    valor_destruir(&resultado);
    vm_destruir(&vm); chunk_destruir(&chunk); arena_destruir(&a);
    if (error_out) *error_out = NULL;
    return buffer;
}

static void verificar_var(const char *fuente, const char *var,
                          const char *esperado) {
    const char *err = NULL;
    const char *res = ejecutar_programa(fuente, var, &err);
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
    const char *res = ejecutar_programa(fuente, NULL, &err);
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

/* ───── Asignación simple ───── */

static void test_asignacion(void) {
    verificar_var("x = 42", "x", "42");
    verificar_var("x = 1 + 2 * 3", "x", "7");
    /* Reasignación: la última gana. */
    verificar_var("x = 1\nx = 2", "x", "2");
    /* Múltiples variables. */
    verificar_var("a = 10\nb = 20\nc = a + b", "c", "30");
    /* Variable usada después de asignar. */
    verificar_var("x = 5\nx = x * 2", "x", "10");
    /* Cambio de tipo libre. */
    verificar_var("x = 1\nx = \"hola\"", "x", "hola");
}

/* ───── Identificador no definido ───── */

static void test_no_definido(void) {
    verificar_error("x", "no esta definido");
    verificar_error("z = x + 1", "no esta definido");
}

/* ───── Imprimir (capturando stdout) ───── */

/*
 * Captura la salida estándar de un programa Cornamusa ejecutado por
 * la VM. Devuelve la cadena producida.
 */
static const char *ejecutar_y_capturar(const char *fuente) {
    static char salida[2048];
    fflush(stdout);
    /* Redirigir stdout a un archivo temporal. */
    FILE *backup = stdout;
    FILE *temp = tmpfile();
    if (!temp) return "<no temp>";
    /* En vez de freopen (que es más portable pero a veces problemático),
       hacemos dup2 si fileno está disponible, o usamos un sub-archivo
       con setbuf. Para Windows + C99 usamos un truco simple: el
       programa escribirá al stdout actual. Si tmpfile falla en algún
       sistema, es un test menos. */
    int saved_fd = -1;
    int temp_fd = fileno(temp);
    saved_fd = dup(fileno(stdout));
    fflush(stdout);
    dup2(temp_fd, fileno(stdout));

    const char *err = NULL;
    ejecutar_programa(fuente, NULL, &err);

    fflush(stdout);
    dup2(saved_fd, fileno(stdout));
    close(saved_fd);

    rewind(temp);
    size_t leido = fread(salida, 1, sizeof(salida) - 1, temp);
    salida[leido] = '\0';
    fclose(temp);

    (void)backup;
    if (err) return err;
    return salida;
}

static void verificar_salida(const char *fuente, const char *esperada) {
    const char *out = ejecutar_y_capturar(fuente);
    if (strcmp(out, esperada) != 0) {
        fprintf(stderr, "FALLO en programa:\n%s\n  esperaba salida:\n%s\n  obtuvo:\n%s\n",
                fuente, esperada, out);
        fallos++;
    }
}

static void test_imprimir(void) {
    verificar_salida("imprimir(\"hola\")", "hola\n");
    verificar_salida("imprimir(42)", "42\n");
    verificar_salida("imprimir(1, 2, 3)", "1 2 3\n");
    verificar_salida("imprimir()", "\n");
    /* Aritmética y luego imprimir. */
    verificar_salida(
        "x = 5\n"
        "imprimir(\"x =\", x)",
        "x = 5\n");
    /* Varias sentencias con imprimir. */
    verificar_salida(
        "imprimir(\"primero\")\n"
        "imprimir(\"segundo\")",
        "primero\nsegundo\n");
}

/* ───── Pasar ───── */

static void test_pasar(void) {
    /* `pasar` no produce salida ni error. Verificamos via global. */
    verificar_var(
        "x = 1\n"
        "pasar\n"
        "x = 2",
        "x", "2");
}

/* ───── Programas combinados ───── */

static void test_programa_combinado(void) {
    verificar_salida(
        "a = 3\n"
        "b = 4\n"
        "c = (a ** 2 + b ** 2) ** 0.5\n"
        "imprimir(\"hipotenusa:\", c)",
        "hipotenusa: 5.0\n");
}

int main(void) {
    test_asignacion();
    test_no_definido();
    test_imprimir();
    test_pasar();
    test_programa_combinado();

    if (fallos == 0) {
        printf("OK: todos los tests del programa bytecode pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
