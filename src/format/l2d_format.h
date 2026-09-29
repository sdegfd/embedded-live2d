/**
 * Load a legacy .live image into a framework object.
 * Failure frees only the object passed in. It does not touch any other model.
 */
#ifndef L2D_FORMAT_H
#define L2D_FORMAT_H

#include "l2d/l2d_types.h"
#include "PX_LiveFramework.h"

#include <stddef.h>

l2d_status_t l2d_format_import(px_memorypool *mp, PX_LiveFramework *live, const void *bytes,
                               size_t size);

#endif
