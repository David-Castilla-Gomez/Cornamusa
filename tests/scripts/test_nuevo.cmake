# Test end-to-end del subcomando `nuevo` (v1.98).
#
# Invocado desde tests/CMakeLists.txt con:
#   cmake -DCORNAMUSA=<ruta cornamusa.exe> -DREPO_ROOT=<...> -P test_nuevo.cmake
#
# Crea un proyecto temporal en build/test_nuevo_e2e, verifica que se
# generan los 4 archivos esperados, ejecuta el test generado y
# verifica exit code 0. Limpia al final.

if(NOT DEFINED CORNAMUSA)
    message(FATAL_ERROR "CORNAMUSA no definida (ruta al binario)")
endif()
if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT no definida (raiz del repo)")
endif()

# Directorio temporal en la raiz del repo (donde stdlib/ esta accesible).
set(PROYECTO_DIR "${REPO_ROOT}/_test_nuevo_e2e")

# Limpieza preventiva por si quedo de una ejecucion anterior.
if(EXISTS "${PROYECTO_DIR}")
    file(REMOVE_RECURSE "${PROYECTO_DIR}")
endif()

# 1. Invocar `cornamusa nuevo _test_nuevo_e2e` desde REPO_ROOT.
execute_process(
    COMMAND "${CORNAMUSA}" nuevo "_test_nuevo_e2e"
    WORKING_DIRECTORY "${REPO_ROOT}"
    RESULT_VARIABLE rc_nuevo
    OUTPUT_VARIABLE out_nuevo
)
if(NOT rc_nuevo EQUAL 0)
    message(FATAL_ERROR "cornamusa nuevo fallo con codigo ${rc_nuevo}\n${out_nuevo}")
endif()

# 2. Verificar archivos generados.
foreach(esperado IN ITEMS
        "${PROYECTO_DIR}/main.cor"
        "${PROYECTO_DIR}/tests/test_main.cor"
        "${PROYECTO_DIR}/README.md"
        "${PROYECTO_DIR}/.gitignore")
    if(NOT EXISTS "${esperado}")
        file(REMOVE_RECURSE "${PROYECTO_DIR}")
        message(FATAL_ERROR "Archivo esperado no creado: ${esperado}")
    endif()
endforeach()

# 3. Ejecutar el test generado y verificar exit code 0.
execute_process(
    COMMAND "${CORNAMUSA}" --bytecode "_test_nuevo_e2e/tests/test_main.cor"
    WORKING_DIRECTORY "${REPO_ROOT}"
    RESULT_VARIABLE rc_test
    OUTPUT_VARIABLE out_test
)
if(NOT rc_test EQUAL 0)
    file(REMOVE_RECURSE "${PROYECTO_DIR}")
    message(FATAL_ERROR "Test generado fallo (rc=${rc_test}):\n${out_test}")
endif()
if(NOT "${out_test}" MATCHES "Pasados: 2")
    file(REMOVE_RECURSE "${PROYECTO_DIR}")
    message(FATAL_ERROR "Test no reporto 'Pasados: 2':\n${out_test}")
endif()

# 4. Verificar que un segundo `nuevo` falla (proyecto ya existe).
execute_process(
    COMMAND "${CORNAMUSA}" nuevo "_test_nuevo_e2e"
    WORKING_DIRECTORY "${REPO_ROOT}"
    RESULT_VARIABLE rc_dup
)
if(rc_dup EQUAL 0)
    file(REMOVE_RECURSE "${PROYECTO_DIR}")
    message(FATAL_ERROR "Segundo `nuevo` deberia fallar; rc=${rc_dup}")
endif()

# 5. Limpieza.
file(REMOVE_RECURSE "${PROYECTO_DIR}")

message(STATUS "nuevo_end_to_end: OK")
