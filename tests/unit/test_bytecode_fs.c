/*
 * Tests de operaciones de filesystem (v1.97):
 * archivo_es_directorio, directorio_listar, obtener_cwd, directorio_crear.
 *
 * Se asume que se ejecuta desde la raiz del repo (WORKING_DIRECTORY en
 * tests/CMakeLists.txt), donde existen `examples/`, `stdlib/`,
 * `README.md`, `CMakeLists.txt`.
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
        "test_fs_out.txt";
#else
        "/tmp/test_fs_out.txt";
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
    /* archivo_es_directorio */
    {
        char out[1024];
        ejecutar_capturando(
            "imprimir(archivo_es_directorio(\"examples\"))\n"
            "imprimir(archivo_es_directorio(\"README.md\"))\n"
            "imprimir(archivo_es_directorio(\"no_existe_xyz\"))\n", out, sizeof(out));
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 1, "es_directorio_examples_v");
        AFIRMAR(n_f == 2, "es_directorio_readme_y_inexistente_f");
    }

    /* directorio_listar */
    {
        char out[4096];
        ejecutar_capturando(
            "ents = directorio_listar(\"examples\")\n"
            "imprimir(longitud(ents) > 10)\n"
            "imprimir(\"01_hola_mundo.cor\" en ents)\n", out, sizeof(out));
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        AFIRMAR(n_v == 2, "listar_examples_no_vacio_y_contiene_hola_mundo");
    }

    /* directorio_listar lanza ErrorDeIO si no es dir */
    {
        char out[1024];
        ejecutar_capturando(
            "intentar:\n"
            "    directorio_listar(\"no_existe_dir_xyz\")\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"err atrapado\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err atrapado") != NULL, "listar_dir_inexistente_lanza");
    }

    /* obtener_cwd retorna cadena no vacia */
    {
        char out[1024];
        ejecutar_capturando(
            "c = obtener_cwd()\n"
            "imprimir(tipo(c))\n"
            "imprimir(longitud(c) > 0)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "cadena") != NULL, "cwd_es_cadena");
        AFIRMAR(strstr(out, "verdadero") != NULL, "cwd_no_vacia");
    }

    /* directorio_crear: crea, comprueba, intenta crear de nuevo (falla),
     * limpia. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "nombre = \"_test_dir_v197\"\n"
            "directorio_crear(nombre)\n"
            "imprimir(archivos.es_directorio(nombre))\n"
            /* Segundo crear: falla porque ya existe */
            "intentar:\n"
            "    directorio_crear(nombre)\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"existe ya\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "directorio_crear_funciona");
        AFIRMAR(strstr(out, "existe ya") != NULL, "directorio_crear_falla_si_existe");

        /* Limpieza: borramos el directorio para que el test sea
         * idempotente. Como Cornamusa aun no tiene directorio_borrar,
         * lo hacemos via stdlib C. */
#ifdef _WIN32
        _rmdir("_test_dir_v197");
#else
        rmdir("_test_dir_v197");
#endif
    }

    /* Wrappers de archivos.cor */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "imprimir(archivos.es_directorio(\"examples\"))\n"
            "imprimir(archivos.es_directorio(\"README.md\"))\n"
            "ents = archivos.listar(\"examples\")\n"
            "imprimir(longitud(ents) > 10)\n"
            "imprimir(tipo(archivos.directorio_actual()))\n", out, sizeof(out));
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v >= 2, "archivos_wrappers_v");
        AFIRMAR(n_f >= 1, "archivos_wrappers_f");
        AFIRMAR(strstr(out, "cadena") != NULL, "archivos_directorio_actual_cadena");
    }

    /* Metodos de Ruta para FS */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "r = ruta.Ruta(\"examples\")\n"
            "imprimir(r.existe())\n"
            "imprimir(r.es_archivo())\n"
            "imprimir(r.es_directorio())\n"
            "f = ruta.Ruta(\"README.md\")\n"
            "imprimir(f.es_archivo())\n"
            "imprimir(f.es_directorio())\n"
            "imprimir(f.existe())\n", out, sizeof(out));
        /* examples: existe=V, es_archivo=F, es_directorio=V
         * README.md: es_archivo=V, es_directorio=F, existe=V */
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 4, "ruta_fs_4_verdaderos");
        AFIRMAR(n_f == 2, "ruta_fs_2_falsos");
    }

    /* ruta.cwd() devuelve Ruta */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "c = ruta.cwd()\n"
            "imprimir(tipo(c))\n"
            "imprimir(longitud(c.cadena()) > 0)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "instancia") != NULL, "cwd_es_ruta");
        AFIRMAR(strstr(out, "verdadero") != NULL, "cwd_ruta_no_vacia");
    }

    /* Ruta.listar y listar_rutas */
    {
        char out[2048];
        ejecutar_capturando(
            "importar ruta\n"
            "r = ruta.Ruta(\"examples\")\n"
            "imprimir(longitud(r.listar()) > 10)\n"
            "rutas = r.listar_rutas()\n"
            "imprimir(longitud(rutas) > 10)\n"
            "imprimir(rutas[0].es_archivo())\n", out, sizeof(out));
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        AFIRMAR(n_v == 3, "ruta_listar_y_listar_rutas");
    }

    if (fallos == 0) {
        printf("fs: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "fs: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
