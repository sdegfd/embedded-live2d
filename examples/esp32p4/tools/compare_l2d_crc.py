#!/usr/bin/env python3
"""Compare complete per-frame ARGB8888/RGB565 correctness captures."""
import argparse
import csv
from pathlib import Path


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("full", type=Path)
    parser.add_argument("roi", type=Path)
    args = parser.parse_args()
    full, roi = read_rows(args.full), read_rows(args.roi)
    if not full or len(full) != len(roi):
        print(f"FAIL: row count full={len(full)} roi={len(roi)}")
        return 1
    fields = ("pose", "scale_q100", "frame_id", "argb_crc32", "rgb565_crc32",
              "render_ok", "submit_ok", "backend")
    mismatches = []
    for index, (a, b) in enumerate(zip(full, roi), start=1):
        for field in fields:
            if a[field] != b[field]:
                mismatches.append((index, field, a[field], b[field]))
        if a["render_ok"] != "1" or a["submit_ok"] != "1" or \
           b["render_ok"] != "1" or b["submit_ok"] != "1":
            mismatches.append((index, "frame status", a["render_ok"], b["render_ok"]))
    if mismatches:
        for item in mismatches[:20]:
            print("FAIL:", *item)
        print(f"mismatches={len(mismatches)} frames={len(full)}")
        return 1
    print(f"PASS: {len(full)} ordered frames; ARGB8888 and RGB565 CRCs match; all submits succeeded")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
