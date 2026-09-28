# Live2D 引擎组件，基于 PainterEngine 实现 Live2D 模型的加载、动画和渲染
set(PAINTERENGINE_CORE
    "painterengine/core/PX_Typedef.c"
    "painterengine/core/PX_Log.c"
    "painterengine/core/PX_MemoryPool.c"
    "painterengine/core/PX_Memory.c"
    "painterengine/core/PX_Vector.c"
    "painterengine/core/PX_Quicksort.c"
    "painterengine/core/PX_Surface.c"
    "painterengine/core/PX_Texture.c"
    "painterengine/core/PX_BaseGeo.c"
    "painterengine/core/PX_Delaunay.c")

idf_component_register(
    SRCS
        "live2d_engine.c"
        "painterengine/kernel/PX_LiveFramework.c"
        "painterengine/kernel/PX_LiveRealtime.c"
        "painterengine/kernel/PX_LiveDeviceFormat.c"
        ${PAINTERENGINE_CORE}
    INCLUDE_DIRS
        "include"
        "painterengine/core"
        "painterengine/kernel"
    REQUIRES esp_common esp_timer heap)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O2
    -Wno-unused-function
    -Wno-maybe-uninitialized
    -Wno-error=array-parameter
    -Wno-error=char-subscripts
    -Wno-error=misleading-indentation
    -Wno-error=strict-aliasing
    -Wno-error=uninitialized
    -Wno-parentheses
    -Wno-format)
