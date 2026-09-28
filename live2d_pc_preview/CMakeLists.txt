cmake_minimum_required(VERSION 3.16)

option(
  CORE_CRL_MD
  "Use Cubism Core that is multithread-specific and DLL-specific version"
  OFF
)

set(APP_NAME Live2DPreview)
set(WORKSPACE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SDK_ROOT_PATH ${WORKSPACE_ROOT}/CubismSdkForNative-5-r.5)
set(CORE_PATH ${SDK_ROOT_PATH}/Core)
set(FRAMEWORK_PATH ${SDK_ROOT_PATH}/Framework)
set(D3D11_SAMPLE_SRC_PATH ${SDK_ROOT_PATH}/Samples/D3D11/Demo/proj.d3d11.cmake/src)
set(D3D11_SAMPLE_RESOURCES_PATH ${SDK_ROOT_PATH}/Samples/Resources)
set(SAMPLE_SHADER_PATH ${SDK_ROOT_PATH}/Samples/D3D11/Shaders)
set(FRAMEWORK_SHADER_PATH ${FRAMEWORK_PATH}/src/Rendering/D3D11/Shaders)
set(COMMON_SRC_PATH ${SDK_ROOT_PATH}/Samples/Common)
set(PREVIEW_RESOURCE_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyPreviewResources.cmake)
set(PREVIEW_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)

project(${APP_NAME})

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${APP_NAME})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/${APP_NAME})
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "Configurations" FORCE)
set(CMAKE_SUPPRESS_REGENERATION ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH x86_64)
else()
  set(ARCH x86)
endif()

if(MSVC_VERSION GREATER_EQUAL 1910 AND MSVC_VERSION LESS 1920)
  set(COMPILER 141)
elseif(MSVC_VERSION GREATER_EQUAL 1920 AND MSVC_VERSION LESS 1930)
  set(COMPILER 142)
elseif(MSVC_VERSION GREATER_EQUAL 1930 AND MSVC_VERSION LESS 1950)
  set(COMPILER 143)
elseif(MSVC_VERSION GREATER_EQUAL 1950)
  set(COMPILER 143)
elseif(MSVC)
  message(FATAL_ERROR "[${APP_NAME}] Unsupported Visual C++ compiler used.")
else()
  message(FATAL_ERROR "[${APP_NAME}] This preview project requires MSVC because the packaged Cubism Core libraries are built for MSVC 141/142/143-compatible toolsets.")
endif()

if(CORE_CRL_MD)
  set(CRT MD)
else()
  set(CRT MT)
endif()

add_library(Live2DCubismCore STATIC IMPORTED)
set(CORE_LIB_SUFFIX ${CORE_PATH}/lib/windows/${ARCH}/${COMPILER})
set_target_properties(Live2DCubismCore
  PROPERTIES
    IMPORTED_LOCATION_DEBUG ${CORE_LIB_SUFFIX}/Live2DCubismCore_${CRT}d.lib
    IMPORTED_LOCATION_RELEASE ${CORE_LIB_SUFFIX}/Live2DCubismCore_${CRT}.lib
    INTERFACE_INCLUDE_DIRECTORIES ${CORE_PATH}/include
)

set(FRAMEWORK_SOURCE D3D11)
add_subdirectory(${FRAMEWORK_PATH} ${CMAKE_CURRENT_BINARY_DIR}/Framework)
target_link_libraries(Framework
  Live2DCubismCore
  d3d11.lib
  d3dcompiler.lib
)

add_executable(${APP_NAME} WIN32)
add_subdirectory(${COMMON_SRC_PATH} ${CMAKE_CURRENT_BINARY_DIR}/src/Common)
add_subdirectory(${D3D11_SAMPLE_SRC_PATH} ${CMAKE_CURRENT_BINARY_DIR}/src/D3D11Sample)

target_sources(${APP_NAME}
  PRIVATE
    ${PREVIEW_SRC_PATH}/ComBootstrap.cpp
    ${PREVIEW_SRC_PATH}/WICTextureLoader.cpp
)

target_include_directories(${APP_NAME}
  PRIVATE
    ${PREVIEW_SRC_PATH}
)

target_link_libraries(${APP_NAME}
  Framework
  windowscodecs.lib
  ole32.lib
  debug -NODEFAULTLIB:libcmtd.lib
  optimized -NODEFAULTLIB:libcmt.lib
)

target_compile_options(${APP_NAME} PRIVATE /MP)

add_custom_command(
  TARGET ${APP_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${APP_NAME}>/Resources
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${D3D11_SAMPLE_RESOURCES_PATH}/back_class_normal.png $<TARGET_FILE_DIR:${APP_NAME}>/Resources/back_class_normal.png
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${D3D11_SAMPLE_RESOURCES_PATH}/icon_gear.png $<TARGET_FILE_DIR:${APP_NAME}>/Resources/icon_gear.png
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${D3D11_SAMPLE_RESOURCES_PATH}/close.png $<TARGET_FILE_DIR:${APP_NAME}>/Resources/close.png
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${SAMPLE_SHADER_PATH} $<TARGET_FILE_DIR:${APP_NAME}>/SampleShaders
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${FRAMEWORK_SHADER_PATH} $<TARGET_FILE_DIR:${APP_NAME}>/FrameworkShaders
  COMMAND ${CMAKE_COMMAND} -D TARGET_DIR=$<TARGET_FILE_DIR:${APP_NAME}>/Resources -D WORKSPACE_ROOT=${WORKSPACE_ROOT} -P ${PREVIEW_RESOURCE_SCRIPT}
)

set_target_properties(${APP_NAME} PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin/${APP_NAME}/${CMAKE_CFG_INTDIR}
)