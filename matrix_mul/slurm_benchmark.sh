#!/bin/bash
#SBATCH --job-name=matmul-bench
#SBATCH --output=job_%j.log
#SBATCH --nodes=2
#SBATCH --ntasks=8
#SBATCH --ntasks-per-node=4
#SBATCH --time=00:30:00
#SBATCH --partition=debug

echo "=========================================="
echo "SLURM_JOB_ID    = $SLURM_JOB_ID"
echo "SLURM_NODELIST  = $SLURM_NODELIST"
echo "SLURM_NNODES    = $SLURM_NNODES"
echo "SLURM_NTASKS    = $SLURM_NTASKS"
echo "=========================================="

module purge
module load openmpi/4.1.5

echo "Using MPI:"
which mpicxx
which mpirun

cd "$SLURM_SUBMIT_DIR"

# Build both binaries
make clean
make all

# Run the full size x process-count sweep.
# mpirun -np P below never exceeds the 8 tasks this job requested, so no
# extra flags (--oversubscribe / --allow-run-as-root) are needed here --
# those are only for ad-hoc local testing outside of SLURM.
./run_experiments.sh

# Build the report tables (report_tables.md) and speedup_vs_p.png
python3 collect_results.py

echo "=========================================="
echo "Benchmark complete. See results.csv, report_tables.md, speedup_vs_p.png"
echo "=========================================="
