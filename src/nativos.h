#ifndef CORNAMUSA_NATIVOS_H
#define CORNAMUSA_NATIVOS_H

#include "entorno.h"
#include "valor.h"   /* Diccionario */

/*
 * Built-ins de Cornamusa (Fase 4 sesión 4).
 *
 * Conjunto mínimo para v0.4.0: `imprimir`, `longitud`, `tipo`, `rango`,
 * con semántica suficiente para los ejemplos jugables.
 *
 * Cada built-in es una función C con la firma `FnNativa` (declarada
 * en `valor.h`) registrada como `Valor` de tipo `VAL_NATIVA` en el
 * entorno proporcionado. La biblioteca estándar más amplia (`cadenas`,
 * `matematicas`, `io`, etc.) llegará en Fase 9.
 *
 * Los built-ins NO toman posesión de los args — el llamador (eval de
 * EXPR_LLAMADA) los destruye al volver. El Valor devuelto pasa al
 * llamador, que es responsable de destruirlo.
 */

/*
 * Registra los built-ins en el entorno proporcionado (típicamente el
 * global). Idempotente: re-registrar sobreescribe sin liberar nada
 * crítico (los Valores nativa son inmutables y livianos).
 *
 * `nativos_registrar` es para el evaluador tree-walking (que usa
 * `Entorno` como scope). `nativos_registrar_dicc` es la variante para
 * la VM bytecode (que usa `Diccionario` como tabla de globales). Ambas
 * registran exactamente la misma lista de nativas — el motor solo
 * decide la estructura subyacente.
 */
void nativos_registrar(Entorno *globales);
void nativos_registrar_dicc(Diccionario *globales);

/*
 * Configura los argumentos del proceso visibles desde Cornamusa via
 * `obtener_argv()` (y por tanto `sistema.argv`). Se llama desde main.c
 * antes de ejecutar el programa, pasándole los args ya filtrados (sin
 * el ejecutable ni los flags consumidos).
 */
void nativos_set_argv(int argc, char **argv);

/*
 * Calcula la longitud de `v` (cadena, lista, dicc, conjunto, tupla,
 * rango) y la devuelve como Valor entero. Si `v` es VAL_INSTANCIA, NO
 * intenta `__longitud__` (los dunders los maneja la VM directamente
 * vía OP_LONGITUD). Para otros tipos no soportados, setea `*err` con
 * ErrorDeTipo y devuelve nulo.
 *
 * Compartido entre la nativa `longitud` y el handler de `OP_LONGITUD`
 * (v1.3) para garantizar comportamiento idéntico.
 */
Valor nativos_calcular_longitud(struct EvalError *err, const Valor *v,
                                  int linea, int columna);

#endif /* CORNAMUSA_NATIVOS_H */
