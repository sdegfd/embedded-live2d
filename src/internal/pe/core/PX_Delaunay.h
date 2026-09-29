/** @file @brief Delaunay 三角剖分的数据结构与接口声明 */

#ifndef PX_DELAUNAY_H
#define PX_DELAUNAY_H

#include "PX_Vector.h"

/** Delaunay 三角形：由三个顶点索引构成 */
typedef struct
{
    px_int index1; /**< 第一个顶点索引 */
    px_int index2; /**< 第二个顶点索引 */
    px_int index3; /**< 第三个顶点索引 */
} PX_Delaunay_Triangle;

#endif
