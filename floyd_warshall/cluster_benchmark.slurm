#!/bin/bash
#SBATCH --job-name=q5_fw_bench
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=16
#SBATCH --time=00:20:00
#SBATCH --output=%x_%j.out
#SBATCH --error=%x_%j.err

# Runs the Q5 Floyd-Warshall benchmark matrix (seq vs. dist at P=1,2,4,8,16
# for several graph sizes) inside a single Slurm allocation, so the login
# node and shared scheduler only see one modest job instead of many.
#
# Submit from ~/smehta/q5 with:  sbatch cluster_benchmark.slurm

set -euo pipefail
module load openmpi/4.1.5

# sbatch copies the script into a spool dir before running it, so $0 does
# NOT point at the submission directory -- must cd explicitly.
cd /home/cs3401.10/smehta/q5

SIZES=(50 300 700 1200 2000)
PROCS=(1 2 4 8 16)
EDGE_DENSITY=5
SEED=1

BUILD_DIR=../q5_bench/build
DATA_DIR=../q5_bench/data
RESULTS=../q5_bench/results.csv

mkdir -p "$BUILD_DIR" "$DATA_DIR"

echo "Node: $(hostname)   Allocated CPUs: $SLURM_NTASKS   Job: $SLURM_JOB_ID"

mpic++ -O3 -o "$BUILD_DIR/main_dist" main_dist.cpp
g++    -O3 -o "$BUILD_DIR/main_seq"  main_seq.cpp
g++    -O3 -o "$BUILD_DIR/input_generator" input_generator.cpp

echo "vertices,edges,procs,seq_time_sec,dist_time_sec,compute_sec,comm_sec,comm_pct,speedup_vs_seq,speedup_vs_p1,efficiency,correct" > "$RESULTS"

for V in "${SIZES[@]}"; do
    E=$(( V * EDGE_DENSITY ))
    INFILE="$DATA_DIR/graph_${V}.txt"

    if [[ ! -f "$INFILE" ]]; then
        "$BUILD_DIR/input_generator" "$V" "$E" 100 "$SEED" "$INFILE" 0 >/dev/null
    fi
    ACTUAL_E=$(head -1 "$INFILE" | awk '{print $2}')

    echo "== V=$V E=$ACTUAL_E =="

    SEQ_OUT="$DATA_DIR/seq_${V}.out"
    SEQ_ERR="$DATA_DIR/seq_${V}.err"
    "$BUILD_DIR/main_seq" < "$INFILE" > "$SEQ_OUT" 2> "$SEQ_ERR"
    SEQ_TIME=$(grep TIME_SECONDS "$SEQ_ERR" | awk '{print $2}')
    echo "  sequential: ${SEQ_TIME}s"

    T1=""

    for P in "${PROCS[@]}"; do
        DIST_OUT="$DATA_DIR/dist_${V}_${P}.out"
        DIST_ERR="$DATA_DIR/dist_${V}_${P}.err"

        # Confined to this job's own allocation (SLURM_NTASKS=16) -- never
        # requests more ranks than were reserved, and never touches nodes
        # outside this allocation.
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
        echo "${V},${ACTUAL_E},${P},${SEQ_TIME},${DIST_TIME},${COMPUTE_TIME},${COMM_TIME},${COMM_PCT},${SPEEDUP_SEQ},${SPEEDUP_P1},${EFFICIENCY},${CORRECT}" >> "$RESULTS"
    done
done

echo "Done. Results at $RESULTS"
