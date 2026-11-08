import time
import sys

# Increase recursion depth for deep trees
sys.setrecursionlimit(200000)

NUM_RECORDS = 100000

class Node:
    __slots__ = ('key', 'value', 'left', 'right')
    def __init__(self, key, value):
        self.key = key
        self.value = value
        self.left = None
        self.right = None

def insert(root, key, value):
    if root is None:
        return Node(key, value)
    cur = root
    while True:
        if key < cur.key:
            if cur.left is None:
                cur.left = Node(key, value)
                break
            cur = cur.left
        elif key > cur.key:
            if cur.right is None:
                cur.right = Node(key, value)
                break
            cur = cur.right
        else:
            cur.value = value
            break
    return root

def search(root, key):
    cur = root
    while cur:
        if key == cur.key:
            return cur.value
        if key < cur.key:
            cur = cur.left
        else:
            cur = cur.right
    return -1

def range_sum(root, min_k, max_k):
    if not root:
        return 0
    stack = [root]
    total = 0
    while stack:
        n = stack.pop()
        if min_k <= n.key <= max_k:
            total += n.value
        if n.left and n.key > min_k:
            stack.append(n.left)
        if n.right and n.key < max_k:
            stack.append(n.right)
    return total

def main():
    start = time.perf_counter()
    root = None
    pseudo_rand = 133742

    for i in range(NUM_RECORDS):
        pseudo_rand = (pseudo_rand * 1103515245 + 12345) & 0x7FFFFFFF
        key = pseudo_rand % 200000
        val = i * 3
        root = insert(root, key, val)

    found_count = 0
    for _ in range(50000):
        pseudo_rand = (pseudo_rand * 1103515245 + 12345) & 0x7FFFFFFF
        key = pseudo_rand % 200000
        if search(root, key) >= 0:
            found_count += 1

    total_sum = range_sum(root, 50000, 150000)
    root = None  # GC deallocation

    elapsed_sec = time.perf_counter() - start
    print(f"[PYTHON-3.13] Systems Workload: {elapsed_sec:.4f} s | Found: {found_count} | RangeSum: {total_sum}")

if __name__ == "__main__":
    main()

