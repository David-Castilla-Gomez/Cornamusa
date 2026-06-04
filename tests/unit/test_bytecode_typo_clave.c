/*
 * Tests de typo suggestions en ErrorDeClave (v1.125).
 *
 * Antes: el inventario de fase 5 detecto que ErrorDeAtributo y
 * ErrorDeNombre invocaban sugerir_atributo_cercano / sugerir_nombre_cercano
 * con Levenshtein, pero ErrorDeClave (en OP_INDICE y OP_BORRAR_INDICE)
 * solo imprimia la clave repr sin sugerencia. v1.125 invoca
 * sugerir_nombre_cercano sobre el dict cuando la clave es VAL_CADENA.
 *
 * Cases:
 *   d["nomre"]    -> "(¿quisiste decir 'nombre'?)"  (Levenshtein 1)
 *   d["NOMBRE"]   -> "(¿quisiste decir 'nombre'?)"  (case-insensitive)
 *   d["xyz"]      -> sin sugerencia (Levenshtein > umbral)
 *   borrar d["x"] -> mismo comportamiento
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
        "test_typo_clave_out.txt";
#else
        "/tmp/test_typo_clave_out.txt";
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
    /* Typo Levenshtein 1: nomre -> nombre */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"nombre\": \"Ana\", \"edad\": 30}\n"
            "intentar:\n"
            "    imprimir(d[\"nomre\"])\n"
            "atrapar ErrorDeClave como e:\n"
            "    imprimir(e)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "quisiste decir 'nombre'") != NULL, "typo_nomre");
    }

    /* Case-insensitive: NOMBRE -> nombre */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"nombre\": 1}\n"
            "intentar:\n"
            "    imprimir(d[\"NOMBRE\"])\n"
            "atrapar ErrorDeClave como e:\n"
            "    imprimir(e)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "quisiste decir 'nombre'") != NULL, "case_insensitive");
    }

    /* Sin sugerencia para claves muy distintas */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"nombre\": 1, \"edad\": 2}\n"
            "intentar:\n"
            "    imprimir(d[\"xyz123\"])\n"
            "atrapar ErrorDeClave como e:\n"
            "    imprimir(e)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "quisiste decir") == NULL, "sin_sugerencia");
        AFIRMAR(strstr(out, "ErrorDeClave") != NULL, "sigue_dando_error");
    }

    /* borrar d[clave]: tambien sugiere */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"ciudad\": \"Sevilla\"}\n"
            "intentar:\n"
            "    borrar d[\"ciuda\"]\n"
            "atrapar ErrorDeClave como e:\n"
            "    imprimir(e)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "quisiste decir 'ciudad'") != NULL, "borrar_sugiere");
    }

    /* Clave no-cadena: no debe crashear ni sugerir cadenas */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1}\n"
            "intentar:\n"
            "    imprimir(d[42])\n"
            "atrapar ErrorDeClave como e:\n"
            "    imprimir(e)\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ErrorDeClave") != NULL, "clave_int_sin_sugerencia");
        AFIRMAR(strstr(out, "quisiste decir") == NULL, "int_no_sugiere_cadenas");
    }

    if (fallos == 0) {
        printf("typo_clave: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "typo_clave: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
