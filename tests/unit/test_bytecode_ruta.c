/*
 * Tests de stdlib/ruta.cor (v1.94).
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
        "test_ruta_out.txt";
#else
        "/tmp/test_ruta_out.txt";
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
    /* nombre, tronco, extension */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.nombre(\"/a/b/c.txt\"))\n"
            "imprimir(ruta.tronco(\"foo.cor\"))\n"
            "imprimir(ruta.extension(\"x.tar.gz\"))\n"
            "imprimir(ruta.extension(\"sinpunto\"))\n"
            "imprimir(ruta.tronco(\"sinpunto\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "c.txt") != NULL, "nombre_normal");
        AFIRMAR(strstr(out, "foo") != NULL, "tronco");
        AFIRMAR(strstr(out, ".gz") != NULL, "extension_doble");
        AFIRMAR(strstr(out, "sinpunto") != NULL, "sin_extension");
    }

    /* padre */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.padre(\"/a/b/c\"))\n"
            "imprimir(ruta.padre(\"/x\"))\n"
            "imprimir(ruta.padre(\"/\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "/a/b") != NULL, "padre_normal");
        AFIRMAR(strstr(out, "/\n") != NULL || strstr(out, "/\r\n") != NULL, "padre_raiz_o_x");
    }

    /* es_absoluta */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.es_absoluta(\"/etc\"))\n"
            "imprimir(ruta.es_absoluta(\"rel\"))\n"
            "imprimir(ruta.es_absoluta(\"C:/Users\"))\n"
            "imprimir(ruta.es_absoluta(\"\"))\n", out, sizeof(out));
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 2 && n_f == 2, "es_absoluta_4casos");
    }

    /* unir_partes */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.unir_partes([\"a\", \"b\", \"c\"]))\n"
            "imprimir(ruta.unir_partes([\"/a\", \"b\"]))\n"
            "imprimir(ruta.unir_partes([\"a\", \"/b\"]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "a/b/c") != NULL, "unir_relativo");
        AFIRMAR(strstr(out, "/a/b") != NULL, "unir_con_abs_inicial");
        AFIRMAR(strstr(out, "/b") != NULL, "abs_reset");
    }

    /* normalizar */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.normalizar(\"a/./b/../c\"))\n"
            "imprimir(ruta.normalizar(\"x/y/..\"))\n"
            "imprimir(ruta.normalizar(\"\"))\n"
            "imprimir(ruta.normalizar(\"./.\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "a/c") != NULL, "normalizar_dotdot");
        AFIRMAR(strstr(out, ".\n") != NULL || strstr(out, ".\r\n") != NULL, "normalizar_vacio_es_dot");
    }

    /* Clase Ruta: getters basicos */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "r = ruta.Ruta(\"/home/david/x.txt\")\n"
            "imprimir(r.nombre())\n"
            "imprimir(r.tronco())\n"
            "imprimir(r.extension())\n"
            "imprimir(r.padre().cadena())\n"
            "imprimir(r.absoluta())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "x.txt") != NULL, "ruta_nombre");
        AFIRMAR(strstr(out, "x") != NULL, "ruta_tronco");
        AFIRMAR(strstr(out, ".txt") != NULL, "ruta_extension");
        AFIRMAR(strstr(out, "/home/david") != NULL, "ruta_padre");
        AFIRMAR(strstr(out, "verdadero") != NULL, "ruta_absoluta");
    }

    /* unir encadenado */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "r = ruta.Ruta(\"/etc\").unir(\"nginx\").unir(\"conf.d\")\n"
            "imprimir(r.cadena())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "/etc/nginx/conf.d") != NULL, "unir_encadenado");
    }

    /* unir con Ruta como argumento */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "a = ruta.Ruta(\"/a\")\n"
            "b = ruta.Ruta(\"b/c\")\n"
            "imprimir(a.unir(b).cadena())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "/a/b/c") != NULL, "unir_con_ruta");
    }

    /* con_nombre / con_extension */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "r = ruta.Ruta(\"/home/x.txt\")\n"
            "imprimir(r.con_extension(\".md\").cadena())\n"
            "imprimir(r.con_nombre(\"y.cor\").cadena())\n"
            "imprimir(r.con_extension(\"\").cadena())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "/home/x.md") != NULL, "con_extension");
        AFIRMAR(strstr(out, "/home/y.cor") != NULL, "con_nombre");
        AFIRMAR(strstr(out, "/home/x\n") != NULL ||
                strstr(out, "/home/x\r\n") != NULL, "ext_vacia_quita");
    }

    /* Igualdad por valor */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "a = ruta.Ruta(\"/x\")\n"
            "b = ruta.Ruta(\"/x\")\n"
            "c = ruta.Ruta(\"/y\")\n"
            "imprimir(a == b)\n"
            "imprimir(a == c)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "igualdad_valor");
        AFIRMAR(strstr(out, "falso") != NULL, "desigualdad");
    }

    /* normaliza separadores Windows */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "r = ruta.Ruta(\"C:\\\\Users\\\\david\")\n"
            "imprimir(r.cadena())\n"
            "imprimir(r.absoluta())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "C:/Users/david") != NULL, "normaliza_backslash");
        AFIRMAR(strstr(out, "verdadero") != NULL, "windows_es_absoluta");
    }

    /* partes para ruta absoluta */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.Ruta(\"/a/b/c\").partes())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[\"/\", \"a\", \"b\", \"c\"]") != NULL ||
                strstr(out, "['/', 'a', 'b', 'c']") != NULL, "partes_absoluta");
    }

    /* vacia */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.Ruta(\"\").vacia())\n"
            "imprimir(ruta.Ruta(\"/x\").vacia())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "vacia_v");
        AFIRMAR(strstr(out, "falso") != NULL, "vacia_f");
    }

    if (fallos == 0) {
        printf("ruta: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "ruta: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
