/*
 * Cornamusa — punto de entrada.
 *
 * En esta versión (v0.4.0):
 *   - `cornamusa archivo.cor` ejecuta el programa.
 *   - Sin argumentos abre el REPL interactivo.
 *   - `--tokens`, `--ast` siguen disponibles para inspección del lexer
 *     y del parser.
 *
 * Tree-walking interpreter (decisión B2) — primer release jugable
 * documentado en `decisiones/B2-tree-walking-vs-bytecode.md`. Las
 * sentencias soportadas son: asignación, `si`/`mientras`/`para`,
 * funciones top-level (con recursión, sin closures), `retornar`,
 * `romper`/`continuar`, llamadas a built-ins. Aplazadas a v0.5+:
 * listas, diccionarios, conjuntos, clases, excepciones, módulos.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "common.h"
#include "compilador.h"
#include "entorno.h"
#include "docs.h"
#include "errores.h"
#include "evaluador.h"
#include "formateador.h"
#include "fuente.h"
#include "lexer.h"
#include "linter.h"
#include "lsp.h"
#include "nativos.h"
#include "parser.h"
#include "repl_line.h"
#include "valor.h"
#include "vm.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define LINEA_MAX 1024
#define BUFFER_REPL_MAX 16384

/*
 * Configura la consola para emitir y aceptar UTF-8 (decisión I5).
 * En Linux/macOS no hace nada — la consola ya es UTF-8 por defecto.
 */
static void configurar_consola_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static void imprimir_uso(const char *programa) {
    fprintf(stderr,
        "Uso: %s [opciones] [archivo.cor]\n"
        "\n"
        "Opciones:\n"
        "  -h, --ayuda      Muestra esta ayuda\n"
        "  -v, --version    Muestra la versión\n"
        "      --tokens     Tokeniza el archivo y vuelca los tokens\n"
        "      --ast        Parsea el archivo y vuelca el AST\n"
        "      --bytecode   Ejecuta el archivo con el motor bytecode (Fase 6)\n"
        "                   en lugar del tree-walking. v0.6 sin SENT_PARA;\n"
        "                   programas que usan `para` deben usar el default.\n"
        "      --check      Valida sintaxis y compilación sin ejecutar.\n"
        "                   Exit 0 si OK, 65 si hay errores. Para CI/editores.\n"
        "\n"
        "Subcomandos:\n"
        "  fmt [opciones] <archivo>   Reformatea el archivo in-place.\n"
        "                             Opciones:\n"
        "                               --check    no escribe; exit 0 si ya\n"
        "                                          esta formateado, 1 si no.\n"
        "                               --stdout   imprime resultado a stdout.\n"
        "                             Usa '-' como archivo para leer stdin.\n"
        "  lint <archivo>             Analiza y reporta avisos de estilo.\n"
        "                             Exit 0 sin avisos, 1 con avisos.\n"
        "  docs [-o salida.md] <arch> Extrae documentacion (firmas + comentarios)\n"
        "                             y emite Markdown a stdout o al archivo dado.\n"
        "  lsp                        Inicia el Language Server Protocol (stdio,\n"
        "                             JSON-RPC). Para integracion con editores.\n"
        "  prof [--top=N] <archivo>   Ejecuta el script con profiler determinista\n"
        "                             y vuelca tabla por funcion (a stderr).\n"
        "\n"
        "Sin argumentos abre el REPL interactivo (motor tree-walking).\n",
        programa);
}

static void imprimir_version(void) {
    printf("Cornamusa %s\n", CORNAMUSA_VERSION);
}

static void imprimir_banner_repl(void) {
    printf("Cornamusa %s — REPL interactivo\n", CORNAMUSA_VERSION);
    printf("Escribe 'salir' o pulsa Ctrl-D (Ctrl-Z en Windows) para terminar.\n");
    printf("Una línea vacía ejecuta el bloque acumulado.\n\n");
}

/* ──────────────────────────────────────────────────────────────────
 * Errores de runtime con caret
 *
 * Convierte un EvalError + texto fuente en un mensaje formateado al
 * estilo MENSAJES.md §2, reusando `error_imprimir` del módulo de
 * errores. Los runtime errors no tienen un span exacto — usamos
 * 1 byte de subrayado en la columna reportada como aproximación.
 * ────────────────────────────────────────────────────────────────── */

static void imprimir_error_runtime(const EvalError *e,
                                    const char *fuente,
                                    const char *archivo) {
    /* Extraer categoría del prefijo del mensaje cuando empiece por
       "ErrorXxx:". Si no, usar "Error". */
    char categoria[64] = "Error";
    char mensaje[EVAL_MENSAJE_MAX];
    const char *src = e->mensaje;
    const char *colon = strchr(src, ':');
    if (colon != NULL && colon - src < 60) {
        size_t cat_len = (size_t)(colon - src);
        memcpy(categoria, src, cat_len);
        categoria[cat_len] = '\0';
        /* Saltar ": " tras la categoría. */
        const char *resto = colon + 1;
        while (*resto == ' ') resto++;
        snprintf(mensaje, sizeof(mensaje), "%s", resto);
    } else {
        snprintf(mensaje, sizeof(mensaje), "%s", src);
    }

    Error err;
    error_iniciar(&err, "");          /* la categoría se asigna abajo */
    err.categoria = categoria;        /* puntero a buffer local — válido durante esta llamada */
    err.archivo = archivo;
    err.linea = e->linea;
    err.columna_inicio = e->columna;
    err.columna_fin = e->columna + 1;
    error_set_mensaje(&err, mensaje);

    error_imprimir(&err, fuente, 1, stderr);

    /* Liberar mensaje (la categoría es estática local — no se libera). */
    err.categoria = NULL;
    error_destruir(&err);
}

/* ──────────────────────────────────────────────────────────────────
 * Ejecutar una cadena fuente en un entorno dado
 *
 * Reutilizable desde el REPL (entorno persistente entre líneas) y
 * desde la ejecución de archivos (entorno fresco que se descarta).
 *
 * Devuelve 0 si OK, !=0 si hubo error. NO destruye `globales`.
 * ────────────────────────────────────────────────────────────────── */

/*
 * Ejecuta un fragmento de código fuente.
 *
 * Si `arena_compartida` es NULL, se crea una arena local que se
 * destruye al volver. Si es no-NULL, la arena la posee el llamador y
 * persiste al volver — útil para el REPL donde los nodos AST de
 * funciones definidas previamente deben seguir vivos.
 *
 * `fuente` debe permanecer vivo al menos lo que dure la arena: el AST
 * referencia bytes del buffer fuente directamente (nombres de
 * variables, claves de entorno, etc.) sin copiarlos.
 */
static int ejecutar_fuente(const char *fuente,
                            const char *archivo,
                            Entorno *globales,
                            Arena *arena_compartida) {
    Lexer l;
    lexer_iniciar(&l, fuente, archivo);

    Arena local;
    bool arena_propia = (arena_compartida == NULL);
    Arena *arena = arena_compartida;
    if (arena_propia) {
        arena_iniciar(&local, 16384);
        arena = &local;
    }

    Parser p;
    parser_iniciar(&p, &l, arena, fuente, archivo);

    int n = 0;
    Sent **sents = parser_parsear_programa(&p, &n);

    if (p.tuvo_error) {
        if (arena_propia) arena_destruir(arena);
        return 65;  /* EX_DATAERR */
    }

    Evaluador ev;
    evaluador_iniciar(&ev, globales);
    evaluador_ejecutar_programa(&ev, sents, n);

    int rc = 0;
    if (ev.error.tuvo_error) {
        imprimir_error_runtime(&ev.error, fuente, archivo);
        rc = 70;  /* EX_SOFTWARE: error en runtime */
    }
    valor_destruir(&ev.valor_retorno);
    if (arena_propia) arena_destruir(arena);
    return rc;
}

static int ejecutar_archivo(const char *ruta) {
    FuenteCargada fc = fuente_cargar_archivo(ruta);
    if (fc.codigo != FUENTE_OK) {
        fprintf(stderr, "Error al cargar '%s': %s\n", ruta, fc.mensaje_error);
        return 74;  /* EX_IOERR */
    }

    Entorno globales;
    entorno_iniciar(&globales, NULL);
    nativos_registrar(&globales);

    int rc = ejecutar_fuente(fc.fuente, ruta, &globales, NULL);

    entorno_destruir(&globales);
    fuente_destruir(&fc);
    return rc;
}

/*
 * Pipeline alternativo con motor bytecode: lex → parse → compilar →
 * VM. v0.6 sin soporte de `para`; los demás programas se ejecutan
 * idénticamente al tree-walking. El cliente activa esta ruta con
 * la flag `--bytecode`.
 */
static int ejecutar_archivo_bc_opciones(const char *ruta, bool con_profiler,
                                        int top_n_profiler) {
    FuenteCargada fc = fuente_cargar_archivo(ruta);
    if (fc.codigo != FUENTE_OK) {
        fprintf(stderr, "Error al cargar '%s': %s\n", ruta, fc.mensaje_error);
        return 74;
    }

    Lexer l;
    lexer_iniciar(&l, fc.fuente, ruta);

    Arena a;
    arena_iniciar(&a, 16384);

    Parser p;
    parser_iniciar(&p, &l, &a, fc.fuente, ruta);

    int n = 0;
    Sent **sents = parser_parsear_programa(&p, &n);

    if (p.tuvo_error) {
        arena_destruir(&a);
        fuente_destruir(&fc);
        return 65;
    }

    Chunk chunk;
    chunk_iniciar(&chunk);
    Compilador c;
    compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, sents, n)) {
        EvalError er = c.error;
        imprimir_error_runtime(&er, fc.fuente, ruta);
        chunk_destruir(&chunk); arena_destruir(&a); fuente_destruir(&fc);
        return 65;
    }

    VM vm;
    vm_iniciar(&vm);
    if (con_profiler) profiler_activar(&vm.profiler);
    Valor resultado = valor_nulo();
    ResultadoVM rc_vm = vm_ejecutar(&vm, &chunk, &resultado);

    int rc = 0;
    if (rc_vm != VM_OK) {
        imprimir_error_runtime(&vm.error, fc.fuente, ruta);
        /* v1.38: traza de la cadena de llamadas, si el error ocurrió
           dentro de una función anidada. */
        if (vm.traceback[0] != '\0') {
            fputs(vm.traceback, stderr);
        }
        rc = 70;
    }
    if (con_profiler) {
        profiler_desactivar(&vm.profiler);
        profiler_dump(&vm.profiler, stderr, top_n_profiler);
    }
    valor_destruir(&resultado);
    vm_destruir(&vm);
    chunk_destruir(&chunk);
    arena_destruir(&a);
    fuente_destruir(&fc);
    return rc;
}

static int ejecutar_archivo_bytecode(const char *ruta) {
    return ejecutar_archivo_bc_opciones(ruta, false, 0);
}

/* ──────────────────────────────────────────────────────────────────
 * Subcomando `prof` (v1.71 - Fase 5 tooling).
 *
 * Ejecuta el script con el profiler determinista activado. Al
 * terminar (normal o por error), vuelca a stderr una tabla ordenada
 * por self time descendente. Para no contaminar stdout, el dump va
 * a stderr — el programa puede seguir usando stdout sin interferencia.
 *
 * Uso:
 *   cornamusa prof script.cor [args...]
 *   cornamusa prof --top=10 script.cor
 * ────────────────────────────────────────────────────────────────── */

static int subcomando_prof(int argc, char **argv) {
    const char *archivo = NULL;
    int top_n = 20;
    int idx_archivo = -1;
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--ayuda") == 0
            || strcmp(arg, "--help") == 0) {
            fprintf(stderr,
                "uso: %s prof [--top=N] script.cor [args...]\n"
                "  Ejecuta el script bajo el profiler determinista.\n"
                "  --top=N   muestra solo las N funciones con mas self time (default: 20, 0 = todas)\n",
                argv[0]);
            return 0;
        }
        if (strncmp(arg, "--top=", 6) == 0) {
            top_n = atoi(arg + 6);
            if (top_n < 0) top_n = 0;
            continue;
        }
        if (arg[0] == '-' && strcmp(arg, "-") != 0) {
            fprintf(stderr, "Opcion no reconocida para prof: %s\n", arg);
            return 64;
        }
        archivo = arg;
        idx_archivo = i;
        break;  /* el resto son argv del programa */
    }
    if (!archivo) {
        fprintf(stderr, "prof: se requiere un archivo .cor\n");
        return 64;
    }
    /* Pasar argv al programa, igual que en el flujo normal. */
    if (idx_archivo >= 0) {
        nativos_set_argv(argc - idx_archivo, &argv[idx_archivo]);
    } else {
        nativos_set_argv(0, NULL);
    }
    return ejecutar_archivo_bc_opciones(archivo, true, top_n);
}

/* ──────────────────────────────────────────────────────────────────
 * REPL
 *
 * Heurística para decidir si esperar más líneas:
 *   - `:` al final de la línea → abre un bloque, profundidad+1.
 *   - keyword `fin ` (al inicio o tras whitespace) → cierra, profundidad-1.
 *
 * Cuando profundidad llega a 0 ejecutamos el buffer acumulado.
 * ────────────────────────────────────────────────────────────────── */

static bool linea_abre_bloque(const char *linea) {
    /* Quitar comentario y whitespace final. */
    int len = (int)strlen(linea);
    int fin = len;
    /* Buscar '#' fuera de comillas — versión simplificada: descontar
       whitespace y comprobar el último carácter no espacio. */
    while (fin > 0 && (linea[fin - 1] == '\n' || linea[fin - 1] == '\r'
                       || linea[fin - 1] == ' '  || linea[fin - 1] == '\t')) {
        fin--;
    }
    return fin > 0 && linea[fin - 1] == ':';
}

static bool linea_cierra_bloque(const char *linea) {
    /* Buscamos `fin ` precedido de whitespace o inicio de línea. */
    const char *p = linea;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "fin ", 4) == 0) return true;
    if (strcmp(p, "fin\n") == 0)    return true;
    if (strcmp(p, "fin\r\n") == 0)  return true;
    if (strcmp(p, "fin") == 0)      return true;
    return false;
}

/*
 * Lanza el REPL con una arena compartida que vive toda la sesión.
 *
 * Cada bloque de entrada se duplica en heap (strdup) y se le pasa el
 * mismo arena. Los nodos AST referencian al strdup; si liberamos el
 * strdup o destruimos el arena entre llamadas, las funciones definidas
 * antes referenciarían punteros colgantes. Las cadenas duplicadas se
 * "filtran" deliberadamente — viven hasta que termine el proceso.
 */
static int correr_repl(void) {
    imprimir_banner_repl();

    Entorno globales;
    entorno_iniciar(&globales, NULL);
    nativos_registrar(&globales);

    Arena arena_repl;
    arena_iniciar(&arena_repl, 65536);

    /* v1.47: historial persistente + edición de línea con
       `repl_leer_linea`. El historial se carga al iniciar (si hay
       `.cornamusa_historial`) y se guarda al salir. */
    ReplHistorial *historial = repl_historial_nuevo();
    if (historial) repl_historial_cargar(historial);

    char buffer[BUFFER_REPL_MAX] = "";
    int profundidad = 0;
    const char *prompt = ">>> ";

    for (;;) {
        char *linea_din = repl_leer_linea(prompt, historial);
        if (linea_din == NULL) {
            /* EOF (Ctrl-D POSIX / Ctrl-Z Windows con buffer vacío). */
            break;
        }
        const char *linea = linea_din;

        /* `salir` solo en el primer nivel y con buffer vacío. */
        if (profundidad == 0 && buffer[0] == '\0') {
            const char *p = linea;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "salir", 5) == 0) {
                const char *resto = p + 5;
                while (*resto == ' ' || *resto == '\t') resto++;
                if (*resto == '\0') {
                    free(linea_din);
                    break;
                }
            }
        }

        bool vacia = (linea[0] == '\0');

        if (vacia && buffer[0] != '\0') {
            /* Línea vacía cierra un bloque multilínea en curso. */
            char *fuente_persistente = strdup(buffer);
            if (fuente_persistente) {
                ejecutar_fuente(fuente_persistente, "<repl>",
                                 &globales, &arena_repl);
            }
            buffer[0] = '\0';
            profundidad = 0;
            prompt = ">>> ";
            free(linea_din);
            continue;
        }
        if (vacia) {
            free(linea_din);
            continue;
        }

        /* Agregar al historial. Si estamos en multilínea, agregamos
           cada línea por separado (igual que readline). */
        if (historial) repl_historial_agregar(historial, linea);

        /* Acumular al buffer (con \n entre líneas en multilínea). */
        size_t blen = strlen(buffer);
        size_t llen = strlen(linea);
        size_t necesario = blen + llen + 2;  /* +1 \n +1 \0 */
        if (necesario < sizeof(buffer)) {
            if (blen > 0) buffer[blen++] = '\n';
            memcpy(buffer + blen, linea, llen);
            buffer[blen + llen] = '\n';
            buffer[blen + llen + 1] = '\0';
        } else {
            fprintf(stderr,
                "Buffer del REPL lleno; descartando entrada acumulada.\n");
            buffer[0] = '\0';
            profundidad = 0;
            prompt = ">>> ";
            free(linea_din);
            continue;
        }

        /* Línea para detectar apertura/cierre de bloque (necesitamos
           el `\n` al final para que linea_*_bloque las acepte como
           antes). Reutilizamos un buffer local. */
        char linea_con_nl[LINEA_MAX];
        size_t ll = strlen(linea);
        if (ll + 2 < sizeof(linea_con_nl)) {
            memcpy(linea_con_nl, linea, ll);
            linea_con_nl[ll] = '\n';
            linea_con_nl[ll + 1] = '\0';
            if (linea_cierra_bloque(linea_con_nl)) {
                if (profundidad > 0) profundidad--;
            }
            if (linea_abre_bloque(linea_con_nl)) {
                profundidad++;
            }
        }

        if (profundidad == 0) {
            char *fuente_persistente = strdup(buffer);
            if (fuente_persistente) {
                ejecutar_fuente(fuente_persistente, "<repl>",
                                 &globales, &arena_repl);
            }
            buffer[0] = '\0';
            prompt = ">>> ";
        } else {
            prompt = "... ";
        }
        free(linea_din);
    }

    if (historial) {
        repl_historial_guardar(historial);
        repl_historial_liberar(historial);
    }
    entorno_destruir(&globales);
    arena_destruir(&arena_repl);
    return 0;
}

/* ──────────────────────────────────────────────────────────────────
 * --ast: parsea y vuelca el AST en formato S-expression.
 * ────────────────────────────────────────────────────────────────── */

static int parsear_y_volcar_ast(const char *ruta) {
    FuenteCargada fc = fuente_cargar_archivo(ruta);
    if (fc.codigo != FUENTE_OK) {
        fprintf(stderr, "Error al cargar '%s': %s\n", ruta, fc.mensaje_error);
        return 74;
    }

    Lexer l;
    lexer_iniciar(&l, fc.fuente, ruta);

    Arena a;
    arena_iniciar(&a, 16384);

    Parser p;
    parser_iniciar(&p, &l, &a, fc.fuente, ruta);

    int n;
    Sent **sents = parser_parsear_programa(&p, &n);

    if (p.tuvo_error) {
        arena_destruir(&a);
        fuente_destruir(&fc);
        fprintf(stderr,
            "\nFallo de parseo. Corrige los errores arriba y reintenta.\n");
        return 65;
    }

    printf("(programa\n");
    for (int i = 0; i < n; i++) {
        printf("  ");
        sent_imprimir(sents[i], stdout);
        printf("\n");
    }
    printf(")\n");
    printf("\n%d sentencia(s) en el programa.\n", n);

    arena_destruir(&a);
    fuente_destruir(&fc);
    return 0;
}

/* ──────────────────────────────────────────────────────────────────
 * --tokens: vuelca todos los tokens del archivo (debug del lexer).
 * ────────────────────────────────────────────────────────────────── */

static int volcar_tokens_archivo(const char *ruta) {
    FuenteCargada fc = fuente_cargar_archivo(ruta);
    if (fc.codigo != FUENTE_OK) {
        fprintf(stderr, "Error al cargar '%s': %s\n", ruta, fc.mensaje_error);
        return 74;
    }

    Lexer l;
    lexer_iniciar(&l, fc.fuente, ruta);

    int errores = 0;
    int tokens_emitidos = 0;
    Token t;
    do {
        t = lexer_siguiente(&l);
        tokens_emitidos++;

        if (t.tipo == TT_ERROR) {
            error_imprimir_token(&t, fc.fuente, ruta, stderr);
            errores++;
            continue;
        }

        printf("%4d:%-3d  %-25s  ", t.linea, t.columna,
            tipo_token_nombre(t.tipo));
        if (t.tipo == TT_FIN_ARCHIVO) {
            printf("(fin)\n");
        } else {
            fputc('"', stdout);
            for (int i = 0; i < t.longitud; i++) {
                char c = t.inicio[i];
                if (c == '\n')      fputs("\\n", stdout);
                else if (c == '\t') fputs("\\t", stdout);
                else if (c == '"')  fputs("\\\"", stdout);
                else                fputc(c, stdout);
            }
            fputs("\"\n", stdout);
        }
    } while (t.tipo != TT_FIN_ARCHIVO);

    fuente_destruir(&fc);

    if (errores > 0) {
        fprintf(stderr,
            "\n%d error(es) léxicos.\n", errores);
        return 65;
    }
    printf("\n%d tokens emitidos (incluyendo TT_FIN_ARCHIVO).\n",
        tokens_emitidos);
    return 0;
}

/* ──────────────────────────────────────────────────────────────────
 * --check: valida sintaxis + compilación SIN ejecutar.
 *
 * Pipeline: lex → parse → compilar. Si todo OK, imprime una línea de
 * confirmación y sale con 0. Si hay error de parseo o compilación, lo
 * reporta y sale con 65. Pensado para CI, hooks de pre-commit y
 * editores que quieran validar al guardar sin correr el programa.
 * ────────────────────────────────────────────────────────────────── */

static int validar_archivo(const char *ruta) {
    FuenteCargada fc = fuente_cargar_archivo(ruta);
    if (fc.codigo != FUENTE_OK) {
        fprintf(stderr, "Error al cargar '%s': %s\n", ruta, fc.mensaje_error);
        return 74;
    }

    Lexer l;
    lexer_iniciar(&l, fc.fuente, ruta);

    Arena a;
    arena_iniciar(&a, 16384);

    Parser p;
    parser_iniciar(&p, &l, &a, fc.fuente, ruta);

    int n = 0;
    Sent **sents = parser_parsear_programa(&p, &n);

    if (p.tuvo_error) {
        arena_destruir(&a);
        fuente_destruir(&fc);
        fprintf(stderr, "\n%s: fallo de sintaxis.\n", ruta);
        return 65;
    }

    Chunk chunk;
    chunk_iniciar(&chunk);
    Compilador c;
    compilador_iniciar(&c, &chunk);
    if (!compilador_compilar_programa(&c, sents, n)) {
        EvalError er = c.error;
        imprimir_error_runtime(&er, fc.fuente, ruta);
        chunk_destruir(&chunk); arena_destruir(&a); fuente_destruir(&fc);
        fprintf(stderr, "\n%s: fallo de compilacion.\n", ruta);
        return 65;
    }

    chunk_destruir(&chunk);
    arena_destruir(&a);
    fuente_destruir(&fc);
    printf("%s: OK (%d sentencia%s, sin errores de sintaxis ni compilacion)\n",
           ruta, n, n == 1 ? "" : "s");
    return 0;
}

/* ──────────────────────────────────────────────────────────────────
 * Subcomando `fmt` (v1.48 - Fase 5 tooling).
 *
 * Reformatea fuente Cornamusa con reglas conservadoras:
 * reindentacion a 4 espacios, trim de trailing whitespace, colapso de
 * blancas y trailing newline normalizado. Los detalles viven en
 * `src/formateador.c`.
 *
 * Modos:
 *   in-place : reescribe el archivo si la salida difiere.
 *   --check  : no escribe; exit 0 si ya estaba formateado, 1 si no.
 *   --stdout : imprime resultado a stdout (deja archivo intacto).
 *   `-`      : lee stdin → escribe stdout (siempre).
 * ────────────────────────────────────────────────────────────────── */

static int leer_stdin_completo(char **out_buf, size_t *out_len) {
    size_t cap = 4096;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return -1;
    for (;;) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nuevo = (char *)realloc(buf, cap);
            if (!nuevo) { free(buf); return -1; }
            buf = nuevo;
        }
        size_t leido = fread(buf + len, 1, cap - len - 1, stdin);
        len += leido;
        if (leido == 0) break;
    }
    buf[len] = '\0';
    *out_buf = buf;
    *out_len = len;
    return 0;
}

static int subcomando_fmt(int argc, char **argv) {
    bool modo_check = false;
    bool modo_stdout = false;
    const char *archivo = NULL;

    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--check") == 0) { modo_check = true; continue; }
        if (strcmp(arg, "--stdout") == 0) { modo_stdout = true; continue; }
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--ayuda") == 0
            || strcmp(arg, "--help") == 0) {
            imprimir_uso(argv[0]);
            return 0;
        }
        if (arg[0] == '-' && strcmp(arg, "-") != 0) {
            fprintf(stderr, "Opción no reconocida para fmt: %s\n", arg);
            return 64;
        }
        if (archivo != NULL) {
            fprintf(stderr, "fmt acepta un solo archivo (recibido también: %s)\n", arg);
            return 64;
        }
        archivo = arg;
    }

    if (archivo == NULL) {
        fprintf(stderr, "fmt: se requiere un archivo .cor (o '-' para stdin)\n");
        return 64;
    }

    char *fuente_buf = NULL;
    size_t fuente_len = 0;
    bool fuente_es_stdin = (strcmp(archivo, "-") == 0);

    if (fuente_es_stdin) {
        if (leer_stdin_completo(&fuente_buf, &fuente_len) != 0) {
            fprintf(stderr, "fmt: error leyendo stdin\n");
            return 74;
        }
    } else {
        FuenteCargada fc = fuente_cargar_archivo(archivo);
        if (fc.codigo != FUENTE_OK) {
            fprintf(stderr, "fmt: no se pudo cargar '%s': %s\n",
                    archivo, fc.mensaje_error);
            fuente_destruir(&fc);
            return 74;
        }
        fuente_len = strlen(fc.fuente);
        fuente_buf = (char *)malloc(fuente_len + 1);
        if (!fuente_buf) {
            fuente_destruir(&fc);
            return 71;
        }
        memcpy(fuente_buf, fc.fuente, fuente_len + 1);
        fuente_destruir(&fc);
    }

    FormatoResultado r = formateador_formatear(fuente_buf);
    if (r.mensaje_error != NULL) {
        fprintf(stderr, "fmt: %s\n", r.mensaje_error);
        formato_resultado_destruir(&r);
        free(fuente_buf);
        return 70;
    }

    int rc = 0;

    if (fuente_es_stdin) {
        /* stdin → stdout siempre. */
        fwrite(r.fuente, 1, r.longitud, stdout);
    } else if (modo_check) {
        if (r.cambiada) {
            fprintf(stderr, "%s\n", archivo);
            rc = 1;
        }
    } else if (modo_stdout) {
        fwrite(r.fuente, 1, r.longitud, stdout);
    } else {
        if (r.cambiada) {
            FILE *f = fopen(archivo, "wb");
            if (!f) {
                fprintf(stderr, "fmt: no se pudo escribir '%s'\n", archivo);
                rc = 73;
            } else {
                fwrite(r.fuente, 1, r.longitud, f);
                fclose(f);
                fprintf(stderr, "formateado: %s\n", archivo);
            }
        }
    }

    formato_resultado_destruir(&r);
    free(fuente_buf);
    return rc;
}

/* ──────────────────────────────────────────────────────────────────
 * Subcomando `lint` (v1.49 - Fase 5 tooling).
 *
 * Parsea el archivo y aplica analisis estatico ligero via `linter.c`.
 * Reporta cada aviso en el formato `archivo:linea:col: warning [tipo]: mensaje`.
 * Exit 0 si no hay avisos; 1 si los hay.
 * ────────────────────────────────────────────────────────────────── */

static int subcomando_lint(int argc, char **argv) {
    const char *archivo = NULL;
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--ayuda") == 0
            || strcmp(arg, "--help") == 0) {
            imprimir_uso(argv[0]);
            return 0;
        }
        if (arg[0] == '-' && strcmp(arg, "-") != 0) {
            fprintf(stderr, "Opcion no reconocida para lint: %s\n", arg);
            return 64;
        }
        if (archivo) {
            fprintf(stderr, "lint acepta un solo archivo\n");
            return 64;
        }
        archivo = arg;
    }
    if (!archivo) {
        fprintf(stderr, "lint: se requiere un archivo .cor\n");
        return 64;
    }

    FuenteCargada fc = fuente_cargar_archivo(archivo);
    if (fc.codigo != FUENTE_OK) {
        fprintf(stderr, "lint: no se pudo cargar '%s': %s\n",
                archivo, fc.mensaje_error);
        fuente_destruir(&fc);
        return 74;
    }

    Lexer l;
    lexer_iniciar(&l, fc.fuente, archivo);

    Arena a;
    arena_iniciar(&a, 16384);

    Parser p;
    parser_iniciar(&p, &l, &a, fc.fuente, archivo);

    int n = 0;
    Sent **sents = parser_parsear_programa(&p, &n);

    if (p.tuvo_error) {
        arena_destruir(&a);
        fuente_destruir(&fc);
        fprintf(stderr, "\n%s: fallo de sintaxis (lint omitido).\n", archivo);
        return 65;
    }

    LinterResultado r = linter_analizar(sents, n, fc.fuente);

    int rc = 0;
    if (r.n > 0) {
        for (int i = 0; i < r.n; i++) {
            Warning *w = &r.avisos[i];
            fprintf(stdout, "%s:%d:%d: warning [%s]: %s\n",
                    archivo, w->linea, w->columna,
                    linter_tipo_nombre(w->tipo), w->mensaje);
        }
        fprintf(stdout, "%d aviso%s.\n", r.n, r.n == 1 ? "" : "s");
        rc = 1;
    }

    linter_resultado_destruir(&r);
    arena_destruir(&a);
    fuente_destruir(&fc);
    return rc;
}

/* ──────────────────────────────────────────────────────────────────
 * Subcomando `docs` (v1.51 - Fase 5 tooling).
 *
 * Parsea el archivo y emite Markdown con la documentacion del modulo.
 * Por defecto a stdout; `-o salida.md` redirige a archivo.
 *
 * El "nombre de modulo" se deriva del basename del archivo (sin
 * extension `.cor`) y se usa como H1.
 * ────────────────────────────────────────────────────────────────── */

static const char *basename_modulo(const char *ruta, char *out, size_t cap) {
    const char *p = ruta;
    const char *base = ruta;
    for (; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    /* Copiar quitando sufijo `.cor` si lo hay. */
    size_t len = strlen(base);
    if (len > 4 && strcmp(base + len - 4, ".cor") == 0) len -= 4;
    if (len >= cap) len = cap - 1;
    memcpy(out, base, len);
    out[len] = '\0';
    return out;
}

static int subcomando_docs(int argc, char **argv) {
    const char *archivo = NULL;
    const char *salida = NULL;

    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--ayuda") == 0
            || strcmp(arg, "--help") == 0) {
            imprimir_uso(argv[0]);
            return 0;
        }
        if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "docs: -o requiere un nombre de archivo\n");
                return 64;
            }
            salida = argv[++i];
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "Opcion no reconocida para docs: %s\n", arg);
            return 64;
        }
        if (archivo) {
            fprintf(stderr, "docs acepta un solo archivo\n");
            return 64;
        }
        archivo = arg;
    }
    if (!archivo) {
        fprintf(stderr, "docs: se requiere un archivo .cor\n");
        return 64;
    }

    FuenteCargada fc = fuente_cargar_archivo(archivo);
    if (fc.codigo != FUENTE_OK) {
        fprintf(stderr, "docs: no se pudo cargar '%s': %s\n",
                archivo, fc.mensaje_error);
        fuente_destruir(&fc);
        return 74;
    }

    Lexer l;
    lexer_iniciar(&l, fc.fuente, archivo);

    Arena a;
    arena_iniciar(&a, 16384);

    Parser p;
    parser_iniciar(&p, &l, &a, fc.fuente, archivo);

    int n = 0;
    Sent **sents = parser_parsear_programa(&p, &n);

    if (p.tuvo_error) {
        arena_destruir(&a);
        fuente_destruir(&fc);
        fprintf(stderr, "\n%s: fallo de sintaxis (docs omitido).\n", archivo);
        return 65;
    }

    char nombre[256];
    basename_modulo(archivo, nombre, sizeof(nombre));

    DocsResultado r = docs_generar(fc.fuente, nombre, sents, n);

    int rc = 0;
    if (r.mensaje_error) {
        fprintf(stderr, "docs: %s\n", r.mensaje_error);
        rc = 70;
    } else if (salida) {
        FILE *f = fopen(salida, "wb");
        if (!f) {
            fprintf(stderr, "docs: no se pudo escribir '%s'\n", salida);
            rc = 73;
        } else {
            fwrite(r.markdown, 1, r.longitud, f);
            fclose(f);
            fprintf(stderr, "documentacion escrita en: %s\n", salida);
        }
    } else {
        fwrite(r.markdown, 1, r.longitud, stdout);
    }

    docs_resultado_destruir(&r);
    arena_destruir(&a);
    fuente_destruir(&fc);
    return rc;
}

/* ──────────────────────────────────────────────────────────────────
 * Entry point
 * ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    configurar_consola_utf8();

    /* Subcomandos antes de flags planas (estilo `git`, `gofmt`). */
    if (argc >= 2 && strcmp(argv[1], "fmt") == 0) {
        return subcomando_fmt(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "lint") == 0) {
        return subcomando_lint(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "docs") == 0) {
        return subcomando_docs(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "prof") == 0) {
        return subcomando_prof(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "lsp") == 0) {
#ifdef _WIN32
        /* En Windows stdin/stdout estan en text mode por defecto, lo
         * que traduce \r\n a \n al leer — roto para el framing
         * LSP que cuenta bytes literales. Modo binario obligatorio. */
        _setmode(_fileno(stdin),  _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        return lsp_run();
    }

    const char *archivo = NULL;
    bool volcar_tokens = false;
    bool volcar_ast = false;
    bool usar_bytecode = false;
    bool solo_validar = false;
    int idx_archivo = -1;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--ayuda") == 0 ||
            strcmp(arg, "--help") == 0) {
            imprimir_uso(argv[0]);
            return 0;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0 ||
            strcmp(arg, "--versión") == 0) {
            imprimir_version();
            return 0;
        }
        if (strcmp(arg, "--tokens") == 0) {
            volcar_tokens = true;
            continue;
        }
        if (strcmp(arg, "--ast") == 0) {
            volcar_ast = true;
            continue;
        }
        if (strcmp(arg, "--bytecode") == 0) {
            usar_bytecode = true;
            continue;
        }
        if (strcmp(arg, "--check") == 0 || strcmp(arg, "--validar") == 0) {
            solo_validar = true;
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "Opción no reconocida: %s\n", arg);
            imprimir_uso(argv[0]);
            return 64;
        }
        archivo = arg;
        idx_archivo = i;
        break;  /* lo que venga tras el .cor es argv del programa */
    }

    /* Argv visible desde Cornamusa: argv[0] = archivo .cor, resto son
       argumentos pasados tras él. Si no hay archivo, queda vacío. */
    if (idx_archivo >= 0) {
        nativos_set_argv(argc - idx_archivo, &argv[idx_archivo]);
    } else {
        nativos_set_argv(0, NULL);
    }

    if (archivo != NULL) {
        if (solo_validar)  return validar_archivo(archivo);
        if (volcar_ast)    return parsear_y_volcar_ast(archivo);
        if (volcar_tokens) return volcar_tokens_archivo(archivo);
        if (usar_bytecode) return ejecutar_archivo_bytecode(archivo);
        return ejecutar_archivo(archivo);
    }

    if (volcar_tokens || volcar_ast || usar_bytecode || solo_validar) {
        fprintf(stderr, "%s requiere un archivo .cor\n",
            volcar_ast ? "--ast" :
            volcar_tokens ? "--tokens" :
            solo_validar ? "--check" : "--bytecode");
        return 64;
    }

    return correr_repl();
}
