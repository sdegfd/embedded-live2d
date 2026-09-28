#!/usr/bin/env python3
"""Capture the ESP32-P4 Live2D profile stream into reproducible CSV files.

Run before resetting the board:
  python tools/capture_l2d_profile.py --port COM5 --out doc/profiling-run
Or parse an existing serial log:
  python tools/capture_l2d_profile.py --input-log monitor.log --out doc/profiling-run
"""
import argparse
import pathlib
import re
import sys
import time

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def main() -> int:
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--port", help="COM port or /dev/ttyUSB device")
    source.add_argument("--input-log", type=pathlib.Path)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    paths = {"L2D_FRAME": args.out / "frames.csv",
             "L2D_SUMMARY": args.out / "summary.csv",
             "L2D_DETAIL": args.out / "detail.csv",
             "L2D_CORRECTNESS": args.out / "correctness.csv"}
    files = {}
    raw = (args.out / "serial.log").open("wb") if args.port else None
    metadata = []
    in_meta = False
    done = False
    counts = {key: 0 for key in paths}
    serial_port = None
    try:
        if args.port:
            try:
                import serial  # type: ignore
            except ImportError as exc:
                raise SystemExit("Install pyserial: python -m pip install pyserial") from exc
            serial_port = serial.Serial(args.port, args.baud, timeout=1)
            serial_port.dtr = False
            serial_port.rts = False
            deadline = time.monotonic() + args.timeout
            def incoming():
                while time.monotonic() < deadline:
                    line = serial_port.readline()
                    if line:
                        raw.write(line)
                        yield line
        else:
            def incoming():
                yield from args.input_log.open("rb")
        for payload in incoming():
            line = ANSI.sub("", payload.decode("utf-8", "replace")).strip("\r\n")
            if line == "L2D_META_BEGIN":
                in_meta = True
                continue
            if line == "L2D_META_END":
                in_meta = False
                continue
            if in_meta:
                metadata.append(line)
                continue
            if line == "L2D_DONE":
                done = True
                break
            for key, path in paths.items():
                if line.startswith(key + "_HEADER,"):
                    if key not in files:
                        files[key] = path.open("w", encoding="utf-8", newline="")
                    files[key].write(line.split(",", 1)[1] + "\n")
                    break
                if line.startswith(key + ","):
                    if key not in files:
                        files[key] = path.open("w", encoding="utf-8", newline="")
                    files[key].write(line.split(",", 1)[1] + "\n")
                    counts[key] += 1
                    break
        (args.out / "metadata.txt").write_text("\n".join(metadata) + "\n", encoding="utf-8")
    finally:
        if serial_port:
            serial_port.close()
        if raw:
            raw.close()
        for file in files.values():
            file.close()
    print("capture", "complete" if done else "incomplete", "frames", counts["L2D_FRAME"],
          "summary rows", counts["L2D_SUMMARY"], "detail rows", counts["L2D_DETAIL"],
          "correctness rows", counts["L2D_CORRECTNESS"])
    return 0 if done and any(counts.values()) else 2


if __name__ == "__main__":
    sys.exit(main())
