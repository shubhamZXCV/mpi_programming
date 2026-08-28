Committed test fixtures for a quick correctness check without running the generator.

| Input | Expected output | What it checks |
|---|---|---|
| `sample_input.txt` | `sample_expected_output.txt` | 6 hand-written records, small enough to verify by eye |
| `tiny_more_procs_input.txt` | `tiny_more_procs_expected_output.txt` | 5 records with P=8 (most processes get 0 records) |

Both expected-output files were produced by `main_seq.cpp` and cross-checked
against `main_dist.cpp` at P=1, 3, 8, and 16; see `REPORT.md` for the full
correctness table.

Check either one with:

```bash
mpic++ -O3 -o main_dist ../main_dist.cpp
mpirun -np 4 --oversubscribe ./main_dist < sample_input.txt | diff - sample_expected_output.txt && echo MATCH
```
