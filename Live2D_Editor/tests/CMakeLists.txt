# This test uses the PC PainterEngine types; never link it to l2d_runtime.
set(PE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../PainterEngine")
add_executable(l2d_test_pc_jpeg test_jpeg_loader.c
    ${PE_ROOT}/core/PX_jpg.c
    ${PE_ROOT}/core/PX_Typedef.c
    ${PE_ROOT}/core/PX_MemoryPool.c
    ${PE_ROOT}/core/PX_Surface.c
    ${PE_ROOT}/core/PX_Log.c)
target_include_directories(l2d_test_pc_jpeg PRIVATE ${PE_ROOT}/core)
target_compile_definitions(l2d_test_pc_jpeg PRIVATE
    JPEG_BASELINE_FILE="${CMAKE_CURRENT_LIST_DIR}/assets/jpeg-baseline.jpg"
    JPEG_PROGRESSIVE_FILE="${CMAKE_CURRENT_LIST_DIR}/assets/jpeg-progressive.jpg")
target_compile_options(l2d_test_pc_jpeg PRIVATE -O2 -fno-strict-aliasing -ffunction-sections -fdata-sections)
target_link_options(l2d_test_pc_jpeg PRIVATE -Wl,--gc-sections)
target_link_libraries(l2d_test_pc_jpeg PRIVATE m)
foreach(sanitizer ASAN UBSAN)
    if(L2D_ENABLE_${sanitizer})
        if(sanitizer STREQUAL "ASAN")
            set(flag -fsanitize=address)
        else()
            set(flag -fsanitize=undefined)
        endif()
        target_compile_options(l2d_test_pc_jpeg PRIVATE ${flag} -fno-omit-frame-pointer)
        target_link_options(l2d_test_pc_jpeg PRIVATE ${flag})
    endif()
endforeach()
add_test(NAME l2d_pc_jpeg COMMAND l2d_test_pc_jpeg)
