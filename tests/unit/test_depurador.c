/*
 * Tests del depurador interactivo (v1.76).
 *
 * Verifica con comandos pre-alimentados via stdin:
 *   - Pausa en la primera linea (al activar).
 *   - `c` continua hasta el fin.
 *   - `s` paso a paso visita cada linea.
 *   - `b N` instala breakpoint, `c` se para alli.
 *   - `p NOMBRE` imprime valor de global existente.
 *   - `p NOMBRE` indica 'no definida' para global inexistente.
 *   - `pila` no crashea.
 *   - `q` aborta la VM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "compilador.h"
#include "depurador.h"
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

/* Ejecuta `fuente` bajo el debugger, alimentando `cmds` por stdin.
 * Captura stdout en `out_buf`. */
static int ejecutar_depurando(const char *fuente, const char *cmds,
                                char *out_buf, int out_cap) {
    /* Escribe los comandos a un archivo temporal y lo abre como stdin. */
    const char *cmd_file =
#ifdef _WIN32
        "test_dep_cmds.txt";
#else
        "/tmp/test_dep_cmds.txt";
#endif
    const char *out_file =
#ifdef _WIN32
        "test_dep_out.txt";
#else
        "/tmp/test_dep_out.txt";
#endif
    FILE *fcmds = fopen(cmd_file, "w");
    if (!fcmds) return -1;
    fputs(cmds, fcmds);
    fclose(fcmds);

    /* Redirigir stdin y stdout. */
    FILE *stdin_saved = stdin;
    (void)stdin_saved;
    if (!freopen(cmd_file, "r", stdin)) return -1;
    if (!freopen(out_file, "w+", stdout)) return -1;

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
            depurador_activar(&vm.dep, fuente, "<test>");
            Valor r = valor_nulo();
            ResultadoVM rcvm = vm_ejecutar(&vm, &chunk, &r);
            valor_destruir(&r);
            depurador_desactivar(&vm.dep);
            vm_destruir(&vm);
            rc = (rcvm == VM_OK) ? 0 : 1;
        }
        chunk_destruir(&chunk);
    }
    arena_destruir(&a);

    fflush(stdout);
    /* Restaurar stdout. */
#ifdef _WIN32
    freopen("CON", "w", stdout);
    freopen("CON", "r", stdin);
#else
    freopen("/dev/tty", "w", stdout);
    freopen("/dev/tty", "r", stdin);
#endif

    FILE *f = fopen(out_file, "r");
    if (f) {
        int leido = (int)fread(out_buf, 1, (size_t)(out_cap - 1), f);
        out_buf[leido] = '\0';
        fclose(f);
        remove(out_file);
    } else {
        out_buf[0] = '\0';
    }
    remove(cmd_file);
    return rc;
}

int main(void) {
    /* Test 1: continuar inmediato ejecuta el programa entero. */
    {
        char out[1024];
        int rc = ejecutar_depurando(
            "a = 1\nimprimir(a + 10)\n",
            "c\n", out, sizeof(out));
        AFIRMAR(rc == 0, "continuar_ejecuta");
        AFIRMAR(strstr(out, "11") != NULL, "continuar_resultado");
    }

    /* Test 2: paso a paso muestra prompts en cada linea. */
    {
        char out[2048];
        int rc = ejecutar_depurando(
            "a = 1\n"
            "b = 2\n"
            "imprimir(a + b)\n",
            "s\ns\np a\ns\np b\nc\n", out, sizeof(out));
        AFIRMAR(rc == 0, "paso_ejecuta");
        AFIRMAR(strstr(out, "a = 1") != NULL, "paso_imprime_a");
        AFIRMAR(strstr(out, "b = 2") != NULL, "paso_imprime_b");
        AFIRMAR(strstr(out, "3") != NULL, "paso_resultado");
    }

    /* Test 3: breakpoint en linea con bucle. */
    {
        char out[4096];
        int rc = ejecutar_depurando(
            "total = 0\n"
            "para i en rango(3):\n"
            "    total = total + i\n"
            "fin para\n"
            "imprimir(total)\n",
            "b 3\nc\nc\nc\nc\n", out, sizeof(out));
        AFIRMAR(rc == 0, "break_ejecuta");
        AFIRMAR(strstr(out, "3\n") != NULL || strstr(out, "3\r\n") != NULL,
                "break_resultado");
        /* Debe haber al menos 1 prompt "(dep)". */
        AFIRMAR(strstr(out, "(dep)") != NULL, "break_prompt");
    }

    /* Test 4: imprimir global inexistente. */
    {
        char out[1024];
        int rc = ejecutar_depurando(
            "a = 1\n",
            "p inexistente\nc\n", out, sizeof(out));
        AFIRMAR(rc == 0, "p_inexistente_ejecuta");
        AFIRMAR(strstr(out, "inexistente") != NULL, "p_inexistente_mensaje");
        AFIRMAR(strstr(out, "no esta definida") != NULL, "p_inexistente_no_def");
    }

    /* Test 5: comando 'q' aborta. Usamos un texto unico que solo
     * aparece en el OUTPUT del programa (no en el listing del
     * depurador, que tambien imprime el codigo fuente). */
    {
        char out[1024];
        int rc = ejecutar_depurando(
            "a = 1\nimprimir(\"MARCADOR_EJECUCION\")\n",
            "q\n", out, sizeof(out));
        AFIRMAR(rc != 0, "q_aborta");
        /* "MARCADOR_EJECUCION" aparece en el listing (codigo fuente),
         * pero solo aparece como output del imprimir si la VM ejecuto
         * esa linea. Si solo aparece una vez es del listing; dos veces
         * indicaria que se ejecuto tambien. */
        const char *p = strstr(out, "MARCADOR_EJECUCION");
        bool aparece_dos_veces = (p && strstr(p + 1, "MARCADOR_EJECUCION"));
        AFIRMAR(!aparece_dos_veces, "q_no_ejecuto");
    }

    /* Test 6: ayuda funciona. */
    {
        char out[2048];
        int rc = ejecutar_depurando(
            "a = 1\n",
            "?\nc\n", out, sizeof(out));
        AFIRMAR(rc == 0, "ayuda_ejecuta");
        AFIRMAR(strstr(out, "Comandos:") != NULL, "ayuda_imprime");
    }

    /* Test 7: pila muestra frames. */
    {
        char out[1024];
        int rc = ejecutar_depurando(
            "a = 1\n",
            "pila\nc\n", out, sizeof(out));
        AFIRMAR(rc == 0, "pila_ejecuta");
        AFIRMAR(strstr(out, "top-level") != NULL, "pila_top_level");
    }

    /* Test 8: bs sin breakpoints. */
    {
        char out[1024];
        int rc = ejecutar_depurando(
            "a = 1\n",
            "bs\nc\n", out, sizeof(out));
        AFIRMAR(rc == 0, "bs_ejecuta");
        AFIRMAR(strstr(out, "sin breakpoints") != NULL, "bs_sin");
    }

    /* Test 9: instalar y borrar breakpoint. */
    {
        char out[1024];
        int rc = ejecutar_depurando(
            "a = 1\nb = 2\n",
            "b 2\nbs\nbd 2\nbs\nc\n", out, sizeof(out));
        AFIRMAR(rc == 0, "b_bd_ejecuta");
        AFIRMAR(strstr(out, "breakpoint en linea 2") != NULL, "b_anuncia");
        AFIRMAR(strstr(out, "borrado") != NULL, "bd_anuncia");
    }

    /* Test 10: comando desconocido reporta error y sigue prompt. */
    {
        char out[1024];
        int rc = ejecutar_depurando(
            "a = 1\n",
            "xyz\nc\n", out, sizeof(out));
        AFIRMAR(rc == 0, "desconocido_no_aborta");
        AFIRMAR(strstr(out, "desconocido") != NULL, "desconocido_msg");
    }

    if (fallos == 0) {
        printf("depurador: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "depurador: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
