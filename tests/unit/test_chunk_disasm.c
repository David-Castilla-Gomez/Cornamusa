/*
 * Tests del bytecode chunk y disassembler — Fase 6 sesión 1.
 *
 * Cobertura:
 *   - chunk_iniciar/destruir con y sin contenido.
 *   - chunk_emitir_byte y chunk_emitir_byte2 (capacity growth).
 *   - chunk_agregar_constante: ownership + índices secuenciales.
 *   - chunk_emitir_constante: usa OP_CONST cuando idx <= 255 y
 *     OP_CONST_LARGO cuando supera.
 *   - opcode_nombre cubre todos los opcodes válidos.
 *   - desensamblar_chunk produce el formato esperado.
 */

#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "debug.h"
#include "valor.h"

static int fallos = 0;

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n",                            \
                    __FILE__, __LINE__, #cond);                                \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

#define AFIRMAR_CADENA_CONTIENE(haystack, needle)                              \
    do {                                                                       \
        if (strstr((haystack), (needle)) == NULL) {                            \
            fprintf(stderr, "FALLO en %s:%d: '%s' no contiene '%s'\n",         \
                    __FILE__, __LINE__, (haystack), (needle));                 \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

/* Captura `desensamblar_chunk` en un buffer en memoria (vía tmpfile).
   Devuelve el contenido como cadena estática. */
static const char *desensamblar_a_buffer(const Chunk *c, const char *nombre) {
    static char buffer[8192];
    FILE *f = tmpfile();
    if (!f) return NULL;
    desensamblar_chunk(c, nombre, f);
    fflush(f);
    rewind(f);
    size_t n = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[n] = '\0';
    fclose(f);
    return buffer;
}

/* ───── chunk básicos ───── */

static void test_chunk_vacio(void) {
    Chunk c;
    chunk_iniciar(&c);
    AFIRMAR(c.codigo == NULL);
    AFIRMAR(c.cuenta == 0);
    AFIRMAR(c.capacidad == 0);
    AFIRMAR(c.constantes_cuenta == 0);
    chunk_destruir(&c);
    /* Idempotente: segunda destrucción no rompe. */
    chunk_destruir(&c);
}

static void test_emitir_bytes(void) {
    Chunk c;
    chunk_iniciar(&c);
    for (int i = 0; i < 100; i++) {
        chunk_emitir_byte(&c, (uint8_t)(i & 0xff), i);
    }
    AFIRMAR(c.cuenta == 100);
    AFIRMAR(c.codigo[0] == 0);
    AFIRMAR(c.codigo[99] == 99);
    AFIRMAR(c.lineas[0] == 0);
    AFIRMAR(c.lineas[99] == 99);
    chunk_destruir(&c);
}

static void test_emitir_byte2(void) {
    Chunk c;
    chunk_iniciar(&c);
    chunk_emitir_byte2(&c, OP_CONST, 7, 42);
    AFIRMAR(c.cuenta == 2);
    AFIRMAR(c.codigo[0] == OP_CONST);
    AFIRMAR(c.codigo[1] == 7);
    AFIRMAR(c.lineas[0] == 42);
    AFIRMAR(c.lineas[1] == 42);
    chunk_destruir(&c);
}

/* ───── Constantes ───── */

static void test_agregar_constante(void) {
    Chunk c;
    chunk_iniciar(&c);
    int i1 = chunk_agregar_constante(&c, valor_entero_de_long(42));
    int i2 = chunk_agregar_constante(&c, valor_decimal(3.14));
    int i3 = chunk_agregar_constante(&c, valor_cadena_duplicar("hola", 4));
    AFIRMAR(i1 == 0);
    AFIRMAR(i2 == 1);
    AFIRMAR(i3 == 2);
    AFIRMAR(c.constantes_cuenta == 3);
    AFIRMAR(c.constantes[0].tipo == VAL_ENTERO);
    AFIRMAR(c.constantes[1].tipo == VAL_DECIMAL);
    AFIRMAR(c.constantes[2].tipo == VAL_CADENA);
    /* destruir libera mp_int y la cadena duplicada — sin leaks. */
    chunk_destruir(&c);
}

static void test_emitir_constante_corta(void) {
    Chunk c;
    chunk_iniciar(&c);
    chunk_emitir_constante(&c, valor_entero_de_long(100), 1);
    AFIRMAR(c.cuenta == 2);
    AFIRMAR(c.codigo[0] == OP_CONST);
    AFIRMAR(c.codigo[1] == 0);   /* primer índice */
    chunk_destruir(&c);
}

static void test_emitir_constante_larga(void) {
    Chunk c;
    chunk_iniciar(&c);
    /* Llenamos el pool con 256 constantes para forzar OP_CONST_LARGO. */
    for (int i = 0; i < 256; i++) {
        chunk_agregar_constante(&c, valor_entero_de_long(i));
    }
    /* Esta es la 257ª, índice 256 → fuera de uint8_t. */
    chunk_emitir_constante(&c, valor_entero_de_long(999), 1);
    /* Estructura esperada: OP_CONST_LARGO, 0x00, 0x01, 0x00 (256 little
       endian). */
    AFIRMAR(c.codigo[0] == OP_CONST_LARGO);
    AFIRMAR(c.codigo[1] == 0x00);
    AFIRMAR(c.codigo[2] == 0x01);
    AFIRMAR(c.codigo[3] == 0x00);
    AFIRMAR(c.constantes_cuenta == 257);
    chunk_destruir(&c);
}

/* ───── Disassembler ───── */

static void test_disasm_simple(void) {
    Chunk c;
    chunk_iniciar(&c);
    chunk_emitir_constante(&c, valor_entero_de_long(42), 1);
    chunk_emitir_byte(&c, OP_RETORNAR, 1);

    const char *salida = desensamblar_a_buffer(&c, "test");
    AFIRMAR(salida != NULL);
    AFIRMAR_CADENA_CONTIENE(salida, "== test ==");
    AFIRMAR_CADENA_CONTIENE(salida, "OP_CONST");
    AFIRMAR_CADENA_CONTIENE(salida, "OP_RETORNAR");
    AFIRMAR_CADENA_CONTIENE(salida, "'42'");
    /* La segunda instrucción comparte línea con la primera → caret `|` */
    AFIRMAR_CADENA_CONTIENE(salida, "    | ");
    chunk_destruir(&c);
}

static void test_disasm_aritmetica(void) {
    Chunk c;
    chunk_iniciar(&c);
    /* (1 + 2) * 3 → CONST 1, CONST 2, SUMAR, CONST 3, MULTIPLICAR, RET */
    chunk_emitir_constante(&c, valor_entero_de_long(1), 1);
    chunk_emitir_constante(&c, valor_entero_de_long(2), 1);
    chunk_emitir_byte(&c, OP_SUMAR, 1);
    chunk_emitir_constante(&c, valor_entero_de_long(3), 1);
    chunk_emitir_byte(&c, OP_MULTIPLICAR, 1);
    chunk_emitir_byte(&c, OP_RETORNAR, 1);

    const char *salida = desensamblar_a_buffer(&c, "aritm");
    AFIRMAR_CADENA_CONTIENE(salida, "OP_SUMAR");
    AFIRMAR_CADENA_CONTIENE(salida, "OP_MULTIPLICAR");
    AFIRMAR_CADENA_CONTIENE(salida, "'1'");
    AFIRMAR_CADENA_CONTIENE(salida, "'2'");
    AFIRMAR_CADENA_CONTIENE(salida, "'3'");
    chunk_destruir(&c);
}

static void test_disasm_cambio_de_linea(void) {
    Chunk c;
    chunk_iniciar(&c);
    chunk_emitir_byte(&c, OP_NULO, 1);
    chunk_emitir_byte(&c, OP_VERDADERO, 2);
    chunk_emitir_byte(&c, OP_FALSO, 2);

    const char *salida = desensamblar_a_buffer(&c, "lineas");
    /* Las líneas distintas se imprimen explícitas; las repetidas con `|`. */
    AFIRMAR(strstr(salida, "    1 OP_NULO") != NULL);
    AFIRMAR(strstr(salida, "    2 OP_VERDADERO") != NULL);
    AFIRMAR(strstr(salida, "    | OP_FALSO") != NULL);
    chunk_destruir(&c);
}

static void test_opcode_nombre(void) {
    AFIRMAR(strcmp(opcode_nombre(OP_CONST), "OP_CONST") == 0);
    AFIRMAR(strcmp(opcode_nombre(OP_RETORNAR), "OP_RETORNAR") == 0);
    AFIRMAR(strcmp(opcode_nombre(OP_NEGAR), "OP_NEGAR") == 0);
    AFIRMAR(strcmp(opcode_nombre(OP_LLAMAR), "OP_LLAMAR") == 0);
    /* Opcode inexistente → NULL */
    AFIRMAR(opcode_nombre((OpCode)255) == NULL);
}

/* ───── Constantes propietarias ───── */

static void test_chunk_dueno_de_constantes(void) {
    /* Si chunk_destruir libera bien las constantes, no hay leak. Lo
     * verificamos indirectamente: añadimos una cadena con dueño y
     * comprobamos que la lectura tras emisión sigue siendo correcta. */
    Chunk c;
    chunk_iniciar(&c);
    chunk_emitir_constante(&c, valor_cadena_duplicar("xyz", 3), 1);
    AFIRMAR(c.constantes_cuenta == 1);
    AFIRMAR(c.constantes[0].tipo == VAL_CADENA);
    AFIRMAR(c.constantes[0].dueno_cadena);
    AFIRMAR(c.constantes[0].como.cadena.longitud == 3);
    chunk_destruir(&c);
}

int main(void) {
    test_chunk_vacio();
    test_emitir_bytes();
    test_emitir_byte2();
    test_agregar_constante();
    test_emitir_constante_corta();
    test_emitir_constante_larga();
    test_disasm_simple();
    test_disasm_aritmetica();
    test_disasm_cambio_de_linea();
    test_opcode_nombre();
    test_chunk_dueno_de_constantes();

    if (fallos == 0) {
        printf("OK: todos los tests del chunk y disassembler pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
