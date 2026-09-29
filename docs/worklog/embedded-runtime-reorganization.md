# Embedded Runtime reorganization worklog

Work continued from `bc99a14` on `refactor/embedded-runtime`; Gate A–D code and historical captures were retained. Commit `b9e8f4b` exposed Timeline playback in the public C API and pruned editor authoring functions from the embedded LiveFramework. Commit `43e9d01` placed the shared runtime in `Live2D/`, active Windows editor in `Live2D_Editor/`, P4/S3 projects in `examples/`, the formal model in `models/`, and current and historical material under `docs/`.

`PainterEngine/` is retained byte for byte as a PC source reference. The old editor source duplicate, PC preview copy, `project/release.live`, transient image output and superseded root `src/`, `include/`, `ports/`, `firmware/`, `project/`, `doc/`, `bench/` and `tools/` paths were moved or removed after their active content was relocated. Retired tracked copies remain recoverable from Git history. No build target refers to those old root paths.

The Host and P4 runners now share the `short-v1` scenario generator under `Live2D/bench/scenarios/`; P4 serial capture and CRC comparison stay with its example. The P4 reload test exposed an oversized fixed 16 MiB model arena. The model loader's reserve is now based on wire size; the 800796-byte formal file reserves 2650168 bytes and uses 837184 bytes. `RELOAD_STATIC` and `RELOAD_POSE` reload that same file, not a different model.

Verification, raw CSVs, serial logs, configuration and timing comparison are indexed in [the final reorganization report](../benchmarks/reorg-final.md). The next runtime performance phase can start from its unchanged five-axis CRC baseline and the measured +2.05% MULTI_AXIS frame average regression.
