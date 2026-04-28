#include "fuente.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utf8proc.h"

static FuenteCargada error_resultado(FuenteResultado codigo,
                                     const char *mensaje) {
    FuenteCargada fc;
    fc.fuente = NULL;
    fc.longitud = 0;
    fc.codigo = codigo;
    fc.mensaje_error = mensaje;
    return fc;
}

static FuenteCargada exito(char *texto, size_t longitud) {
    FuenteCargada fc;
    fc.fuente = texto;
    fc.longitud = longitud;
    fc.codigo = FUENTE_OK;
    fc.mensaje_error = NULL;
    return fc;
}

/*
 * Aplica la normalización NFC a `texto` (terminado en \0). Devuelve
 * un nuevo buffer alocado con malloc. El cliente libera con free().
 */
static char *normalizar_a_nfc(const char *texto) {
    /*
     * utf8proc_NFC requiere uint8_t y devuelve una cadena alocada con
     * malloc. Si el input no es UTF-8 válido devuelve NULL.
     */
    utf8proc_uint8_t *resultado = utf8proc_NFC(
        (const utf8proc_uint8_t *)texto);
    return (char *)resultado;
}

FuenteCargada fuente_normalizar(const char *texto) {
    if (texto == NULL) {
        return error_resultado(FUENTE_ERROR_IO, "puntero de entrada nulo");
    }
    char *normalizado = normalizar_a_nfc(texto);
    if (normalizado == NULL) {
        return error_resultado(FUENTE_ERROR_UTF8,
            "el contenido no es UTF-8 válido o falló la normalización NFC");
    }
    return exito(normalizado, strlen(normalizado));
}

FuenteCargada fuente_cargar_archivo(const char *ruta) {
    FILE *f = fopen(ruta, "rb");
    if (f == NULL) {
        return error_resultado(FUENTE_ERROR_IO,
            "no se pudo abrir el archivo");
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return error_resultado(FUENTE_ERROR_IO,
            "no se pudo determinar el tamaño del archivo");
    }
    long tamano = ftell(f);
    if (tamano < 0) {
        fclose(f);
        return error_resultado(FUENTE_ERROR_IO,
            "tamaño de archivo inválido");
    }
    rewind(f);

    /* Reservamos +1 byte para terminador \0. */
    char *bruto = (char *)malloc((size_t)tamano + 1);
    if (bruto == NULL) {
        fclose(f);
        return error_resultado(FUENTE_ERROR_MEMORIA,
            "memoria insuficiente para leer el archivo");
    }

    size_t leidos = fread(bruto, 1, (size_t)tamano, f);
    fclose(f);
    if (leidos != (size_t)tamano) {
        free(bruto);
        return error_resultado(FUENTE_ERROR_IO,
            "lectura incompleta del archivo");
    }
    bruto[tamano] = '\0';

    /* Saltar BOM UTF-8 si aparece al inicio. */
    const char *texto_efectivo = bruto;
    if (tamano >= 3
        && (unsigned char)bruto[0] == 0xEF
        && (unsigned char)bruto[1] == 0xBB
        && (unsigned char)bruto[2] == 0xBF) {
        texto_efectivo = bruto + 3;
    }

    /* Normalizar a NFC. utf8proc_NFC aloca un nuevo buffer. */
    char *normalizado = normalizar_a_nfc(texto_efectivo);
    free(bruto);

    if (normalizado == NULL) {
        return error_resultado(FUENTE_ERROR_UTF8,
            "el archivo no es UTF-8 válido o falló la normalización NFC");
    }

    return exito(normalizado, strlen(normalizado));
}

void fuente_destruir(FuenteCargada *fc) {
    if (fc == NULL) return;
    free(fc->fuente);
    fc->fuente = NULL;
    fc->longitud = 0;
    fc->codigo = FUENTE_OK;
    fc->mensaje_error = NULL;
}
