# Usage:
#   python3 test_solution.py

from __future__ import annotations

from pathlib import Path
import subprocess
import sys


CASES = [
    ('11\n3 5 1 6 2 0 8 null null 7 4\n5 1\n', '3'),
    ('11\n3 5 1 6 2 0 8 null null 7 4\n5 4\n', '5'),
    ('3\n1 2 3\n2 3\n', '1'),
    ('3\n1 2 3\n1 3\n', '1'),
    ('7\n4 2 6 1 3 5 7\n1 3\n', '2'),
    ('7\n4 2 6 1 3 5 7\n1 7\n', '4'),
    ('5\n1 2 null 3 4\n3 4\n', '2'),
    ('5\n1 null 2 3 4\n3 4\n', '2'),
    ('7\n0 -3 9 -10 -1 5 12\n-10 -1\n', '-3'),
    ('9\n8 4 12 2 6 10 14 1 3\n1 6\n', '4'),
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
