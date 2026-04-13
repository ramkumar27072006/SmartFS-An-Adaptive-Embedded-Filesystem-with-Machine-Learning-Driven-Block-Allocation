# SmartFS + TinyML

> **An adaptive, crash-resilient embedded filesystem with on-device ML-driven block allocation — targeting Teensy 4.1 and PC simulation.**

![Language](https://img.shields.io/badge/language-C%2B%2B17-blue?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Teensy%204.1%20%7C%20PC%20Simulator-green?style=flat-square)
![Build](https://img.shields.io/badge/build-CMake%203.14%2B-orange?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-lightgrey?style=flat-square)

---

SmartFS is a FAT-style embedded filesystem built from scratch that uses a **TinyML decision tree** to dynamically select the optimal block allocation strategy at runtime. It features a **write-ahead journal** for crash recovery, a **wear-leveling layer** to extend flash/SD lifetime, and a full **PC simulator** so you can develop and test without hardware.

---

## Features

- **Adaptive allocation** — ML model switches between Sequential, Random, and Wear-Aware strategies based on live workload metrics
- **Crash recovery** — Write-ahead journal ensures consistency across power failures and abrupt resets
- **Wear leveling** — Per-block erase counters with histogram visualization and GC hints
- **PC simulator** — Full filesystem runs against a file-backed `sd.img`; no hardware required
- **Interactive shell** — REPL with `create`, `write`, `read`, `ls`, `fsck`, `gc`, `map`, and more
- **FSCK** — Offline consistency checker that detects bitmap mismatches, FAT corruption, and orphaned blocks
- **Teensy 4.1 port** — Drop-in firmware using SdFat sector I/O

---

## Architecture

```
Application
     │
 SmartFS API
     │
 Allocation Engine ◄── TinyML Decision Tree
     │                  (avg_write_size, writes_per_sec)
 FAT + Bitmap + Directory
     │
 Wear Tracking Layer
     │
 Journal (Write-Ahead Log)
     │
 Block Device Driver
     │
 SD Card  /  sd.img (simulation)
```

---

## Storage Layout

| Block(s)  | Purpose        |
|-----------|----------------|
| 0         | Superblock     |
| 1         | Free bitmap    |
| 2         | Wear table     |
| 3 – 34    | FAT table      |
| 35 – 66   | Directory      |
| 67 – 4093 | Data blocks    |
| 4094 – 4095 | Journal      |

Block size: **512 bytes** · Total: **4096 blocks (2 MB image)**

---

## Allocation Modes

| Mode | Strategy    | When Selected by ML                  |
|:----:|-------------|--------------------------------------|
| 0    | Sequential  | Large writes (> 1024 B), low WPS (< 10) |
| 1    | Random      | Small writes (< 512 B), high WPS (> 30) |
| 2    | Wear-Aware  | Default / balanced workloads         |

The embedded decision tree (`ml_predict.cpp`) runs in O(log n) with zero heap allocation, making it safe for bare-metal targets.

---

## Getting Started

### Prerequisites

| Tool | Version |
|------|---------|
| CMake | 3.14+ |
| C++ compiler | GCC / Clang / MSVC with C++17 |
| Python | 3.9+ (ML pipeline only) |

### Build (PC Simulator)

```bash
git clone https://github.com/<your-username>/smartfs-tinyml.git
cd smartfs-tinyml

mkdir build && cd build
cmake ..
cmake --build .
```

Six binaries are produced in `build/`:

| Binary  | Purpose                              |
|---------|--------------------------------------|
| `run`   | Full workload demonstration          |
| `view`  | Block map visualization              |
| `crash` | Crash simulation + journal recovery  |
| `fsck`  | Filesystem consistency checker       |
| `wear`  | Wear distribution + histogram        |
| `shell` | Interactive filesystem REPL          |

---

## Usage

### Quick Demo
```bash
./run
```
Formats the image, writes small and large files, reads them back, deletes, runs GC, and prints wear statistics.

### Interactive Shell
```bash
./shell
```

Available commands:

```
format   mount    unmount  create   write    read
delete   ls       gc       fsck     map      wear
mode     help     exit
```

### Crash Recovery Test
```bash
./crash
```
Writes data, simulates a mid-write power failure, remounts, and verifies that journal rollback restores a consistent state.

### Disk Inspection
```bash
./view      # Visual block map (free / used / journal / metadata)
./fsck      # Consistency check — reports errors and mismatches
./wear      # Per-block erase counts + ASCII histogram
```

---

## ML Pipeline

### 1 — Generate Dataset

```bash
cd ml
python generate_dataset.py
```

Produces `smartfs_dataset.csv` with 50,000 synthetic workload samples covering all three allocation regimes.

### 2 — Train Models

```bash
python train_models.py
```

Trains a **Decision Tree**, **Logistic Regression**, and a **Tiny Neural Network** (Keras). The decision tree is automatically exported to `ml_predict_generated.cpp` as a standalone C++ if-else function — no inference library required on device.

**Python dependencies:**

```bash
pip install scikit-learn numpy
pip install tensorflow   # optional — only for neural network training
```

### 3 — Integrate

Copy (or diff) `ml_predict_generated.cpp` into `src/ml_predict.cpp`, rebuild, and the updated model is live.

---

## Teensy 4.1 Deployment

1. Install **SdFat** via the Arduino Library Manager (or PlatformIO)
2. Open `teensy/smartfs_teensy/smartfs_teensy.ino` in Arduino IDE or PlatformIO
3. Select board → **Teensy 4.1**
4. Flash and open the Serial Monitor at **115200 baud**

The Teensy firmware reuses the same filesystem logic and ML inference. The only change is the block device backend, which replaces `fread`/`fwrite` on `sd.img` with `sd.card()->readSector()` / `writeSector()`.

---

## Project Structure

```
smartfs-tinyml/
├── CMakeLists.txt              CMake build system
├── README.md
├── include/
│   ├── smartfs_config.h        Block layout constants
│   ├── smartfs.h               Core FS structures + public API
│   ├── block_device.h          Block device abstraction
│   ├── journal.h               Write-ahead journal interface
│   ├── wear.h                  Wear tracking interface
│   ├── ml_predict.h            ML inference header
│   ├── fsck.h                  Consistency checker interface
│   └── viewer.h                Block map visualization
├── src/
│   ├── smartfs.cpp             Core filesystem implementation
│   ├── block_device.cpp        File-backed block device (simulator)
│   ├── journal.cpp             Journal + crash recovery
│   ├── wear.cpp                Wear tracking layer
│   ├── ml_predict.cpp          Embedded decision tree inference
│   ├── fsck.cpp                FSCK implementation
│   └── viewer.cpp              Disk map renderer
├── tools/
│   ├── run.cpp                 Workload demo
│   ├── view.cpp                Block map tool
│   ├── crash.cpp               Crash simulation
│   ├── fsck_tool.cpp           FSCK CLI
│   ├── wear_tool.cpp           Wear stats + histogram
│   └── shell.cpp               Interactive REPL shell
├── ml/
│   ├── generate_dataset.py     Synthetic dataset generator
│   ├── train_models.py         Model training + C++ export
│   ├── smartfs_dataset.csv     Generated dataset
│   └── ml_predict_generated.cpp  Exported C++ decision tree
└── teensy/
    └── smartfs_teensy/
        └── smartfs_teensy.ino  Teensy 4.1 firmware
```

---

## How It Works

```
1. SmartFS monitors: avg_write_size, writes_per_sec
2. Every N operations, the ML model re-evaluates the workload
3. The decision tree selects an allocation mode (0 / 1 / 2)
4. The block allocator uses that mode until the next evaluation
5. All writes go through the journal before being committed
6. The wear layer records erase counts and biases allocation away from hot blocks
```

---

## Contributing

Contributions are welcome. Please:

1. Fork and create a feature branch
2. Keep C++ code within the existing C++17 style (no STL in hot paths, no dynamic allocation in the FS core)
3. Add or update the relevant tool in `tools/` if your feature needs a demo
4. Open a pull request with a clear description

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

*Built for embedded systems research and education. Tested on GCC 12 / Clang 15 / MSVC 2022 and Teensy 4.1 with a 32 GB SD card.*
