import re
import subprocess
import sys
from collections import defaultdict

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch

color_map = {"gpb-lite-": "#1f77b4", "gpb-": "#0d4a8c", "spb-": "#2ca02c", "nanopb-": "#d62728"}

def parse_benchmark_output(output: str):
    """Parse pyperf-style table."""
    results = []
    lines = output.splitlines()
    table_started = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("|---"):
            table_started = True
            continue
        if table_started and stripped.startswith("|") and "`" in stripped:
            match = re.search(r"\|\s*([\d,]+\.?\d*)\s*\|.*?\|\s*`([^`]+)`", stripped)
            if match:
                try:
                    ns_op = float(match.group(1).replace(",", ""))
                    name = match.group(2).strip()
                    results.append({"benchmark": name, "ns_op": ns_op})
                except ValueError:
                    continue
    return results


def get_base_name(name: str) -> str:
    for prefix in color_map.keys():
        if name.startswith(prefix):
            return name[len(prefix) :]
    return name


def main():
    if len(sys.argv) < 2:
        print(
            "Usage: python benchmark_plotter.py <benchmark_exe1> [benchmark_exe2] ..."
        )
        sys.exit(1)

    RUNS = 3
    all_raw_results = []  # will store all runs

    for exe in sys.argv[1:]:
        print(f"Running {exe} ({RUNS} times)...")
        exe_results = defaultdict(list)

        for run in range(1, RUNS + 1):
            print(f"  Run {run}/{RUNS}")
            try:
                proc = subprocess.run(
                    [exe], capture_output=True, text=True, timeout=120, check=False
                )
                if proc.returncode != 0:
                    print(f"    Warning: exited with code {proc.returncode}")

                parsed = parse_benchmark_output(proc.stdout)
                for r in parsed:
                    exe_results[r["benchmark"]].append(r["ns_op"])
            except Exception as e:
                print(f"    Error on run {run}: {e}")

        # Compute average for this executable
        for bench, values in exe_results.items():
            if values:
                avg_ns = np.mean(values)
                all_raw_results.append({"benchmark": bench, "ns_op": avg_ns})
        print(f"  Finished {exe} - averaged over {RUNS} runs")

    if not all_raw_results:
        print("No results found.")
        sys.exit(1)

    # Group by base name
    groups = defaultdict(dict)
    for r in all_raw_results:
        base = get_base_name(r["benchmark"])
        prefix = next(
            (p for p in color_map.keys() if r["benchmark"].startswith(p)),
            "other",
        )
        groups[base][prefix] = r["ns_op"]

    # Sort groups by SUM of ns/op (lowest total first)
    base_names = sorted(
        groups.keys(), key=lambda b: sum(v for v in groups[b].values() if v is not None)
    )

    # Plot
    bar_width = 0.22
    x = np.arange(len(base_names))

    fig, ax = plt.subplots(figsize=(max(12, len(base_names) * 0.7), 9))

    for i, base in enumerate(base_names):
        sorted_impls = sorted(groups[base].items(), key=lambda item: item[1])

        for j, (prefix, value) in enumerate(sorted_impls):
            pos = x[i] + (j - len(sorted_impls) / 2 + 0.5) * bar_width
            color = color_map.get(prefix, "#7f7f7f")
            ax.bar(
                pos, value, width=bar_width, color=color, edgecolor="black", alpha=0.9
            )

    ax.set_xticks(x)
    ax.set_xticklabels(base_names, rotation=45, ha="right", fontsize=10)
    ax.set_ylabel("Average ns/op (lower is better)", fontsize=12)
    ax.set_title(
        "Benchmark Comparison (3 runs averaged)\n"
        "(Groups sorted by total sum • Bars sorted lowest → highest)",
        fontsize=13,
        pad=20,
    )
    ax.grid(axis="y", linestyle="--", alpha=0.7)

    legend_elements = [
        Patch(facecolor=color, label=label.rstrip('-'))
        for label, color in color_map.items()
    ]

    ax.legend(
        handles=legend_elements, title="Implementation", loc="upper left", fontsize=10
    )

    plt.tight_layout()
    plt.savefig("speed-benchmark.png", dpi=200, bbox_inches="tight")
    print("\nPlot saved to: speed-benchmark.png")

    # Summary
    print("\n=== Averaged Results (groups sorted by sum) ===")
    for base in base_names:
        print(f"\n{base}:")
        for prefix, val in sorted(groups[base].items(), key=lambda item: item[1]):
            print(f"   {prefix:<10} {val:12.2f} ns/op")


if __name__ == "__main__":
    main()
