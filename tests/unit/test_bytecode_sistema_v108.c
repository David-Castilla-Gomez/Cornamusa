/*
 * Tests de identidad del sistema (v1.108): usuario_actual, hostname,
 * directorio_temporal + wrappers en stdlib/sistema.
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
        "test_sys108_out.txt";
#else
        "/tmp/test_sys108_out.txt";
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
    /* usuario_actual devuelve cadena no vacia */
    {
        char out[1024];
        ejecutar_capturando(
            "u = usuario_actual()\n"
            "imprimir(tipo(u))\n"
            "imprimir(longitud(u) > 0)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "cadena") != NULL, "usuario_es_cadena");
        AFIRMAR(strstr(out, "verdadero") != NULL, "usuario_no_vacio");
    }

    /* hostname devuelve cadena no vacia */
    {
        char out[1024];
        ejecutar_capturando(
            "h = hostname()\n"
            "imprimir(tipo(h))\n"
            "imprimir(longitud(h) > 0)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "cadena") != NULL, "hostname_es_cadena");
        AFIRMAR(strstr(out, "verdadero") != NULL, "hostname_no_vacio");
    }

    /* directorio_temporal devuelve cadena, separadores normalizados */
    {
        char out[1024];
        ejecutar_capturando(
            "importar cadenas\n"
            "t = directorio_temporal()\n"
            "imprimir(tipo(t))\n"
            "imprimir(longitud(t) > 0)\n"
            "imprimir(cadenas.contiene(t, \"\\\\\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "cadena") != NULL, "temp_es_cadena");
        AFIRMAR(strstr(out, "verdadero") != NULL, "temp_no_vacio");
        AFIRMAR(strstr(out, "falso") != NULL, "temp_sin_backslash");
    }

    /* directorio_temporal apunta a un directorio existente */
    {
        char out[1024];
        ejecutar_capturando(
            "t = directorio_temporal()\n"
            "imprimir(archivo_es_directorio(t))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "temp_es_dir_real");
    }

    /* Wrappers en stdlib/sistema */
    {
        char out[1024];
        ejecutar_capturando(
            "importar sistema\n"
            "imprimir(tipo(sistema.usuario()))\n"
            "imprimir(tipo(sistema.host()))\n"
            "imprimir(tipo(sistema.directorio_temp()))\n", out, sizeof(out));
        int n_cad = 0;
        const char *p = out;
        while ((p = strstr(p, "cadena")) != NULL) { n_cad++; p++; }
        AFIRMAR(n_cad == 3, "wrappers_tres_cadenas");
    }

    /* Errores: argumentos no aceptados */
    {
        char out[1024];
        ejecutar_capturando(
            "intentar:\n"
            "    usuario_actual(\"extra\")\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(\"err usuario\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    hostname(\"extra\")\n"
            "atrapar ErrorDeTipo como e:\n"
            "    imprimir(\"err host\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err usuario") != NULL, "usuario_sin_args");
        AFIRMAR(strstr(out, "err host") != NULL, "hostname_sin_args");
    }

    /* Combinacion: crear archivo en temp con nombre por usuario */
    {
        char out[1024];
        ejecutar_capturando(
            "importar sistema\n"
            "importar archivos\n"
            "importar ruta\n"
            "tmp = ruta.Ruta(sistema.directorio_temp())\n"
            "f = tmp.unir(\"_test_v108_\" + sistema.usuario() + \".tmp\")\n"
            "archivos.escribir(f.cadena(), \"x\")\n"
            "imprimir(archivos.existe(f.cadena()))\n"
            "archivos.eliminar(f.cadena())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "combinacion_temp_usuario");
    }

    if (fallos == 0) {
        printf("sistema_v108: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "sistema_v108: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
