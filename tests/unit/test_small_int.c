/*
 * Tests de boundaries para small-int tagging (B9, v0.11).
 *
 * Cubre los casos peligrosos identificados en decisiones/B9-small-int-tagging.md:
 *   - Overflow SMALL+SMALL → BIG correcto (suma, resta, mult).
 *   - SMALL_MIN / -1 (UB en C) → promote a BIG.
 *   - SMALL_MIN * -1 → promote a BIG.
 *   - mp_neg(SMALL_MIN) → promote a BIG.
 *   - Hash divergente SMALL(5) vs BIG(5) — deben colisionar al mismo slot.
 *   - Igualdad cross-tag SMALL(5) == BIG(5) y == 5.0 == True.
 *   - Comparaciones cross-tag con valores en frontera.
 *   - valor_entero_de_mp_normalizado: BIG demote a SMALL si cabe.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tommath.h"
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

/* Construye un VAL_ENTERO BIG forzado (sin pasar por _normalizado),
   para tests que quieren verificar comportamiento cross-tag aunque
   el valor cabría en SMALL. */
static Valor forzar_big(int64_t n) {
    mp_int *m = (mp_int *)malloc(sizeof(mp_int));
    mp_err r = mp_init(m); (void)r;
    mp_set_i64(m, n);
    Valor v;
    v.tipo = VAL_ENTERO;
    v.dueno_cadena = false;
    v.como.entero = m;
    return v;
}

/* ───── 1. Constructor: rango SMALL produce SMALL, fuera produce BIG ───── */

static void test_constructor_rango_small(void) {
    Valor v0 = valor_entero_de_i64(0);
    Valor v1 = valor_entero_de_i64(1);
    Valor vneg = valor_entero_de_i64(-1);
    Valor vmax = valor_entero_de_i64(CORNAMUSA_SMALL_INT_MAX);
    Valor vmin = valor_entero_de_i64(CORNAMUSA_SMALL_INT_MIN);

    AFIRMAR(v0.tipo == VAL_ENTERO_SMALL);
    AFIRMAR(v1.tipo == VAL_ENTERO_SMALL);
    AFIRMAR(vneg.tipo == VAL_ENTERO_SMALL);
    AFIRMAR(vmax.tipo == VAL_ENTERO_SMALL);
    AFIRMAR(vmin.tipo == VAL_ENTERO_SMALL);

    /* Justo fuera del rango: BIG. */
    Valor vmax_plus1 = valor_entero_de_i64(CORNAMUSA_SMALL_INT_MAX + 1);
    Valor vmin_minus1 = valor_entero_de_i64(CORNAMUSA_SMALL_INT_MIN - 1);
    AFIRMAR(vmax_plus1.tipo == VAL_ENTERO);
    AFIRMAR(vmin_minus1.tipo == VAL_ENTERO);

    valor_destruir(&v0); valor_destruir(&v1); valor_destruir(&vneg);
    valor_destruir(&vmax); valor_destruir(&vmin);
    valor_destruir(&vmax_plus1); valor_destruir(&vmin_minus1);
}

/* ───── 2. valor_entero_de_mp_normalizado: BIG demote a SMALL ───── */

static void test_normalizado_demote(void) {
    /* mp_int con valor 42 → debe demote a SMALL. */
    mp_int *m = (mp_int *)malloc(sizeof(mp_int));
    mp_err r1 = mp_init(m); (void)r1;
    mp_set_i64(m, 42);
    Valor v = valor_entero_de_mp_normalizado(m);
    AFIRMAR(v.tipo == VAL_ENTERO_SMALL);
    int64_t out;
    AFIRMAR(valor_entero_a_i64(&v, &out));
    AFIRMAR(out == 42);
    valor_destruir(&v);

    /* mp_int con valor 2^63 → no cabe en SMALL_MAX (2^62-1), queda BIG. */
    mp_int *big = (mp_int *)malloc(sizeof(mp_int));
    mp_err r2 = mp_init(big); (void)r2;
    mp_err r3 = mp_2expt(big, 63); (void)r3;  /* 2^63 */
    Valor v_big = valor_entero_de_mp_normalizado(big);
    AFIRMAR(v_big.tipo == VAL_ENTERO);
    valor_destruir(&v_big);
}

/* ───── 3. Igualdad cross-tag: SMALL(5) == BIG(5) == 5.0 == True ───── */

static void test_igualdad_cross_tag(void) {
    Valor s5 = valor_entero_de_i64(5);
    Valor b5 = forzar_big(5);
    Valor d5 = valor_decimal(5.0);
    Valor t = valor_booleano(true);
    Valor t5 = valor_entero_de_i64(1);  /* True == 1 */

    AFIRMAR(s5.tipo == VAL_ENTERO_SMALL);
    AFIRMAR(b5.tipo == VAL_ENTERO);
    AFIRMAR(valor_iguales(&s5, &b5));   /* SMALL == BIG con mismo valor */
    AFIRMAR(valor_iguales(&s5, &d5));   /* SMALL == DECIMAL */
    AFIRMAR(valor_iguales(&b5, &d5));   /* BIG == DECIMAL */
    AFIRMAR(valor_iguales(&t, &t5));    /* True == 1 (SMALL) */

    /* Distinto valor: false. */
    Valor s6 = valor_entero_de_i64(6);
    AFIRMAR(!valor_iguales(&s5, &s6));
    AFIRMAR(!valor_iguales(&b5, &s6));

    valor_destruir(&s5); valor_destruir(&b5); valor_destruir(&d5);
    valor_destruir(&t); valor_destruir(&t5); valor_destruir(&s6);
}

/* ───── 4. Hash equivalente SMALL/BIG/Bool ───── */

/* Indirecto: usamos un Diccionario para verificar que SMALL(5) y BIG(5)
   acceden al mismo slot. */
static void test_hash_equivalente(void) {
    Diccionario *d = dicc_nuevo();
    Valor s5 = valor_entero_de_i64(5);
    Valor v_uno = valor_cadena_duplicar("uno", 3);
    AFIRMAR(dicc_asignar(d, s5, v_uno));  /* d[5_SMALL] = "uno" */

    /* Buscar con clave BIG con mismo valor: debe encontrarla. */
    Valor b5 = forzar_big(5);
    Valor out;
    AFIRMAR(dicc_obtener(d, &b5, &out));
    char buf[64];
    valor_a_cadena(&out, buf, sizeof(buf));
    AFIRMAR(strcmp(buf, "uno") == 0);

    valor_destruir(&out);
    valor_destruir(&b5);
    /* d toma posesión de s5 y v_uno, no destruir. */
    dicc_liberar(d);
}

/* ───── 5. Aritmética: overflow SMALL+SMALL promueve a BIG ───── */

/* Verificamos vía el evaluador (donde está small_op_small). Pero como
   este test es unit puro (no levanta evaluador completo), llamamos
   directamente al wrapper público y a valor_entero_de_i64. */
#include "evaluador.h"
#include "lexer.h"  /* para TT_MAS, etc. */

/*
 * Validador común para casos donde dos resultados son aceptables:
 *   - aplic=true + tipo=VAL_ENTERO (promovido a BIG con valor esperado).
 *   - aplic=false (caller hace fallback BIG; verificamos que NO produjo
 *     un Valor SMALL erróneo).
 */
static void verificar_overflow_promueve(bool aplic, Valor r,
                                          int64_t valor_esperado) {
    if (aplic) {
        AFIRMAR(r.tipo == VAL_ENTERO);
        int64_t out;
        AFIRMAR(valor_entero_a_i64(&r, &out));
        AFIRMAR(out == valor_esperado);
    } else {
        /* Si no aplicable, el resultado debe ser nulo (sentinel). */
        AFIRMAR(r.tipo == VAL_NULO);
    }
    valor_destruir(&r);
}

static void test_overflow_suma_promueve(void) {
    /* SMALL_MAX + 1 = 2^62. Con margen 2^62 cabe en int64; valor_entero_de_i64
       lo demote a BIG porque excede SMALL_INT_MAX. */
    bool aplic;
    Valor r = evaluador_small_op_small(NULL, TT_MAS,
        CORNAMUSA_SMALL_INT_MAX, 1, 0, 0, &aplic);
    verificar_overflow_promueve(aplic, r, CORNAMUSA_SMALL_INT_MAX + 1);
}

static void test_overflow_resta_promueve(void) {
    bool aplic;
    Valor r = evaluador_small_op_small(NULL, TT_MENOS,
        CORNAMUSA_SMALL_INT_MIN, 1, 0, 0, &aplic);
    verificar_overflow_promueve(aplic, r, CORNAMUSA_SMALL_INT_MIN - 1);
}

static void test_overflow_mult_promueve(void) {
    /* SMALL_MAX * 2 ≈ 2^63 — overflow de int64 con __builtin_mul_overflow. */
    bool aplic;
    Valor r = evaluador_small_op_small(NULL, TT_ASTERISCO,
        CORNAMUSA_SMALL_INT_MAX, 2, 0, 0, &aplic);
    /* Si __builtin_mul_overflow detecta → aplic=false (sentinel nulo).
       Si MSVC fallback bailea por rango int32 → aplic=false también.
       Si por alguna razón aplic=true, debe ser BIG con el valor doblado. */
    verificar_overflow_promueve(aplic, r,
        (int64_t)CORNAMUSA_SMALL_INT_MAX * 2);
}

/* SMALL_MIN * -1 — caso peligroso: -SMALL_MIN cabe en SMALL_MAX+1
   = 2^62, así que cabe en int64 pero no en SMALL. Debe promover. */
static void test_smallmin_mult_neg1(void) {
    bool aplic;
    Valor r = evaluador_small_op_small(NULL, TT_ASTERISCO,
        CORNAMUSA_SMALL_INT_MIN, -1, 0, 0, &aplic);
    /* Resultado matemático: -CORNAMUSA_SMALL_INT_MIN = 2^62 = SMALL_MAX+1. */
    verificar_overflow_promueve(aplic,
        r, -((int64_t)CORNAMUSA_SMALL_INT_MIN));
}

/* ───── 6. División SMALL_MIN / -1 (UB en C) → promueve a BIG ───── */

static void test_division_min_neg1(void) {
    bool aplic;
    Valor r = evaluador_small_op_small(NULL, TT_DOBLE_BARRA,
        CORNAMUSA_SMALL_INT_MIN, -1, 0, 0, &aplic);
    /* small_op_small detecta este caso especial y devuelve aplic=false
       para que el caller use mp_int. */
    AFIRMAR(!aplic);
    (void)r;
}

/* ───── 7. División por cero ───── */

static void test_division_por_cero(void) {
    EvalError err = {0};
    bool aplic;
    Valor r = evaluador_small_op_small(&err, TT_DOBLE_BARRA, 5, 0, 0, 0, &aplic);
    AFIRMAR(aplic);  /* manejado dentro de small_op_small */
    AFIRMAR(err.tuvo_error);
    AFIRMAR(strstr(err.mensaje, "division por cero") != NULL);
    valor_destruir(&r);
}

/* ───── 8. Módulo Python-style con SMALL+SMALL ───── */

static void test_modulo_python(void) {
    bool aplic;
    /* -7 % 3 = 2 (Python). */
    Valor r = evaluador_small_op_small(NULL, TT_PORCENTAJE, -7, 3, 0, 0, &aplic);
    AFIRMAR(aplic);
    int64_t out;
    AFIRMAR(valor_entero_a_i64(&r, &out));
    AFIRMAR(out == 2);
    valor_destruir(&r);

    /* 7 % -3 = -2 (Python). */
    r = evaluador_small_op_small(NULL, TT_PORCENTAJE, 7, -3, 0, 0, &aplic);
    AFIRMAR(aplic);
    AFIRMAR(valor_entero_a_i64(&r, &out));
    AFIRMAR(out == -2);
    valor_destruir(&r);
}

/* ───── 9. Round-trip clone preserva tipo ───── */

static void test_clone_preserva_tipo(void) {
    Valor s = valor_entero_de_i64(42);
    Valor cs = valor_clonar(&s);
    AFIRMAR(cs.tipo == VAL_ENTERO_SMALL);
    AFIRMAR(valor_iguales(&s, &cs));
    valor_destruir(&s); valor_destruir(&cs);

    Valor b = forzar_big(42);
    Valor cb = valor_clonar(&b);
    AFIRMAR(cb.tipo == VAL_ENTERO);  /* clone de BIG produce BIG */
    AFIRMAR(valor_iguales(&b, &cb));
    valor_destruir(&b); valor_destruir(&cb);
}

/* ───── 10. valor_entero_a_i64 y valor_entero_a_mp_int ───── */

static void test_helpers_extraccion(void) {
    Valor s = valor_entero_de_i64(123);
    int64_t out;
    AFIRMAR(valor_entero_a_i64(&s, &out));
    AFIRMAR(out == 123);

    bool propio;
    mp_int *m = valor_entero_a_mp_int(&s, &propio);
    AFIRMAR(m != NULL);
    AFIRMAR(propio);  /* SMALL produce mp_int temporal */
    AFIRMAR(mp_get_i64(m) == 123);
    mp_clear(m); free(m);
    valor_destruir(&s);

    Valor b = forzar_big(999);
    AFIRMAR(valor_entero_a_i64(&b, &out));
    AFIRMAR(out == 999);
    m = valor_entero_a_mp_int(&b, &propio);
    AFIRMAR(m != NULL);
    AFIRMAR(!propio);  /* BIG: el mp_int es de v, no liberar */
    AFIRMAR(mp_get_i64(m) == 999);
    valor_destruir(&b);
}

int main(void) {
    test_constructor_rango_small();
    test_normalizado_demote();
    test_igualdad_cross_tag();
    test_hash_equivalente();
    test_overflow_suma_promueve();
    test_overflow_resta_promueve();
    test_overflow_mult_promueve();
    test_smallmin_mult_neg1();
    test_division_min_neg1();
    test_division_por_cero();
    test_modulo_python();
    test_clone_preserva_tipo();
    test_helpers_extraccion();
    if (fallos == 0) {
        printf("test_small_int: 13 tests PASS\n");
        return 0;
    }
    fprintf(stderr, "test_small_int: %d FALLO(s)\n", fallos);
    return 1;
}
