# Tests unitarios en C.
# Por ahora: framework casero minimalista (sin dependencias externas).
# Cada test_*.c se compila como ejecutable independiente y se registra con CTest.

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "unit/test_*.c")

# Reutilizamos los .c del proyecto (excepto main.c) para enlazar con los tests.
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_SOURCE_DIR}/src/main.c")

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src} ${CORE_SOURCES})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/unit
    )
    target_compile_definitions(${test_name} PRIVATE UTF8PROC_STATIC)
    target_link_libraries(${test_name} PRIVATE utf8proc)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
