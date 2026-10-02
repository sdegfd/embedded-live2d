#!/usr/bin/env python3
"""Capture the ESP32-P4 Live2D profile stream into reproducible CSV files.

Run before resetting the board:
  python examples/esp32p4/tools/capture_l2d_profile.py --port COM5 --out captures/profiling-run
Or parse an existing serial log:
  python examples/esp32p4/tools/capture_l2d_profile.py --input-log monitor.log --out captures/profiling-run
"""
import argparse
import csv
import pathlib
import re
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
BASELINE_CHECK = ROOT / "Live2D" / "tools" / "check_l2d_model_baseline.py"

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def main() -> int:
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--port", help="COM port or /dev/ttyUSB device")
    source.add_argument("--input-log", type=pathlib.Path)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--model", choices=("esp", "light"), default="esp")
    parser.add_argument("--reset", action="store_true", help="Reset after opening the serial port")
    parser.add_argument("--demo-seconds", type=int, default=0,
                        help="Keep the same serial connection after L2D_DONE to record ongoing playback")
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()
    # The model verifier runs from ROOT; preserve paths supplied from any cwd.
    args.out = args.out.resolve()
    if args.demo_seconds < 0 or (args.demo_seconds and not args.port):
        parser.error("--demo-seconds requires --port and a nonnegative duration")
    preflight = subprocess.run([sys.executable, str(BASELINE_CHECK), "--model", args.model], cwd=ROOT)
    if preflight.returncode != 0:
        return preflight.returncode
    args.out.mkdir(parents=True, exist_ok=True)
    paths = {"L2D_FRAME": args.out / "frames.csv",
             "L2D_SUMMARY": args.out / "summary.csv",
             "L2D_DETAIL": args.out / "detail.csv",
             "L2D_CORRECTNESS": args.out / "correctness.csv",
             "L2D_VISUAL_POSE": args.out / "visual_pose.csv",
             "L2D_VISUAL": args.out / "visual.csv",
             "L2D_ROI": args.out / "roi.csv",
             "L2D_WORK": args.out / "work.csv",
             "L2D_LIGHT_WINDOW": args.out / "cpu_windows.csv",
             "L2D_LIGHT_SUMMARY": args.out / "light_summary.csv",
             "L2D_LIGHT_CRC": args.out / "light_correctness.csv",
             "L2D_LIGHT_MEMORY": args.out / "memory.csv"}
    files = {}
    raw = (args.out / "serial.log").open("wb", buffering=0) if args.port else None
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
            # Configure control lines before opening to avoid a reset pulse.
            serial_port = serial.Serial(port=None, baudrate=args.baud, timeout=1)
            serial_port.dtr = False
            serial_port.rts = False
            serial_port.port = args.port
            serial_port.open()
            if sys.platform.startswith("linux"):
                # Keep control lines unchanged on close; HUPCL can reset ESP boards.
                import termios
                attrs = termios.tcgetattr(serial_port.fileno())
                attrs[2] &= ~termios.HUPCL
                termios.tcsetattr(serial_port.fileno(), termios.TCSANOW, attrs)
            if args.reset:
                from esptool.reset import HardReset
                HardReset(serial_port)()
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
                if args.port and args.demo_seconds:
                    deadline = time.monotonic() + args.demo_seconds
                    continue
                break
            for key, path in paths.items():
                if line.startswith(key + "_HEADER,"):
                    if key not in files:
                        files[key] = path.open("w", encoding="utf-8", newline="", buffering=1)
                    files[key].write(line.split(",", 1)[1] + "\n")
                    break
                if line.startswith(key + ","):
                    if key not in files:
                        files[key] = path.open("w", encoding="utf-8", newline="", buffering=1)
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
          "correctness rows", counts["L2D_CORRECTNESS"],
          "visual rows", counts["L2D_VISUAL"], "visual pose rows", counts["L2D_VISUAL_POSE"],
          "light CRC rows", counts["L2D_LIGHT_CRC"], "light summaries", counts["L2D_LIGHT_SUMMARY"])
    if not done or not any(counts.values()):
        return 2
    if args.model.startswith("light"):
        meta_fields = dict(line.split("=", 1) for line in metadata if "=" in line)
        pose_count = int(meta_fields.get("correctness_poses", "44"))
        scene_count = int(meta_fields.get("scene_count", "4"))
        if counts["L2D_LIGHT_CRC"] != pose_count or counts["L2D_LIGHT_SUMMARY"] != scene_count:
            print("STOP light test: incomplete correctness/performance suite", file=sys.stderr)
            return 2
        crc_rows = list(csv.DictReader(paths["L2D_LIGHT_CRC"].open()))
        summary_rows = list(csv.DictReader(paths["L2D_LIGHT_SUMMARY"].open()))
        if any(row[key] != "1" for row in crc_rows
               for key in ("render_ok", "submit_ok", "conversion_ok")) or any(
                int(row["errors"]) or int(row["frames"]) <= 0 or
                int(row["cpu0"]) < 0 or int(row["cpu1"]) < 0 for row in summary_rows):
            print("STOP light test: render/submit/conversion failure or missing CPU samples", file=sys.stderr)
            return 2
    compared = subprocess.run(
        [sys.executable, str(BASELINE_CHECK), "--model", args.model,
         "--metadata", str(args.out / "metadata.txt")],
        cwd=ROOT)
    if compared.returncode != 0:
        print(f"STOP model test: captured model does not match selected {args.model}. "
              "Not switching models.", file=sys.stderr)
        return compared.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
