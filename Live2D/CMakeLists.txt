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
option(L2D_RASTER_BATCH "Exercise preallocated disjoint-row raster jobs" OFF)
target_compile_definitions(l2d_runtime PRIVATE L2D_CFG_SRM_ROI=1 L2D_CFG_RASTER_BATCH=$<BOOL:${L2D_RASTER_BATCH}>)
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
    L2D_MODEL_ESP_LIVE="${CMAKE_SOURCE_DIR}/models/esp.live")

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

set_target_properties(l2d_test_runtime PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME l2d_runtime COMMAND l2d_test_runtime)

add_executable(l2d_test_light tests/runtime/test_light_model.c
    tests/support/l2d_test_allocator.c)
target_include_directories(l2d_test_light PRIVATE tests/support bench/scenarios)
target_compile_definitions(l2d_test_light PRIVATE
    L2D_MODEL_LIGHT_LIVE="${CMAKE_SOURCE_DIR}/models/light.live")
target_link_libraries(l2d_test_light PRIVATE l2d_runtime m z crypto)
target_link_options(l2d_test_light PRIVATE -Wl,--gc-sections)
target_compile_options(l2d_test_light PRIVATE -O2 -fno-strict-aliasing -ffp-contract=off ${L2D_SAN_FLAGS})
set_target_properties(l2d_test_light PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME l2d_light_model COMMAND l2d_test_light)
if(L2D_RASTER_BATCH)
    add_test(NAME l2d_light_rgb565_bands COMMAND l2d_test_light --bands)
endif()

add_executable(l2d_test_nearest tests/raster/test_nearest.c tests/raster/reference_nearest.c)
target_include_directories(l2d_test_nearest PRIVATE src/internal src/internal/pe/core src/internal/pe/kernel)
target_compile_options(l2d_test_nearest PRIVATE -O2 -fno-strict-aliasing -ffp-contract=off ${L2D_SAN_FLAGS})
target_link_libraries(l2d_test_nearest PRIVATE l2d_runtime m)
set_target_properties(l2d_test_nearest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME l2d_nearest_pixels COMMAND l2d_test_nearest)

add_executable(l2d_host_benchmark bench/runners/host_benchmark.c)
target_include_directories(l2d_host_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bench/scenarios)
target_compile_definitions(l2d_host_benchmark PRIVATE
    L2D_MODEL_ESP_LIVE="${CMAKE_SOURCE_DIR}/models/esp.live")
target_link_libraries(l2d_host_benchmark PRIVATE l2d_runtime m crypto)
target_link_options(l2d_host_benchmark PRIVATE -Wl,--gc-sections)
set_target_properties(l2d_host_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
