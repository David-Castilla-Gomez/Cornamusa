# Ejecuta cornamusa --bytecode sobre un archivo .cor con stdin
# redirigido desde un fichero temporal con líneas predefinidas.
#
# Util para validar built-ins interactivos como `leer()`.
#
# Variables esperadas (pasadas con -D):
#   CORNAMUSA_BIN  — ruta al ejecutable cornamusa.
#   ARCHIVO_COR    — ruta al .cor a ejecutar.
#   ENTRADA_LINEAS — lista CMake de líneas a alimentar por stdin (sin '\n').
#   ESPERADO       — regex que debe matchear stdout del programa.
#   WORKDIR        — directorio de trabajo (default: parent del .cor).

if(NOT CORNAMUSA_BIN)
    message(FATAL_ERROR "run_con_stdin.cmake: falta -DCORNAMUSA_BIN=...")
endif()
if(NOT ARCHIVO_COR)
    message(FATAL_ERROR "run_con_stdin.cmake: falta -DARCHIVO_COR=...")
endif()
if(NOT ENTRADA_LINEAS)
    message(FATAL_ERROR "run_con_stdin.cmake: falta -DENTRADA_LINEAS=...")
endif()
if(NOT ESPERADO)
    message(FATAL_ERROR "run_con_stdin.cmake: falta -DESPERADO=...")
endif()
if(NOT WORKDIR)
    get_filename_component(WORKDIR ${ARCHIVO_COR} DIRECTORY)
endif()

# Construir archivo de entrada con las líneas separadas por LF.
get_filename_component(EJ_NOMBRE ${ARCHIVO_COR} NAME_WE)
set(STDIN_FILE ${CMAKE_CURRENT_BINARY_DIR}/stdin_${EJ_NOMBRE}.txt)
string(REPLACE ";" "\n" ENTRADA_TEXT "${ENTRADA_LINEAS}")
set(ENTRADA_TEXT "${ENTRADA_TEXT}\n")
file(WRITE ${STDIN_FILE} "${ENTRADA_TEXT}")

execute_process(
    COMMAND ${CORNAMUSA_BIN} --bytecode ${ARCHIVO_COR}
    WORKING_DIRECTORY ${WORKDIR}
    INPUT_FILE ${STDIN_FILE}
    OUTPUT_VARIABLE SALIDA
    ERROR_VARIABLE  STDERR
    RESULT_VARIABLE RC
)

if(NOT RC EQUAL 0)
    message(FATAL_ERROR
        "Cornamusa fallo (rc=${RC}) en ${ARCHIVO_COR}\n"
        "stderr:\n${STDERR}\n"
        "stdout:\n${SALIDA}")
endif()

if(NOT SALIDA MATCHES "${ESPERADO}")
    message(FATAL_ERROR
        "Salida no contiene la regex esperada.\n"
        "  archivo:   ${ARCHIVO_COR}\n"
        "  regex:     ${ESPERADO}\n"
        "  stdout:\n${SALIDA}")
endif()
