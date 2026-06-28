/*
 * Tests: los errores de importación son atrapables (v1.209).
 *
 * Antes `importar modulo_inexistente` lanzaba ErrorDeImportacion pero
 * mataba el programa (return VM_ERROR_RUNTIME directo). Ahora
 * OP_IMPORTAR y OP_IMPORTAR_PARA_DESDE usan RAISE_OR_DIE, así que un
 * `atrapar ErrorDeImportacion` (o `Excepcion`) activo lo captura.
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
        "test_import_atrap_out.txt";
#else
        "/tmp/test_import_atrap_out.txt";
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

int main(void) {
    /* importar inexistente, atrapado por ErrorDeImportacion como e; el
     * programa continúa. */
    {
        char out[512];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    importar modulo_que_no_existe_xyz\n"
            "atrapar ErrorDeImportacion como e:\n"
            "    imprimir(\"A\", cadena(e))\n"
            "fin intentar\n"
            "imprimir(\"SIGUE\")\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "atrapar_rc");
        AFIRMAR(strstr(out, "A ErrorDeImportacion") != NULL, "atrapar_clase");
        AFIRMAR(strstr(out, "SIGUE") != NULL, "atrapar_continua");
    }

    /* Patrón de fallback: la bandera de éxito queda en falso. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "exito = falso\n"
            "intentar:\n"
            "    importar opcional_inexistente\n"
            "    exito = verdadero\n"
            "atrapar ErrorDeImportacion:\n"
            "    imprimir(\"FALLBACK\")\n"
            "fin intentar\n"
            "imprimir(\"E\", exito)\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "fallback_rc");
        AFIRMAR(strstr(out, "FALLBACK") != NULL, "fallback_corre");
        AFIRMAR(strstr(out, "E falso") != NULL, "fallback_no_exito");
    }

    /* atrapar Excepcion (catch-all) también captura el error de import. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    importar otro_inexistente\n"
            "atrapar Excepcion:\n"
            "    imprimir(\"CATCHALL\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "catchall_rc");
        AFIRMAR(strstr(out, "CATCHALL") != NULL, "catchall_ok");
    }

    /* `desde X importar Y` con módulo inexistente también es atrapable. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    desde mod_inexistente importar algo\n"
            "atrapar ErrorDeImportacion:\n"
            "    imprimir(\"DESDE\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "desde_rc");
        AFIRMAR(strstr(out, "DESDE") != NULL, "desde_atrapado");
    }

    /* SIN atrapar: el error sigue matando el programa (rc 70). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "importar sin_atrapar_inexistente\n"
            "imprimir(\"NO_DEBE_LLEGAR\")\n",
            out, sizeof(out));
        AFIRMAR(rc == 70, "sin_atrapar_rc");
        AFIRMAR(strstr(out, "NO_DEBE_LLEGAR") == NULL, "sin_atrapar_para");
    }

    /* Repetidos en bucle dentro de un intentar: no debe acumular estado
     * roto (varios import fallidos atrapados seguidos). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "cuenta = 0\n"
            "para i en rango(5):\n"
            "    intentar:\n"
            "        importar inexistente_bucle\n"
            "    atrapar ErrorDeImportacion:\n"
            "        cuenta = cuenta + 1\n"
            "    fin intentar\n"
            "fin para\n"
            "imprimir(\"CUENTA\", cuenta)\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "bucle_rc");
        AFIRMAR(strstr(out, "CUENTA 5") != NULL, "bucle_cuenta");
    }

    if (fallos == 0) {
        printf("import_atrapable: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "import_atrapable: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
