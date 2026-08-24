cmake_minimum_required(VERSION 3.16)

project(hexpuzzle3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(IMATH REQUIRED IMPORTED_TARGET Imath)
pkg_check_modules(GLUT REQUIRED IMPORTED_TARGET glut)
pkg_check_modules(GLU REQUIRED IMPORTED_TARGET glu)
pkg_check_modules(OPENGL REQUIRED IMPORTED_TARGET gl)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)

add_library(hexpuzzle_core
    hexplanet.cpp
    puzzle.cpp
    camera.cpp
    bounded_adams_bashforth.cpp
    debug_log.cpp
    sequence_tracker.cpp
)
target_include_directories(hexpuzzle_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hexpuzzle_core PUBLIC PkgConfig::IMATH)
target_compile_options(hexpuzzle_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(hexp_main
    hexp_main.cpp
    application.cpp
    renderer.cpp
    texture.cpp
)
target_link_libraries(hexp_main PRIVATE
    hexpuzzle_core
    PkgConfig::GLUT
    PkgConfig::GLU
    PkgConfig::OPENGL
    PkgConfig::PNG
)
target_compile_options(hexp_main PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(hexpuzzle_core_tests tests/core_tests.cpp)
    target_link_libraries(hexpuzzle_core_tests PRIVATE hexpuzzle_core)
    target_compile_options(hexpuzzle_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME hexpuzzle_core_tests COMMAND hexpuzzle_core_tests)
endif()
