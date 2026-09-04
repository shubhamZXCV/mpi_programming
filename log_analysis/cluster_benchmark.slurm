#!/bin/bash
#SBATCH --job-name=q7_log_bench
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=16
#SBATCH --time=00:20:00
#SBATCH --output=%x_%j.out
#SBATCH --error=%x_%j.err

# Runs the Q7 log analytics benchmark matrix (seq vs. dist at P=1,2,4,8,16
# for several record counts) inside a single Slurm allocation, so the login
# node and shared scheduler only see one modest job instead of many.
#
# Submit from ~/smehta/q7 with:  sbatch cluster_benchmark.slurm

set -euo pipefail
module load openmpi/4.1.5

# sbatch copies the script into a spool dir before running it, so $0 does
# NOT point at the submission directory -- must cd explicitly.
cd /home/cs3401.10/smehta/q7

SIZES=(100000 500000 1000000 2000000 5000000)
PROCS=(1 2 4 8 16)
K=10
S=1000
SEED=1

BUILD_DIR=../q7_bench/build
DATA_DIR=../q7_bench/data
RESULTS=../q7_bench/results.csv

mkdir -p "$BUILD_DIR" "$DATA_DIR"

echo "Node: $(hostname)   Allocated CPUs: $SLURM_NTASKS   Job: $SLURM_JOB_ID"

mpic++ -O3 -o "$BUILD_DIR/main_dist" main_dist.cpp
g++    -O3 -o "$BUILD_DIR/main_seq"  main_seq.cpp
g++    -O3 -o "$BUILD_DIR/input_generator" input_generator.cpp

echo "records,procs,seq_time_sec,dist_time_sec,compute_sec,comm_sec,comm_pct,speedup_vs_seq,speedup_vs_p1,efficiency,correct" > "$RESULTS"

for N in "${SIZES[@]}"; do
    INFILE="$DATA_DIR/log_${N}.txt"

    if [[ ! -f "$INFILE" ]]; then
        "$BUILD_DIR/input_generator" "$N" "$K" "$S" "$SEED" "$INFILE" >/dev/null
    fi

    echo "== N=$N =="

    SEQ_OUT="$DATA_DIR/seq_${N}.out"
    SEQ_ERR="$DATA_DIR/seq_${N}.err"
    "$BUILD_DIR/main_seq" < "$INFILE" > "$SEQ_OUT" 2> "$SEQ_ERR"
    SEQ_TIME=$(grep TIME_SECONDS "$SEQ_ERR" | awk '{print $2}')
    echo "  sequential: ${SEQ_TIME}s"

    T1=""

    for P in "${PROCS[@]}"; do
        DIST_OUT="$DATA_DIR/dist_${N}_${P}.out"
        DIST_ERR="$DATA_DIR/dist_${N}_${P}.err"

        mpirun -np "$P" --bind-to none "$BUILD_DIR/main_dist" < "$INFILE" \
            > "$DIST_OUT" 2> "$DIST_ERR"

        DIST_TIME=$(grep TIME_SECONDS "$DIST_ERR" | awk '{print $2}')
        COMPUTE_TIME=$(grep COMPUTE_SECONDS "$DIST_ERR" | awk '{print $2}')
        COMM_TIME=$(grep COMM_SECONDS "$DIST_ERR" | awk '{print $2}')
        [[ "$P" == "1" ]] && T1="$DIST_TIME"

        if diff -q <(tr -s ' \t' ' ' < "$SEQ_OUT") <(tr -s ' \t' ' ' < "$DIST_OUT") > /dev/null; then
            CORRECT="yes"
        else
            CORRECT="NO_MISMATCH"
        fi

        COMM_PCT=$(python3 -c "print(f'{100*${COMM_TIME}/${DIST_TIME}:.2f}')" 2>/dev/null || echo "NA")
        SPEEDUP_SEQ=$(python3 -c "print(f'{${SEQ_TIME}/${DIST_TIME}:.3f}')" 2>/dev/null || echo "NA")
        SPEEDUP_P1=$(python3 -c "print(f'{${T1}/${DIST_TIME}:.3f}')" 2>/dev/null || echo "NA")
        EFFICIENCY=$(python3 -c "print(f'{(${T1}/${DIST_TIME})/${P}:.3f}')" 2>/dev/null || echo "NA")

        echo "  P=${P}: ${DIST_TIME}s  S(P)=${SPEEDUP_P1}  E(P)=${EFFICIENCY}  comm%=${COMM_PCT}  correct=${CORRECT}"
        echo "${N},${P},${SEQ_TIME},${DIST_TIME},${COMPUTE_TIME},${COMM_TIME},${COMM_PCT},${SPEEDUP_SEQ},${SPEEDUP_P1},${EFFICIENCY},${CORRECT}" >> "$RESULTS"
    done
done

echo "Done. Results at $RESULTS"
