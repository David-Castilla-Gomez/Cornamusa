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
    if(WIN32)
        target_link_libraries(${test_name} PRIVATE ws2_32)
    endif()
    # v1.67: tests que importan stdlib (jwt, etc.) necesitan correr
    # desde la raiz del repo para que `cargar_modulo_desde_archivo`
    # encuentre `stdlib/*.cor`. Setear WORKING_DIRECTORY uniformemente
    # es inocuo para los demas tests.
    add_test(NAME ${test_name} COMMAND ${test_name}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()

# ───── v1.39: tests del flag --check ─────
# --check valida sintaxis + compilación sin ejecutar. Verificamos que
# un ejemplo válido pasa (exit 0) y que un fixture con error de
# sintaxis falla (exit != 0, vía WILL_FAIL).
add_test(
    NAME check_ejemplo_valido
    COMMAND $<TARGET_FILE:cornamusa> --check
            ${CMAKE_SOURCE_DIR}/examples/01_hola_mundo.cor
)
set_tests_properties(check_ejemplo_valido PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "OK"
)
add_test(
    NAME check_sintaxis_mala
    COMMAND $<TARGET_FILE:cornamusa> --check
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/check_sintaxis_mala.cor
)
set_tests_properties(check_sintaxis_mala PROPERTIES
    LABELS "integracion"
    WILL_FAIL TRUE
)

# ───── v1.98: tests del subcomando `nuevo` ─────
# nuevo --ayuda debe imprimir texto de uso.
add_test(
    NAME nuevo_ayuda
    COMMAND $<TARGET_FILE:cornamusa> nuevo --ayuda
)
set_tests_properties(nuevo_ayuda PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Crea un nuevo proyecto"
)
# nuevo sin nombre falla con exit code 64.
add_test(
    NAME nuevo_sin_args_falla
    COMMAND $<TARGET_FILE:cornamusa> nuevo
)
set_tests_properties(nuevo_sin_args_falla PROPERTIES
    LABELS "integracion"
    WILL_FAIL TRUE
)
# nuevo end-to-end: crear scaffold en un directorio temporal,
# ejecutar el test generado, limpiar. Usa cmake -P para encadenar.
add_test(
    NAME nuevo_end_to_end
    COMMAND ${CMAKE_COMMAND}
        -DCORNAMUSA=$<TARGET_FILE:cornamusa>
        -DREPO_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_nuevo.cmake
)
set_tests_properties(nuevo_end_to_end PROPERTIES
    LABELS "integracion"
)

# ───── Tests de integración: lexar los ejemplos en examples/ ─────
# Cada ejemplo se tokeniza con `cornamusa --tokens <archivo>`. Exit
# code 0 indica que el lexer no encontró errores. Solo verifica el
# lexer — no requiere que el ejemplo sea ejecutable.
file(GLOB EJEMPLOS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/examples/*.cor")
foreach(ejemplo ${EJEMPLOS})
    get_filename_component(nombre_ejemplo ${ejemplo} NAME_WE)
    add_test(
        NAME lex_${nombre_ejemplo}
        COMMAND $<TARGET_FILE:cornamusa> --tokens ${ejemplo}
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

# ───── Tests de ejecución end-to-end (Fase 4 v0.4.0) ─────
# Estos ejemplos ejecutan completamente con el intérprete tree-walking
# y producen salida verificada con PASS_REGULAR_EXPRESSION. Cubren los
# casos representativos del runtime jugable: imprimir, recursión, para,
# si/sino si/sino, rango(), longitud(), aritmética bignum.
add_test(
    NAME run_01_hola_mundo
    COMMAND $<TARGET_FILE:cornamusa>
            ${CMAKE_SOURCE_DIR}/examples/01_hola_mundo.cor
)
set_tests_properties(run_01_hola_mundo PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Bienvenido a Cornamusa"
)

add_test(
    NAME run_13_factorial_jugable
    COMMAND $<TARGET_FILE:cornamusa>
            ${CMAKE_SOURCE_DIR}/examples/13_factorial_jugable.cor
)
set_tests_properties(run_13_factorial_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "100! = 9332621544394415268169923885626670049071596826438162146859296389521759999322991560894146397615651828625369792082722375825118521091686400000000000000000000000"
)

add_test(
    NAME run_14_contar_vocales
    COMMAND $<TARGET_FILE:cornamusa>
            ${CMAKE_SOURCE_DIR}/examples/14_contar_vocales.cor
)
set_tests_properties(run_14_contar_vocales PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "murcielago tiene 5 vocales"
)

add_test(
    NAME run_15_fizzbuzz_jugable
    COMMAND $<TARGET_FILE:cornamusa>
            ${CMAKE_SOURCE_DIR}/examples/15_fizzbuzz_jugable.cor
)
set_tests_properties(run_15_fizzbuzz_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "FizzBuzz"
)

# ───── Tests de ejecución end-to-end (Fase 5 v0.5.0) ─────
# Ejemplos que ejercitan listas, diccionarios, conjuntos y tuplas
# completas (literal + indexación + iteración + métodos vía built-ins).
add_test(
    NAME run_16_lista_busqueda
    COMMAND $<TARGET_FILE:cornamusa>
            ${CMAKE_SOURCE_DIR}/examples/16_lista_busqueda.cor
)
set_tests_properties(run_16_lista_busqueda PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Indice del 99: -1"
)

add_test(
    NAME run_17_dicc_frecuencia
    COMMAND $<TARGET_FILE:cornamusa>
            ${CMAKE_SOURCE_DIR}/examples/17_dicc_frecuencia.cor
)
set_tests_properties(run_17_dicc_frecuencia PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Total de letras: 11"
)

add_test(
    NAME run_18_conj_y_tupla
    COMMAND $<TARGET_FILE:cornamusa>
            ${CMAKE_SOURCE_DIR}/examples/18_conj_y_tupla.cor
)
set_tests_properties(run_18_conj_y_tupla PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Suma de unicos: 30"
)

# ───── Tests de ejecución end-to-end con motor BYTECODE (Fase 6) ─────
# Confirman que el motor bytecode ejecuta los ejemplos compatibles
# (sin SENT_PARA — eso queda aplazado a v0.6.1) produciendo la misma
# salida que el tree-walking. Los demás ejemplos siguen ejecutándose
# con el motor por defecto.
add_test(
    NAME bc_run_01_hola_mundo
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/01_hola_mundo.cor
)
set_tests_properties(bc_run_01_hola_mundo PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Bienvenido a Cornamusa"
)

add_test(
    NAME bc_run_13_factorial_jugable
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/13_factorial_jugable.cor
)
set_tests_properties(bc_run_13_factorial_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "100! = 9332621544394415268169923885626670049071596826438162146859296389521759999322991560894146397615651828625369792082722375825118521091686400000000000000000000000"
)

# v1.58: ejemplo del modulo csv. Round-trip y separadores alternativos.
add_test(
    NAME bc_run_66_csv
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/66_csv.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_66_csv PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "queso, manchego"
)

# v1.59: ejemplo del modulo base64. Round-trip y HTTP Basic Auth.
add_test(
    NAME bc_run_67_base64
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/67_base64.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_67_base64 PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Q29ybmFtdXNhIGhhYmxhIGNhc3RlbGxhbm8="
)

# v1.60: ejemplo del modulo hashing. SHA-256 abc test vector.
add_test(
    NAME bc_run_68_hashing
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/68_hashing.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_68_hashing PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
)

# v1.67: ejemplo del modulo jwt. Round-trip + rechazo de token alterado.
add_test(
    NAME bc_run_69_jwt
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/69_jwt.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_69_jwt PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Token alterado verifica\\? falso"
)

# v0.6.1: tests bytecode adicionales que ahora pasan gracias a iteradores
# y aug-assign con destino indice.
add_test(
    NAME bc_run_02_fizzbuzz
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/02_fizzbuzz.cor
)
set_tests_properties(bc_run_02_fizzbuzz PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "FizzBuzz"
)

add_test(
    NAME bc_run_14_contar_vocales
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/14_contar_vocales.cor
)
set_tests_properties(bc_run_14_contar_vocales PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "murcielago tiene 5 vocales"
)

add_test(
    NAME bc_run_15_fizzbuzz_jugable
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/15_fizzbuzz_jugable.cor
)
set_tests_properties(bc_run_15_fizzbuzz_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "FizzBuzz"
)

add_test(
    NAME bc_run_17_dicc_frecuencia
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/17_dicc_frecuencia.cor
)
set_tests_properties(bc_run_17_dicc_frecuencia PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Total de letras: 11"
)

add_test(
    NAME bc_run_18_conj_y_tupla
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/18_conj_y_tupla.cor
)
set_tests_properties(bc_run_18_conj_y_tupla PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Suma de unicos: 30"
)

# v0.6.2: closures + lambdas + slicing en bytecode.
add_test(
    NAME bc_run_16_lista_busqueda
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/16_lista_busqueda.cor
)
set_tests_properties(bc_run_16_lista_busqueda PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Indice del 99: -1"
)

add_test(
    NAME bc_run_19_closures_jugable
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/19_closures_jugable.cor
)
set_tests_properties(bc_run_19_closures_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "15"
)

# v0.7.0: clases con herencia y polimorfismo en bytecode.
add_test(
    NAME bc_run_20_clases_jugable
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/20_clases_jugable.cor
)
set_tests_properties(bc_run_20_clases_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Rex tiene 8 anos y dice: guau"
)

# v0.9.0: módulos con stdlib.
add_test(
    NAME bc_run_21_modulos_jugable
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/21_modulos_jugable.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_21_modulos_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "factorial.20. = 2432902008176640000"
)

# v0.9.1: módulos con `desde X importar Y`, alias, indexación s[i].
add_test(
    NAME bc_run_22_modulos_avanzado
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/22_modulos_avanzado.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_22_modulos_avanzado PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fact.8. = 40320"
)

# v0.9.2: módulo `sistema` (argv) y built-in `salir`.
add_test(
    NAME bc_run_23_sistema_jugable
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/23_sistema_jugable.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_23_sistema_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "sin argumentos extra"
)

# v1.0 sesión 5: ejemplos avanzados (no más micro-demos).
add_test(
    NAME bc_run_24_notas_clase
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/24_notas_clase.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_24_notas_clase PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Aprobados .>=7.: 4 / 6"
)

add_test(
    NAME bc_run_25_biblioteca_oop
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/25_biblioteca_oop.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_25_biblioteca_oop PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Items prestados ahora: 4"
)

# v1.1: f-cadenas con interpolación real.
add_test(
    NAME bc_run_27_fstrings_jugable
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/27_fstrings_jugable.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_27_fstrings_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Hola Ana, tienes 30 anos"
)

# v1.2: dunders aritmeticos + __cadena__ + __indice__.
add_test(
    NAME bc_run_28_dunders_jugable
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/28_dunders_jugable.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_28_dunders_jugable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "v . w = <4, 6>"
)

# v1.3: operadores reflejados + __llamar__ + __longitud__.
add_test(
    NAME bc_run_29_oop_avanzado
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/29_oop_avanzado.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_29_oop_avanzado PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "contador interno: 3"
)

# v1.4: nolocal + closures completas.
add_test(
    NAME bc_run_30_closures_nolocal
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/30_closures_nolocal.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_30_closures_nolocal PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "115"
)

# v1.8: stdlib archivos (I/O persistente).
add_test(
    NAME bc_run_31_archivos
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/31_archivos.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_31_archivos PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "evento: fin"
)

# v1.9: stdlib json + interop con archivos.
add_test(
    NAME bc_run_32_json_archivos
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/32_json_archivos.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_32_json_archivos PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Tras update"
)

# v1.10: errores atrapables en nativas.
add_test(
    NAME bc_run_33_atrapar_robusto
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/33_atrapar_robusto.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_33_atrapar_robusto PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "sin terminar abruptamente"
)

# v1.18 + v1.18.1: stdlib formato + cadenas extendida. Este ejemplo
# importa formato Y cadenas en el principal; formato a su vez importa
# cadenas. Con el bug v1.18 (re-import) fallaba; el fix v1.18.1 lo
# resuelve.
add_test(
    NAME bc_run_43_formato
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/43_formato.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_43_formato PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "REPORTE TRIMESTRAL"
)

# v1.19: stdlib fechas con built-ins de tiempo (tiempo_actual,
# tiempo_descomponer, tiempo_componer, tiempo_formato).
add_test(
    NAME bc_run_44_fechas
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/44_fechas.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_44_fechas PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "2400 → bisiesto"
)

# v1.20: diccionarios preservan orden de inserción.
add_test(
    NAME bc_run_45_dict_ordenado
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/45_dict_ordenado.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_45_dict_ordenado PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "Rehash preserva orden"
)

# v1.21: destructuring assignment.
add_test(
    NAME bc_run_46_destructuring
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/46_destructuring.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_46_destructuring PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 46"
)

# v1.22: *args en definiciones y llamadas.
add_test(
    NAME bc_run_47_varargs
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/47_varargs.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_47_varargs PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 47"
)

# v1.23: keyword arguments en llamadas.
add_test(
    NAME bc_run_48_kwargs
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/48_kwargs.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_48_kwargs PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 48"
)

# v1.24: **kwargs en definición.
add_test(
    NAME bc_run_49_kwkw
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/49_kwkw.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_49_kwkw PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 49"
)

# v1.25: spread **dict en llamadas.
add_test(
    NAME bc_run_50_dspread
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/50_dspread.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_50_dspread PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 50"
)

# v1.26: stdlib `azar` (PRNG xoshiro256**).
add_test(
    NAME bc_run_51_azar
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/51_azar.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_51_azar PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 51"
)

# v1.27: stdlib `proceso` (ejecutar procesos externos).
# El ejemplo es Windows-flavored; en POSIX habría que cambiarlo a sh.
# Solo ejecutamos el bc_run en Windows; el unit test es portable.
if(WIN32)
    add_test(
        NAME bc_run_52_proceso
        COMMAND $<TARGET_FILE:cornamusa> --bytecode
                ${CMAKE_SOURCE_DIR}/examples/52_proceso.cor
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bc_run_52_proceso PROPERTIES
        LABELS "integracion"
        PASS_REGULAR_EXPRESSION "fin ejemplo 52"
    )
endif()

# v1.28: stdlib `regex`.
add_test(
    NAME bc_run_53_regex
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/53_regex.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_53_regex PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 53"
)

# v1.29: stdlib `red`. NO se ejecuta por defecto porque requiere
# conexión a internet (puerto 80 saliente a httpbin.org). Activar con
# `cmake -DCORNAMUSA_TEST_RED=ON ..` o pasando -L con la etiqueta.
option(CORNAMUSA_TEST_RED "Ejecutar tests de red que requieren internet" OFF)
if(CORNAMUSA_TEST_RED)
    add_test(
        NAME bc_run_54_red
        COMMAND $<TARGET_FILE:cornamusa> --bytecode
                ${CMAKE_SOURCE_DIR}/examples/54_red.cor
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(bc_run_54_red PROPERTIES
        LABELS "integracion;red"
        PASS_REGULAR_EXPRESSION "fin ejemplo 54"
    )
endif()

# v1.30: comprehensions.
add_test(
    NAME bc_run_55_comprehensions
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/55_comprehensions.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_55_comprehensions PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 55"
)

# v1.31: generadores con `producir`.
add_test(
    NAME bc_run_56_generadores
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/56_generadores.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_56_generadores PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 56"
)

# v1.35/v1.36: mensajes de error con sugerencias.
add_test(
    NAME bc_run_57_sugerencias
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/57_sugerencias.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_57_sugerencias PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 57"
)

# v1.41: dunders de coerción `__repr__` y `__booleano__`.
add_test(
    NAME bc_run_58_repr_booleano
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/58_repr_booleano.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_58_repr_booleano PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 58"
)

# v1.42: instancias hashables con __hash__ + __igual__.
add_test(
    NAME bc_run_59_hashable
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/59_hashable.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_59_hashable PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 59"
)

# v1.43: iteradores lazy con __siguiente__.
add_test(
    NAME bc_run_60_iteradores_lazy
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/60_iteradores_lazy.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_60_iteradores_lazy PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 60"
)

# v1.44: expresión ternaria + slicing assignment.
add_test(
    NAME bc_run_61_ternaria_slicing
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/61_ternaria_slicing.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_61_ternaria_slicing PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 61"
)

# v1.45: f-string format specifiers.
add_test(
    NAME bc_run_62_fstring_formato
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/62_fstring_formato.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_62_fstring_formato PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 62"
)

# v1.46: multi-recurso `con` + combinar *args con **kwargs.
add_test(
    NAME bc_run_63_con_multi_spread
    COMMAND $<TARGET_FILE:cornamusa> --bytecode
            ${CMAKE_SOURCE_DIR}/examples/63_con_multi_spread.cor
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(bc_run_63_con_multi_spread PROPERTIES
    LABELS "integracion"
    PASS_REGULAR_EXPRESSION "fin ejemplo 63"
)

# v1.1: leer() interactivo + conversores. Stdin redirigido vía script.
add_test(
    NAME bc_run_26_leer_jugable
    COMMAND ${CMAKE_COMMAND}
            -DCORNAMUSA_BIN=$<TARGET_FILE:cornamusa>
            -DARCHIVO_COR=${CMAKE_SOURCE_DIR}/examples/26_leer_jugable.cor
            "-DENTRADA_LINEAS=Ana;30;65;1.70"
            "-DESPERADO=Categoria: normal"
            -DWORKDIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/tests/integracion/run_con_stdin.cmake
)
set_tests_properties(bc_run_26_leer_jugable PROPERTIES
    LABELS "integracion"
)

# ───── Tests diferenciales tree-walking vs bytecode (v0.9.2) ─────
# Para cada ejemplo compatible con ambos motores, ejecutamos ambos y
# verificamos que produzcan EXACTAMENTE la misma salida. Es la red de
# seguridad ante regresiones semánticas en cualquiera de los dos
# pipelines (lex→parse→eval vs lex→parse→compile→VM).
# Solo incluimos ejemplos que ambos motores ejecutan a fin (rc=0).
# Algunos ejemplos (03, 05, 06, 10) usan f-strings o desempaquetado de
# tuplas que ningún motor soporta todavía — fallan idénticamente al
# parsear, pero el test diferencial chequea ejecución end-to-end, no
# parseo (el parser es compartido y se cubre en tests/unit/test_parser_*).
set(EJEMPLOS_DIFERENCIALES
    01_hola_mundo
    02_fizzbuzz
    13_factorial_jugable
    14_contar_vocales
    15_fizzbuzz_jugable
    16_lista_busqueda
    17_dicc_frecuencia
    18_conj_y_tupla
    27_fstrings_jugable
)
foreach(ej ${EJEMPLOS_DIFERENCIALES})
    add_test(
        NAME diff_${ej}
        COMMAND ${CMAKE_COMMAND}
                -DCORNAMUSA_BIN=$<TARGET_FILE:cornamusa>
                -DARCHIVO_COR=${CMAKE_SOURCE_DIR}/examples/${ej}.cor
                -DWORKDIR=${CMAKE_SOURCE_DIR}
                -P ${CMAKE_SOURCE_DIR}/tests/integracion/diff_motores.cmake
    )
    set_tests_properties(diff_${ej} PROPERTIES LABELS "diferencial")
endforeach()
