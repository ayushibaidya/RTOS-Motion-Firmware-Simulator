#!/usr/bin/env python3

from pathlib import Path
import os
import subprocess
import sys


def resolve_demo_path(repo_root: Path) -> Path:
    configured_demo_path = os.environ.get("DEMO_PATH")
    demo_path = (
        Path(configured_demo_path)
        if configured_demo_path
        else repo_root / "build-freertos" / "motion_freertos_scheduler_demo"
    )

    if not demo_path.is_absolute():
        demo_path = repo_root / demo_path

    return demo_path


def output_contains_in_order(output: str, fragments: list[str]) -> bool:
    search_start = 0

    for fragment in fragments:
        fragment_index = output.find(fragment, search_start)

        if fragment_index == -1:
            return False

        search_start = fragment_index + len(fragment)

    return True


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    demo_path = resolve_demo_path(repo_root)

    if not demo_path.exists():
        print(f"missing FreeRTOS scheduler demo executable: {demo_path}", file=sys.stderr)
        print(
            "run: cmake -S . -B build-freertos -G Ninja -DUSE_FREERTOS=ON && "
            "cmake --build build-freertos",
            file=sys.stderr,
        )
        return 1

    result = subprocess.run(
        [str(demo_path)],
        check=False,
        text=True,
        capture_output=True,
        timeout=10,
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
        "state=MOVING",
        "OK ESTOP",
        "state=FAULT",
        "fault=1",
        "OK FAULT CLEARED",
        "state=IDLE",
        "OK STATUS",
    ]

    missing = [fragment for fragment in required_fragments if fragment not in output]

    if missing:
        print("FreeRTOS scheduler demo output is missing expected fragments:", file=sys.stderr)
        for fragment in missing:
            print(f"- {fragment}", file=sys.stderr)
        print("\nactual output:", file=sys.stderr)
        print(output, file=sys.stderr)
        return 1

    expected_flow = [
        "OK BOOT RTOS_MOTION_FW_SIM",
        "OK PONG",
        "OK MOVE QUEUED",
        "OK ESTOP",
        "OK FAULT CLEARED",
        "OK STATUS",
    ]

    if not output_contains_in_order(output, expected_flow):
        print("FreeRTOS scheduler demo command flow is out of order:", file=sys.stderr)
        print(output, file=sys.stderr)
        return 1

    print("FreeRTOS scheduler demo integration test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
