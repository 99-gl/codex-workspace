# Usage:
#   python3 test_solution.py

from __future__ import annotations

from pathlib import Path
import subprocess
import sys


CASES = [
    ('4\n1 2 3 1\n', '4'),
    ('5\n2 7 9 3 1\n', '12'),
    ('1\n5\n', '5'),
    ('2\n2 1\n', '2'),
    ('5\n0 0 0 0 0\n', '0'),
    ('6\n6 7 1 30 8 2\n', '39'),
    ('5\n10 1 1 10 1\n', '20'),
    ('7\n2 1 4 9 2 7 3\n', '18'),
    ('4\n100 1 1 100\n', '200'),
    ('8\n1 3 1 3 100 3 1 3\n', '106'),
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
