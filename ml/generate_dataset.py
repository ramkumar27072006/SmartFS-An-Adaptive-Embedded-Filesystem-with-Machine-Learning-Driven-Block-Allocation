"""
SmartFS TinyML — Synthetic Dataset Generator

Generates 50,000 samples with features:
  - avg_write_size: 64-4096 bytes
  - writes_per_sec: 1-100

Labels (allocation mode):
  0 = SEQUENTIAL  (large writes, low throughput)
  1 = RANDOM      (small writes, high throughput)
  2 = WEAR_AWARE  (balanced / default)

Output: smartfs_dataset.csv
"""

import csv
import random
import os


def label_sample(avg_write_size, writes_per_sec):
    """Apply labeling rule from spec."""
    if avg_write_size < 512 and writes_per_sec > 30:
        return 1  # RANDOM
    elif avg_write_size > 1024 and writes_per_sec < 10:
        return 0  # SEQUENTIAL
    else:
        return 2  # WEAR_AWARE


def generate_dataset(num_samples=50000, output_file="smartfs_dataset.csv"):
    random.seed(42)

    samples = []
    for _ in range(num_samples):
        avg_write_size = random.uniform(64, 4096)
        writes_per_sec = random.uniform(1, 100)
        label = label_sample(avg_write_size, writes_per_sec)
        samples.append((avg_write_size, writes_per_sec, label))

    # Write CSV
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(script_dir, output_file)

    with open(output_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["avg_write_size", "writes_per_sec", "alloc_mode"])
        writer.writerows(samples)

    # Print distribution
    counts = {0: 0, 1: 0, 2: 0}
    for _, _, label in samples:
        counts[label] += 1

    mode_names = {0: "SEQUENTIAL", 1: "RANDOM", 2: "WEAR_AWARE"}
    print(f"Generated {num_samples} samples -> {output_path}")
    print("Class distribution:")
    for mode, count in sorted(counts.items()):
        print(f"  {mode_names[mode]:12s}: {count:6d} ({100*count/num_samples:.1f}%)")


if __name__ == "__main__":
    generate_dataset()
