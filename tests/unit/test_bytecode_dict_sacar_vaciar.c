/*
 * Tests de `dicc.sacar(k[, default])` y `dicc.vaciar()` (v1.151).
 *
 * Python `dict.pop(k[, default])` y `dict.clear()`. Cornamusa
 * tenia `borrar d[k]` para borrar una clave (sin devolver valor)
 * y `d.actualizar(otro)` desde v1.150, pero no:
 *
 *   d.sacar(k)              — quita y devuelve valor; ErrorDeClave
 *                             si no existe.
 *   d.sacar(k, default)     — como arriba pero default si no existe.
 *   d.vaciar()              — elimina todas las entradas in-place.
 *
 * Sin cambios a bytecode ni VM. Las nativas usan la API
 * `dicc_quitar` existente y manipulan `entradas` directamente
 * para vaciar.
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
        "test_dict_sv_out.txt";
#else
        "/tmp/test_dict_sv_out.txt";
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
    /* sacar con clave presente: devuelve valor y quita la entrada */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1, \"b\": 2, \"c\": 3}\n"
            "v = d.sacar(\"b\")\n"
            "imprimir(v)\n"
            "imprimir(longitud(d))\n"
            "imprimir(\"b\" en d)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "sacar_valor");
        AFIRMAR(strstr(out, "falso") != NULL, "sacar_quita_clave");
    }

    /* sacar con default: devuelve default si no existe */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"x\": 10}\n"
            "v = d.sacar(\"ausente\", -1)\n"
            "imprimir(v)\n"
            "imprimir(longitud(d))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-1") != NULL, "sacar_default_valor");
        AFIRMAR(strstr(out, "1") != NULL, "sacar_default_no_muta");
    }

    /* sacar sin default sobre clave ausente lanza ErrorDeClave */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    {\"a\": 1}.sacar(\"ausente\")\n"
            "atrapar ErrorDeClave:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "sacar_sin_default_lanza");
    }

    /* sacar con default de tipo complejo (lista) */
    {
        char out[256];
        ejecutar_capturando(
            "d = {}\n"
            "v = d.sacar(\"k\", [1, 2, 3])\n"
            "imprimir(v)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "sacar_default_lista");
    }

    /* vaciar: dict no vacio queda vacio */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1, \"b\": 2}\n"
            "d.vaciar()\n"
            "imprimir(longitud(d))\n"
            "imprimir(d)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "vaciar_longitud");
        AFIRMAR(strstr(out, "{}") != NULL, "vaciar_repr");
    }

    /* vaciar sobre dict ya vacio no crashea */
    {
        char out[256];
        ejecutar_capturando(
            "d = {}\n"
            "d.vaciar()\n"
            "imprimir(longitud(d))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "vaciar_ya_vacio");
    }

    /* Usar dict tras vaciar — sigue funcionando como dict normal */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1, \"b\": 2}\n"
            "d.vaciar()\n"
            "d[\"nuevo\"] = 99\n"
            "imprimir(d[\"nuevo\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "99") != NULL, "vaciar_y_reusar");
    }

    /* sacar rechaza no-diccionario */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    (1, 2).sacar(0)\n"   /* tupla no tiene `sacar` */
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "sacar_rechaza_tupla");
    }

    if (fallos == 0) {
        printf("dict_sv: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "dict_sv: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
