#!/usr/bin/env bash
# Benchmarks the distributed Floyd-Warshall (main_dist.cpp) against the
# sequential baseline (main_seq.cpp), verifying correctness at every point
# and logging timings to a CSV.
#
# Usage:
#   ./benchmark.sh                       # default sizes/process counts
#   ./benchmark.sh "100 300 500" "1 2 4 8"
#
set -euo pipefail
cd "$(dirname "$0")"

SIZES=(${1:-100 200 500 1000})
PROCS=(${2:-1 2 4 8 16})
EDGE_DENSITY=${EDGE_DENSITY:-5}   # avg out-degree per vertex; E = V * EDGE_DENSITY
SEED=${SEED:-1}

BUILD_DIR=build
DATA_DIR=data
RESULTS=results.csv

mkdir -p "$BUILD_DIR" "$DATA_DIR"

echo "Building..."
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

    T1=""  # dist wall time at P=1, used as the S(P)=T1/TP baseline per report_format.md

    for P in "${PROCS[@]}"; do
        DIST_OUT="$DATA_DIR/dist_${V}_${P}.out"
        DIST_ERR="$DATA_DIR/dist_${V}_${P}.err"

        mpirun -np "$P" --oversubscribe "$BUILD_DIR/main_dist" < "$INFILE" \
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

echo
echo "Done. Results written to $RESULTS"
