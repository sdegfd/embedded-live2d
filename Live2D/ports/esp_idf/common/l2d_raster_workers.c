#include "l2d_raster_workers.h"
#include "l2d_raster_nearest.h"
#include "l2d_config.h"

#if L2D_CFG_RASTER_BATCH
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#define SCRATCH_BYTES ((size_t)CONFIG_L2D_RASTER_SCRATCH_BYTES)
static void *scratch[2];

static void raster_rows(const l2d_raster_job *jobs, int count, const px_surface *surface, unsigned phase)
{
#if CONFIG_L2D_RENDER_RGB565_BANDS
    if (surface->rgb565_sink) {
        l2d_raster_jobs_rgb565(jobs,count,surface,scratch[phase],SCRATCH_BYTES,2,phase);
        return;
    }
#endif
#if CONFIG_L2D_RASTER_SCRATCH
    l2d_raster_jobs_scratch(jobs,count,surface,scratch[phase],SCRATCH_BYTES,2,phase);
#else
    l2d_raster_jobs(jobs,count,surface,2,phase);
#endif
}

static unsigned owners;
static TaskHandle_t worker;
static SemaphoreHandle_t gate, work_ready, work_done;
static struct {
    const l2d_raster_job *jobs;
    int count;
    px_surface surface;
} batch;

/** @brief 仅绘制分配给本核的 SRAM 条带，完成后通知提交方；不访问实例状态或性能全局变量。 */
static void raster_worker(void *arg)
{
    (void)arg;
    for (;;) {
        xSemaphoreTake(work_ready,portMAX_DELAY);
        raster_rows(batch.jobs,batch.count,&batch.surface,1);
        xSemaphoreGive(work_done);
    }
}

static void release_resources(void)
{
    if (worker) { vTaskDelete(worker); worker=NULL; }
    for (int i=0;i<2;++i) { heap_caps_free(scratch[i]); scratch[i]=NULL; }
    if (gate) { vSemaphoreDelete(gate); gate=NULL; }
    if (work_ready) { vSemaphoreDelete(work_ready); work_ready=NULL; }
    if (work_done) { vSemaphoreDelete(work_done); work_done=NULL; }
}

void l2d_raster_workers_shutdown(void)
{
    if (owners && --owners==0) release_resources();
}

bool l2d_raster_workers_init(void)
{
    if (worker) { ++owners; return true; }
#if CONFIG_L2D_RASTER_SCRATCH || CONFIG_L2D_RENDER_RGB565_BANDS
    scratch[0]=heap_caps_aligned_alloc(16,SCRATCH_BYTES,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    scratch[1]=heap_caps_aligned_alloc(16,SCRATCH_BYTES,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#endif
    gate=xSemaphoreCreateMutex();
    work_ready=xSemaphoreCreateBinary();
    work_done=xSemaphoreCreateBinary();
    if (
#if CONFIG_L2D_RASTER_SCRATCH || CONFIG_L2D_RENDER_RGB565_BANDS
        !scratch[0] || !scratch[1] ||
#endif
        !gate || !work_ready || !work_done ||
        xTaskCreatePinnedToCore(raster_worker,"l2d_raster",8192,NULL,6,&worker,1)!=pdPASS) {
        release_resources();
        return false;
    }
    owners=1;
    return true;
}

void l2d_port_raster_dispatch(const l2d_raster_job *jobs, int count, const px_surface *surface)
{
    if (!worker) {
        l2d_raster_jobs(jobs,count,surface,0,0);
        return;
    }
    /* Serializes concurrent instances. The caller retains jobs and pixels until
     * both row sets have completed. Semaphores provide publication barriers. */
    xSemaphoreTake(gate,portMAX_DELAY);
    batch.jobs=jobs; batch.count=count; batch.surface=*surface;
    xSemaphoreGive(work_ready);
    raster_rows(jobs,count,surface,0);
    xSemaphoreTake(work_done,portMAX_DELAY);
    xSemaphoreGive(gate);
}
#else
bool l2d_raster_workers_init(void) { return true; }
void l2d_raster_workers_shutdown(void) {}
void l2d_port_raster_dispatch(const l2d_raster_job *jobs, int count, const px_surface *surface)
{
    l2d_raster_jobs(jobs,count,surface,0,0);
}
#endif

int l2d_port_rgb565_supported(void)
{
#if L2D_CFG_RASTER_BATCH && CONFIG_L2D_RENDER_RGB565_BANDS
    return worker!=NULL;
#else
    return 0;
#endif
}
