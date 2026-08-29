# Usage:
#   python3 test_solution.py

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


CASES = [
    ("8\n10 9 2 5 3 7 101 18\n", "4"),
    ("6\n0 1 0 3 2 3\n", "4"),
    ("7\n7 7 7 7 7 7 7\n", "1"),
    ("1\n42\n", "1"),
    ("5\n1 2 3 4 5\n", "5"),
    ("5\n5 4 3 2 1\n", "1"),
    ("6\n4 10 4 3 8 9\n", "3"),
    ("2\n-2 -1\n", "2"),
    ("4\n2 2 1 3\n", "2"),
    ("11\n3 5 6 2 5 4 19 5 6 7 12\n", "6"),
]


def normalized(text: str) -> str:
    return " ".join(text.split())


def main() -> int:
    problem_dir = Path(__file__).resolve().parent
    solution = Path(os.environ.get("PYLITE_SOLUTION", problem_dir / "solution.py"))
    if not solution.is_file():
        print(f"找不到提交代码：{solution}")
        return 1

    passed = 0
    for index, (case_input, expected) in enumerate(CASES, start=1):
        try:
            result = subprocess.run(
                [sys.executable, "-u", str(solution)],
                input=case_input,
                text=True,
                encoding="utf-8",
                errors="replace",
                capture_output=True,
                timeout=2,
                cwd=problem_dir,
                check=False,
            )
        except subprocess.TimeoutExpired:
            print(f"[FAIL] #{index}: 超过 2 秒")
            continue

        actual = normalized(result.stdout)
        wanted = normalized(expected)
        if result.returncode == 0 and actual == wanted:
            passed += 1
            print(f"[PASS] #{index}")
            continue

        print(f"[FAIL] #{index}")
        print(f"  输入：{case_input.strip()}")
        print(f"  期望：{wanted!r}")
        print(f"  实际：{actual!r}")
        if result.returncode != 0:
            print(f"  退出码：{result.returncode}")
        if result.stderr.strip():
            print(f"  错误：{result.stderr.strip()}")

    print(f"\n通过 {passed}/{len(CASES)} 组测试")
    return 0 if passed == len(CASES) else 1


if __name__ == "__main__":
    raise SystemExit(main())
