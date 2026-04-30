/*
 * Tests del runtime — Fase 4 Sesión 1: Valor y Entorno.
 *
 * Cobertura:
 *   - Construcción y destrucción de cada tipo de Valor.
 *   - Bignum: enteros grandes, aritmética básica vía libtommath.
 *   - Conversión a cadena (formato pretty-print).
 *   - Verdadez según ESPEC §6.2.
 *   - Igualdad estructural según ESPEC §6.3 (incluye entero==decimal).
 *   - Entorno: definir, obtener, asignar, scope chain.
 *   - Sin leaks de memoria (verificado con destrucciones explícitas;
 *     valgrind/ASan en CI cubre fugas reales).
 */

#include <stdio.h>
#include <string.h>

#include "valor.h"
#include "entorno.h"
#include "tommath.h"

static int fallos = 0;

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n",                            \
                    __FILE__, __LINE__, #cond);                                \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

#define AFIRMAR_CADENA(v_ptr, esperada)                                        \
    do {                                                                       \
        char buffer[256];                                                      \
        valor_a_cadena((v_ptr), buffer, sizeof(buffer));                       \
        if (strcmp(buffer, esperada) != 0) {                                   \
            fprintf(stderr, "FALLO en %s:%d:\n  esperaba: %s\n  obtuvo:   %s\n",\
                    __FILE__, __LINE__, esperada, buffer);                     \
            fallos++;                                                          \
        }                                                                      \
    } while (0)

/* ───── Construcción de cada tipo ───── */

static void test_valor_nulo(void) {
    Valor v = valor_nulo();
    AFIRMAR(v.tipo == VAL_NULO);
    AFIRMAR_CADENA(&v, "nulo");
    AFIRMAR(strcmp(valor_nombre_tipo(&v), "nulo") == 0);
    valor_destruir(&v);
}

static void test_valor_booleano(void) {
    Valor t = valor_booleano(true);
    Valor f = valor_booleano(false);
    AFIRMAR_CADENA(&t, "verdadero");
    AFIRMAR_CADENA(&f, "falso");
    AFIRMAR(strcmp(valor_nombre_tipo(&t), "booleano") == 0);
    valor_destruir(&t);
    valor_destruir(&f);
}

static void test_valor_decimal(void) {
    Valor v = valor_decimal(3.14);
    AFIRMAR(v.tipo == VAL_DECIMAL);
    AFIRMAR_CADENA(&v, "3.14");
    valor_destruir(&v);
}

static void test_valor_decimal_entero_visual(void) {
    /* 5.0 debe imprimirse como "5.0" no "5" para distinguir de int. */
    Valor v = valor_decimal(5.0);
    AFIRMAR_CADENA(&v, "5.0");
    valor_destruir(&v);
}

static void test_valor_entero_pequeno(void) {
    Valor v = valor_entero_de_lexema("42", 2);
    AFIRMAR(valor_es_entero(&v));  /* SMALL o BIG, ambos válidos */
    AFIRMAR_CADENA(&v, "42");
    valor_destruir(&v);
}

static void test_valor_entero_con_underscores(void) {
    Valor v = valor_entero_de_lexema("1_000_000", 9);
    AFIRMAR_CADENA(&v, "1000000");
    valor_destruir(&v);
}

static void test_valor_entero_hex(void) {
    Valor v = valor_entero_de_lexema("0xff", 4);
    AFIRMAR_CADENA(&v, "255");
    valor_destruir(&v);
}

static void test_valor_entero_octal(void) {
    Valor v = valor_entero_de_lexema("0o755", 5);
    AFIRMAR_CADENA(&v, "493");
    valor_destruir(&v);
}

static void test_valor_entero_binario(void) {
    Valor v = valor_entero_de_lexema("0b1010", 6);
    AFIRMAR_CADENA(&v, "10");
    valor_destruir(&v);
}

static void test_valor_entero_de_long(void) {
    Valor v = valor_entero_de_long(123456L);
    AFIRMAR_CADENA(&v, "123456");
    valor_destruir(&v);

    Valor neg = valor_entero_de_long(-99L);
    AFIRMAR_CADENA(&neg, "-99");
    valor_destruir(&neg);
}

static void test_valor_entero_grande(void) {
    /* factorial(20) = 2_432_902_008_176_640_000 — cabe en i64.
       Probamos con un número aún más grande para forzar bignum. */
    Valor v = valor_entero_de_lexema("123456789012345678901234567890", 30);
    AFIRMAR_CADENA(&v, "123456789012345678901234567890");
    valor_destruir(&v);
}

/* ───── Bignum: aritmética básica ───── */

static void test_bignum_factorial_100(void) {
    /* Calcular 100! usando la API de libtommath para verificar que
       enteros grandes funcionan correctamente.
       100! = 9332621544394415268169923885626670049071596826438162146859296389521759999322991560894146397615651828625369792082722375825118521091686400000000000000000000000
       v0.11: usa los helpers porque los factores 2..62 son SMALL. */
    Valor v = valor_entero_de_long(1);
    for (long i = 2; i <= 100; i++) {
        Valor factor = valor_entero_de_long(i);
        bool propio_v, propio_f;
        mp_int *mv = valor_entero_a_mp_int(&v, &propio_v);
        mp_int *mf = valor_entero_a_mp_int(&factor, &propio_f);
        mp_int *temp = (mp_int *)malloc(sizeof(mp_int));
        mp_init(temp);
        mp_mul(mv, mf, temp);
        if (propio_v) { mp_clear(mv); free(mv); }
        if (propio_f) { mp_clear(mf); free(mf); }
        valor_destruir(&v);
        valor_destruir(&factor);
        v = valor_entero_de_mp_normalizado(temp);
    }
    /* Verificar que tiene 158 dígitos (100! tiene 158). */
    char buffer[1024];
    int n = valor_a_cadena(&v, buffer, sizeof(buffer));
    AFIRMAR(n == 158);
    /* Primeros y últimos dígitos para sanity check. */
    AFIRMAR(strncmp(buffer, "93326215443944", 14) == 0);
    valor_destruir(&v);
}

/* ───── Verdadez ───── */

static void test_verdadez_basica(void) {
    Valor n = valor_nulo();
    Valor t = valor_booleano(true);
    Valor f = valor_booleano(false);
    Valor cero = valor_entero_de_long(0);
    Valor uno = valor_entero_de_long(1);
    Valor cero_d = valor_decimal(0.0);
    Valor pi = valor_decimal(3.14);
    Valor cad_vacia = valor_cadena_referencia("", 0);
    Valor cad_hola = valor_cadena_referencia("hola", 4);

    AFIRMAR(!valor_es_verdadero(&n));
    AFIRMAR(valor_es_verdadero(&t));
    AFIRMAR(!valor_es_verdadero(&f));
    AFIRMAR(!valor_es_verdadero(&cero));
    AFIRMAR(valor_es_verdadero(&uno));
    AFIRMAR(!valor_es_verdadero(&cero_d));
    AFIRMAR(valor_es_verdadero(&pi));
    AFIRMAR(!valor_es_verdadero(&cad_vacia));
    AFIRMAR(valor_es_verdadero(&cad_hola));

    valor_destruir(&n);
    valor_destruir(&t);
    valor_destruir(&f);
    valor_destruir(&cero);
    valor_destruir(&uno);
    valor_destruir(&cero_d);
    valor_destruir(&pi);
    valor_destruir(&cad_vacia);
    valor_destruir(&cad_hola);
}

/* ───── Igualdad ───── */

static void test_igualdad_mismo_tipo(void) {
    Valor a = valor_entero_de_long(42);
    Valor b = valor_entero_de_long(42);
    Valor c = valor_entero_de_long(99);
    AFIRMAR(valor_iguales(&a, &b));
    AFIRMAR(!valor_iguales(&a, &c));
    valor_destruir(&a);
    valor_destruir(&b);
    valor_destruir(&c);
}

static void test_igualdad_entero_decimal(void) {
    /* 1 == 1.0 según ESPEC §6.3. */
    Valor i = valor_entero_de_long(1);
    Valor d = valor_decimal(1.0);
    AFIRMAR(valor_iguales(&i, &d));
    AFIRMAR(valor_iguales(&d, &i));
    valor_destruir(&i);
    valor_destruir(&d);
}

static void test_igualdad_tipos_distintos(void) {
    Valor i = valor_entero_de_long(1);
    Valor s = valor_cadena_referencia("1", 1);
    /* "1" != 1 aunque visualmente parezcan iguales. */
    AFIRMAR(!valor_iguales(&i, &s));
    valor_destruir(&i);
    valor_destruir(&s);
}

static void test_igualdad_cadenas(void) {
    Valor a = valor_cadena_referencia("hola", 4);
    Valor b = valor_cadena_duplicar("hola", 4);
    Valor c = valor_cadena_referencia("adios", 5);
    AFIRMAR(valor_iguales(&a, &b));    /* mismo contenido, distintos buffers */
    AFIRMAR(!valor_iguales(&a, &c));
    valor_destruir(&a);
    valor_destruir(&b);
    valor_destruir(&c);
}

/* ───── Clonación ───── */

static void test_clonar_entero_es_independiente(void) {
    /* v0.11: clonar un SMALL es copia trivial de la unión. Para BIG
       sí hay alocación separada. La invariante observable es que
       destruir uno no afecta al otro. */
    Valor a = valor_entero_de_long(42);
    Valor b = valor_clonar(&a);
    if (a.tipo == VAL_ENTERO && b.tipo == VAL_ENTERO) {
        AFIRMAR(a.como.entero != b.como.entero);  /* BIG: diferentes mp_int */
    }
    AFIRMAR(valor_iguales(&a, &b));
    valor_destruir(&a);
    /* Tras destruir a, b debe seguir siendo válido. */
    AFIRMAR(valor_es_entero(&b));
    char buf[64];
    valor_a_cadena(&b, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "42") == 0);
    valor_destruir(&b);
}

/* ───── Entorno ───── */

static void test_entorno_definir_y_obtener(void) {
    Entorno e;
    entorno_iniciar(&e, NULL);

    AFIRMAR(entorno_definir(&e, "x", 1, valor_entero_de_long(42)));

    Valor recuperado;
    AFIRMAR(entorno_obtener(&e, "x", 1, &recuperado));
    AFIRMAR(valor_es_entero(&recuperado));
    char buf[64];
    valor_a_cadena(&recuperado, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "42") == 0);
    valor_destruir(&recuperado);

    entorno_destruir(&e);
}

static void test_entorno_obtener_inexistente(void) {
    Entorno e;
    entorno_iniciar(&e, NULL);
    Valor v;
    AFIRMAR(!entorno_obtener(&e, "no_existe", 9, &v));
    entorno_destruir(&e);
}

static void test_entorno_sobreescritura(void) {
    Entorno e;
    entorno_iniciar(&e, NULL);
    entorno_definir(&e, "x", 1, valor_entero_de_long(1));
    entorno_definir(&e, "x", 1, valor_entero_de_long(2));
    Valor v;
    entorno_obtener(&e, "x", 1, &v);
    char buf[64];
    valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "2") == 0);
    valor_destruir(&v);
    entorno_destruir(&e);
}

static void test_entorno_asignar_existente(void) {
    Entorno e;
    entorno_iniciar(&e, NULL);
    entorno_definir(&e, "y", 1, valor_entero_de_long(10));
    AFIRMAR(entorno_asignar(&e, "y", 1, valor_entero_de_long(99)));

    Valor v;
    entorno_obtener(&e, "y", 1, &v);
    char buf[64];
    valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "99") == 0);
    valor_destruir(&v);
    entorno_destruir(&e);
}

static void test_entorno_asignar_inexistente(void) {
    Entorno e;
    entorno_iniciar(&e, NULL);
    /* Asignar a variable inexistente debe fallar (devolver false). */
    AFIRMAR(!entorno_asignar(&e, "z", 1, valor_entero_de_long(1)));
    entorno_destruir(&e);
}

static void test_entorno_scope_chain(void) {
    /* Padre define x. Hijo busca x — debe encontrarlo en el padre. */
    Entorno padre;
    entorno_iniciar(&padre, NULL);
    entorno_definir(&padre, "x", 1, valor_entero_de_long(42));

    Entorno hijo;
    entorno_iniciar(&hijo, &padre);

    Valor v;
    AFIRMAR(entorno_obtener(&hijo, "x", 1, &v));
    char buf[64];
    valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "42") == 0);
    valor_destruir(&v);

    /* Asignar a x desde el hijo debe modificar la del padre. */
    entorno_asignar(&hijo, "x", 1, valor_entero_de_long(100));
    entorno_obtener(&padre, "x", 1, &v);
    valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "100") == 0);
    valor_destruir(&v);

    entorno_destruir(&hijo);
    entorno_destruir(&padre);
}

static void test_entorno_shadow(void) {
    /* Hijo define x localmente — sombra a la del padre. */
    Entorno padre;
    entorno_iniciar(&padre, NULL);
    entorno_definir(&padre, "x", 1, valor_entero_de_long(1));

    Entorno hijo;
    entorno_iniciar(&hijo, &padre);
    entorno_definir(&hijo, "x", 1, valor_entero_de_long(2));

    Valor v;
    entorno_obtener(&hijo, "x", 1, &v);
    char buf[64];
    valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "2") == 0);  /* hijo ve su propia x */
    valor_destruir(&v);

    entorno_obtener(&padre, "x", 1, &v);
    valor_a_cadena(&v, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "1") == 0);  /* padre conserva la suya */
    valor_destruir(&v);

    entorno_destruir(&hijo);
    entorno_destruir(&padre);
}

static void test_entorno_muchas_variables(void) {
    /* Forzar redimensionamientos: añadir 100 variables. */
    Entorno e;
    entorno_iniciar(&e, NULL);

    char nombres[100][16];
    for (int i = 0; i < 100; i++) {
        snprintf(nombres[i], sizeof(nombres[i]), "var_%d", i);
        AFIRMAR(entorno_definir(&e, nombres[i], (int)strlen(nombres[i]),
                                  valor_entero_de_long((long)i)));
    }

    /* Verificar que todas siguen accesibles. */
    for (int i = 0; i < 100; i++) {
        Valor v;
        AFIRMAR(entorno_obtener(&e, nombres[i], (int)strlen(nombres[i]), &v));
        char buf[64];
        valor_a_cadena(&v, buf, sizeof(buf));
        char esperado[16];
        snprintf(esperado, sizeof(esperado), "%d", i);
        AFIRMAR(strcmp(buf, esperado) == 0);
        valor_destruir(&v);
    }

    entorno_destruir(&e);
}

int main(void) {
    /* Construcción */
    test_valor_nulo();
    test_valor_booleano();
    test_valor_decimal();
    test_valor_decimal_entero_visual();
    test_valor_entero_pequeno();
    test_valor_entero_con_underscores();
    test_valor_entero_hex();
    test_valor_entero_octal();
    test_valor_entero_binario();
    test_valor_entero_de_long();
    test_valor_entero_grande();

    /* Bignum */
    test_bignum_factorial_100();

    /* Verdadez */
    test_verdadez_basica();

    /* Igualdad */
    test_igualdad_mismo_tipo();
    test_igualdad_entero_decimal();
    test_igualdad_tipos_distintos();
    test_igualdad_cadenas();

    /* Clonación */
    test_clonar_entero_es_independiente();

    /* Entorno */
    test_entorno_definir_y_obtener();
    test_entorno_obtener_inexistente();
    test_entorno_sobreescritura();
    test_entorno_asignar_existente();
    test_entorno_asignar_inexistente();
    test_entorno_scope_chain();
    test_entorno_shadow();
    test_entorno_muchas_variables();

    if (fallos == 0) {
        printf("test_runtime_valor: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stdout, "test_runtime_valor: %d fallo(s)\n", fallos);
    return 1;
}
