/*
 * Tests de operaciones de filesystem v1.99:
 * archivo_borrar, directorio_borrar, archivo_info + wrappers.
 *
 * Usa un nombre de archivo unico por test para evitar colisiones
 * cuando ctest corre en paralelo.
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
        "test_fs2_out.txt";
#else
        "/tmp/test_fs2_out.txt";
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
    /* archivo_borrar: crea archivo, lo borra, comprueba */
    {
        char out[1024];
        ejecutar_capturando(
            "archivo_escribir(\"_t_fs2_a.txt\", \"hola\")\n"
            "imprimir(archivo_existe(\"_t_fs2_a.txt\"))\n"
            "archivo_borrar(\"_t_fs2_a.txt\")\n"
            "imprimir(archivo_existe(\"_t_fs2_a.txt\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "borrar_pre_existe");
        AFIRMAR(strstr(out, "falso") != NULL, "borrar_post_no_existe");
    }

    /* archivo_borrar: lanza ErrorDeIO si no existe */
    {
        char out[1024];
        ejecutar_capturando(
            "intentar:\n"
            "    archivo_borrar(\"_no_existe_xyz\")\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "borrar_inexistente_lanza");
    }

    /* directorio_borrar: crea, borra, comprueba */
    {
        char out[1024];
        ejecutar_capturando(
            "directorio_crear(\"_t_fs2_d\")\n"
            "imprimir(archivo_es_directorio(\"_t_fs2_d\"))\n"
            "directorio_borrar(\"_t_fs2_d\")\n"
            "imprimir(archivo_es_directorio(\"_t_fs2_d\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "rmdir_pre_existe");
        AFIRMAR(strstr(out, "falso") != NULL, "rmdir_post_no_existe");
    }

    /* directorio_borrar: lanza si no esta vacio */
    {
        char out[1024];
        ejecutar_capturando(
            "directorio_crear(\"_t_fs2_e\")\n"
            "archivo_escribir(\"_t_fs2_e/inner.txt\", \"x\")\n"
            "intentar:\n"
            "    directorio_borrar(\"_t_fs2_e\")\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"err vacio\")\n"
            "fin intentar\n"
            /* limpieza */
            "archivo_borrar(\"_t_fs2_e/inner.txt\")\n"
            "directorio_borrar(\"_t_fs2_e\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err vacio") != NULL, "rmdir_no_vacio_lanza");
    }

    /* archivo_info: campos basicos */
    {
        char out[1024];
        ejecutar_capturando(
            "archivo_escribir(\"_t_fs2_i.txt\", \"abcdef\")\n"
            "info = archivo_info(\"_t_fs2_i.txt\")\n"
            "imprimir(info[\"tamano\"])\n"
            "imprimir(info[\"es_archivo\"])\n"
            "imprimir(info[\"es_directorio\"])\n"
            "imprimir(info[\"mtime_epoch_ms\"] > 1000000000000)\n"
            "archivo_borrar(\"_t_fs2_i.txt\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "6") != NULL, "info_tamano_6_bytes");
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        /* es_archivo + mtime>... = 2 verdaderos */
        AFIRMAR(n_v == 2, "info_2_verdaderos");
        AFIRMAR(strstr(out, "falso") != NULL, "info_es_directorio_falso");
    }

    /* archivo_info: lanza si no existe */
    {
        char out[1024];
        ejecutar_capturando(
            "intentar:\n"
            "    archivo_info(\"_no_existe_info\")\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"err info\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err info") != NULL, "info_inexistente_lanza");
    }

    /* Wrappers de archivos.cor */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.escribir(\"_t_fs2_w.txt\", \"wrapper\")\n"
            "info = archivos.info(\"_t_fs2_w.txt\")\n"
            "imprimir(info[\"tamano\"])\n"
            "archivos.eliminar(\"_t_fs2_w.txt\")\n"
            "imprimir(archivos.existe(\"_t_fs2_w.txt\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "7") != NULL, "wrapper_info_tamano");
        AFIRMAR(strstr(out, "falso") != NULL, "wrapper_eliminar_funciona");
    }

    /* Ruta.eliminar / .info / .tamano / .mtime_ms */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "importar archivos\n"
            "archivos.escribir(\"_t_fs2_r.txt\", \"abcd\")\n"
            "r = ruta.Ruta(\"_t_fs2_r.txt\")\n"
            "imprimir(r.tamano())\n"
            "imprimir(r.mtime_ms() > 1000000000000)\n"
            "imprimir(r.info()[\"es_archivo\"])\n"
            "r.eliminar()\n"
            "imprimir(r.existe())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "ruta_tamano_4");
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        AFIRMAR(n_v == 2, "ruta_2_verdaderos");
        AFIRMAR(strstr(out, "falso") != NULL, "ruta_eliminada");
    }

    if (fallos == 0) {
        printf("fs2: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "fs2: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
