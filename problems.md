# Distributed Algorithms

## Distributed Matrix Multiplication using the Row-Row Method

You are given two matrices, **A** (of dimension `m × n`) and **B** (of dimension `n × p`). Implement an MPI program that computes:

```text
C = A × B
```

using the **Row-Row method**.

In this method, matrix `A` is partitioned row-wise across processes, while matrix `B` is fully replicated by broadcasting it to every process. Each row of `C` is computed as a weighted sum of the rows of `B`, where the weights are the entries of the corresponding row of `A`:

```text
cᵢ = aᵢ[1] · B[1, :] + aᵢ[2] · B[2, :] + ... + aᵢ[n] · B[n, :]
```

---

## Input Constraints

- `m`, `n`, and `p` range from small matrices (for example, `3 × 3`, suitable for manual verification) to large matrices (for example, `1000 × 1000` or more, suitable for timing and scaling experiments).
- Matrices need not be square. Test skewed shapes such as:
  - Tall matrices: `m ≫ n`
  - Wide matrices: `n ≫ m`
- Test both:
  - `m` divisible by the number of processes `P`
  - `m` not divisible by `P`
- Edge cases:
  - `m = 1` — single row
  - `n = 1` — single column in `A` / single row in `B`
- Matrix entries are integers.

---

## Matrix Layout

- The master process reads or generates:
  - `A` of size `m × n`
  - `B` of size `n × p`
- Matrix `A` is partitioned horizontally across `P` processes.
- Each process receives approximately `m / P` rows.
- The implementation must correctly handle cases where `m` is not divisible by `P`.
- Matrix `B` is broadcast in full to every process.

---

## Computation

Each process independently computes the rows of `C` corresponding to its assigned rows of `A`.

There should be no inter-worker communication during computation.

For a row `aᵢ` of `A`, the corresponding row of `C` is:

```text
cᵢ = Σₖ A[i][k] · B[k, :]
```

---

## Collection

After computation:

- Each process sends its computed row-slice of `C` back to the master.
- The master gathers all row-slices.
- The master reconstructs the complete matrix `C`.

---

# Example 1 — Even Split, P = 3

### Input

```text
A = [  1   2 ]
    [  0   3 ]
    [ -1   4 ]

B = [ 2   3   4 ]
    [ 1   0  -1 ]
```

`A` is `3 × 2` and `B` is `2 × 3`.

### Distribution

```text
P0 → Row 1
P1 → Row 2
P2 → Row 3
```

### Computation

```text
c₁ = 1 · (2, 3, 4) + 2 · (1, 0, -1)
   = (4, 3, 2)

c₂ = 0 · (2, 3, 4) + 3 · (1, 0, -1)
   = (3, 0, -3)

c₃ = -1 · (2, 3, 4) + 4 · (1, 0, -1)
   = (2, -3, -8)
```

### Result

```text
C = [ 4   3   2 ]
    [ 3   0  -3 ]
    [ 2  -3  -8 ]
```

`C` is a `3 × 3` matrix.

---

# Example 2 — Uneven Split, P = 3

### Input

```text
A = [ 1   0 ]
    [ 2  -1 ]
    [ 0   3 ]
    [ 1   1 ]

B = [ 1   2 ]
    [ 0   1 ]
```

`A` is `4 × 2` and `B` is `2 × 2`.

### Distribution

Since `m = 4` and `P = 3`:

```text
P0 → 2 rows
P1 → 1 row
P2 → 1 row
```

### Computation

```text
c₁ = (1, 2)
c₂ = (2, 3)
c₃ = (0, 3)
c₄ = (1, 3)
```

### Result

```text
C = [ 1   2 ]
    [ 2   3 ]
    [ 0   3 ]
    [ 1   3 ]
```

`C` is a `4 × 2` matrix.

# Graph Algorithms

## All-Pairs Shortest Path using Floyd-Warshall

- Find the shortest distance between all pairs of nodes in a weighted graph using the **Floyd-Warshall algorithm**.
- Distribute the rows of the distance matrix across MPI processes.

### Constraints

- Number of vertices `N`: `2 ≤ N ≤ 1000`
- Edge weights `w`: `-1000 ≤ w ≤ 1000`
- The graph is guaranteed to have no negative weight cycles.
- A value of `1000000` in the input represents `∞` (no direct edge).

### Input Format

```text
First line: Two integers V and E
```

Where:

- `V` is the number of vertices.
- `E` is the number of edges.

The next `E` lines contain three integers:

```text
u v w
```

representing a directed edge from vertex `u` to vertex `v` with weight `w`.

### Output Format

Print `N` lines, each containing `N` space-separated integers, representing the shortest distance between every pair of nodes.

### Sample Input

```text
3 3
0 1 5
1 2 3
2 0 2
```

### Sample Output

```text
0 5 8
5 0 3
2 7 0
```

---

# Real-World Applications

## Large-Scale Server Log Analytics

### Input

The first line contains:

```text
N K S
```

The next `N` lines contain:

```text
timestamp server_id endpoint_id user_id status_code response_time bytes_sent
```

A request is considered **successful** if:

```text
status_code < 400
```

### Required Computations

- Total, successful, and failed requests.
- Average, minimum, and maximum response time.
- Total bytes sent.
- Counts of:
  - `2xx` responses
  - `3xx` responses
  - `4xx` responses
  - `5xx` responses
- Top-`K` servers by request count, along with their average response time.
- Top-`K` endpoints by request count, along with their total bytes sent.
- The busiest 60-second interval.

### Output Format

```text
TOTAL_REQUESTS <value>
SUCCESSFUL_REQUESTS <value>
FAILED_REQUESTS <value>
AVERAGE_RESPONSE_TIME <value>
MIN_RESPONSE_TIME <value>
MAX_RESPONSE_TIME <value>
TOTAL_BYTES <value>
STATUS_2XX <value>
STATUS_3XX <value>
STATUS_4XX <value>
STATUS_5XX <value>
BUSIEST_INTERVAL <interval_id> <count>

TOP_SERVERS
<server_id> <request_count> <average_response_time>
...

TOP_ENDPOINTS
<endpoint_id> <request_count> <total_bytes>
...
```

### Sorting Requirements

Top-`K` results must be sorted by:

1. Decreasing request count.
2. Increasing ID in case of a tie.

For computing the busiest interval, use:

```text
interval_id = timestamp / 60
```

