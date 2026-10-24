#!/usr/bin/env python3
"""Exercise the real Buddy and SLUB implementations over host-backed RAM."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="slub-tests-") as directory:
    binary = str(Path(directory) / "slub_host_test")
    command = [
        "clang", "-std=gnu99", "-O2", "-g", "-fsanitize=address,undefined",
        "-fno-sanitize-recover=all", "-fno-builtin", "-nostdinc",
        "-Wall", "-Werror", "-Wno-unused",
    ]
    command += ["-I" + str(root / path) for path in [
        "libs", "kern/debug", "kern/driver", "kern/mm"]]
    command += [str(root / "tools/slub_host_test.c"), "-o", binary]
    subprocess.run(command, check=True, timeout=30)
    result = subprocess.run([binary], text=True, capture_output=True, timeout=30)
    print(result.stdout + result.stderr, end="")
    if result.returncode != 0 or result.stderr:
        raise SystemExit("FAIL: SLUB host tests")
print("PASS: SLUB host tests; no ASan/UBSan diagnostics")
