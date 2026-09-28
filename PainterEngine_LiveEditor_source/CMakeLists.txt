cmake_minimum_required(VERSION 3.12)
project(PainterEngine_LiveEditor C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(PAINTERENGINE_ROOT "/home/ubuntu/live2d/PainterEngine" CACHE PATH
    "Path to the PainterEngine source tree")

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

file(GLOB PAINTERENGINE_SOURCES CONFIGURE_DEPENDS
    "${PAINTERENGINE_ROOT}/core/*.c"
    "${PAINTERENGINE_ROOT}/kernel/*.c"
    "${PAINTERENGINE_ROOT}/runtime/*.c"
    "${PAINTERENGINE_ROOT}/platform/linux/*.c"
    "${PAINTERENGINE_ROOT}/platform/linux/*.cpp"
)
# This optional backend references a module not present in this engine checkout.
list(FILTER PAINTERENGINE_SOURCES EXCLUDE REGEX "/px_hardware\\.c$")
add_library(PainterEngine STATIC ${PAINTERENGINE_SOURCES})
target_include_directories(PainterEngine PUBLIC
    "${PAINTERENGINE_ROOT}"
    "${PAINTERENGINE_ROOT}/core"
    "${PAINTERENGINE_ROOT}/kernel"
    "${PAINTERENGINE_ROOT}/runtime"
    "${PAINTERENGINE_ROOT}/platform/linux"
)
target_link_libraries(PainterEngine PUBLIC
    OpenGL::GL glut Threads::Threads m dl X11
)

file(GLOB LIVEEDITOR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
add_executable(PainterEngine_LiveEditor ${LIVEEDITOR_SOURCES})
target_link_libraries(PainterEngine_LiveEditor PRIVATE PainterEngine)
target_include_directories(PainterEngine_LiveEditor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PAINTERENGINE_ROOT}"
)

set_target_properties(PainterEngine_LiveEditor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(PainterEngine_LiveFrameworkPancTest
        tests/PX_LiveFrameworkPancTest.c
    )
    target_link_libraries(PainterEngine_LiveFrameworkPancTest PRIVATE PainterEngine)
    target_include_directories(PainterEngine_LiveFrameworkPancTest PRIVATE
        "${PAINTERENGINE_ROOT}"
    )
    add_test(
        NAME liveframework_panc_tracks_keypoints
        COMMAND PainterEngine_LiveFrameworkPancTest
    )
endif()
