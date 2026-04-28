#include "lexer.h"

#include <string.h>

#include "utf8proc.h"

/* ──────────────────────────────────────────────────────────────────
 * Utilidades internas
 * ────────────────────────────────────────────────────────────────── */

static bool en_fin(const Lexer *l) {
    return *l->actual == '\0';
}

static char avanzar(Lexer *l) {
    char c = *l->actual;
    l->actual++;
    return c;
}

static char mirar(const Lexer *l) {
    return *l->actual;
}

static char mirar_siguiente(const Lexer *l) {
    if (*l->actual == '\0') return '\0';
    return l->actual[1];
}

static bool coincidir(Lexer *l, char esperado) {
    if (en_fin(l)) return false;
    if (*l->actual != (unsigned char)esperado) return false;
    l->actual++;
    return true;
}

static int columna_actual(const Lexer *l) {
    return (int)(l->inicio_token - l->inicio_linea) + 1;
}

static Token crear_token(const Lexer *l, TipoToken tipo) {
    Token t;
    t.tipo = tipo;
    t.inicio = l->inicio_token;
    t.longitud = (int)(l->actual - l->inicio_token);
    t.linea = l->linea;
    t.columna = columna_actual(l);
    return t;
}

/*
 * Token de error léxico. `mensaje` debe ser una cadena estática
 * (literal o gestionada externamente), porque la guardamos por puntero
 * en `inicio` sin copiarla.
 */
static Token token_error(const Lexer *l, const char *mensaje) {
    Token t;
    t.tipo = TT_ERROR;
    t.inicio = mensaje;
    t.longitud = (int)strlen(mensaje);
    t.linea = l->linea;
    t.columna = columna_actual(l);
    return t;
}

/* ──────────────────────────────────────────────────────────────────
 * Predicados de carácter
 * ────────────────────────────────────────────────────────────────── */

static bool es_digito(char c) {
    return c >= '0' && c <= '9';
}

static bool es_digito_hex(char c) {
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

static bool es_digito_octal(char c) {
    return c >= '0' && c <= '7';
}

static bool es_digito_binario(char c) {
    return c == '0' || c == '1';
}

/*
 * ASCII rápido: ¿puede iniciar un identificador?
 * Letras a-z, A-Z, '_' o '$'.
 */
static bool es_inicio_ident_ascii(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || c == '_' || c == '$';
}

/*
 * ASCII rápido: ¿puede aparecer dentro de un identificador?
 * Letras, dígitos, '_' o '$'.
 */
static bool es_continua_ident_ascii(char c) {
    return es_inicio_ident_ascii(c) || (c >= '0' && c <= '9');
}

/*
 * Unicode: ¿es un code point válido para iniciar un identificador?
 * Aceptamos categorías de Letter (Lu, Ll, Lt, Lm, Lo) y Letter Number (Nl).
 */
static bool es_inicio_ident_unicode(int32_t cp) {
    utf8proc_category_t cat = utf8proc_category((utf8proc_int32_t)cp);
    return cat == UTF8PROC_CATEGORY_LU
        || cat == UTF8PROC_CATEGORY_LL
        || cat == UTF8PROC_CATEGORY_LT
        || cat == UTF8PROC_CATEGORY_LM
        || cat == UTF8PROC_CATEGORY_LO
        || cat == UTF8PROC_CATEGORY_NL;
}

/*
 * Unicode: ¿es un code point válido para continuar un identificador?
 * Letters, dígitos decimales, marks (combining), connector punctuation.
 */
static bool es_continua_ident_unicode(int32_t cp) {
    utf8proc_category_t cat = utf8proc_category((utf8proc_int32_t)cp);
    switch (cat) {
        case UTF8PROC_CATEGORY_LU:
        case UTF8PROC_CATEGORY_LL:
        case UTF8PROC_CATEGORY_LT:
        case UTF8PROC_CATEGORY_LM:
        case UTF8PROC_CATEGORY_LO:
        case UTF8PROC_CATEGORY_NL:
        case UTF8PROC_CATEGORY_ND:   /* dígitos decimales */
        case UTF8PROC_CATEGORY_MN:   /* marks no espaciantes (combining) */
        case UTF8PROC_CATEGORY_MC:   /* marks espaciantes */
        case UTF8PROC_CATEGORY_PC:   /* connector punctuation (incluye '_' en Pc) */
            return true;
        default:
            return false;
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Whitespace y comentarios
 * ────────────────────────────────────────────────────────────────── */

/*
 * Salta espacios, tabuladores, retornos de carro, saltos de línea y
 * comentarios `# ...`. Los saltos de línea avanzan el contador de línea
 * y reinician el inicio de línea para el cómputo de columna.
 *
 * Decisión B1: la indentación NO es semántica, así que tabuladores y
 * espacios al principio de línea son simplemente whitespace.
 */
static void saltar_irrelevante(Lexer *l) {
    for (;;) {
        char c = mirar(l);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                avanzar(l);
                break;
            case '\n':
                avanzar(l);
                l->linea++;
                l->inicio_linea = l->actual;
                break;
            case '#':
                /* Comentario hasta fin de línea. El '\n' se trata en la
                   siguiente iteración para que se contabilice bien. */
                while (!en_fin(l) && mirar(l) != '\n') {
                    avanzar(l);
                }
                break;
            default:
                return;
        }
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Escaneo de literales numéricos
 *
 * Soporta:
 *   - Decimales en base 10 con guiones bajos opcionales (1_000_000).
 *   - Hexadecimal (0xff, 0xCa_fE), octal (0o755), binario (0b1010).
 *   - Punto decimal y notación científica (3.14, 1.5e-3, 1e10).
 *
 * Reglas de underscore (decisión de sesión 2):
 *   - No al inicio (1_2 ✓, _12 es identificador).
 *   - No al final (1_2 ✓, 12_ ✗).
 *   - No consecutivos (1__2 ✗).
 *   - Permitido inmediatamente tras prefijo de base (0x_ff ✓).
 *
 * El lexer solo verifica que el LEXEMA es válido; la conversión a
 * int64/bignum/double la hace el parser cuando construye el AST.
 * ────────────────────────────────────────────────────────────────── */

/*
 * Consume una secuencia de dígitos (de la base indicada por es_digito_xx)
 * intercalados con guiones bajos. Garantiza que no hay '__' consecutivos
 * ni '_' final. Devuelve true si tras la secuencia hay al menos un
 * dígito (es decir, el literal no está vacío).
 *
 * Si encuentra error, escribe el mensaje en *msg y devuelve false.
 */
static bool consumir_digitos(Lexer *l,
                             bool (*es_digito_de_base)(char),
                             const char **msg) {
    bool hubo_digito = false;
    char anterior = '_';  /* sentinel inicial: rechaza '_' como primer char */
    while (true) {
        char c = mirar(l);
        if (es_digito_de_base(c)) {
            hubo_digito = true;
            anterior = c;
            avanzar(l);
        } else if (c == '_') {
            if (anterior == '_') {
                *msg = "no se permiten guiones bajos consecutivos en literales numéricos";
                return false;
            }
            anterior = '_';
            avanzar(l);
        } else {
            break;
        }
    }
    if (anterior == '_' && hubo_digito) {
        *msg = "literal numérico no puede terminar en '_'";
        return false;
    }
    return hubo_digito;
}

/*
 * Tras `0x` o `0X`, consume los dígitos hex. Tras prefijo se permite
 * `_` inicial inmediato (0x_ff ✓), por eso reseteamos el sentinel.
 */
static Token escanear_base(Lexer *l,
                           bool (*es_digito_de_base)(char),
                           const char *nombre_base) {
    /* Permitimos '_' tras el prefijo de base: usar 'X' como sentinel. */
    char anterior = 'X';
    bool hubo_digito = false;
    while (true) {
        char c = mirar(l);
        if (es_digito_de_base(c)) {
            hubo_digito = true;
            anterior = c;
            avanzar(l);
        } else if (c == '_') {
            if (anterior == '_') {
                return token_error(l, "no se permiten guiones bajos consecutivos en literales numéricos");
            }
            anterior = '_';
            avanzar(l);
        } else {
            break;
        }
    }
    if (!hubo_digito) {
        /* Mensajes específicos por base; cadenas literales estáticas. */
        if (nombre_base[0] == 'h') {
            return token_error(l, "literal hexadecimal vacío tras '0x'");
        }
        if (nombre_base[0] == 'o') {
            return token_error(l, "literal octal vacío tras '0o'");
        }
        return token_error(l, "literal binario vacío tras '0b'");
    }
    if (anterior == '_') {
        return token_error(l, "literal numérico no puede terminar en '_'");
    }
    return crear_token(l, TT_ENTERO);
}

/*
 * Llamado tras consumir el primer dígito decimal. Determina si el
 * literal es entero o decimal según vea '.' (seguido de dígito) o
 * 'e'/'E'.
 */
static Token escanear_numero_decimal(Lexer *l) {
    const char *msg = NULL;
    /* La parte entera ya empezó (un dígito consumido); seguir leyendo. */
    /* Re-consumimos por simplicidad: el primer dígito ya cuenta como
       'hubo_digito' implícitamente. */
    while (true) {
        char c = mirar(l);
        if (es_digito(c)) {
            avanzar(l);
        } else if (c == '_') {
            if (l->actual > l->inicio_token && *(l->actual - 1) == '_') {
                return token_error(l, "no se permiten guiones bajos consecutivos en literales numéricos");
            }
            avanzar(l);
        } else {
            break;
        }
    }
    if (l->actual > l->inicio_token && *(l->actual - 1) == '_') {
        return token_error(l, "literal numérico no puede terminar en '_'");
    }

    bool es_decimal = false;

    /* Parte fraccionaria: '.' SOLO si tras el punto hay dígito.
       Si el punto va a otra cosa (atributo, separador), no consumir. */
    if (mirar(l) == '.' && es_digito(mirar_siguiente(l))) {
        es_decimal = true;
        avanzar(l); /* punto */
        if (!consumir_digitos(l, es_digito, &msg)) {
            return token_error(l, msg);
        }
    }

    /* Notación científica: 'e' o 'E' opcionalmente seguida de signo
       y obligatoriamente seguida de al menos un dígito. */
    if (mirar(l) == 'e' || mirar(l) == 'E') {
        es_decimal = true;
        avanzar(l);
        if (mirar(l) == '+' || mirar(l) == '-') avanzar(l);
        if (!es_digito(mirar(l))) {
            return token_error(l, "exponente vacío en literal decimal");
        }
        if (!consumir_digitos(l, es_digito, &msg)) {
            return token_error(l, msg);
        }
    }

    return crear_token(l, es_decimal ? TT_DECIMAL : TT_ENTERO);
}

/*
 * Punto de entrada al escaneo numérico. `primero` es el primer dígito
 * ya consumido (es_digito(primero) == true).
 */
static Token escanear_numero(Lexer *l, char primero) {
    /* Detectar bases especiales: solo si el primer dígito es '0'. */
    if (primero == '0') {
        char c = mirar(l);
        if (c == 'x' || c == 'X') {
            avanzar(l);
            return escanear_base(l, es_digito_hex, "hexadecimal");
        }
        if (c == 'o' || c == 'O') {
            avanzar(l);
            return escanear_base(l, es_digito_octal, "octal");
        }
        if (c == 'b' || c == 'B') {
            avanzar(l);
            return escanear_base(l, es_digito_binario, "binario");
        }
    }
    return escanear_numero_decimal(l);
}

/* ──────────────────────────────────────────────────────────────────
 * Escaneo de cadenas
 *
 * En sesión 2 soportamos cadenas de una línea con comilla simple `'`
 * o doble `"`. El lexema incluye las comillas; el parser hace unescape
 * cuando construye el nodo del AST.
 *
 * Escapes válidos: \n \t \r \\ \' \" \0 \x... \u...
 * La validación profunda de \x (2 hex dígitos) y \u (4 hex o
 * \u{HHHHHH}) llega en sesión 5. Aquí solo aceptamos el prefijo y
 * dejamos pasar lo que siga.
 *
 * Saltos de línea dentro de cadena simple son error: para multilínea
 * usar `"""..."""` (sesión 4).
 * ────────────────────────────────────────────────────────────────── */
static Token escanear_cadena(Lexer *l, char delimitador) {
    while (!en_fin(l) && mirar(l) != delimitador) {
        char c = mirar(l);
        if (c == '\n') {
            return token_error(l, "cadena sin cerrar antes del fin de línea");
        }
        if (c == '\\') {
            avanzar(l);
            if (en_fin(l)) {
                return token_error(l, "secuencia de escape sin completar al fin de archivo");
            }
            char esc = mirar(l);
            /* Aceptamos: n t r \\ ' " 0 x u — la validación de los
               argumentos de \x y \u se hace en sesión 5. Cualquier otra
               letra es escape no reconocido. */
            if (esc != 'n' && esc != 't' && esc != 'r' && esc != '\\' &&
                esc != '\'' && esc != '"' && esc != '0' && esc != 'x' &&
                esc != 'u') {
                return token_error(l, "secuencia de escape no reconocida");
            }
            avanzar(l);
            /* Si el escape consume más bytes (\xHH, \uHHHH, \u{H...}),
               los validaremos en sesión 5. Por ahora seguimos. */
            continue;
        }
        avanzar(l);
    }

    if (en_fin(l)) {
        return token_error(l, "cadena sin cerrar antes del fin de archivo");
    }

    avanzar(l); /* consumir delimitador de cierre */
    return crear_token(l, TT_CADENA);
}

/* ──────────────────────────────────────────────────────────────────
 * Identificadores y palabras clave
 *
 * Estructura:
 *   - Si el primer carácter del lexema es ASCII letter/_/$, fast path.
 *   - Si es no-ASCII (byte alto), decodificamos UTF-8 con utf8proc y
 *     verificamos que sea letra Unicode.
 *   - Tras consumir el primer carácter, se invoca el bucle común que
 *     extiende el identificador por cualquier carácter de continuación.
 *
 * El lexer asume que la fuente está en NFC (decisión B4); la
 * normalización es responsabilidad del que carga el archivo (ver
 * fuente.c). Para tests con ASCII puro NFC es no-op.
 * ────────────────────────────────────────────────────────────────── */

/*
 * Bucle compartido por identificadores ASCII y Unicode. Asume que ya
 * se ha consumido al menos un carácter válido de inicio. Avanza
 * mientras el siguiente carácter (ASCII o Unicode) es válido como
 * continuación de identificador.
 */
static void escanear_ident_continuar(Lexer *l) {
    while (true) {
        char c = mirar(l);
        if (c == '\0') break;
        if ((unsigned char)c < 0x80) {
            if (es_continua_ident_ascii(c)) {
                avanzar(l);
            } else {
                break;
            }
        } else {
            utf8proc_int32_t cp;
            utf8proc_ssize_t consumed = utf8proc_iterate(
                (const utf8proc_uint8_t *)l->actual, -1, &cp);
            if (consumed <= 0) break;
            if (!es_continua_ident_unicode((int32_t)cp)) break;
            l->actual += consumed;
        }
    }
}

/*
 * Tabla de keywords. Devuelve TT_IDENT si el lexema no coincide con
 * ninguna keyword. La estructura switch-on-first-char es similar a la
 * de clox, optimizada para que la mayoría de identificadores rechacen
 * en O(1) sin comparar más caracteres.
 */
#define COINCIDE(esperado)                                                     \
    (len == (int)(sizeof(esperado) - 1)                                        \
     && memcmp(texto, esperado, sizeof(esperado) - 1) == 0)

static TipoToken buscar_keyword(const char *texto, int len) {
    if (len == 0) return TT_IDENT;
    switch (texto[0]) {
        case 'a':
            if (COINCIDE("asincrono")) return TT_ASINCRONO;
            if (COINCIDE("atrapar"))   return TT_ATRAPAR;
            break;
        case 'b':
            if (COINCIDE("borrar"))    return TT_BORRAR;
            break;
        case 'c':
            if (COINCIDE("clase"))     return TT_CLASE;
            if (COINCIDE("coincidir")) return TT_COINCIDIR;
            if (COINCIDE("como"))      return TT_COMO;
            if (COINCIDE("con"))       return TT_CON;
            if (COINCIDE("continuar")) return TT_CONTINUAR;
            break;
        case 'd':
            if (COINCIDE("desde"))     return TT_DESDE;
            break;
        case 'e':
            if (COINCIDE("en"))        return TT_EN;
            if (COINCIDE("es"))        return TT_ES;
            if (COINCIDE("esperar"))   return TT_ESPERAR;
            if (COINCIDE("extiende"))  return TT_EXTIENDE;
            break;
        case 'f':
            if (COINCIDE("falso"))     return TT_FALSO;
            if (COINCIDE("fin"))       return TT_FIN;
            if (COINCIDE("finalmente"))return TT_FINALMENTE;
            if (COINCIDE("funcion"))   return TT_FUNCION;
            break;
        case 'g':
            if (COINCIDE("global"))    return TT_GLOBAL;
            break;
        case 'i':
            if (COINCIDE("importar"))  return TT_IMPORTAR;
            if (COINCIDE("intentar"))  return TT_INTENTAR;
            break;
        case 'l':
            if (COINCIDE("lambda"))    return TT_LAMBDA;
            if (COINCIDE("lanzar"))    return TT_LANZAR;
            break;
        case 'm':
            if (COINCIDE("mientras"))  return TT_MIENTRAS;
            break;
        case 'n':
            if (COINCIDE("no"))        return TT_NO;
            if (COINCIDE("nolocal"))   return TT_NOLOCAL;
            if (COINCIDE("nulo"))      return TT_NULO;
            break;
        case 'o':
            if (COINCIDE("o"))         return TT_O;
            break;
        case 'p':
            if (COINCIDE("para"))      return TT_PARA;
            if (COINCIDE("pasar"))     return TT_PASAR;
            if (COINCIDE("producir"))  return TT_PRODUCIR;
            break;
        case 'r':
            if (COINCIDE("retornar"))  return TT_RETORNAR;
            if (COINCIDE("romper"))    return TT_ROMPER;
            break;
        case 's':
            if (COINCIDE("si"))        return TT_SI;
            if (COINCIDE("sino"))      return TT_SINO;
            if (COINCIDE("super"))     return TT_SUPER;
            break;
        case 'v':
            if (COINCIDE("verdadero")) return TT_VERDADERO;
            break;
        case 'y':
            if (COINCIDE("y"))         return TT_Y;
            break;
    }
    return TT_IDENT;
}

#undef COINCIDE

/*
 * Camino rápido: el primer carácter es ASCII letter/_/$. Lo hemos
 * consumido ya en lexer_siguiente. Solo hay que extender el
 * identificador y consultar la tabla de keywords.
 */
static Token escanear_ident_ascii(Lexer *l) {
    escanear_ident_continuar(l);
    int len = (int)(l->actual - l->inicio_token);
    TipoToken tipo = buscar_keyword(l->inicio_token, len);
    return crear_token(l, tipo);
}

/*
 * Camino lento: el primer byte tenía bit alto. l->actual ya ha
 * avanzado UN BYTE (consumido en lexer_siguiente). Para usar
 * utf8proc desde el inicio del code point retrocedemos.
 *
 * El identificador resultante puede o no coincidir con una keyword.
 * En la práctica las keywords son ASCII, así que un identificador con
 * bytes Unicode nunca es keyword: optimizamos retornando TT_IDENT
 * directamente sin consultar la tabla.
 */
static Token escanear_ident_unicode(Lexer *l) {
    /* Retroceder al inicio del code point que disparó este caso. */
    l->actual = l->inicio_token;

    utf8proc_int32_t cp;
    utf8proc_ssize_t consumed = utf8proc_iterate(
        (const utf8proc_uint8_t *)l->actual, -1, &cp);
    if (consumed <= 0) {
        avanzar(l); /* consume el byte inválido para no bucle infinito */
        return token_error(l, "byte UTF-8 inválido");
    }
    if (!es_inicio_ident_unicode((int32_t)cp)) {
        l->actual += consumed;
        return token_error(l, "carácter no reconocido");
    }
    l->actual += consumed;

    escanear_ident_continuar(l);

    /* Identificadores con bytes Unicode no pueden ser keywords (todas
       las keywords de Cornamusa son ASCII por decisión B4). */
    return crear_token(l, TT_IDENT);
}

/* ──────────────────────────────────────────────────────────────────
 * API pública
 * ────────────────────────────────────────────────────────────────── */

void lexer_iniciar(Lexer *l, const char *fuente, const char *archivo) {
    l->fuente = fuente;
    l->actual = fuente;
    l->inicio_token = fuente;
    l->linea = 1;
    l->inicio_linea = fuente;
    l->archivo = archivo;
}

Token lexer_siguiente(Lexer *l) {
    saltar_irrelevante(l);

    l->inicio_token = l->actual;

    if (en_fin(l)) {
        return crear_token(l, TT_FIN_ARCHIVO);
    }

    char c = avanzar(l);

    /* Literales numéricos: el primer carácter es un dígito ASCII. */
    if (es_digito(c)) {
        return escanear_numero(l, c);
    }

    /* Cadenas literales: comilla simple o doble. */
    if (c == '"' || c == '\'') {
        return escanear_cadena(l, c);
    }

    /* Identificadores ASCII (camino rápido). */
    if (es_inicio_ident_ascii(c)) {
        return escanear_ident_ascii(l);
    }

    /* Bytes con bit alto: posible inicio de identificador Unicode. */
    if ((unsigned char)c >= 0x80) {
        return escanear_ident_unicode(l);
    }

    switch (c) {
        /* Símbolos individuales */
        case '(': return crear_token(l, TT_PARENT_IZQ);
        case ')': return crear_token(l, TT_PARENT_DER);
        case '[': return crear_token(l, TT_CORCH_IZQ);
        case ']': return crear_token(l, TT_CORCH_DER);
        case '{': return crear_token(l, TT_LLAVE_IZQ);
        case '}': return crear_token(l, TT_LLAVE_DER);
        case ',': return crear_token(l, TT_COMA);
        case '.': return crear_token(l, TT_PUNTO);
        case ':': return crear_token(l, TT_DOS_PUNTOS);
        case ';': return crear_token(l, TT_PUNTO_COMA);
        case '@': return crear_token(l, TT_AT);
        case '~': return crear_token(l, TT_TILDE_BIT);

        /* Aritméticos con posible '=' compuesto */
        case '+':
            return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_MAS : TT_MAS);
        case '-':
            if (coincidir(l, '=')) return crear_token(l, TT_ASIGNAR_MENOS);
            if (coincidir(l, '>')) return crear_token(l, TT_FLECHA);
            return crear_token(l, TT_MENOS);
        case '*':
            if (coincidir(l, '*')) {
                return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_DOBLE_ASTER
                                                        : TT_DOBLE_ASTERISCO);
            }
            return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_ASTERISCO
                                                    : TT_ASTERISCO);
        case '/':
            if (coincidir(l, '/')) {
                return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_DOBLE_BARRA
                                                        : TT_DOBLE_BARRA);
            }
            return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_BARRA : TT_BARRA);
        case '%':
            return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_PORCENTAJE
                                                    : TT_PORCENTAJE);

        /* Asignación / igualdad */
        case '=':
            return crear_token(l, coincidir(l, '=') ? TT_IGUAL : TT_ASIGNAR);

        /* '!' solo válido como '!=' */
        case '!':
            if (coincidir(l, '=')) return crear_token(l, TT_DISTINTO);
            return token_error(l, "carácter '!' inesperado (¿quisiste decir '!='?)");

        /* Comparaciones y desplazamientos */
        case '<':
            if (coincidir(l, '=')) return crear_token(l, TT_MENOR_IGUAL);
            if (coincidir(l, '<')) return crear_token(l, TT_DESPL_IZQ);
            return crear_token(l, TT_MENOR);
        case '>':
            if (coincidir(l, '=')) return crear_token(l, TT_MAYOR_IGUAL);
            if (coincidir(l, '>')) return crear_token(l, TT_DESPL_DER);
            return crear_token(l, TT_MAYOR);

        /* Bitwise simples */
        case '&': return crear_token(l, TT_AMPERSAND);
        case '|': return crear_token(l, TT_BARRA_VERT);
        case '^': return crear_token(l, TT_CIRCUNFLEJO);

        default:
            /* Identificadores, números y cadenas llegan en sesiones 2-3.
               Por ahora cualquier otro carácter es un error léxico. */
            return token_error(l, "carácter no reconocido");
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Nombre simbólico de un tipo de token (para tests y depuración)
 * ────────────────────────────────────────────────────────────────── */

const char *tipo_token_nombre(TipoToken t) {
    switch (t) {
        case TT_PARENT_IZQ:          return "TT_PARENT_IZQ";
        case TT_PARENT_DER:          return "TT_PARENT_DER";
        case TT_LLAVE_IZQ:           return "TT_LLAVE_IZQ";
        case TT_LLAVE_DER:           return "TT_LLAVE_DER";
        case TT_CORCH_IZQ:           return "TT_CORCH_IZQ";
        case TT_CORCH_DER:           return "TT_CORCH_DER";
        case TT_COMA:                return "TT_COMA";
        case TT_PUNTO:               return "TT_PUNTO";
        case TT_DOS_PUNTOS:          return "TT_DOS_PUNTOS";
        case TT_PUNTO_COMA:          return "TT_PUNTO_COMA";
        case TT_FLECHA:              return "TT_FLECHA";
        case TT_AT:                  return "TT_AT";

        case TT_MAS:                 return "TT_MAS";
        case TT_MENOS:               return "TT_MENOS";
        case TT_ASTERISCO:           return "TT_ASTERISCO";
        case TT_BARRA:               return "TT_BARRA";
        case TT_DOBLE_BARRA:         return "TT_DOBLE_BARRA";
        case TT_PORCENTAJE:          return "TT_PORCENTAJE";
        case TT_DOBLE_ASTERISCO:     return "TT_DOBLE_ASTERISCO";

        case TT_IGUAL:               return "TT_IGUAL";
        case TT_DISTINTO:            return "TT_DISTINTO";
        case TT_MENOR:               return "TT_MENOR";
        case TT_MENOR_IGUAL:         return "TT_MENOR_IGUAL";
        case TT_MAYOR:               return "TT_MAYOR";
        case TT_MAYOR_IGUAL:         return "TT_MAYOR_IGUAL";

        case TT_ASIGNAR:             return "TT_ASIGNAR";
        case TT_ASIGNAR_MAS:         return "TT_ASIGNAR_MAS";
        case TT_ASIGNAR_MENOS:       return "TT_ASIGNAR_MENOS";
        case TT_ASIGNAR_ASTERISCO:   return "TT_ASIGNAR_ASTERISCO";
        case TT_ASIGNAR_BARRA:       return "TT_ASIGNAR_BARRA";
        case TT_ASIGNAR_DOBLE_BARRA: return "TT_ASIGNAR_DOBLE_BARRA";
        case TT_ASIGNAR_PORCENTAJE:  return "TT_ASIGNAR_PORCENTAJE";
        case TT_ASIGNAR_DOBLE_ASTER: return "TT_ASIGNAR_DOBLE_ASTER";

        case TT_AMPERSAND:           return "TT_AMPERSAND";
        case TT_BARRA_VERT:          return "TT_BARRA_VERT";
        case TT_CIRCUNFLEJO:         return "TT_CIRCUNFLEJO";
        case TT_TILDE_BIT:           return "TT_TILDE_BIT";
        case TT_DESPL_IZQ:           return "TT_DESPL_IZQ";
        case TT_DESPL_DER:           return "TT_DESPL_DER";

        case TT_ENTERO:              return "TT_ENTERO";
        case TT_DECIMAL:             return "TT_DECIMAL";
        case TT_CADENA:              return "TT_CADENA";
        case TT_F_CADENA:            return "TT_F_CADENA";

        case TT_IDENT:               return "TT_IDENT";

        case TT_SI:                  return "TT_SI";
        case TT_SINO:                return "TT_SINO";
        case TT_MIENTRAS:            return "TT_MIENTRAS";
        case TT_PARA:                return "TT_PARA";
        case TT_EN:                  return "TT_EN";
        case TT_ROMPER:              return "TT_ROMPER";
        case TT_CONTINUAR:           return "TT_CONTINUAR";
        case TT_RETORNAR:            return "TT_RETORNAR";
        case TT_PASAR:               return "TT_PASAR";
        case TT_FIN:                 return "TT_FIN";

        case TT_FUNCION:             return "TT_FUNCION";
        case TT_LAMBDA:              return "TT_LAMBDA";
        case TT_CLASE:               return "TT_CLASE";
        case TT_EXTIENDE:            return "TT_EXTIENDE";
        case TT_SUPER:               return "TT_SUPER";
        case TT_IMPORTAR:            return "TT_IMPORTAR";
        case TT_DESDE:               return "TT_DESDE";
        case TT_COMO:                return "TT_COMO";
        case TT_GLOBAL:              return "TT_GLOBAL";
        case TT_NOLOCAL:             return "TT_NOLOCAL";

        case TT_INTENTAR:            return "TT_INTENTAR";
        case TT_ATRAPAR:             return "TT_ATRAPAR";
        case TT_FINALMENTE:          return "TT_FINALMENTE";
        case TT_LANZAR:              return "TT_LANZAR";

        case TT_Y:                   return "TT_Y";
        case TT_O:                   return "TT_O";
        case TT_NO:                  return "TT_NO";
        case TT_ES:                  return "TT_ES";

        case TT_VERDADERO:           return "TT_VERDADERO";
        case TT_FALSO:               return "TT_FALSO";
        case TT_NULO:                return "TT_NULO";

        case TT_PRODUCIR:            return "TT_PRODUCIR";
        case TT_ASINCRONO:           return "TT_ASINCRONO";
        case TT_ESPERAR:             return "TT_ESPERAR";
        case TT_CON:                 return "TT_CON";
        case TT_BORRAR:              return "TT_BORRAR";
        case TT_COINCIDIR:           return "TT_COINCIDIR";

        case TT_FIN_ARCHIVO:         return "TT_FIN_ARCHIVO";
        case TT_ERROR:               return "TT_ERROR";
    }
    return "TT_DESCONOCIDO";
}
