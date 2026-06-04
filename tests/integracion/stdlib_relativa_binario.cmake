# Test de fase 4: `importar X` funciona desde un cwd que NO contiene
# stdlib/. Crea un archivo .cor en un temp dir, lo ejecuta con cwd = ese
# temp dir, y verifica que `importar funcionales` (que esta en
# ${CMAKE_SOURCE_DIR}/stdlib/) se resuelve via la busqueda relativa al
# binario (vm_set_ruta_binario en main.c).
#
# Variables esperadas con -D:
#   CORNAMUSA_BIN  -- ruta al binario (con stdlib/ al lado del binario,
#                     o cerca via .., gracias al diseño build/).
#   TEMP_DIR       -- directorio temporal de trabajo (CMake lo
#                     proporciona via $<TARGET_FILE_DIR:...>).

if(NOT CORNAMUSA_BIN)
    message(FATAL_ERROR "stdlib_relativa_binario.cmake: falta CORNAMUSA_BIN")
endif()
if(NOT TEMP_DIR)
    message(FATAL_ERROR "stdlib_relativa_binario.cmake: falta TEMP_DIR")
endif()

# Crear temp dir limpio.
file(REMOVE_RECURSE "${TEMP_DIR}/_test_cwd")
file(MAKE_DIRECTORY "${TEMP_DIR}/_test_cwd")
set(TEST_COR "${TEMP_DIR}/_test_cwd/test.cor")
file(WRITE "${TEST_COR}"
"importar funcionales\n"
"r = funcionales.suma([1, 2, 3, 4, 5], 0)\n"
"imprimir(r)\n"
)

# Ejecutar con cwd = temp dir (que NO contiene stdlib/).
execute_process(
    COMMAND "${CORNAMUSA_BIN}" --bytecode "${TEST_COR}"
    WORKING_DIRECTORY "${TEMP_DIR}/_test_cwd"
    OUTPUT_VARIABLE OUT
    ERROR_VARIABLE  ERR
    RESULT_VARIABLE RC
)

# Cleanup.
file(REMOVE_RECURSE "${TEMP_DIR}/_test_cwd")

if(NOT RC EQUAL 0)
    message(FATAL_ERROR
        "stdlib_relativa: exit ${RC}\nstdout: ${OUT}\nstderr: ${ERR}")
endif()

string(REPLACE "\r\n" "\n" OUT "${OUT}")
if(NOT OUT MATCHES "15")
    message(FATAL_ERROR
        "stdlib_relativa: salida no contiene '15': ${OUT}")
endif()

message(STATUS "stdlib_relativa: OK (${OUT})")
