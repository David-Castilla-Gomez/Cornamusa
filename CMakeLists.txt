cmake_minimum_required(VERSION 3.16)

project(cornamusa
    VERSION 0.11.6
    DESCRIPTION "Lenguaje de programación dinámico en castellano"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de build" FORCE)
endif()

# ───── Flags de compilación ─────
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wstrict-prototypes
        -Wno-unused-parameter
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O0 -g3)
    else()
        add_compile_options(-O2)
    endif()
elseif(MSVC)
    add_compile_options(/W4 /utf-8)
endif()

# ───── Opciones del proyecto ─────
option(CORNAMUSA_GC_STRESS "Forzar GC en cada allocation (debugging)" OFF)
option(CORNAMUSA_DEBUG_TRACE "Imprimir traza de ejecución de la VM" OFF)
option(CORNAMUSA_BUILD_TESTS "Construir suite de tests" ON)

if(CORNAMUSA_GC_STRESS)
    add_compile_definitions(CORNAMUSA_GC_STRESS=1)
endif()
if(CORNAMUSA_DEBUG_TRACE)
    add_compile_definitions(CORNAMUSA_DEBUG_TRACE=1)
endif()

# ───── Dependencia vendoreada: utf8proc ─────
# Se compila como librería estática que se enlaza al binario y a los tests.
# Solo se lista utf8proc.c; utf8proc_data.c se incluye desde utf8proc.c
# con #include directiva (no es una unidad de traducción separada).
# Se desactivan los warnings estrictos para esta dependencia externa.
add_library(utf8proc STATIC
    vendor/utf8proc/utf8proc.c
)
target_include_directories(utf8proc PUBLIC vendor/utf8proc)
target_compile_definitions(utf8proc PRIVATE UTF8PROC_STATIC)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(utf8proc PRIVATE -w)
elseif(MSVC)
    target_compile_options(utf8proc PRIVATE /w)
endif()

# ───── Dependencia vendoreada: libtommath ─────
# Bignum para enteros de precisión arbitraria (decisión B3).
# Se compila como librería estática. La lista de fuentes está en
# vendor/libtommath/sources.cmake (autogenerado por upstream).
include(vendor/libtommath/sources.cmake)
list(TRANSFORM SOURCES PREPEND "vendor/libtommath/")
add_library(tommath STATIC ${SOURCES})
target_include_directories(tommath PUBLIC vendor/libtommath)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tommath PRIVATE -w)
elseif(MSVC)
    target_compile_options(tommath PRIVATE /w)
endif()

# ───── Ejecutable principal ─────
file(GLOB CORNAMUSA_SOURCES CONFIGURE_DEPENDS "src/*.c")

add_executable(cornamusa ${CORNAMUSA_SOURCES})
target_include_directories(cornamusa PRIVATE src)
target_compile_definitions(cornamusa PRIVATE UTF8PROC_STATIC)
target_link_libraries(cornamusa PRIVATE utf8proc tommath)

# ───── Tests ─────
if(CORNAMUSA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ───── Instalación ─────
install(TARGETS cornamusa RUNTIME DESTINATION bin)
install(DIRECTORY stdlib/ DESTINATION share/cornamusa/stdlib)

message(STATUS "Cornamusa ${PROJECT_VERSION} (${CMAKE_BUILD_TYPE})")
message(STATUS "  GC_STRESS:    ${CORNAMUSA_GC_STRESS}")
message(STATUS "  DEBUG_TRACE:  ${CORNAMUSA_DEBUG_TRACE}")
message(STATUS "  BUILD_TESTS:  ${CORNAMUSA_BUILD_TESTS}")
