# MPI Programming 

Three MPI programs, each with its own sequential baseline, benchmark
harness, and write-up. See [`problems.md`](problems.md) for the full
assignment statements this repo implements.

| Directory | Problem | Approach |
|---|---|---|
| [`matrix_mul/`](matrix_mul/) | Distributed matrix multiplication (`C = A x B`) | Row-row method: `A` scattered by rows, `B` broadcast in full, `C` gathered |
| [`floyd_warshall/`](floyd_warshall/) | All-pairs shortest path (Floyd-Warshall) | `N x N` distance matrix partitioned by rows; owner of pivot row `k` broadcasts it each iteration |
| [`log_analysis/`](log_analysis/) | Large-scale server log analytics | Embarrassingly-parallel map-reduce: each rank scans its shard, partial results combined with `MPI_Reduce`/`MPI_Allreduce` |

Each directory is self-contained (its own `README.md` for build/run
instructions and `REPORT.md` for results, charts, and analysis) and can be
built and benchmarked independently of the others.

## Common layout

Every problem directory follows the same structure:

```
main_seq.cpp            sequential reference implementation (correctness oracle)
main_dist.cpp            MPI implementation
input_generator.cpp     random input generator for timing/scaling experiments
benchmark.sh             local build + sweep + correctness-check + results.csv
cluster_benchmark.slurm SLURM job running the same sweep on the IIIT RCE cluster
REPORT.md                results tables, speed-up/efficiency charts, comm-vs-compute analysis
tests/                   committed sample input/expected-output pairs
report_assets/           chart images referenced by REPORT.md
cluster_results/         raw results.csv + job log from the cluster run
```

(`matrix_mul/` predates this convention slightly — see its own README for
its exact file names and workflow.)

## Quick start

Each subproject builds with `mpic++`/`g++` and runs its own benchmark
script; see the directory's `README.md` for exact commands. In general:

```bash
cd <matrix_mul|floyd_warshall|log_analysis>
./benchmark.sh          # or run_experiments.sh for matrix_mul
```

This builds the binaries, generates inputs across a range of sizes, times
the sequential and MPI versions across process counts `{1,2,4,8,...}`,
verifies MPI output matches the sequential oracle, and writes `results.csv`
plus the tables/charts consumed by that directory's `REPORT.md`.

## Headline results

- **Matrix multiplication**: near-linear speed-up for medium-to-large
  inputs (up to ~6.2x at P=8), but small inputs are dominated by
  communication/launch overhead and barely speed up at all — see
  [`matrix_mul/report_tables.md`](matrix_mul/report_tables.md).
- **Floyd-Warshall**: correctness holds unconditionally across all tested
  sizes, process counts, and edge cases (including `P > N` and negative
  edge weights); speed-up is real but sub-linear and only kicks in once
  `N` is large enough for compute to dominate the per-iteration broadcast —
  see [`floyd_warshall/REPORT.md`](floyd_warshall/REPORT.md).
- **Log analytics**: correctness holds unconditionally; real speed-up once
  I/O is excluded (up to ~2.6x at N=5M, P=16), but weaker than
  Floyd-Warshall's because each record is cheap to process relative to the
  I/O and reduction cost — see [`log_analysis/REPORT.md`](log_analysis/REPORT.md).
