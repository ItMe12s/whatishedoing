# Format code taken from LuauAPI mod :3

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CPP_DIRS = ("src", "tests")
CPP_GLOBS = ("*.cpp", "*.hpp")


def _run(cmd: list[str]) -> int:
    return subprocess.run(cmd, cwd=ROOT).returncode


def _cpp_files() -> list[Path]:
    out: list[Path] = []
    for d in CPP_DIRS:
        for g in CPP_GLOBS:
            out.extend((ROOT / d).rglob(g))
    return sorted(out)


def format_clang(check: bool) -> int:
    files = _cpp_files()
    if not files:
        print("No C++ files found for clang-format.", file=sys.stderr)
        return 1
    flag = ["--dry-run", "--Werror"] if check else ["-i"]
    failed = 0
    for f in files:
        if _run(["clang-format", *flag, str(f)]) != 0:
            failed += 1
    if failed:
        return 1
    action = "OK" if check else "formatted"
    print(f"clang-format: {action} {len(files)} files")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--target", choices=("clang", "all"), default="all")
    p.add_argument("--check", action="store_true", help="check only, do not write")
    args = p.parse_args()
    rc = 0
    if args.target in ("clang", "all"):
        if format_clang(args.check) != 0:
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
