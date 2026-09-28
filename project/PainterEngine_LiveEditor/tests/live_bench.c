/*
 * Headless stage timer for a .live model. Does not modify the file.
 *
 * Build: tests\build_live_bench.bat
 * Run:   live_bench.exe D:\live2d\project\esp.live
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

#include "kernel/PX_LiveFramework.h"

#define BENCH_POOL_BYTES (64u * 1024u * 1024u)
#define BENCH_IDLE_FRAMES 40
#define BENCH_SWEEP_STEP 1

static int g_sampler = -1;
static LARGE_INTEGER g_freq;

static double usec_between(LARGE_INTEGER a, LARGE_INTEGER b)
{
	return (double)(b.QuadPart - a.QuadPart) * 1000000.0 / (double)g_freq.QuadPart;
}

static int compare_double(const void *pa, const void *pb)
{
	double a = *(const double *)pa;
	double b = *(const double *)pb;
	return (a > b) - (a < b);
}

static void print_stats(const char *name, double *values, int count)
{
	double sorted[128];
	double sum = 0;
	int i;
	int n = count > 128 ? 128 : count;
	for (i = 0; i < n; i++)
	{
		sorted[i] = values[i];
		sum += values[i];
	}
	qsort(sorted, (size_t)n, sizeof(double), compare_double);
	printf("  %s n=%d avg=%.1f p50=%.1f p95=%.1f p99=%.1f min=%.1f max=%.1f us\n",
		name, n, n ? sum / n : 0,
		n ? sorted[(n - 1) * 50 / 100] : 0,
		n ? sorted[(n - 1) * 95 / 100] : 0,
		n ? sorted[(n - 1) * 99 / 100] : 0,
		n ? sorted[0] : 0,
		n ? sorted[n - 1] : 0);
}

static px_uint32 fnv_bytes(const void *data, px_uint32 size, px_uint32 hash)
{
	const px_byte *bytes = (const px_byte *)data;
	px_uint32 i;
	for (i = 0; i < size; i++)
	{
		hash ^= bytes[i];
		hash *= 16777619u;
	}
	return hash;
}

static px_uint32 pose_checksum(const PX_LiveFramework *live)
{
	px_uint32 hash = 2166136261u;
	px_int layerIndex;
	for (layerIndex = 0; layerIndex < live->layers.size; layerIndex++)
	{
		const PX_LiveLayer *layer = PX_VECTORAT(PX_LiveLayer, &live->layers, layerIndex);
		px_int vertexIndex;
		hash = fnv_bytes(&layer->currentKeyPoint, sizeof(layer->currentKeyPoint), hash);
		hash = fnv_bytes(&layer->rel_currentTranslation, sizeof(layer->rel_currentTranslation), hash);
		hash = fnv_bytes(&layer->rel_currentRotationAngle, sizeof(layer->rel_currentRotationAngle), hash);
		hash = fnv_bytes(&layer->rel_currentStretch, sizeof(layer->rel_currentStretch), hash);
		hash = fnv_bytes(&layer->RenderTextureIndex, sizeof(layer->RenderTextureIndex), hash);
		for (vertexIndex = 0; vertexIndex < layer->vertices.size; vertexIndex++)
		{
			const PX_LiveVertex *vertex = PX_VECTORAT(PX_LiveVertex, &layer->vertices, vertexIndex);
			hash = fnv_bytes(&vertex->currentPosition, sizeof(vertex->currentPosition), hash);
			hash = fnv_bytes(&vertex->currentTranslation, sizeof(vertex->currentTranslation), hash);
		}
	}
	return hash;
}

static px_uint32 pixel_checksum(const px_surface *surface)
{
	return fnv_bytes(surface->surfaceBuffer,
		(px_uint32)surface->width * (px_uint32)surface->height * (px_uint32)sizeof(px_color),
		2166136261u);
}

static int count_triangles(const PX_LiveFramework *live)
{
	int count = 0;
	int i;
	for (i = 0; i < live->layers.size; i++)
	{
		count += PX_VECTORAT(PX_LiveLayer, &live->layers, i)->triangles.size;
	}
	return count;
}

static int count_vertices(const PX_LiveFramework *live)
{
	int count = 0;
	int i;
	for (i = 0; i < live->layers.size; i++)
	{
		count += PX_VECTORAT(PX_LiveLayer, &live->layers, i)->vertices.size;
	}
	return count;
}

static int load_file(const char *path, px_byte **outData, int *outSize)
{
	FILE *file = fopen(path, "rb");
	long size;
	px_byte *data;
	if (!file)
	{
		printf("打不开文件: %s\n", path);
		return 0;
	}
	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return 0;
	}
	size = ftell(file);
	if (size <= 0 || size > 64 * 1024 * 1024)
	{
		printf("文件大小异常: %ld\n", size);
		fclose(file);
		return 0;
	}
	rewind(file);
	data = (px_byte *)malloc((size_t)size);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size)
	{
		free(data);
		fclose(file);
		return 0;
	}
	fclose(file);
	*outData = data;
	*outSize = (int)size;
	return 1;
}

static void frame_once(PX_LiveFramework *live, px_surface *surface, double *evalUs, double *physicalUs, double *renderUs)
{
	LARGE_INTEGER a, b;
	QueryPerformanceCounter(&a);
	PX_LiveRealtimeUpdate(live);
	QueryPerformanceCounter(&b);
	*evalUs = usec_between(a, b);
	QueryPerformanceCounter(&a);
	PX_LiveFrameworkUpdate(live, 16);
	QueryPerformanceCounter(&b);
	*physicalUs = usec_between(a, b);
	PX_SurfaceClearAll(surface, PX_COLOR(0, 0, 0, 0));
	QueryPerformanceCounter(&a);
	PX_LiveFrameworkRenderCurrent(surface, live, 0, 0, PX_ALIGN_LEFTTOP);
	QueryPerformanceCounter(&b);
	*renderUs = usec_between(a, b);
}

static void measure_sequence(PX_LiveFramework *live, px_surface *surface, const char *title, int frames, int movingAxis, float positionBegin, float positionStep)
{
	double evals[128];
	double physicals[128];
	double renders[128];
	int i;
	if (frames > 128)
	{
		frames = 128;
	}
	printf("%s\n", title);
	for (i = 0; i < frames; i++)
	{
		if (movingAxis >= 0)
		{
			PX_LiveRealtimeSetAxisPosition(live, movingAxis, positionBegin + positionStep * (float)i, PX_LIVE_REALTIME_WEIGHT_ONE_Q15);
		}
		frame_once(live, surface, &evals[i], &physicals[i], &renders[i]);
	}
	print_stats("parameter", evals, frames);
	print_stats("hierarchy+vertices", physicals, frames);
	print_stats("raster", renders, frames);
	printf("  pose=0x%08X pixels=0x%08X\n", pose_checksum(live), pixel_checksum(surface));
}

static float cross2(px_point a, px_point b, px_point c)
{
	return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static void print_first_flip(const PX_LiveFramework *live)
{
	int layerIndex;
	for (layerIndex = 0; layerIndex < live->layers.size; layerIndex++)
	{
		const PX_LiveLayer *layer = PX_VECTORAT(PX_LiveLayer, &live->layers, layerIndex);
		int triangleIndex;
		for (triangleIndex = 0; triangleIndex < layer->triangles.size; triangleIndex++)
		{
			const PX_LiveTriangle *triangle = PX_VECTORAT(PX_LiveTriangle, &layer->triangles, triangleIndex);
			const PX_LiveVertex *v0;
			const PX_LiveVertex *v1;
			const PX_LiveVertex *v2;
			float sourceCross;
			float currentCross;
			if (triangle->index1 < 0 || triangle->index2 < 0 || triangle->index3 < 0 ||
				triangle->index1 >= layer->vertices.size || triangle->index2 >= layer->vertices.size ||
				triangle->index3 >= layer->vertices.size)
			{
				continue;
			}
			v0 = PX_VECTORAT(PX_LiveVertex, &layer->vertices, triangle->index1);
			v1 = PX_VECTORAT(PX_LiveVertex, &layer->vertices, triangle->index2);
			v2 = PX_VECTORAT(PX_LiveVertex, &layer->vertices, triangle->index3);
			sourceCross = cross2(v0->sourcePosition, v1->sourcePosition, v2->sourcePosition);
			currentCross = cross2(v0->currentPosition, v1->currentPosition, v2->currentPosition);
			if (sourceCross * currentCross < 0 && fabsf(sourceCross) > 0.05f && fabsf(currentCross) > 0.05f)
			{
				printf("  首个翻折 图层=%s 三角=%d 顶点=%d,%d,%d 源面积=%.3f 当前面积=%.3f\n",
					layer->id, triangleIndex, triangle->index1, triangle->index2, triangle->index3,
					sourceCross, currentCross);
				return;
			}
		}
	}
}

static void compare_samplers(PX_LiveFramework *live, px_surface *surface)
{
	px_color *reference;
	int pixels = surface->width * surface->height;
	int mode;
	reference = (px_color *)malloc((size_t)pixels * sizeof(px_color));
	if (!reference)
	{
		return;
	}
	live->samplerMode = 0;
	PX_SurfaceClearAll(surface, PX_COLOR(0, 0, 0, 0));
	PX_LiveFrameworkRenderCurrent(surface, live, 0, 0, PX_ALIGN_LEFTTOP);
	memcpy(reference, surface->surfaceBuffer, (size_t)pixels * sizeof(px_color));
	for (mode = 1; mode <= 3; mode++)
	{
		int i;
		int diffPixels = 0;
		int maxDiff = 0;
		LARGE_INTEGER a, b;
		const char *name = mode == 1 ? "float双线性" : mode == 2 ? "最近邻" : "预乘过滤";
		live->samplerMode = (px_uchar)mode;
		PX_SurfaceClearAll(surface, PX_COLOR(0, 0, 0, 0));
		QueryPerformanceCounter(&a);
		PX_LiveFrameworkRenderCurrent(surface, live, 0, 0, PX_ALIGN_LEFTTOP);
		QueryPerformanceCounter(&b);
		for (i = 0; i < pixels; i++)
		{
			int dr = abs((int)surface->surfaceBuffer[i]._argb.r - (int)reference[i]._argb.r);
			int dg = abs((int)surface->surfaceBuffer[i]._argb.g - (int)reference[i]._argb.g);
			int db = abs((int)surface->surfaceBuffer[i]._argb.b - (int)reference[i]._argb.b);
			int da = abs((int)surface->surfaceBuffer[i]._argb.a - (int)reference[i]._argb.a);
			int diff = dr;
			if (dg > diff) diff = dg;
			if (db > diff) diff = db;
			if (da > diff) diff = da;
			if (diff)
			{
				diffPixels++;
				if (diff > maxDiff) maxDiff = diff;
			}
		}
		printf("  采样对照 %s  差异像素=%d  最大通道差=%d  单帧 %.1f us\n",
			name, diffPixels, maxDiff, usec_between(a, b));
	}
	live->samplerMode = (px_uchar)(g_sampler >= 0 ? g_sampler : 0);
	free(reference);
}

static void contribution_oracle(PX_LiveFramework *live)
{
	int axisIndex;
	int step;
	px_uint32 incremental;
	px_uint32 rebuilt;
	if (PX_LiveRealtimeGetAxisCount(live) <= 0)
	{
		printf("  无实时轴，跳过贡献对照\n");
		return;
	}
	for (step = 0; step < 8; step++)
	{
		float position = (float)step * 3.7f;
		if (position > 29.0f) position = 29.0f;
		for (axisIndex = 0; axisIndex < PX_LiveRealtimeGetAxisCount(live); axisIndex++)
		{
			float axisPosition = (axisIndex == (step & 3)) ? position : (float)(29 - step);
			PX_LiveRealtimeSetAxisPosition(live, axisIndex, axisPosition, PX_LIVE_REALTIME_WEIGHT_ONE_Q15);
		}
		if (!PX_LiveRealtimeUpdate(live))
		{
			printf("  贡献对照：增量求值失败\n");
			return;
		}
		PX_LiveFrameworkUpdate(live, 16);
		incremental = pose_checksum(live);
		PX_LiveRealtimeInvalidateContributions(live);
		if (!PX_LiveRealtimeUpdate(live))
		{
			printf("  贡献对照：全量求值失败\n");
			return;
		}
		live->meshPoseRevision = 0;
		PX_LiveFrameworkUpdate(live, 16);
		rebuilt = pose_checksum(live);
		if (incremental != rebuilt)
		{
			printf("  贡献对照失败 step=%d 增量=0x%08X 全量=0x%08X\n", step, incremental, rebuilt);
			return;
		}
	}
	printf("  贡献对照 8 组姿态与全量重建一致\n");
}

static int alpha_contract(void)
{
	px_byte storage[64 * 1024];
	px_memorypool pool = MP_Create(storage, sizeof(storage));
	px_surface surface;
	px_color blackHit;
	px_color whiteHit;
	int straight;
	int onWhite;
	if (!PX_SurfaceCreate(&pool, 4, 2, &surface))
	{
		printf("Alpha 契约：表面创建失败\n");
		return 1;
	}
	PX_SurfaceClearAll(&surface, PX_COLOR(0, 0, 0, 0));
	PX_SurfaceDrawPixel(&surface, 0, 0, PX_COLOR(128, 200, 200, 200));
	blackHit = surface.surfaceBuffer[0];
	straight = (200 * 129) >> 8;
	PX_SurfaceClearAll(&surface, PX_COLOR(255, 255, 255, 255));
	PX_SurfaceDrawPixel(&surface, 1, 0, PX_COLOR(128, 200, 200, 200));
	whiteHit = surface.surfaceBuffer[1];
	onWhite = ((256 - 128) * 255 + 200 * 129) >> 8;
	printf("Alpha 契约 黑底=%d 期望=%d 白底=%d 期望=%d\n",
		blackHit._argb.r, straight, whiteHit._argb.r, onWhite);
	PX_SurfaceFree(&surface);
	if (blackHit._argb.r != straight || whiteHit._argb.r != (px_uchar)onWhite)
	{
		printf("Alpha 契约失败：画布不是直通 Alpha\n");
		return 1;
	}
	return 0;
}

static int bench_model(const char *path)
{
	px_byte *fileData = PX_NULL;
	int fileSize = 0;
	px_byte *poolStorage;
	px_memorypool pool;
	PX_LiveFramework live;
	px_surface surface;
	LARGE_INTEGER a, b;
	int axisIndex;
	PX_LiveMeshDefects defects;
	if (!load_file(path, &fileData, &fileSize))
	{
		return 1;
	}
	poolStorage = (px_byte *)malloc(BENCH_POOL_BYTES);
	if (!poolStorage)
	{
		free(fileData);
		return 1;
	}
	pool = MP_Create(poolStorage, BENCH_POOL_BYTES);
	memset(&live, 0, sizeof(live));
	QueryPerformanceCounter(&a);
	if (!PX_LiveFrameworkImport(&pool, &live, fileData, fileSize))
	{
		printf("导入失败: %s\n", path);
		free(fileData);
		free(poolStorage);
		return 1;
	}
	QueryPerformanceCounter(&b);
	printf("\n模型 %s  %d 字节\n", path, fileSize);
	printf("尺寸 %d x %d  图层 %d  顶点 %d  三角形 %d  纹理 %d  动画 %d  轴 %d\n",
		live.width, live.height, live.layers.size, count_vertices(&live), count_triangles(&live),
		live.livetextures.size, live.liveAnimations.size, PX_LiveRealtimeGetAxisCount(&live));
	printf("导入 %.1f us  池已用 %u  峰值 %u\n", usec_between(a, b), MP_GetCurrentUsage(&pool), MP_GetPeakUsage(&pool));
	if (g_sampler >= 0)
	{
		live.samplerMode = (px_uchar)g_sampler;
		printf("采样模式 %d\n", g_sampler);
	}
	for (axisIndex = 0; axisIndex < PX_LiveRealtimeGetAxisCount(&live); axisIndex++)
	{
		const PX_LiveRealtimeAxis *axis = PX_LiveRealtimeGetAxisConst(&live, axisIndex);
		printf("  轴 %d id=%s bindings=%u stride=%u weight=%u sample=%u\n",
			axisIndex, axis->id, (unsigned)axis->bindingCount, (unsigned)axis->sampleStride,
			(unsigned)axis->weightQ15, (unsigned)axis->sampleIndex);
	}
	if (!PX_SurfaceCreate(&pool, live.width > 0 ? live.width : 320, live.height > 0 ? live.height : 320, &surface))
	{
		printf("表面创建失败\n");
		PX_LiveFrameworkFree(&live);
		free(fileData);
		free(poolStorage);
		return 1;
	}
	if (!PX_LiveRealtimeEnter(&live))
	{
		printf("进入实时模式失败\n");
		PX_SurfaceFree(&surface);
		PX_LiveFrameworkFree(&live);
		free(fileData);
		free(poolStorage);
		return 1;
	}
	for (axisIndex = 0; axisIndex < PX_LiveRealtimeGetAxisCount(&live); axisIndex++)
	{
		PX_LiveRealtimeSetAxisSample(&live, axisIndex, 0, 0);
	}
	measure_sequence(&live, &surface, "静止（权重 0，参数不变）", BENCH_IDLE_FRAMES, -1, 0, 0);
	for (axisIndex = 0; axisIndex < PX_LiveRealtimeGetAxisCount(&live); axisIndex++)
	{
		const PX_LiveRealtimeAxis *axis = PX_LiveRealtimeGetAxisConst(&live, axisIndex);
		char title[128];
		int other;
		for (other = 0; other < PX_LiveRealtimeGetAxisCount(&live); other++)
		{
			PX_LiveRealtimeSetAxisSample(&live, other, 0, other == axisIndex ? PX_LIVE_REALTIME_WEIGHT_ONE_Q15 : 0);
		}
		sprintf(title, "单轴连续 %s  0→29 步长 %d", axis->id, BENCH_SWEEP_STEP);
		measure_sequence(&live, &surface, title, 30 / BENCH_SWEEP_STEP, axisIndex, 0, (float)BENCH_SWEEP_STEP);
	}
	{
		int other;
		for (other = 0; other < PX_LiveRealtimeGetAxisCount(&live); other++)
		{
			PX_LiveRealtimeSetAxisPosition(&live, other, 14.5f, PX_LIVE_REALTIME_WEIGHT_ONE_Q15);
		}
		measure_sequence(&live, &surface, "全部轴同时停在 14.5，再重复求值", 20, -1, 0, 0);
		PX_LiveFrameworkCountMeshDefects(&live, &defects);
		printf("  14.5 全轴缺陷 退化=%d 翻折=%d UV越界=%d\n", defects.degenerate, defects.flipped, defects.uvOutside);
		for (other = 0; other < PX_LiveRealtimeGetAxisCount(&live); other++)
		{
			PX_LiveRealtimeSetAxisPosition(&live, other, 29.0f, PX_LIVE_REALTIME_WEIGHT_ONE_Q15);
		}
		measure_sequence(&live, &surface, "全部轴同时停在 29", 5, -1, 0, 0);
		PX_LiveFrameworkCountMeshDefects(&live, &defects);
		printf("  29 全轴缺陷 退化=%d 翻折=%d UV越界=%d\n", defects.degenerate, defects.flipped, defects.uvOutside);
		print_first_flip(&live);
	}
	compare_samplers(&live, &surface);
	contribution_oracle(&live);
	printf("结束池已用 %u 峰值 %u\n", MP_GetCurrentUsage(&pool), MP_GetPeakUsage(&pool));
	PX_SurfaceFree(&surface);
	PX_LiveFrameworkFree(&live);
	free(fileData);
	free(poolStorage);
	return 0;
}

int main(int argc, char **argv)
{
	int i;
	int failed = 0;
	QueryPerformanceFrequency(&g_freq);
	failed |= alpha_contract();
	if (argc < 2)
	{
		printf("用法: live_bench.exe [--sampler N] <model.live> [更多模型]\n");
		return failed ? 1 : 2;
	}
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--sampler") == 0 && i + 1 < argc)
		{
			g_sampler = atoi(argv[++i]);
			continue;
		}
		failed |= bench_model(argv[i]);
	}
	return failed ? 1 : 0;
}
