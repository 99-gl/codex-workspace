# Usage:
#   python3 test_solution.py

from __future__ import annotations

from pathlib import Path
import subprocess
import sys


CASES = [
    ('6 2\n3 2 1 5 6 4\n', '5'),
    ('9 4\n3 2 3 1 2 4 5 5 6\n', '4'),
    ('1 1\n1\n', '1'),
    ('5 5\n5 4 3 2 1\n', '1'),
    ('5 1\n-1 -2 -3 -4 -5\n', '-1'),
    ('6 3\n2 2 2 3 3 1\n', '2'),
    ('4 2\n0 0 0 0\n', '0'),
    ('7 6\n9 8 7 6 5 4 3\n', '4'),
    ('8 4\n10 -10 20 -20 30 -30 0 5\n', '5'),
    ('5 2\n100 50 100 25 75\n', '100'),
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
