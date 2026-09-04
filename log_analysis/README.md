# Q7: Large-Scale Server Log Analytics (MPI)

Every one of the N log records can be processed completely independently, and
then all P processes' partial results combine into one final answer using an
operator that does not care what order they arrive in (sum, min, max). This
is a classic **map-reduce style reduction**, and MPI has it built in as
`MPI_Reduce`/`MPI_Allreduce`.

See [`REPORT.md`](REPORT.md) for the full results, charts, and conclusions.
This README covers building, running, and reproducing them.

## Files

| File                     | Purpose                                                        |
|--------------------------|-----------------------------------------------------------------|
| `main_seq.cpp`           | Sequential reference implementation (correctness oracle)        |
| `main_dist.cpp`          | MPI reduction-based implementation                               |
| `input_generator.cpp`    | Reproducible random log generator (fixed seed)                  |
| `benchmark.sh`           | Local benchmark: builds everything, generates datasets, times seq vs. dist across process counts, checks correctness, writes `results.csv` |
| `cluster_benchmark.slurm`| Same benchmark matrix, packaged as a single Slurm job for the IIIT RCE cluster |
| `REPORT.md`              | Full write-up: results table, runtime table, efficiency, speed-up graph, comm-vs-compute breakdown, correctness, conclusion |
| `tests/`                 | Committed sample input/expected-output pairs for a quick correctness check |

## Build

```bash
mpic++ -O3 -o main_dist main_dist.cpp
g++    -O3 -o main_seq  main_seq.cpp
g++    -O3 -o input_generator input_generator.cpp
```

## Input format

```
N K S
<N lines of: timestamp server_id endpoint_id user_id status_code response_time bytes_sent>
```

- `N`: number of log records.
- `K`: how many top servers / top endpoints to report.
- `S`: number of distinct servers; every `server_id` and `endpoint_id`
  satisfies `0 <= id < S`.
- A request is successful if `status_code < 400`.

All floating-point output fields (response times) are printed with exactly
6 digits after the decimal point.

## Running

```bash
./main_seq < dataset.txt > seq_out.txt
mpirun -np 4 ./main_dist < dataset.txt > dist_out.txt
diff seq_out.txt dist_out.txt
```

Only rank 0 reads stdin and writes stdout, matching q5's convention.

## Input generator

```bash
./input_generator <N> <K> <S> <seed> <output_file>
```

Writes a log file in the format above. Fully reproducible: the same `N K S
seed` always produces a byte-for-byte identical file, since the RNG is
seeded explicitly rather than from the current time.

```bash
./input_generator 2000000 10 1000 7 big_log.txt
```
generates 2,000,000 records, asks for the top 10 servers/endpoints, and uses
1000 distinct server/endpoint ids.

## Design choice: plain arrays instead of a hash map

`S` is the number of distinct servers, with `server_id`/`endpoint_id` in
`[0, S)`, confirmed in the assignment's clarifications. That means plain
`S`-sized vectors work as per-server/per-endpoint stats instead of a hash
map: every id maps to exactly one slot with zero collisions, and lookups
and updates are `O(1)`.

## Correctness verification

Committed sample input/output pairs are in [`tests/`](tests/) for a quick
check without running the generator. For any other generated (or
hand-written) input, `main_seq.cpp` is the oracle:

```bash
./main_seq < dataset.txt > seq.out
mpirun -np <P> --oversubscribe ./main_dist < dataset.txt > dist.out
diff seq.out dist.out && echo MATCH
```

Verify at minimum:
- A small hand-built example, worked out by hand.
- `P` greater than `N` (most processes receive zero records).
- `P = 1` (must reduce to the sequential result).
- A larger generated dataset, at several process counts.

All of the above match; see [`REPORT.md`](REPORT.md) for the full table and
cluster benchmark results.

## Timing

Both binaries print wall-clock timing lines to **stderr**, so stdout stays
clean for correctness diffing. Same convention as q5:

- `main_seq.cpp`: wraps everything after input reading (all three passes and
  the final sort) using `std::chrono::high_resolution_clock`. Prints
  `TIME_SECONDS <seconds>`.
- `main_dist.cpp`: times every `Scatterv`/`Reduce`/`Allreduce` call
  separately from local computation, using `MPI_Wtime`. `MPI_MAXLOC` picks
  out the rank whose total time was largest and reports its numbers from
  rank 0:
  - `TIME_SECONDS`: total time (compute + communication).
  - `COMPUTE_SECONDS`: time inside local per-record loops only.
  - `COMM_SECONDS`: time inside `Scatterv`/`Reduce`/`Allreduce` calls.
  - `NUM_PROCS`: the process count the run used.

## Benchmarking

`benchmark.sh` automates the whole loop, same shape as q5's: build, generate
a dataset per size, run the sequential baseline once, run the distributed
version at each process count, diff outputs for correctness, and log
everything to `results.csv`.

```bash
./benchmark.sh                              # defaults: N in {100k,500k,1M,2M}, P in {1,2,4,8,16}
./benchmark.sh "500000 2000000" "1 2 4 8"    # custom sizes / process counts
```

Environment variables:
- `K` (default 10): top-K servers/endpoints to report.
- `S` (default 1000): distinct server/endpoint ids.
- `SEED` (default 1): generator seed, for reproducible datasets.

`cluster_benchmark.slurm` runs the same matrix inside a single Slurm
allocation on the IIIT RCE cluster:

```bash
module load openmpi/4.1.5
sbatch cluster_benchmark.slurm
```
