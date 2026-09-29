# Documentation

## Current project

- [Architecture and mode ownership](architecture/overview.md)
- [Public API contracts](architecture/api-contracts.md)
- [Model format and compatibility](formats/compatibility.md)
- [Formal five-axis model identity](formats/model-baseline.md)
- [Porting guide](porting/porting-guide.md)
- [Benchmark workflow](benchmarks/README.md)

## Development record

- `optimization/`: Phase 0–3B reports, raw CRC and timing captures, serial logs and refactor captures.
- `benchmarks/`: curated baselines and auxiliary CPU/detail captures.
- `worklog/`: optimization plans and ongoing engineering notes.
- `history/refactor/`: Gate A–D handoff and audit records. Paths in those records describe the tree at the time of capture.
- `history/`: previous editor, PC preview and P4 handoffs retained as context. Retired code remains recoverable from Git history.

Historical performance numbers must be read with their model SHA, SDK configuration and stage. The current model is documented in [model-baseline.md](formats/model-baseline.md).
