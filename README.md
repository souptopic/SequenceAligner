# Sequence Aligner

A sequence alignment tool for academic purposes. Not accepting contributions.

## Table of Contents
- [Features](#features)
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Tools](#tools)
- [Defaults](#default-file-formats)
- [Performance](#performance)
- [Development](#development)

## Features
- Needleman-Wunsch alignment algorithm
- Memory mapped file I/O
- Multi-threading support
- Cross-platform (Windows/Linux)
- AVX optimization

## Requirements

**Windows (MSYS2/UCRT64)**
- Install [MSYS2](https://www.msys2.org)
- Run in UCRT64: 
```sh
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-make
```

**Linux/WSL2**
- `sudo apt install build-essential` (or distro equivalent)
- For Windows builds: `sudo apt install gcc-mingw-w64`

## Quick Start

1. Clone & enter directory:
```sh
git clone https://github.com/souptopic/SequenceAligner.git
cd SequenceAligner
```

2. Build:
- Windows: `mingw32-make`
- Linux: `make linux`
- Cross-platform: `make`

3. Generate datasets:
```sh
make dataset  # Linux
mingw32-make dataset  # Windows
```

4. Run:
- Single-threaded: `bin/ms(.exe)`
- Multi-threaded: `bin/mt(.exe)`
- Options: `[input_csv_path] [output_csv_path]`, `-h`

## Tools
- Format customization: [config_editor.py](scripts/config_editor.py)
- Performance tuning: Build with `make tune` or `mingw32-make tune`
- Cleanup: `make clean` or `mingw32-make clean`

## Default file formats:

**Input CSV:**
- Standard dataset: [avpdb.csv](testing/datasets/avpdb.csv) | 1042 lines | 27KB
- Large dataset: [created with this script](scripts/create_mega_dataset.py) | 4001280 lines | 97.6MB
```csv
sequence,label
KPVSLS,0
LNNSRA,0
```

**Output CSV:**
- Single-threaded: [results.csv](results/results.csv)
- Multi-threaded: `results_mega.csv`
```csv
sequence1,sequence2,label1,label2,score,alignment
KPVSLS,LNNSRA,0,0,-5,"('KPVSLS', 'LNNSRA')"
```

## Performance

**Methodology:**
- Timing starts on first non-header csv line parsed
- Timing ends on last line aligned
- Excludes startup and initialization like argument parsing
- For development, you can disable writing in the [config_editor.py](scripts/config_editor.py) for consistent results

**Times:**
- Single-thread (standard dataset):
  - WSL2: ~400µs (no write), ~550µs (with write)
  - Windows: Add ~200µs to above times
- Multi-thread (large dataset):
  - Windows: ~450ms (no write), ~900ms (with write)
  - WSL2: Variable performance (~1-2s)

## [Development](TODO.md)