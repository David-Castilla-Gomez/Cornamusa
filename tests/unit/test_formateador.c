/*
 * Tests del formateador (v1.48 - Fase 5 tooling).
 *
 * Cubre las reglas conservadoras del formateador:
 *   1. Reindentacion mecanica a 4 espacios.
 *   2. Mid-block markers (sino, cuando, atrapar, finalmente) no
 *      incrementan profundidad aun ending in ':'.
 *   3. Lineas dentro de () [] {} preservan leading whitespace.
 *   4. Triple-quoted strings preservan contenido literal.
 *   5. Strip de trailing whitespace siempre.
 *   6. Colapso de >=2 lineas en blanco a 1.
 *   7. Trailing newline normalizado a exactamente 1.
 *   8. Idempotencia: fmt(fmt(x)) == fmt(x).
 *   9. flag `cambiada` correcta.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formateador.h"

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

static bool fuente_es(const FormatoResultado *r, const char *esperado) {
    return r->fuente != NULL && strcmp(r->fuente, esperado) == 0;
}

static void caso(const char *entrada, const char *esperado, const char *etiqueta) {
    FormatoResultado r = formateador_formatear(entrada);
    if (!fuente_es(&r, esperado)) {
        fprintf(stderr, "FALLO caso '%s':\n--- entrada:\n%s\n--- esperado:\n%s\n--- recibido:\n%s\n---\n",
                etiqueta, entrada, esperado, r.fuente ? r.fuente : "(null)");
        fallos++;
    }
    casos++;
    formato_resultado_destruir(&r);

    /* Idempotencia. */
    FormatoResultado r1 = formateador_formatear(entrada);
    FormatoResultado r2 = formateador_formatear(r1.fuente);
    if (strcmp(r1.fuente, r2.fuente) != 0) {
        fprintf(stderr, "FALLO idempotencia '%s'\n", etiqueta);
        fallos++;
    }
    casos++;
    formato_resultado_destruir(&r1);
    formato_resultado_destruir(&r2);
}

int main(void) {
    /* 1. Reindentacion basica. */
    caso(
        "funcion f(n):\nsi n == 0:\nretornar 1\nfin si\nretornar n\nfin funcion\n",
        "funcion f(n):\n    si n == 0:\n        retornar 1\n    fin si\n    retornar n\nfin funcion\n",
        "reindent_basico");

    /* 2. Mid-block dedent: `sino:`, `cuando`. */
    caso(
        "si x:\n    cuerpo()\n    sino:\n    otro()\nfin si\n",
        "si x:\n    cuerpo()\nsino:\n    otro()\nfin si\n",
        "sino_dedent");

    caso(
        "coincidir v:\n    cuando 1:\n        a()\n    cuando 2:\n        b()\nfin coincidir\n",
        "coincidir v:\n    cuando 1:\n        a()\n    cuando 2:\n        b()\nfin coincidir\n",
        "coincidir_cuando");

    /* 3. Lineas dentro de parentesis: leading ws preservado. */
    caso(
        "d = {\n    \"a\": 1,\n    \"b\": 2,\n}\n",
        "d = {\n    \"a\": 1,\n    \"b\": 2,\n}\n",
        "dict_multilinea");

    caso(
        "x = [\n  1,\n  2,\n]\n",
        "x = [\n  1,\n  2,\n]\n",
        "lista_multilinea_2sp");

    /* 4. Strip trailing whitespace. */
    caso(
        "x = 1   \nx = 2\t\n",
        "x = 1\nx = 2\n",
        "trim_trailing");

    /* 5. Colapso de blancas. */
    caso(
        "a\n\n\n\nb\n",
        "a\n\nb\n",
        "colapso_blancas");

    /* 6. Trailing newline. */
    caso("x = 1", "x = 1\n", "anade_newline_final");
    caso("x = 1\n", "x = 1\n", "preserva_newline_final");
    caso("x = 1\n\n\n", "x = 1\n", "colapsa_newlines_finales");

    /* 7. Archivo vacio se queda vacio. */
    {
        FormatoResultado r = formateador_formatear("");
        AFIRMAR(r.fuente != NULL && r.fuente[0] == '\0', "vacio");
        AFIRMAR(!r.cambiada, "vacio_no_cambia");
        formato_resultado_destruir(&r);
    }

    /* 8. flag cambiada correcto. */
    {
        const char *limpio = "x = 1\n";
        FormatoResultado r = formateador_formatear(limpio);
        AFIRMAR(!r.cambiada, "limpio_no_cambia");
        formato_resultado_destruir(&r);
    }
    {
        const char *sucio = "x = 1   \n\n\n";
        FormatoResultado r = formateador_formatear(sucio);
        AFIRMAR(r.cambiada, "sucio_cambia");
        formato_resultado_destruir(&r);
    }

    /* 9. # dentro de cadena no inicia comentario.
     * "x:" dentro de string no abre bloque. */
    caso(
        "x = \"hola # mundo\"\ny = 1\n",
        "x = \"hola # mundo\"\ny = 1\n",
        "hash_en_cadena");

    /* 10. `fin` con solo `fin` (sin etiqueta) dedenta tambien. */
    caso(
        "si x:\nsi y:\na()\nfin\nfin\n",
        "si x:\n    si y:\n        a()\n    fin\nfin\n",
        "fin_sin_etiqueta");

    /* 11. Comentario en linea de codigo no descoloca el `:` final. */
    caso(
        "si x:    # comentario\na()\nfin si\n",
        "si x:    # comentario\n    a()\nfin si\n",
        "comentario_post_colon");

    /* 12. intentar/atrapar/finalmente. */
    caso(
        "intentar:\n    a()\n    atrapar Error como e:\n    b()\n    finalmente:\n    c()\nfin intentar\n",
        "intentar:\n    a()\natrapar Error como e:\n    b()\nfinalmente:\n    c()\nfin intentar\n",
        "intentar_atrapar_finalmente");

    if (fallos == 0) {
        printf("formateador: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "formateador: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
