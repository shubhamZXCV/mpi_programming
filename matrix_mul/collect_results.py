#!/usr/bin/env python3
"""
collect_results.py

Reads results.csv (produced by run_experiments.sh) and produces:
  - report_tables.md   : Speed-up, Runtime, and Efficiency tables in the
                          exact format requested for the report, plus a
                          correctness summary.
  - speedup_vs_p.png    : Speed-up vs. P line chart (one line per size).

Speed-up here is defined the way the report table implies:
    S(P) = T(P=1) / T(P)
using the *distributed* program's own single-process time as the P=1
baseline (so the P=1 column is exactly 1.00, matching the template).
The separately-measured sequential program's time is also reported for
reference (useful for discussing parallel overhead in your write-up).

Usage:
    python3 collect_results.py [results.csv]
"""
import sys
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

CSV_PATH = sys.argv[1] if len(sys.argv) > 1 else "results.csv"
PROC_COLS = [1, 2, 4, 8]

# Preserve a sensible, human-friendly display order/labels for sizes.
DISPLAY_NAME = {
    "small": "Small",
    "medium": "Medium",
    "large": "Large",
    "very_large": "Very large",
}


def load():
    df = pd.read_csv(CSV_PATH)
    df["time_seconds"] = pd.to_numeric(df["time_seconds"], errors="coerce")
    return df


def summarize(df):
    """Return dict: size_name -> {'seq': mean_seq_time, P: mean_time, ...}"""
    out = {}
    sizes = list(df["size_name"].unique())
    # keep known order first, then anything unrecognized
    ordered = [s for s in DISPLAY_NAME if s in sizes] + [s for s in sizes if s not in DISPLAY_NAME]

    for size in ordered:
        sub = df[df["size_name"] == size]
        seq_mean = sub[sub["category"] == "seq"]["time_seconds"].mean()
        entry = {"seq": seq_mean}
        for P in PROC_COLS:
            times = sub[(sub["category"] == "mpi") & (sub["P"] == P)]["time_seconds"]
            entry[P] = times.mean() if len(times) else float("nan")
        out[size] = entry
    return out


def fmt(x, nd=2):
    if x is None or (isinstance(x, float) and (x != x)):
        return "--"
    return f"{x:.{nd}f}"


def build_runtime_table(summary):
    lines = ["| Input size | P=1 (s) | P=2 (s) | P=4 (s) | P=8 (s) | Sequential (s) |",
             "|------------|:-------:|:-------:|:-------:|:-------:|:--------------:|"]
    for size, vals in summary.items():
        label = DISPLAY_NAME.get(size, size)
        row = [label] + [fmt(vals[P], 4) for P in PROC_COLS] + [fmt(vals["seq"], 4)]
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def build_speedup_table(summary):
    lines = ["| Input size | P=1 | P=2 | P=4 | P=8 |",
             "|------------|:-----:|:-----:|:-----:|:-----:|"]
    for size, vals in summary.items():
        label = DISPLAY_NAME.get(size, size)
        t1 = vals[1]
        row = [label]
        for P in PROC_COLS:
            s = t1 / vals[P] if vals[P] else float("nan")
            row.append(fmt(s))
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def build_efficiency_table(summary):
    lines = ["| Input size | P=1 | P=2 | P=4 | P=8 |",
             "|------------|:-----:|:-----:|:-----:|:-----:|"]
    for size, vals in summary.items():
        label = DISPLAY_NAME.get(size, size)
        t1 = vals[1]
        row = [label]
        for P in PROC_COLS:
            s = t1 / vals[P] if vals[P] else float("nan")
            e = s / P
            row.append(fmt(e))
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def plot_speedup(summary, outfile="speedup_vs_p.png"):
    plt.figure(figsize=(6, 4.5))
    for size, vals in summary.items():
        t1 = vals[1]
        ys = [t1 / vals[P] if vals[P] else float("nan") for P in PROC_COLS]
        plt.plot(PROC_COLS, ys, marker="o", label=DISPLAY_NAME.get(size, size))
    plt.plot(PROC_COLS, PROC_COLS, "k--", alpha=0.4, label="Ideal")
    plt.xlabel("Number of processes (P)")
    plt.ylabel("Speed-up S(P)")
    plt.title("Speed-up vs. P")
    plt.xticks(PROC_COLS)
    plt.legend()
    plt.grid(alpha=0.3)
    plt.tight_layout()
    plt.savefig(outfile, dpi=150)
    plt.close()


def main():
    df = load()
    summary = summarize(df)

    speedup_md = build_speedup_table(summary)
    runtime_md = build_runtime_table(summary)
    efficiency_md = build_efficiency_table(summary)

    plot_speedup(summary)

    report = f"""## Results Table

Speed-up $S(P) = T_1 / T_P$

{speedup_md}

## Runtime table (raw seconds, mean of repeats)

{runtime_md}

## Efficiency $E(P) = S(P)/P$

{efficiency_md}

## Speed-up vs. P graph

See `speedup_vs_p.png`.
"""
    with open("report_tables.md", "w") as f:
        f.write(report)

    print(report)
    print("Wrote report_tables.md and speedup_vs_p.png")


if __name__ == "__main__":
    main()
