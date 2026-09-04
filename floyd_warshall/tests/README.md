Committed test fixtures for quick correctness checks (no need to run the
generator or benchmark scripts just to verify the implementation works).

| Input | Expected output | What it checks |
|---|---|---|
| `sample_input.txt` | `sample_expected_output.txt` | The assignment's own worked example (3 vertices), byte-for-byte |
| `negative_weights_input.txt` | `negative_weights_expected_output.txt` | 50 vertices, negative edge weights, no negative cycle |

Both expected-output files were produced by `main_seq.cpp` and cross-checked
against `main_dist.cpp` at multiple process counts; see `REPORT.md` for the
full correctness table.

Check either one with:

```bash
mpic++ -O3 -o main_dist ../main_dist.cpp
mpirun -np 4 --oversubscribe ./main_dist < sample_input.txt | diff - sample_expected_output.txt && echo MATCH
```
