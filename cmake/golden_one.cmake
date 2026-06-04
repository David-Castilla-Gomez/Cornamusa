# Comparador de salida (golden test) — un .cor vs un .salida esperado.
#
# Variables que debe pasar el caller con -D...:
#   EXE      Ruta al binario cornamusa.
#   SRC      Ruta al .cor a ejecutar.
#   EXPECTED Ruta al .salida esperado (UTF-8 con line endings tipo LF).
#
# Comportamiento:
#   - Ejecuta `EXE --bytecode SRC` desde la raiz del repo (cwd = directorio
#     padre de este .cmake) y redirige stdout a un .actual junto al .salida.
#     Redirigir a fichero (no OUTPUT_VARIABLE) evita problemas de
#     reinterpretacion de bytes UTF-8 por CMake en Windows.
#   - Si line endings difieren (LF vs CRLF), normaliza ambos a LF y compara
#     en memoria.
#   - Falla si el binario sale con exit code != 0 (los ejemplos no deben
#     crashear). Excepcion: si <EXPECTED>.exitcode existe, se acepta ese
#     codigo en su lugar.

set(ACTUAL_FILE "${EXPECTED}.actual")

execute_process(
    COMMAND "${EXE}" --bytecode "${SRC}"
    OUTPUT_FILE "${ACTUAL_FILE}"
    ERROR_VARIABLE STDERR_OUT
    RESULT_VARIABLE RC
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
)

set(EXIT_ESPERADO 0)
if(EXISTS "${EXPECTED}.exitcode")
    file(READ "${EXPECTED}.exitcode" EXIT_ESPERADO)
    string(STRIP "${EXIT_ESPERADO}" EXIT_ESPERADO)
endif()

if(NOT RC EQUAL EXIT_ESPERADO)
    message(FATAL_ERROR
        "golden: ${SRC} exit ${RC} (esperado ${EXIT_ESPERADO}).\nstderr: ${STDERR_OUT}")
endif()

file(READ "${ACTUAL_FILE}"   ACTUAL_TEXT)
file(READ "${EXPECTED}"      EXPECTED_TEXT)
string(REPLACE "\r\n" "\n" ACTUAL_TEXT   "${ACTUAL_TEXT}")
string(REPLACE "\r\n" "\n" EXPECTED_TEXT "${EXPECTED_TEXT}")

if(ACTUAL_TEXT STREQUAL EXPECTED_TEXT)
    file(REMOVE "${ACTUAL_FILE}")
    return()
endif()

message(FATAL_ERROR
    "golden: ${SRC} difiere de ${EXPECTED}\n"
    "       diff: ${EXPECTED} vs ${ACTUAL_FILE}\n"
    "--- esperado ---\n${EXPECTED_TEXT}--- actual ---\n${ACTUAL_TEXT}--- fin ---")
