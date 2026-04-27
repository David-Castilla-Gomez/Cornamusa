cmake_minimum_required(VERSION 3.16)

project(cornamusa
    VERSION 0.1.0
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

# ───── Ejecutable principal ─────
file(GLOB CORNAMUSA_SOURCES CONFIGURE_DEPENDS "src/*.c")

add_executable(cornamusa ${CORNAMUSA_SOURCES})
target_include_directories(cornamusa PRIVATE src)

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
