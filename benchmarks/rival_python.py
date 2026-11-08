"""Python equivalent of benchmarks/runtime.ykn for same-host comparison."""
import json


def main() -> None:
    total = 0
    values = [0] * 256
    for turn in range(10_000):
        for index in range(256):
            values[index] = (index * 17 + turn) % 997
        for value in values:
            total += value if value % 2 == 0 else -value
    left, right = 0, 1
    for _ in range(40):
        left, right = right, left + right
    if total != 1_277_871 or left != 102_334_155:
        raise SystemExit(1)
    print(json.dumps({"status": "PASS", "checksum": total, "fibonacci": left}))


if __name__ == "__main__":
    main()
