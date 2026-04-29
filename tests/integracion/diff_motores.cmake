# Diferencial tree-walking vs bytecode.
#
# Ejecuta el binario de Cornamusa dos veces sobre el mismo archivo .cor:
#   1. Sin flags  → motor tree-walking (eval recursivo).
#   2. --bytecode → motor compilador + VM.
#
# Captura stdout en sendos archivos y compara byte a byte. Si difieren,
# imprime las salidas y falla. Es la red de seguridad de regresión que
# garantiza que el bytecode no introduzca divergencias semánticas
# respecto al motor de referencia.
#
# Variables esperadas (pasadas con -D):
#   CORNAMUSA_BIN — ruta al ejecutable cornamusa.
#   ARCHIVO_COR  — ruta al .cor a ejecutar (debe ser compatible con AMBOS motores).
#   WORKDIR      — directorio de trabajo (típicamente la raíz del repo).
#   OUT_DIR      — directorio donde escribir los archivos de salida (default: ${CMAKE_CURRENT_BINARY_DIR}).

if(NOT CORNAMUSA_BIN)
    message(FATAL_ERROR "diff_motores.cmake: falta -DCORNAMUSA_BIN=...")
endif()
if(NOT ARCHIVO_COR)
    message(FATAL_ERROR "diff_motores.cmake: falta -DARCHIVO_COR=...")
endif()
if(NOT WORKDIR)
    set(WORKDIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()
if(NOT OUT_DIR)
    set(OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Generar nombres únicos para los archivos de salida (evita colisiones
# si los tests corren en paralelo).
get_filename_component(EJ_NOMBRE ${ARCHIVO_COR} NAME_WE)
set(OUT_TW_FILE ${OUT_DIR}/diff_${EJ_NOMBRE}.tw.out)
set(OUT_BC_FILE ${OUT_DIR}/diff_${EJ_NOMBRE}.bc.out)

# 1. Ejecutar tree-walking, redirigiendo stdout a fichero.
execute_process(
    COMMAND ${CORNAMUSA_BIN} ${ARCHIVO_COR}
    WORKING_DIRECTORY ${WORKDIR}
    OUTPUT_FILE ${OUT_TW_FILE}
    ERROR_VARIABLE  ERR_TW
    RESULT_VARIABLE RC_TW
)

# 2. Ejecutar bytecode, redirigiendo stdout a fichero.
execute_process(
    COMMAND ${CORNAMUSA_BIN} --bytecode ${ARCHIVO_COR}
    WORKING_DIRECTORY ${WORKDIR}
    OUTPUT_FILE ${OUT_BC_FILE}
    ERROR_VARIABLE  ERR_BC
    RESULT_VARIABLE RC_BC
)

# 3. Verificar exit codes.
if(NOT RC_TW EQUAL 0)
    message(FATAL_ERROR "tree-walking falló (rc=${RC_TW}) en ${ARCHIVO_COR}\nstderr:\n${ERR_TW}")
endif()
if(NOT RC_BC EQUAL 0)
    message(FATAL_ERROR "bytecode falló (rc=${RC_BC}) en ${ARCHIVO_COR}\nstderr:\n${ERR_BC}")
endif()

# 4. Comparar salidas byte a byte. cmake -E compare_files retorna no-cero
#    si difieren — manejamos el RESULT_VARIABLE en lugar de COMMAND_ERROR_IS_FATAL
#    para imprimir un diff útil en caso de divergencia.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${OUT_TW_FILE} ${OUT_BC_FILE}
    RESULT_VARIABLE RC_DIFF
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT RC_DIFF EQUAL 0)
    file(READ ${OUT_TW_FILE} OUT_TW HEX)
    file(READ ${OUT_BC_FILE} OUT_BC HEX)
    message(FATAL_ERROR
        "Salida diverge entre tree-walking y bytecode para ${ARCHIVO_COR}\n"
        "  tw=${OUT_TW_FILE}\n"
        "  bc=${OUT_BC_FILE}\n"
        "tree-walking (hex):\n${OUT_TW}\n"
        "bytecode    (hex):\n${OUT_BC}\n")
endif()
