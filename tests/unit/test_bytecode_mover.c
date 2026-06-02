/*
 * Tests de mover y set_mtime (v1.111).
 *
 * archivo_mover: rename atomico via rename() POSIX o MoveFileExA con
 * MOVEFILE_REPLACE_EXISTING (Windows).
 *
 * archivo_set_mtime: utimes() POSIX o SetFileTime() Windows con
 * conversion epoch UNIX -> epoch Windows (1601).
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
        "test_mov_out.txt";
#else
        "/tmp/test_mov_out.txt";
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
    /* archivo_mover basico: source desaparece, destino aparece */
    {
        char out[512];
        ejecutar_capturando(
            "archivo_escribir(\"_t_mv_a.txt\", \"hola\")\n"
            "archivo_mover(\"_t_mv_a.txt\", \"_t_mv_b.txt\")\n"
            "imprimir(archivo_existe(\"_t_mv_a.txt\"))\n"
            "imprimir(archivo_existe(\"_t_mv_b.txt\"))\n"
            "imprimir(archivo_leer(\"_t_mv_b.txt\"))\n"
            "archivo_borrar(\"_t_mv_b.txt\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "falso") != NULL, "mover_origen_desaparece");
        AFIRMAR(strstr(out, "verdadero") != NULL, "mover_destino_aparece");
        AFIRMAR(strstr(out, "hola") != NULL, "mover_preserva_contenido");
    }

    /* mover sobrescribe destino existente (REPLACE_EXISTING) */
    {
        char out[512];
        ejecutar_capturando(
            "archivo_escribir(\"_t_mv_o.txt\", \"nuevo\")\n"
            "archivo_escribir(\"_t_mv_d.txt\", \"viejo\")\n"
            "archivo_mover(\"_t_mv_o.txt\", \"_t_mv_d.txt\")\n"
            "imprimir(archivo_leer(\"_t_mv_d.txt\"))\n"
            "archivo_borrar(\"_t_mv_d.txt\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "nuevo") != NULL && strstr(out, "viejo") == NULL,
                "mover_sobrescribe");
    }

    /* mover origen inexistente lanza ErrorDeIO */
    {
        char out[512];
        ejecutar_capturando(
            "intentar:\n"
            "    archivo_mover(\"_no_existe_mv\", \"_dst\")\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "mover_inexistente_lanza");
    }

    /* archivo_set_mtime: establecer timestamp pasado, leer con info */
    {
        char out[512];
        ejecutar_capturando(
            "archivo_escribir(\"_t_mt_a.txt\", \"x\")\n"
            /* 2020-01-01 00:00:00 UTC = 1577836800000 ms */
            "archivo_set_mtime(\"_t_mt_a.txt\", 1577836800000)\n"
            "imprimir(archivo_info(\"_t_mt_a.txt\")[\"mtime_epoch_ms\"])\n"
            "archivo_borrar(\"_t_mt_a.txt\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "1577836800000") != NULL, "set_mtime_2020");
    }

    /* set_mtime inexistente lanza ErrorDeIO */
    {
        char out[512];
        ejecutar_capturando(
            "intentar:\n"
            "    archivo_set_mtime(\"_no_existe_mt\", 0)\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "set_mtime_inexistente_lanza");
    }

    /* Wrappers archivos.mover / tocar / set_mtime */
    {
        char out[512];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.escribir(\"_t_mv_w.txt\", \"x\")\n"
            "archivos.mover(\"_t_mv_w.txt\", \"_t_mv_w2.txt\")\n"
            "imprimir(archivos.existe(\"_t_mv_w2.txt\"))\n"
            "archivos.set_mtime(\"_t_mv_w2.txt\", 1577836800000)\n"
            "imprimir(archivos.info(\"_t_mv_w2.txt\")[\"mtime_epoch_ms\"])\n"
            "archivos.tocar(\"_t_mv_w2.txt\")\n"
            "imprimir(archivos.info(\"_t_mv_w2.txt\")[\"mtime_epoch_ms\"] > 1700000000000)\n"
            "archivos.eliminar(\"_t_mv_w2.txt\")\n", out, sizeof(out));
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        AFIRMAR(n_v == 2, "wrappers_funcionan");
        AFIRMAR(strstr(out, "1577836800000") != NULL, "wrapper_set_mtime_2020");
    }

    /* Metodos en Ruta */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "importar archivos\n"
            "archivos.escribir(\"_t_mv_r.txt\", \"datos\")\n"
            "r = ruta.Ruta(\"_t_mv_r.txt\")\n"
            "nueva = r.mover(\"_t_mv_r2.txt\")\n"
            "imprimir(tipo(nueva))\n"
            "imprimir(nueva.existe())\n"
            "imprimir(r.existe())\n"
            "nueva.set_mtime(1577836800000)\n"
            "imprimir(nueva.mtime_ms())\n"
            "nueva.tocar()\n"
            "imprimir(nueva.mtime_ms() > 1700000000000)\n"
            "nueva.eliminar()\n", out, sizeof(out));
        AFIRMAR(strstr(out, "instancia") != NULL, "ruta_mover_devuelve_ruta");
        AFIRMAR(strstr(out, "1577836800000") != NULL, "ruta_set_mtime");
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 2 && n_f == 1, "ruta_movido_conteo");
    }

    /* mover puede mover entre directorios (mismo FS) */
    {
        char out[512];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.crear_arbol(\"_t_mv_dir\")\n"
            "archivos.escribir(\"_t_mv_src.txt\", \"x\")\n"
            "archivos.mover(\"_t_mv_src.txt\", \"_t_mv_dir/dst.txt\")\n"
            "imprimir(archivos.existe(\"_t_mv_dir/dst.txt\"))\n"
            "archivos.eliminar_arbol(\"_t_mv_dir\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "mover_entre_dirs");
    }

    if (fallos == 0) {
        printf("mover: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "mover: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
