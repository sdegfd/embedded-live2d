/** @file @brief Delaunay 三角剖分的数据结构与接口声明 */

#ifndef PX_DELAUNAY_H
#define PX_DELAUNAY_H

#include "PX_Vector.h"

/** Delaunay 边：由两个顶点索引构成 */
typedef struct
{
    px_int index1; /**< 第一个顶点索引 */
    px_int index2; /**< 第二个顶点索引 */
} PX_Delaunay_Edge;

/** Delaunay 三角形：由三个顶点索引构成 */
typedef struct
{
    px_int index1; /**< 第一个顶点索引 */
    px_int index2; /**< 第二个顶点索引 */
    px_int index3; /**< 第三个顶点索引 */
} PX_Delaunay_Triangle;

/** 三角剖分返回类型 */
typedef enum
{
    PX_DELAUNAY_RETURN_TYPE_TRIANGLE,       /**< 返回三角形坐标 */
    PX_DELAUNAY_RETURN_TYPE_TRIANGLE_INDEX, /**< 返回三角形顶点索引 */
} PX_DELAUNAY_RETURN_TYPE;

/** 基于点集构建 Delaunay 三角网 */
px_bool PX_DelaunaryPointsBuild(px_memorypool *mp, px_point2D pt[], px_int count,
                                px_vector *out_triangles, PX_DELAUNAY_RETURN_TYPE type);

#endif
