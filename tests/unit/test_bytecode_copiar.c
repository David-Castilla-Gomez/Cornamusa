/*
 * Tests de copia de archivos y arboles (v1.105).
 *
 * archivo_copiar (nativa C, fread/fwrite con buffer 64 KiB).
 * archivos.copiar_arbol (pure-Cornamusa, recursivo, con mkdir -p
 * implicito en el destino).
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
        "test_cp_out.txt";
#else
        "/tmp/test_cp_out.txt";
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
    /* archivo_copiar: contenido y tamano coinciden */
    {
        char out[1024];
        ejecutar_capturando(
            "archivo_escribir(\"_t_cp_a.txt\", \"contenido original\")\n"
            "archivo_copiar(\"_t_cp_a.txt\", \"_t_cp_b.txt\")\n"
            "imprimir(archivo_leer(\"_t_cp_b.txt\"))\n"
            "i1 = archivo_info(\"_t_cp_a.txt\")\n"
            "i2 = archivo_info(\"_t_cp_b.txt\")\n"
            "imprimir(i1[\"tamano\"] == i2[\"tamano\"])\n"
            /* cleanup */
            "archivo_borrar(\"_t_cp_a.txt\")\n"
            "archivo_borrar(\"_t_cp_b.txt\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "contenido original") != NULL, "copia_contenido");
        AFIRMAR(strstr(out, "verdadero") != NULL, "copia_tamanos_coinciden");
    }

    /* archivo_copiar: destino se trunca si existia */
    {
        char out[1024];
        ejecutar_capturando(
            "archivo_escribir(\"_t_cp_o.txt\", \"corto\")\n"
            "archivo_escribir(\"_t_cp_d.txt\", \"contenido mucho mas largo previo\")\n"
            "archivo_copiar(\"_t_cp_o.txt\", \"_t_cp_d.txt\")\n"
            "imprimir(archivo_leer(\"_t_cp_d.txt\"))\n"
            "archivo_borrar(\"_t_cp_o.txt\")\n"
            "archivo_borrar(\"_t_cp_d.txt\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "corto") != NULL && strstr(out, "previo") == NULL,
                "destino_truncado");
    }

    /* archivo_copiar: origen inexistente lanza ErrorDeIO */
    {
        char out[1024];
        ejecutar_capturando(
            "intentar:\n"
            "    archivo_copiar(\"_no_existe.txt\", \"dest.txt\")\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"err origen\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err origen") != NULL, "origen_inexistente_lanza");
    }

    /* Wrapper archivos.copiar */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.escribir(\"_t_cp_w.txt\", \"wrap\")\n"
            "archivos.copiar(\"_t_cp_w.txt\", \"_t_cp_w2.txt\")\n"
            "imprimir(archivos.leer(\"_t_cp_w2.txt\"))\n"
            "archivos.eliminar(\"_t_cp_w.txt\")\n"
            "archivos.eliminar(\"_t_cp_w2.txt\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "wrap") != NULL, "archivos_copiar_wrap");
    }

    /* copiar_arbol: arbol pequeño con archivos en distintos niveles */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.crear_arbol(\"_t_cp_src/sub\")\n"
            "archivos.escribir(\"_t_cp_src/r.txt\", \"raiz\")\n"
            "archivos.escribir(\"_t_cp_src/sub/h.txt\", \"hoja\")\n"
            "archivos.copiar_arbol(\"_t_cp_src\", \"_t_cp_dst\")\n"
            "imprimir(archivos.leer(\"_t_cp_dst/r.txt\"))\n"
            "imprimir(archivos.leer(\"_t_cp_dst/sub/h.txt\"))\n"
            "archivos.eliminar_arbol(\"_t_cp_src\")\n"
            "archivos.eliminar_arbol(\"_t_cp_dst\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "raiz") != NULL, "arbol_copia_raiz");
        AFIRMAR(strstr(out, "hoja") != NULL, "arbol_copia_hoja");
    }

    /* copiar_arbol: crea padres del destino (mkdir -p implicito) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.crear_arbol(\"_t_cp_src2\")\n"
            "archivos.escribir(\"_t_cp_src2/x.txt\", \"x\")\n"
            "archivos.copiar_arbol(\"_t_cp_src2\", \"_t_cp_padres/profundo/dest\")\n"
            "imprimir(archivos.leer(\"_t_cp_padres/profundo/dest/x.txt\"))\n"
            "archivos.eliminar_arbol(\"_t_cp_src2\")\n"
            "archivos.eliminar_arbol(\"_t_cp_padres\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "x") != NULL, "arbol_crea_padres_destino");
    }

    /* copiar_arbol con archivo regular hace fallback a copiar */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.escribir(\"_t_cp_f.txt\", \"file fallback\")\n"
            "archivos.copiar_arbol(\"_t_cp_f.txt\", \"_t_cp_g.txt\")\n"
            "imprimir(archivos.leer(\"_t_cp_g.txt\"))\n"
            "archivos.eliminar(\"_t_cp_f.txt\")\n"
            "archivos.eliminar(\"_t_cp_g.txt\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "file fallback") != NULL, "arbol_fallback_archivo");
    }

    /* copiar_arbol con origen inexistente lanza */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "intentar:\n"
            "    archivos.copiar_arbol(\"_no_existe_cp\", \"_dst\")\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "arbol_origen_inexistente");
    }

    /* copiar_arbol con cadena vacia lanza ErrorDeValor */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "intentar:\n"
            "    archivos.copiar_arbol(\"\", \"dst\")\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err vacia\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err vacia") != NULL, "arbol_vacia_lanza");
    }

    /* Metodos en Ruta */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "importar archivos\n"
            "archivos.escribir(\"_t_cp_r.txt\", \"ruta copia\")\n"
            "r = ruta.Ruta(\"_t_cp_r.txt\")\n"
            "dest = r.copiar(\"_t_cp_r2.txt\")\n"
            "imprimir(tipo(dest))\n"
            "imprimir(archivos.leer(dest.cadena()))\n"
            "archivos.eliminar(\"_t_cp_r.txt\")\n"
            "archivos.eliminar(\"_t_cp_r2.txt\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "instancia") != NULL, "ruta_copiar_devuelve_ruta");
        AFIRMAR(strstr(out, "ruta copia") != NULL, "ruta_copiar_contenido");
    }

    if (fallos == 0) {
        printf("copiar: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "copiar: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
