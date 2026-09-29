cmake_minimum_required(VERSION 3.16)
project(l2d_embedded_runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/l2d_sources.cmake)

add_library(l2d_runtime STATIC
    ${L2D_RUNTIME_SOURCES}
    ports/host/l2d_port_host.c)
target_include_directories(l2d_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/pe/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/pe/kernel)
target_compile_definitions(l2d_runtime PRIVATE L2D_CFG_SRM_ROI=1)
target_compile_options(l2d_runtime PRIVATE
    -O2
    -fno-strict-aliasing
    -ffp-contract=off
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-parentheses
    -Wno-misleading-indentation
    -Wno-unused-but-set-variable
    -ffunction-sections
    -fdata-sections)

add_executable(l2d_test_runtime tests/runtime/test_runtime.c)
target_link_libraries(l2d_test_runtime PRIVATE l2d_runtime m z crypto)
target_link_options(l2d_test_runtime PRIVATE -Wl,--gc-sections)
target_include_directories(l2d_test_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine)
target_compile_options(l2d_test_runtime PRIVATE -O2 -fno-strict-aliasing -ffp-contract=off)

set(L2D_4AXIS_BLOB 4174304b8374697b7dd8cfab034d5398f5243306)
set(L2D_4AXIS_PATH ${CMAKE_CURRENT_BINARY_DIR}/fixtures/esp_4axis.live)
add_custom_command(
    OUTPUT ${L2D_4AXIS_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/fixtures
    COMMAND bash -c "git -C '${CMAKE_CURRENT_SOURCE_DIR}' cat-file blob ${L2D_4AXIS_BLOB} > '${L2D_4AXIS_PATH}'"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/.git/HEAD
    VERBATIM)
add_custom_target(l2d_fixture_4axis DEPENDS ${L2D_4AXIS_PATH})
add_dependencies(l2d_test_runtime l2d_fixture_4axis)
target_compile_definitions(l2d_test_runtime PRIVATE
    L2D_FIXTURE_4AXIS="${L2D_4AXIS_PATH}"
    L2D_RELEASE_LIVE="${CMAKE_CURRENT_SOURCE_DIR}/project/release.live")

enable_testing()
add_test(NAME l2d_runtime COMMAND l2d_test_runtime)
