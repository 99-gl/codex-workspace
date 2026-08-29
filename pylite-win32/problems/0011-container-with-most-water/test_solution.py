# Usage:
#   python3 test_solution.py

from __future__ import annotations

from pathlib import Path
import subprocess
import sys


CASES = [
    ('9\n1 8 6 2 5 4 8 3 7\n', '49'),
    ('2\n1 1\n', '1'),
    ('3\n1 2 1\n', '2'),
    ('5\n1 2 3 4 5\n', '6'),
    ('5\n5 4 3 2 1\n', '6'),
    ('4\n0 0 0 0\n', '0'),
    ('4\n4 3 2 4\n', '12'),
    ('6\n2 3 10 5 7 8\n', '24'),
    ('7\n1 3 2 5 25 24 5\n', '24'),
    ('6\n10 1 1 1 1 10\n', '50'),
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
