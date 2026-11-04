#!/bin/bash
set -euo pipefail
project="$(cd "$(dirname "$0")" && pwd)"
runtime=/Users/liujinhao/Documents/ChatGPT/os
prefix="$runtime/.tools/homebrew"
export PATH="$prefix/bin:$PATH"
export COMPILER_PATH="$prefix/opt/riscv64-elf-binutils/riscv64-elf/bin"
cd "$project"

target="${1:-run}"
case "$target" in
  build) target=TARGETS ;;
  run) target=qemu ;;
  grade|clean|debug) ;;
  *) echo "用法：$0 {build|run|grade|clean|debug}" >&2; exit 2 ;;
esac

if [[ "$target" == grade ]]; then
  # The supplied grader calls qemu and timeout directly rather than $(QEMU).
  shimdir="$(mktemp -d)"
  trap 'rm -rf "$shimdir"' EXIT
  printf '#!/bin/sh\nexec "%s/tools/qemu-lab.sh" "$@"\n' "$runtime" > "$shimdir/qemu-system-riscv64"
  cat > "$shimdir/timeout" <<'PY'
#!/usr/bin/python3
import subprocess
import sys

try:
    result = subprocess.run(sys.argv[2:], timeout=float(sys.argv[1].rstrip("s")))
    sys.exit(result.returncode)
except subprocess.TimeoutExpired:
    sys.exit(124)
PY
  chmod +x "$shimdir/qemu-system-riscv64" "$shimdir/timeout"
  export PATH="$shimdir:$PATH"
fi

# Use the current CSR spelling without modifying the supplied SAVE_ALL.
"$prefix/bin/gmake" "GCCPREFIX=$prefix/bin/riscv64-elf-" \
  "CC=$prefix/bin/riscv64-elf-gcc -Dsbadaddr=stval" \
  "QEMU=$runtime/tools/qemu-lab.sh" "$target"
