#!/usr/bin/env python3
"""Run Buddy initialization, allocation, coalescing and assertion tests."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="buddy-tests-") as directory:
    binary = str(Path(directory) / "buddy_host_test")
    command = [
        "clang", "-std=gnu99", "-O2", "-g", "-fsanitize=undefined",
        "-fno-sanitize-recover=all", "-fno-builtin", "-nostdinc",
        "-Wall", "-Werror", "-Wno-unused",
    ]
    command += ["-I" + str(root / path) for path in [
        "libs", "kern/debug", "kern/driver", "kern/mm"]]
    command += [str(root / "tools/buddy_host_test.c"), "-o", binary]
    subprocess.run(command, check=True, timeout=30)
    cases = [(None, 0)] + [(name, 77) for name in [
        "zero-alloc", "wrong-size", "interior-free", "reserved-free", "double-free"]]
    for name, expected in cases:
        result = subprocess.run([binary] + ([name] if name else []),
                                text=True, capture_output=True, timeout=15)
        if result.returncode != expected or result.stderr:
            print(result.stdout + result.stderr, end="")
            raise SystemExit("FAIL: " + (name or "Buddy functional tests"))
        if name:
            print("PASS: rejected " + name)
        else:
            print(result.stdout, end="")
print("PASS: all Buddy host tests; no UBSan diagnostics")
