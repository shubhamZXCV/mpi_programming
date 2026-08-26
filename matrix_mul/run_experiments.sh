#!/bin/bash
# ==========================================================================
# run_experiments.sh
#
# Sweeps problem size x process count, times main_seq and main_distri,
# checks correctness against the sequential output, and writes every
# timing sample to results.csv for collect_results.py to summarize.
#
# Usage:
#   ./run_experiments.sh
#
# Tunables are the arrays right below. Edit them to match your assignment
# (e.g. change matrix dimensions, add/remove process counts, more repeats
# for noisier machines).
# ==========================================================================
set -u

# ---- Configuration --------------------------------------------------

# name:m:n:p  (square matrices here for simplicity; make m/n/p different
# if your assignment requires non-square inputs)
SIZES=(
  "small:200:200:200"
  "medium:500:500:500"
  "large:1000:1000:1000"
  "very_large:2000:2000:2000"
)

PROCS=(1 2 4 8)
REPEATS=3          # timing samples per (size, P) combination
INPUT_DIR="inputs"
OUTPUT_DIR="outputs"
RESULTS_CSV="results.csv"

# mpirun extra flags. On a real SLURM allocation you normally need NONE
# of these. They only matter when you manually test with more ranks than
# physical cores, or when running as root in a container, e.g.:
#   MPIRUN_EXTRA_FLAGS="--oversubscribe --allow-run-as-root" ./run_experiments.sh
MPIRUN_EXTRA_FLAGS="${MPIRUN_EXTRA_FLAGS:-}"

# ---- Setup ------------------------------------------------------------

mkdir -p "$INPUT_DIR" "$OUTPUT_DIR"

if [ ! -x ./main_seq ] || [ ! -x ./main_distri ]; then
    echo "Binaries not found -- building with make..."
    make all || { echo "Build failed"; exit 1; }
fi

echo "category,size_name,m,n,p,P,run,time_seconds" > "$RESULTS_CSV"
ERR_TMP=$(mktemp)

# ---- Sweep --------------------------------------------------------------

for entry in "${SIZES[@]}"; do
    IFS=":" read -r name m n p <<< "$entry"
    input_file="${INPUT_DIR}/${name}.txt"

    if [ ! -f "$input_file" ]; then
        echo "Generating input for '$name' ($m x $n x $p)..."
        python3 generate_input.py "$m" "$n" "$p" "$input_file" > /dev/null
    fi

    seq_out="${OUTPUT_DIR}/${name}_seq.out"

    echo "== Size: $name ($m x $n x $p) =="

    # ---- Sequential baseline ----
    for run in $(seq 1 "$REPEATS"); do
        ./main_seq < "$input_file" > "$seq_out" 2> "$ERR_TMP"
        t=$(awk '/TIME/{print $2}' "$ERR_TMP")
        echo "seq,${name},${m},${n},${p},0,${run},${t}" >> "$RESULTS_CSV"
        echo "  [seq] run ${run}: ${t}s"
    done

    # ---- Distributed program at each P ----
    for P in "${PROCS[@]}"; do
        mpi_out="${OUTPUT_DIR}/${name}_P${P}.out"
        for run in $(seq 1 "$REPEATS"); do
            mpirun -np "$P" $MPIRUN_EXTRA_FLAGS ./main_distri < "$input_file" \
                > "$mpi_out" 2> "$ERR_TMP"
            t=$(awk '/TIME/{print $2}' "$ERR_TMP")
            echo "mpi,${name},${m},${n},${p},${P},${run},${t}" >> "$RESULTS_CSV"
            echo "  [P=$P] run ${run}: ${t}s"
        done

        # Correctness check (compare last run's output against sequential)
        if diff -q "$seq_out" "$mpi_out" > /dev/null 2>&1; then
            echo "  [P=$P] correctness: MATCH"
        else
            echo "  [P=$P] correctness: MISMATCH  <-- investigate!"
        fi
    done
done

rm -f "$ERR_TMP"

echo ""
echo "Done. Raw timings written to $RESULTS_CSV"
echo "Run: python3 collect_results.py   to build the report tables/plots."
