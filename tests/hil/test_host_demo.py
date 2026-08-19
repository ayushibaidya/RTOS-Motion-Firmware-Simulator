#!/usr/bin/env python3

from pathlib import Path
import os
import subprocess
import sys


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    configured_demo_path = os.environ.get("DEMO_PATH")
    demo_path = Path(configured_demo_path) if configured_demo_path else repo_root / "build" / "mr_robo_demo"

    if not demo_path.is_absolute():
        demo_path = repo_root / demo_path

    if not demo_path.exists():
        print(f"missing demo executable: {demo_path}", file=sys.stderr)
        print("run: cmake -S . -B build -G Ninja && cmake --build build", file=sys.stderr)
        return 1

    result = subprocess.run(
        [str(demo_path)],
        check=False,
        text=True,
        capture_output=True,
    )

    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        return result.returncode

    output = result.stdout

    required_fragments = [
        "OK BOOT RTOS_MOTION_FW_SIM",
        "OK PONG",
        "OK MOVE QUEUED",
        "T ms=",
        "OK ESTOP",
        "state=FAULT",
        "OK FAULT CLEARED",
        "state=IDLE",
        "OK STATUS",
    ]

    missing = [fragment for fragment in required_fragments if fragment not in output]

    if missing:
        print("host demo output is missing expected fragments:", file=sys.stderr)
        for fragment in missing:
            print(f"- {fragment}", file=sys.stderr)
        print("\nactual output:", file=sys.stderr)
        print(output, file=sys.stderr)
        return 1

    print("host demo integration test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
