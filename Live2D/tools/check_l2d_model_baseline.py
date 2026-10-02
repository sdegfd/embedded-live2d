#!/usr/bin/env python3
"""Check the formal five-axis esp.live baseline.

Host file:   models/esp.live
Device file: /sdcard/esp.live

These two copies are the same model. A mismatch stops the formal test.
This script does not select another file.
"""
import argparse
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
HOST_PATH = ROOT / "models" / "esp.live"
BASELINE_SHA = "1d7f21471dcedee2d205904791df7147c63760269462ad5d7169a97afe386100"
BASELINE_SIZE = 800796
FORBIDDEN = {
    "786b18e342f3a7b3e67042822f138824aaad610d6c46130f58f5c59bbb379b0b": "historical four-axis esp.live",
    "5cfd67979fb365cc9edbca7b955aac391ebb39bca062fa855e4209faa02498b1": "release.live",
}


def sha256_file(path: pathlib.Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as handle:
        while True:
            block = handle.read(1024 * 1024)
            if not block:
                break
            size += len(block)
            digest.update(block)
    return digest.hexdigest(), size


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", choices=("esp", "light"), default="esp")
    parser.add_argument("--device-sha", help="SHA256 printed from /sdcard/esp.live")
    parser.add_argument("--metadata", type=pathlib.Path, help="Captured metadata.txt")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "models" / "manifest.json").read_text())
    expected = manifest["formal_model"] if args.model == "esp" else manifest["test_models"][args.model]
    host_path = ROOT / expected["path"]
    baseline_sha, baseline_size = expected["sha256"], expected["size_bytes"]
    # The historical esp baseline is pinned independently of the manifest.
    if args.model == "esp" and (baseline_sha != BASELINE_SHA or baseline_size != BASELINE_SIZE):
        print("STOP: esp formal baseline identity changed", file=sys.stderr)
        return 2
    if not host_path.is_file():
        print(f"STOP model test: missing {host_path}.", file=sys.stderr)
        return 2
    sha, size = sha256_file(host_path)
    print(f"model_path={expected['path']}")
    print(f"device_path={expected['device_path']}")
    print(f"model_sha256={sha}")
    print(f"model_size={size}")
    print(f"axis_count={len(expected['axes'])}")
    if sha in FORBIDDEN:
        print(f"STOP formal test: models/esp.live is {FORBIDDEN[sha]}. Not switching models.",
              file=sys.stderr)
        return 2
    if sha != baseline_sha or size != baseline_size:
        print(f"STOP model test: {expected['path']} identity mismatch.", file=sys.stderr)
        return 2
    device_sha = args.device_sha
    if args.metadata:
        text = args.metadata.read_text(encoding="utf-8", errors="replace")
        for line in text.splitlines():
            if line.startswith("model_sha256="):
                device_sha = line.split("=", 1)[1].strip()
            fields = {"axis_count": len(expected["axes"]), "model_size": size}
            for field, value in fields.items():
                if line.startswith(field + "=") and line.split("=", 1)[1].strip() != str(value):
                    print(f"STOP model test: unexpected {line}", file=sys.stderr)
                    return 2
        if device_sha is None:
            print("STOP model test: metadata lacks model_sha256", file=sys.stderr)
            return 2
    if device_sha is not None and device_sha.lower() != sha:
        print(f"STOP model test: workspace {expected['path']} and device SHA256 differ.",
              file=sys.stderr)
        print(f"workspace={sha}", file=sys.stderr)
        print(f"device={device_sha.lower()}", file=sys.stderr)
        print("Not switching models.", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
