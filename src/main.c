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
#include "errores.h"
#include "evaluador.h"
#include "fuente.h"
#include "lexer.h"
#include "nativos.h"
#include "parser.h"
#include "valor.h"
#include "vm.h"

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
static int ejecutar_archivo_bytecode(const char *ruta) {
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
    Valor resultado = valor_nulo();
    ResultadoVM rc_vm = vm_ejecutar(&vm, &chunk, &resultado);

    int rc = 0;
    if (rc_vm != VM_OK) {
        imprimir_error_runtime(&vm.error, fc.fuente, ruta);
        rc = 70;
    }
    valor_destruir(&resultado);
    vm_destruir(&vm);
    chunk_destruir(&chunk);
    arena_destruir(&a);
    fuente_destruir(&fc);
    return rc;
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

    char buffer[BUFFER_REPL_MAX] = "";
    char linea[LINEA_MAX];
    int profundidad = 0;
    const char *prompt = ">>> ";

    for (;;) {
        fputs(prompt, stdout);
        fflush(stdout);

        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            putchar('\n');
            break;
        }

        /* `salir` solo en el primer nivel y con buffer vacío. */
        if (profundidad == 0 && buffer[0] == '\0') {
            const char *p = linea;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "salir", 5) == 0) {
                const char *resto = p + 5;
                while (*resto == ' ' || *resto == '\t' || *resto == '\n'
                       || *resto == '\r') resto++;
                if (*resto == '\0') break;
            }
        }

        bool vacia = (linea[0] == '\n' || linea[0] == '\r' || linea[0] == '\0');

        if (vacia && buffer[0] != '\0') {
            char *fuente_persistente = strdup(buffer);
            if (fuente_persistente) {
                ejecutar_fuente(fuente_persistente, "<repl>",
                                 &globales, &arena_repl);
            }
            buffer[0] = '\0';
            profundidad = 0;
            prompt = ">>> ";
            continue;
        }
        if (vacia) {
            continue;
        }

        size_t blen = strlen(buffer);
        size_t llen = strlen(linea);
        if (blen + llen + 1 < sizeof(buffer)) {
            memcpy(buffer + blen, linea, llen + 1);
        } else {
            fprintf(stderr,
                "Buffer del REPL lleno; descartando entrada acumulada.\n");
            buffer[0] = '\0';
            profundidad = 0;
            prompt = ">>> ";
            continue;
        }

        if (linea_cierra_bloque(linea)) {
            if (profundidad > 0) profundidad--;
        }
        if (linea_abre_bloque(linea)) {
            profundidad++;
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
 * Entry point
 * ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    configurar_consola_utf8();

    const char *archivo = NULL;
    bool volcar_tokens = false;
    bool volcar_ast = false;
    bool usar_bytecode = false;
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
        if (volcar_ast)    return parsear_y_volcar_ast(archivo);
        if (volcar_tokens) return volcar_tokens_archivo(archivo);
        if (usar_bytecode) return ejecutar_archivo_bytecode(archivo);
        return ejecutar_archivo(archivo);
    }

    if (volcar_tokens || volcar_ast || usar_bytecode) {
        fprintf(stderr, "%s requiere un archivo .cor\n",
            volcar_ast ? "--ast" :
            volcar_tokens ? "--tokens" : "--bytecode");
        return 64;
    }

    return correr_repl();
}
