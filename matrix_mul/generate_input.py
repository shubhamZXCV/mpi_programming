import sys
import random

if len(sys.argv) != 5:
    print("Usage: python3 generate_input.py <m> <n> <p> <filename>")
    sys.exit(1)

m = int(sys.argv[1])
n = int(sys.argv[2])
p = int(sys.argv[3])
filename = sys.argv[4]

# Range of random matrix values
MIN_VALUE = -10
MAX_VALUE = 10

with open(filename, "w") as f:
    # Write dimensions
    f.write(f"{m} {n} {p}\n")

    # Generate matrix A (m x n)
    for i in range(m):
        row = [str(random.randint(MIN_VALUE, MAX_VALUE)) for j in range(n)]
        f.write(" ".join(row) + "\n")

    # Generate matrix B (n x p)
    for i in range(n):
        row = [str(random.randint(MIN_VALUE, MAX_VALUE)) for j in range(p)]
        f.write(" ".join(row) + "\n")

print(f"Generated {filename}")
print(f"A: {m} x {n}")
print(f"B: {n} x {p}")
