# Usage:
#   python3 test_solution.py

from __future__ import annotations

from pathlib import Path
import subprocess
import sys


CASES = [
    ('3 4\nABCE\nSFCS\nADEE\nABCCED\n', 'true'),
    ('3 4\nABCE\nSFCS\nADEE\nSEE\n', 'true'),
    ('3 4\nABCE\nSFCS\nADEE\nABCB\n', 'false'),
    ('1 1\nA\nA\n', 'true'),
    ('1 1\nA\nB\n', 'false'),
    ('1 4\nABCD\nDCBA\n', 'true'),
    ('2 2\nAB\nCD\nACDB\n', 'true'),
    ('2 3\nAAA\nAAA\nAAAAAA\n', 'true'),
    ('2 3\nAAA\nAAA\nAAAAAAA\n', 'false'),
    ('3 3\nCAT\nRRE\nTON\nCARTON\n', 'false'),
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
