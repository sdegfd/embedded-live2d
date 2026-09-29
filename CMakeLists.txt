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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/memory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/format
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

add_executable(l2d_test_runtime
    tests/runtime/test_runtime.c
    tests/support/l2d_test_allocator.c)
target_link_libraries(l2d_test_runtime PRIVATE l2d_runtime m z crypto)
target_link_options(l2d_test_runtime PRIVATE -Wl,--gc-sections)
target_include_directories(l2d_test_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/support)
target_compile_options(l2d_test_runtime PRIVATE -O2 -fno-strict-aliasing -ffp-contract=off)
target_compile_definitions(l2d_test_runtime PRIVATE
    L2D_MODEL_ESP_LIVE="${CMAKE_CURRENT_SOURCE_DIR}/project/esp.live")

option(L2D_ENABLE_ASAN "Build host runtime with AddressSanitizer" OFF)
option(L2D_ENABLE_UBSAN "Build host runtime with UndefinedBehaviorSanitizer" OFF)
set(L2D_SAN_FLAGS "")
if(L2D_ENABLE_ASAN)
    list(APPEND L2D_SAN_FLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()
if(L2D_ENABLE_UBSAN)
    list(APPEND L2D_SAN_FLAGS -fsanitize=undefined -fno-omit-frame-pointer)
endif()
if(L2D_SAN_FLAGS)
    target_compile_options(l2d_runtime PRIVATE ${L2D_SAN_FLAGS})
    target_link_options(l2d_runtime PUBLIC ${L2D_SAN_FLAGS})
    target_compile_options(l2d_test_runtime PRIVATE ${L2D_SAN_FLAGS})
    target_link_options(l2d_test_runtime PRIVATE ${L2D_SAN_FLAGS})
endif()

enable_testing()
add_test(NAME l2d_runtime COMMAND l2d_test_runtime)
