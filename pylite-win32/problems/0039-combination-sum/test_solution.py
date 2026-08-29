# Usage:
#   python3 test_solution.py

from __future__ import annotations

from pathlib import Path
import subprocess
import sys


CASES = [
    ('4 7\n2 3 6 7\n', '2 2 3\n7'),
    ('3 8\n2 3 5\n', '2 2 2 2\n2 3 3\n3 5'),
    ('1 1\n2\n', 'NONE'),
    ('1 4\n2\n', '2 2'),
    ('3 3\n2 4 6\n', 'NONE'),
    ('4 10\n2 3 5 7\n', '2 2 2 2 2\n2 2 3 3\n2 3 5\n3 7\n5 5'),
    ('2 6\n1 4\n', '1 1 1 1 1 1\n1 1 4'),
    ('3 12\n3 4 5\n', '3 3 3 3\n3 4 5\n4 4 4'),
    ('4 9\n2 4 6 8\n', 'NONE'),
    ('5 11\n2 3 5 7 11\n', '2 2 2 2 3\n2 2 2 5\n2 2 7\n2 3 3 3\n3 3 5\n11'),
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
