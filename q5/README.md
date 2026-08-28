# Q5: All-Pairs Shortest Path (Floyd-Warshall) with MPI

Distributed Floyd-Warshall: the `N x N` distance matrix is partitioned by
rows across `P` processes. At iteration `k`, the process owning row `k`
broadcasts it to everyone; every process then relaxes its own rows against
that row. No other inter-process communication happens during the
relaxation itself, which is what makes this parallelizable.

See [`REPORT.md`](REPORT.md) for the full results, charts, and conclusions.
This README covers building, running, and reproducing them.

## Files

| File                     | Purpose                                                        |
|--------------------------|-----------------------------------------------------------------|
| `main_seq.cpp`           | Sequential `O(N^3)` reference implementation (correctness oracle) |
| `main_dist.cpp`          | MPI row-partitioned implementation                              |
| `input_generator.cpp`    | Random graph generator, writes files in the assignment's input format |
| `benchmark.sh`           | Local benchmark: builds everything, generates graphs, times seq vs. dist across process counts, checks correctness, writes `results.csv` |
| `cluster_benchmark.slurm`| Same benchmark matrix, packaged as a single Slurm job for the IIIT RCE cluster |
| `REPORT.md`              | Full write-up: results table, runtime table, efficiency, speed-up graph, comm-vs-compute breakdown, correctness, conclusion |
| `cluster_results/`       | Raw `results.csv` and job log from the cluster run that `REPORT.md` is built from |
| `report_assets/`         | Chart images referenced by `REPORT.md` |
| `tests/`                 | Committed sample input/expected-output pairs for a quick correctness check without running the generator |

## Build

```bash
mpic++ -O3 -o main_dist main_dist.cpp
g++    -O3 -o main_seq  main_seq.cpp
g++    -O3 -o input_generator input_generator.cpp
```

## Input format

```
V E
u v w      (repeated E times, directed edge u -> v with weight w)
```

`V` = number of vertices, `E` = number of edges. A missing edge is implicit
(`INF = 1000000`, matching the assignment's convention). Self-distance is 0.

## Running

```bash
./main_seq < graph.txt > seq_out.txt
mpirun -np 4 ./main_dist < graph.txt > dist_out.txt
diff seq_out.txt dist_out.txt
```

Only rank 0 reads stdin and writes stdout, so this works exactly like a
normal CLI program: pipe/redirect as usual.

## Input generator

```bash
./input_generator <V> <E> <max_weight> <seed> <output_file> [allow_negative:0/1]
```

Writes a Floyd-Warshall input file in the assignment's format (`V E` on the
first line, then `E` lines of `u v w`).

- `V`, `E`: vertex/edge counts. `E` is silently clamped to `V*(V-1)` (no
  self-loops or duplicate edges are generated).
- `max_weight`: caps the magnitude of generated weights.
- `seed`: RNG seed, for reproducible test cases.
- `allow_negative`:
  - `0` (default): all weights uniform in `[0, max_weight]`. Trivially free
    of negative cycles.
  - `1`: weights can go negative, but the graph is still guaranteed
    negative-cycle-free. This uses the standard potential-reweighting trick:
    each vertex `i` gets a random potential `p[i]` uniform in
    `[0, max_weight/2]`, and edge `(u,v)` gets weight `base + p[v] - p[u]`,
    where `base` is uniform in `[0, max_weight/2]`. Around any cycle the
    potential terms telescope to zero, so the cycle's total weight is a sum
    of non-negative `base` terms, never negative. Use this to exercise the
    negative-weight edge handling in both implementations.

Examples for the edge cases called out in the assignment:

```bash
./input_generator 3 3 10 1 tiny.txt          # small, hand-verifiable
./input_generator 1000 5000 1000 1 large.txt # large, for timing
./input_generator 2 1 10 1 min.txt           # N=2 minimum
```

## Correctness verification

Committed sample input/output pairs are in [`tests/`](tests/) for a quick
check without running the generator. For any other generated (or
hand-written) input, `main_seq.cpp` is the oracle:

```bash
./main_seq < graph.txt > seq.out 2>/dev/null
mpirun -np <P> --oversubscribe ./main_dist < graph.txt > dist.out 2>/dev/null
diff seq.out dist.out && echo MATCH
```

Verify at minimum:
- `P` dividing `N` evenly and not evenly.
- `P > N` (some processes get zero rows; `main_dist.cpp` handles this: an
  empty `Scatterv`/`Gatherv` count is valid MPI, and the owner-lookup loop
  just skips processes with `num_rows[i] == 0`).
- `P = 1` (must reduce to the sequential result).
- A graph generated with `allow_negative=1`.

This has been checked by hand for the assignment's sample input, and for
generated graphs across these cases: all match. See
[`REPORT.md`](REPORT.md) for the full correctness table and cluster benchmark
results.

## Timing

Both binaries print wall-clock timing lines to **stderr**, so stdout stays
clean for correctness diffing:

- `main_seq.cpp`: wraps only the triple-nested relaxation loop (excludes
  input parsing and output printing) using `std::chrono::high_resolution_clock`.
  Prints `TIME_SECONDS <seconds>`.
- `main_dist.cpp`: times `Scatterv`, every per-iteration `Bcast`, and
  `Gatherv` separately from the relaxation loop itself, using `MPI_Wtime`.
  A `MPI_Barrier` right before `Scatterv` ensures all ranks start together.
  `MPI_MAXLOC` picks out the rank whose total time was largest (the actual
  wall-clock bottleneck) and reports its numbers from rank 0:
  - `TIME_SECONDS`: total time (compute + communication).
  - `COMPUTE_SECONDS`: time inside the relaxation loop only.
  - `COMM_SECONDS`: time inside `Scatterv` + `Bcast` calls + `Gatherv`.
  - `NUM_PROCS`: the process count the run used.

  `COMPUTE_SECONDS + COMM_SECONDS == TIME_SECONDS` by construction, so the
  communication share as a percentage is `100 * COMM_SECONDS / TIME_SECONDS`.

## Benchmarking

`benchmark.sh` automates the whole loop: build, generate a graph per size,
run the sequential baseline once, run the distributed version at each
process count, diff outputs for correctness, and log everything to
`results.csv`.

```bash
./benchmark.sh                          # defaults: V in {100,200,500,1000}, P in {1,2,4,8,16}
./benchmark.sh "100 500 1000" "1 2 4 8" # custom sizes / process counts
```

Environment variables:
- `EDGE_DENSITY` (default 5): average out-degree, i.e. `E = V * EDGE_DENSITY`.
- `SEED` (default 1): generator seed, for reproducible graphs.

Output (`results.csv`) columns:

```
vertices,edges,procs,seq_time_sec,dist_time_sec,compute_sec,comm_sec,comm_pct,
speedup_vs_seq,speedup_vs_p1,efficiency,correct
```

`speedup_vs_p1` is `S(P) = T_1 / T_P` using the distributed program's own
P=1 run as the baseline (matches the report format's definition);
`speedup_vs_seq` compares against the plain sequential binary instead, for
reference. `efficiency` is `S(P) / P`.

Generated graphs and per-run outputs are cached under `data/`, so re-running
the script won't regenerate a graph that already exists for a given `V`.
Binaries are built into `build/`. Neither directory is committed to the repo
(see `.gitignore`); re-run `benchmark.sh` to regenerate them locally.

Notes for interpreting results:
- At `P=1`, `speedup_vs_seq` is often *below* 1. MPI's own overhead (message
  setup, `--oversubscribe` scheduling) makes a single-process MPI run
  slightly slower than the plain sequential binary. That's expected, not a
  bug.
- Small `V` (tens to low hundreds) can show *negative* scaling as `P`
  grows, because the `O(N)` broadcast-per-iteration communication cost
  starts to dominate the tiny amount of per-process compute. Scaling
  behavior only becomes meaningful once `N` is large enough (many hundreds
  to 1000) that the `O(N^3)` compute term dominates. Use the larger sizes
  for your report's scaling numbers, and mention the small-N crossover as
  an observation.
- Run on a machine with at least as many physical cores as your largest
  `P`, or expect oversubscription artifacts.

## Cluster benchmark

`cluster_benchmark.slurm` runs the same matrix as `benchmark.sh` inside a
single Slurm allocation (1 node, 16 tasks, 20 min walltime), for a proper
multi-core cluster rather than a laptop. Submit it from the cluster:

```bash
module load openmpi/4.1.5
sbatch cluster_benchmark.slurm
squeue -u $USER            # check status
```

It writes `results.csv` next to `data/` and `build/` in a sibling directory,
same layout as the local script. The numbers in `REPORT.md` and
`cluster_results/` came from this script on the IIIT RCE cluster
(`rce.iiit.ac.in`, Slurm job `79941`, node `node06`).
