# Embedded Runtime reorganization worklog

Work continued from `bc99a14` on `refactor/embedded-runtime`; Gate A–D code and historical captures were retained. Commit `b9e8f4b` exposed Timeline playback in the public C API and pruned editor authoring functions from the embedded LiveFramework. Commit `43e9d01` placed the shared runtime in `Live2D/`, active Windows editor in `Live2D_Editor/`, P4/S3 projects in `examples/`, the formal model in `models/`, and current and historical material under `docs/`.

`PainterEngine/` is retained byte for byte as a PC source reference. The old editor source duplicate, PC preview copy, `project/release.live`, transient image output and superseded root `src/`, `include/`, `ports/`, `firmware/`, `project/`, `doc/`, `bench/` and `tools/` paths were moved or removed after their active content was relocated. Retired tracked copies remain recoverable from Git history. No build target refers to those old root paths.

The Host and P4 runners now share the `short-v1` scenario generator under `Live2D/bench/scenarios/`; P4 serial capture and CRC comparison stay with its example. The P4 reload test exposed an oversized fixed 16 MiB model arena. The model loader's reserve is now based on wire size; the 800796-byte formal file reserves 2650168 bytes and uses 837184 bytes. `RELOAD_STATIC` and `RELOAD_POSE` reload that same file, not a different model.

Verification, raw CSVs, serial logs, configuration and timing comparison are indexed in [the final reorganization report](../benchmarks/reorg-final.md). The next runtime performance phase can start from its unchanged five-axis CRC baseline and the measured +2.05% MULTI_AXIS frame average regression.

## 2026-09-29 memory assumption

The earlier note that the hot path must stay friendly to internal-RAM-only chips with no PSRAM is withdrawn. It is recorded here so it is not reapplied.

The working assumption for this performance phase is that the target has PSRAM, at least 8 MB. The current P4 captures still use the board's 32 MB PSRAM at 200 MHz; 8 MB is the floor for this phase, not a new allocator cap. Space-for-time buffers are allowed when a measurement shows they reduce frame time. Steady update and render still do not call the allocator; any extra buffer is reserved at init. A change that does not move measured milliseconds is reverted.

## 2026-09-29 raster span trial

Word loads/stores plus a fully-inside span path were measured on P4 `short-v1` against the `846b65e` baseline in [reorg-p4-timing](../benchmarks/reorg-p4-timing/summary.csv). Host pose CRCs stayed exact. There is no approximate coverage path.

| Scenario | Baseline raster / frame µs | Trial raster / frame µs |
| --- | ---: | ---: |
| STATIC | 19384.86 / 25826.04 | 19512.94 / 25935.00 |
| EYE_L_SWEEP | 19315.70 / 27319.54 | 19429.94 / 27495.34 |
| MULTI_AXIS | 19237.90 / 27896.66 | 19395.46 / 28041.72 |

MULTI_AXIS frame p95 moved from 28627 µs to 28432 µs. Deadline misses stayed 0. The raster average rose about 158 µs, so the trial is not kept. Raw rows are in [phase4_raster_span](../optimization/phase4_raster_span/summary.csv). The write-up is [040-raster-word-span.md](../optimization/040-raster-word-span.md). The extra byte memory operations were not extra PSRAM misses; the loop is stalled on cache fills. `__builtin_prefetch` compiles away on this P4 march, and a 4-wide software pipeline spilled registers, so neither was flashed.

Espressif's documentation MCP (`search_espressif_sources` on `https://mcp.espressif.com/docs`) confirms L2 cache is selectable at 128 / 256 / 512 KB out of the 768 KB L2MEM. The kept platform change is L2 cache 256 KB with the internal DMA reserve lowered from 256 KB to 128 KB. A 256 KB cache alone does not boot: the DMA heap shrinks from 384 KB to 256 KB and `esp_psram_extram_reserve_dma_pool(262144)` returns `ESP_ERR_NO_MEM`. The reserve reduction by itself, with L2 left at 128 KB, matches the baseline within a few tens of microseconds, so the raster gain belongs to the larger cache. Write-up: [041-l2-cache-256.md](../optimization/041-l2-cache-256.md). Shared raster source is unchanged. 512 KB L2 was not flashed; it would take another 256 KB from that same DMA heap.

A host count on one fast-nearest frame showed 8671 new 64-byte texture lines and 7237 new 128-byte lines, with only 597 row changes. The kept follow-up widens the L2 line to 128 bytes and aligns the P4 ARGB and RGB565 buffers to 128 bytes so PPA fill still accepts them. A 64-byte-aligned buffer made PPA fill return `ESP_ERR_INVALID_ARG` and fall back to a CPU clear. With fill restored, MULTI_AXIS raster is 16645.36 µs and the frame average is 25098.22 µs, p95 25687 µs, max 25963 µs, deadline misses 0. Write-up: [042-l2-line-128.md](../optimization/042-l2-line-128.md).

On that cache configuration the opaque fast-nearest pixel still used three byte loads and three byte stores. Replacing only that blend with one 32-bit load and store, same channel formula, passed the host CRC lock and lowered MULTI_AXIS raster to 15457.30 µs and the frame average to 23976.12 µs, p95 24476 µs, max 24751 µs. Write-up: [043-nearest-word-blend.md](../optimization/043-nearest-word-blend.md). The earlier safe-span predicate is not part of this change.

The scanline loop was still inside `PX_LiveFrameworkRenderCurrent`, reloading spilled edge and affine state and marshalling the generic span on every row. A `noinline` fast-nearest kernel keeps that state in its own registers, hoists the 16.16 UV step to the triangle, and uses the same coverage and blend formulas. Host pose CRCs match at both the default flags and `-O3`. MULTI_AXIS raster is 10606.80 µs and the frame average is 19184.78 µs, p95 19782 µs, max 20014 µs, deadline misses 0. RT30, hierarchy, clear, and PPA SRM stay in the same range, so the saving is raster. Write-up: [044-fast-nearest-kernel.md](../optimization/044-fast-nearest-kernel.md). Reciprocal edges and PIE were not part of this step.

## 2026-09-30 runtime closeout

Performance work on this branch stops at the fast-nearest kernel. The follow-up is portability, not another P4 pass. `l2d_raster_fast_nearest` now lives in `Live2D/src/raster/l2d_raster_nearest.c`; the generic span stays in the legacy framework file. The ESP-IDF component uses the host flags `-fno-strict-aliasing -ffp-contract=off`. Host pose CRCs stayed on the locked values. P4 short-v1 on that image measures MULTI_AXIS raster 10957.20 µs and frame 19487.44 µs, p95 20086 µs, max 20147 µs, deadline misses 0. The raster average is about 350 µs above `763c358`. The flags stay so P4 and S3 match the host CRC contract. The 459-row device CRC was not recaptured. Public ABI, Timeline and RT30 were left as they are. Write-up: [045-runtime-closeout.md](../optimization/045-runtime-closeout.md). Status and deferred work: [status.md](../architecture/status.md).
