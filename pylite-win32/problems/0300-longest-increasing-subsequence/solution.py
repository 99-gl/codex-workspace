# Usage:
#   python3 solution.py < input.txt

from bisect import bisect_left
import sys


def main() -> None:
    data = list(map(int, sys.stdin.buffer.read().split()))
    if not data:
        return

    n = data[0]
    numbers = data[1 : n + 1]
    tails: list[int] = []

    for number in numbers:
        position = bisect_left(tails, number)
        if position == len(tails):
            tails.append(number)
        else:
            tails[position] = number

    print(len(tails))


if __name__ == "__main__":
    main()
