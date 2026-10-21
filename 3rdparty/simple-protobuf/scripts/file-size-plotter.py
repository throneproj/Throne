import os
import sys
from collections import defaultdict
#from turtle import color

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch

color_map = {"gpb-lite-": "#1f77b4", "gpb-": "#0d4a8c", "spb-": "#2ca02c", "nanopb-": "#d62728"}

def is_executable(file_path: str) -> bool:
    """Check if file is executable (has execute permission)."""
    return os.access(file_path, os.X_OK)


def get_base_name(name: str) -> str:
    """Extract base name by removing known prefixes."""
    for prefix in color_map.keys():
        if name.startswith(prefix):
            return name[len(prefix) :]
    return name


def main():
    if len(sys.argv) < 2:
        print("Usage: python file_size_plotter.py <directory>")
        sys.exit(1)

    directory = sys.argv[1]

    if not os.path.isdir(directory):
        print(f"Error: '{directory}' is not a valid directory.")
        sys.exit(1)

    executables = []
    for root, _, files in os.walk(directory):
        for f in files:
            full_path = os.path.join(root, f)

            if os.path.isfile(full_path) and is_executable(full_path):
                size_bytes = os.path.getsize(full_path)
                executables.append({
                    "filename": f,
                    "size": size_bytes / 1024,
                })

    if not executables:
        print("No executable files found in the directory.")
        sys.exit(1)

    print(f"Found {len(executables)} executable file(s).")

    # Group by base name
    groups = defaultdict(dict)
    for exe in executables:
        base = get_base_name(exe["filename"])
        prefix = next(
            (p for p in color_map.keys() if exe["filename"].startswith(p)),
            "other",
        )
        groups[base][prefix] = exe["size"]

    # Sort groups by their HIGHEST member (max size), from lowest max to highest max
    base_names = sorted(
        groups.keys(),
        key=lambda b: max((v for v in groups[b].values() if v is not None), default=0),
    )

    bar_width = 0.22
    x = np.arange(len(base_names))

    fig, ax = plt.subplots(figsize=(max(12, len(base_names) * 0.7), 9))

    for i, base in enumerate(base_names):
        sorted_impls = sorted(groups[base].items(), key=lambda item: item[1])

        for j, (prefix, size) in enumerate(sorted_impls):
            pos = x[i] + (j - len(sorted_impls) / 2 + 0.5) * bar_width
            color = color_map.get(prefix, "#7f7f7f")
            ax.bar(
                pos, size, width=bar_width, color=color, edgecolor="black", alpha=0.9
            )

    ax.set_xticks(x)
    ax.set_xticklabels(base_names, rotation=45, ha="right", fontsize=10)
    ax.set_ylabel("File Size (KB)", fontsize=12)
    ax.set_title(
        "Executable File Sizes\n"
        "(Groups sorted by highest member • Bars sorted smallest → largest)",
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
    plt.savefig("file-size-benchmark.png", dpi=200, bbox_inches="tight")
    print("Plot saved to: file-size-benchmark.png")

    # Summary
    print("\n=== File Sizes (KB) ===")
    for base in base_names:
        print(f"\n{base}:")
        for prefix, size in sorted(groups[base].items(), key=lambda item: item[1]):
            print(f"   {prefix:<10} {size:10.2f} KB")


if __name__ == "__main__":
    main()
