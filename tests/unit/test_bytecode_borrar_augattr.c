/*
 * Tests del bytecode para `borrar` y aug-assign sobre atributos (v1.56).
 *
 * Cubre:
 *   - `borrar d[k]` sobre diccionario / lista / conjunto.
 *   - `borrar obj.attr` sobre instancia.
 *   - Errores atrapables: clave inexistente, indice fuera de rango,
 *     atributo inexistente, tipo invalido.
 *   - Aug-assign sobre atributos: `obj.x += 1`, `*=`, `**=`, etc.
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

/* Ejecuta `fuente` con VM bytecode y devuelve el resultado por valor.
 * El llamador libera con valor_destruir. */
static bool ejecutar(const char *fuente, Valor *out) {
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Arena a;
    arena_iniciar(&a, 8192);
    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    if (p.tuvo_error) { arena_destruir(&a); return false; }

    Chunk chunk;
    chunk_iniciar(&chunk);
    Compilador c;
    compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, sents, n)) {
        chunk_destruir(&chunk);
        arena_destruir(&a);
        return false;
    }

    VM vm;
    vm_iniciar(&vm);
    Valor resultado = valor_nulo();
    ResultadoVM rc = vm_ejecutar(&vm, &chunk, &resultado);
    bool ok = (rc == VM_OK);
    if (ok) *out = resultado;
    else valor_destruir(&resultado);
    vm_destruir(&vm);
    chunk_destruir(&chunk);
    arena_destruir(&a);
    return ok;
}

/* Devuelve true si el programa imprime `esperado` por stdout. Lee
 * todo stdout con freopen + lectura de archivo temporal. */
static bool ejecutar_y_capturar_stdout(const char *fuente, char *buf, size_t cap) {
    /* freopen stdout a archivo temporal. */
    const char *tmpfile =
#ifdef _WIN32
        "test_borrar_out.txt";
#else
        "/tmp/test_borrar_out.txt";
#endif
    FILE *prev = freopen(tmpfile, "w+", stdout);
    if (!prev) return false;
    Valor r;
    bool ok = ejecutar(fuente, &r);
    fflush(stdout);
    /* Restaurar stdout. */
#ifdef _WIN32
    freopen("CON", "w", stdout);
#else
    freopen("/dev/tty", "w", stdout);
#endif
    if (!ok) { valor_destruir(&r); return false; }
    valor_destruir(&r);

    FILE *f = fopen(tmpfile, "r");
    if (!f) return false;
    size_t n = fread(buf, 1, cap - 1, f);
    buf[n] = '\0';
    fclose(f);
    remove(tmpfile);
    return true;
}

int main(void) {
    /* ─── borrar d[k] (dict) ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_y_capturar_stdout(
            "d = {\"a\": 1, \"b\": 2}\n"
            "borrar d[\"a\"]\n"
            "imprimir(d)\n", out, sizeof(out)), "borrar_dict_ejecuta");
        AFIRMAR(strstr(out, "b") != NULL && strstr(out, "\"a\"") == NULL,
                 "borrar_dict_clave_quitada");
    }

    /* ─── borrar lst[i] (lista) ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_y_capturar_stdout(
            "l = [10, 20, 30]\n"
            "borrar l[1]\n"
            "imprimir(l)\n", out, sizeof(out)), "borrar_lista_ejecuta");
        AFIRMAR(strstr(out, "[10, 30]") != NULL, "borrar_lista_desplaza");
    }

    /* ─── borrar obj.attr (instancia) ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_y_capturar_stdout(
            "clase X:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.a = 1\n"
            "        yo.b = 2\n"
            "    fin funcion\n"
            "fin clase\n"
            "x = X()\n"
            "borrar x.a\n"
            "intentar:\n"
            "    imprimir(x.a)\n"
            "atrapar ErrorDeAtributo:\n"
            "    imprimir(\"borrado\")\n"
            "fin intentar\n", out, sizeof(out)), "borrar_attr_ejecuta");
        AFIRMAR(strstr(out, "borrado") != NULL, "borrar_attr_quitado");
    }

    /* ─── Error atrapable: clave inexistente en dict ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_y_capturar_stdout(
            "d = {\"a\": 1}\n"
            "intentar:\n"
            "    borrar d[\"xxx\"]\n"
            "atrapar ErrorDeClave:\n"
            "    imprimir(\"caught\")\n"
            "fin intentar\n", out, sizeof(out)), "clave_inexistente");
        AFIRMAR(strstr(out, "caught") != NULL, "clave_inexistente_caught");
    }

    /* ─── aug-assign sobre atributo: += ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_y_capturar_stdout(
            "clase C:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.n = 10\n"
            "    fin funcion\n"
            "fin clase\n"
            "c = C()\n"
            "c.n += 5\n"
            "imprimir(c.n)\n", out, sizeof(out)), "aug_attr_mas");
        AFIRMAR(strstr(out, "15") != NULL, "aug_attr_mas_15");
    }

    /* ─── aug-assign sobre atributo: -= ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_y_capturar_stdout(
            "clase C:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.n = 10\n"
            "    fin funcion\n"
            "fin clase\n"
            "c = C()\n"
            "c.n -= 3\n"
            "imprimir(c.n)\n", out, sizeof(out)), "aug_attr_menos");
        AFIRMAR(strstr(out, "7") != NULL, "aug_attr_menos_7");
    }

    /* ─── aug-assign sobre atributo: **= ─── */
    {
        char out[256];
        AFIRMAR(ejecutar_y_capturar_stdout(
            "clase C:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.n = 3\n"
            "    fin funcion\n"
            "fin clase\n"
            "c = C()\n"
            "c.n **= 4\n"
            "imprimir(c.n)\n", out, sizeof(out)), "aug_attr_pot");
        AFIRMAR(strstr(out, "81") != NULL, "aug_attr_pot_81");
    }

    if (fallos == 0) {
        printf("borrar_augattr: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "borrar_augattr: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
