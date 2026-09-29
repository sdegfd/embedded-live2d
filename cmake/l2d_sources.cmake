# Explicit runtime sources. Do not glob core/*.c or kernel/*.c.
# PX_Memory.c and PX_Delaunay.c stay in the tree for the editor-side history
# of this snapshot, but the playback runtime does not link them.
set(L2D_RUNTIME_SOURCES
    src/engine/live2d_engine.c
    src/api/l2d_api.c
    src/memory/l2d_allocator.c
    src/memory/l2d_arena.c
    src/pipeline/l2d_roi.c
    src/internal/pe/core/PX_Typedef.c
    src/internal/pe/core/PX_Log.c
    src/internal/pe/core/PX_MemoryPool.c
    src/internal/pe/core/PX_Vector.c
    src/internal/pe/core/PX_Quicksort.c
    src/internal/pe/core/PX_Surface.c
    src/internal/pe/core/PX_Texture.c
    src/internal/pe/core/PX_BaseGeo.c
    src/internal/pe/kernel/PX_LiveFramework.c
    src/internal/pe/kernel/PX_LiveRealtime.c
    src/internal/pe/kernel/PX_LiveDeviceFormat.c
)
