# Usage:
#   python3 test_solution.py

from __future__ import annotations

from pathlib import Path
import subprocess
import sys


CASES = [
    ('5\n3 9 20 15 7\n9 3 15 20 7\n', '3 9 20 null null 15 7'),
    ('1\n-1\n-1\n', '-1'),
    ('3\n1 2 3\n2 1 3\n', '1 2 3'),
    ('3\n1 2 3\n3 2 1\n', '1 2 null 3'),
    ('4\n1 2 3 4\n1 2 3 4\n', '1 null 2 null 3 null 4'),
    ('4\n4 3 2 1\n1 2 3 4\n', '4 3 null 2 null 1'),
    ('7\n4 2 1 3 6 5 7\n1 2 3 4 5 6 7\n', '4 2 6 1 3 5 7'),
    ('5\n10 5 2 7 15\n2 5 7 10 15\n', '10 5 15 2 7'),
    ('6\n8 4 2 6 12 10\n2 4 6 8 10 12\n', '8 4 12 2 6 10'),
    ('3\n0 -1 1\n-1 0 1\n', '0 -1 1'),
]


def normalized(text: str) -> str:
    return " ".join(text.split())


def main() -> int:
    problem_dir = Path(__file__).resolve().parent
    solution = problem_dir / "solution.py"
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
