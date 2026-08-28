# MPI Matrix Multiplication Benchmark

Everything needed to build, run, and report on the sequential vs.
MPI-distributed matrix multiplication benchmark.

## Files

| File | Purpose |
|---|---|
| `generate_input.py` | Generates a random `m x n` and `n x p` matrix pair into a text file (fixed indentation bug from the original). |
| `main_seq.cpp` | Sequential matrix multiplication. Prints the result matrix to **stdout** and the wall-clock compute time (`TIME <seconds>`) to **stderr**. |
| `main_distri.cpp` | MPI row-wise-scatter matrix multiplication. Same stdout/stderr convention as above; rank 0 times from just before the broadcast of B to just after the gather of C (communication + computation), taking the max over ranks. |
| `Makefile` | Builds `main_seq` (g++) and `main_distri` (mpicxx). |
| `run_experiments.sh` | Sweeps input sizes x process counts {1,2,4,8}, times everything, verifies correctness against the sequential output, writes `results.csv`. |
| `collect_results.py` | Turns `results.csv` into the report's Speed-up table, plus Runtime and Efficiency tables, and a `speedup_vs_p.png` chart. |
| `slurm_benchmark.sh` | SLURM batch script that builds everything and runs the full sweep on the cluster. |

## Quick start (local machine)

```bash
make all
./run_experiments.sh
python3 collect_results.py
```

This produces:
- `results.csv` — every raw timing sample
- `report_tables.md` — Markdown tables ready to paste into your report
- `speedup_vs_p.png` — Speed-up vs. P chart

If you're testing locally on a machine with fewer than 8 physical cores,
run with:

```bash
MPIRUN_EXTRA_FLAGS="--oversubscribe" ./run_experiments.sh
```

(add `--allow-run-as-root` too if you're testing inside a root container).

## Quick start (SLURM cluster)

```bash
sbatch slurm_benchmark.sh
```

Check progress with `squeue -u $USER`; once it finishes, `job_<id>.log`
has the run output and `results.csv` / `report_tables.md` /
`speedup_vs_p.png` will be in the submission directory.

## Notes / assumptions

- **Problem sizes**: `run_experiments.sh` uses square matrices by default
  — Small=200, Medium=500, Large=1000, Very large=2000 — edit the `SIZES`
  array at the top of the script if your assignment specifies different
  or non-square dimensions.
- **Speed-up baseline**: matching the report template (where the `P=1`
  column is fixed at 1.00), `S(P) = T(P=1) / T(P)` uses the *distributed*
  program's own single-process run as `T_1`, not the separate sequential
  program. The sequential program's time is still measured and included
  as a reference column in the runtime table — useful for discussing
  parallelization overhead (the gap between `main_seq` and `main_distri`
  at P=1) in your write-up.
- **Repeats**: each (size, P) pair is timed `REPEATS=3` times (mean is
  used); bump this up if your cluster timings are noisy.
- **Partitioning**: rows of A are split as evenly as possible across
  ranks (`m / size` rows each, with the first `m % size` ranks getting
  one extra row), so non-divisible sizes are handled without any rows
  being dropped or duplicated.
- **Correctness**: after every `P`, the script diffs the MPI output
  against the sequential output for that size and reports MATCH /
  MISMATCH.
- **Compile flags**: `-O2 -Wall` for both binaries (see `Makefile`).
