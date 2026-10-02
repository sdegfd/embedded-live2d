#!/usr/bin/env python3
"""Compare the shared light model pose sequence on Host and P4."""
import argparse
import csv
import pathlib


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("host", type=pathlib.Path)
    parser.add_argument("device", type=pathlib.Path)
    args = parser.parse_args()
    host = list(csv.DictReader(args.host.open()))
    device = list(csv.DictReader(args.device.open()))
    if len(host) not in (44, 56) or len(device) != len(host):
        print(f"FAIL: expected the same 44 or 56 poses, got Host={len(host)} P4={len(device)}")
        return 2
    for index, (h, d) in enumerate(zip(host, device)):
        for key in ("pose", "argb_crc", "rgb565_crc"):
            if h[key] != d[key]:
                print(f"FAIL: row {index} {key}: Host={h[key]} P4={d[key]}")
                return 2
        if any(d[key] != "1" for key in ("render_ok", "submit_ok", "conversion_ok")):
            print(f"FAIL: row {index}: {d}")
            return 2
    print(f"PASS: {len(host)} ordered light poses match Host BGRA/RGB565; all selected conversions and submits succeeded")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
