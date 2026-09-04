## Results Table

Speed-up $S(P) = T_1 / T_P$

| Input size | P=1 | P=2 | P=4 | P=8 |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 0.58 | 2.13 | 2.69 |
| Medium | 1.00 | 1.90 | 3.37 | 5.37 |
| Large | 1.00 | 1.75 | 3.59 | 6.15 |
| Very large | 1.00 | 1.74 | 3.34 | 6.19 |

## Runtime table (raw seconds, mean of repeats)

| Input size | P=1 (s) | P=2 (s) | P=4 (s) | P=8 (s) | Sequential (s) |
|------------|:-------:|:-------:|:-------:|:-------:|:--------------:|
| Small | 0.0047 | 0.0081 | 0.0022 | 0.0017 | 0.0047 |
| Medium | 0.0671 | 0.0353 | 0.0199 | 0.0125 | 0.0670 |
| Large | 0.5285 | 0.3027 | 0.1471 | 0.0859 | 0.5260 |
| Very large | 4.2046 | 2.4190 | 1.2578 | 0.6790 | 4.2217 |

## Efficiency $E(P) = S(P)/P$

| Input size | P=1 | P=2 | P=4 | P=8 |
|------------|:-----:|:-----:|:-----:|:-----:|
| Small | 1.00 | 0.29 | 0.53 | 0.34 |
| Medium | 1.00 | 0.95 | 0.84 | 0.67 |
| Large | 1.00 | 0.87 | 0.90 | 0.77 |
| Very large | 1.00 | 0.87 | 0.84 | 0.77 |

## Speed-up vs. P graph

See `speedup_vs_p.png`.

## Efficiency vs. P graph

See `efficiency_vs_p.png`.

## Observations: speedup, efficiency, and communication vs. computation

**Speedup.** For Medium, Large and Very large inputs, speedup tracks the ideal $S(P)=P$ line reasonably closely up to P=4 and bends away only modestly at P=8 (6.15–6.19 out of an ideal 8). Small behaves very differently: it barely speeds up at all (2.69 at P=8) and actually *slows down* at P=2 (0.58, i.e. slower than one process). The one-off 0.0183s outlier for the first P=2 run of Small in `run.log` (vs. ~0.003s on the repeats) shows this regime is dominated by run-to-run OS/MPI-launch jitter rather than by the algorithm itself, because the actual work is only a few milliseconds.

**Efficiency.** $E(P)$ decreases with P for every input size (e.g. Very large: 1.00 → 0.87 → 0.84 → 0.77), which is expected — some communication is added at every process count and doesn't parallelize away. What stands out is how much faster efficiency collapses for Small (1.00 → 0.29 → 0.53 → 0.34) than for the larger sizes, which stay above 0.77 even at P=8. Efficiency at a fixed P consistently improves as problem size grows (P=8: 0.34 → 0.67 → 0.77 → 0.77), i.e. the code scales *strongly* only once there is enough work per process to amortize fixed costs.

**Communication vs. computation.** `main_distri.cpp` broadcasts the *entire* matrix B (n×p ints) to every rank with `MPI_Bcast`, then scatters A row-wise with `MPI_Scatterv` and gathers C back with `MPI_Gatherv`. Computation is $O(mnp/P)$ — it shrinks linearly with P — but the `MPI_Bcast` of B costs $O(np)$ regardless of P (its latency even grows slowly with P due to the broadcast tree), and the scatter/gather volume per process is $O(mp/P + mn/P)$. So as P grows for a fixed problem size, the communication share of total runtime necessarily grows relative to the shrinking compute share, which is exactly the efficiency drop-off seen above.

Subtracting the ideal compute-only time ($T_1/P$) from the measured $T_P$ gives an estimate of communication+overhead time per run:

| Input size | Overhead at P=2 (s) | Overhead at P=4 (s) | Overhead at P=8 (s) | Overhead/T1 at P=8 |
|------------|:---:|:---:|:---:|:---:|
| Small | 0.0058 | 0.0010 | 0.0011 | ~24% |
| Medium | 0.0018 | 0.0031 | 0.0041 | ~6% |
| Large | 0.0385 | 0.0150 | 0.0198 | ~4% |
| Very large | 0.3167 | 0.2067 | 0.1534 | ~4% |

The absolute overhead grows with input size (larger B means a bigger broadcast: 16MB of ints for Very large vs. 160KB for Small), but the *computation* grows faster still (cubically in matrix dimension vs. quadratically for the communicated data), so the overhead is a shrinking fraction of total runtime as problems get bigger. This is why Small is communication/overhead-bound and scales poorly, while Medium/Large/Very large are compute-bound and scale close to linearly — a textbook illustration of Amdahl's-law-style behavior driven here by a fixed-per-size communication cost that doesn't shrink with P.
