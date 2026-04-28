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
    target_link_libraries(${test_name} PRIVATE utf8proc tommath)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# ───── Tests de integración: lexar los ejemplos en examples/ ─────
# Cada ejemplo se invoca con `cornamusa <archivo>` y debe terminar con
# exit code 0 (lo que indica que el lexer no encontró errores). En v0.4
# se añadirá comparación con golden output cuando el intérprete ejecute.
file(GLOB EJEMPLOS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/examples/*.cor")
foreach(ejemplo ${EJEMPLOS})
    get_filename_component(nombre_ejemplo ${ejemplo} NAME_WE)
    add_test(
        NAME lex_${nombre_ejemplo}
        COMMAND $<TARGET_FILE:cornamusa> ${ejemplo}
    )
    set_tests_properties(lex_${nombre_ejemplo} PROPERTIES
        LABELS "integracion"
    )
endforeach()

# ───── Tests de integración del parser: --ast sobre ejemplos ─────
# En v0.3.0 los siguientes ejemplos parsean correctamente. Los demás
# (03, 05, 06, 10) usan features aplazadas a v0.3.1: multi-target
# assignment (`a, b = b, a`) y list comprehensions
# (`[x para x en y si cond]`).
set(EJEMPLOS_PARSEAN
    01_hola_mundo
    02_fizzbuzz
    04_factorial
    07_clases_herencia
    08_excepciones
    09_closures
    11_iterador
    12_modulos
)
foreach(nombre ${EJEMPLOS_PARSEAN})
    add_test(
        NAME parse_${nombre}
        COMMAND $<TARGET_FILE:cornamusa> --ast
                ${CMAKE_SOURCE_DIR}/examples/${nombre}.cor
    )
    set_tests_properties(parse_${nombre} PROPERTIES
        LABELS "integracion"
    )
endforeach()
