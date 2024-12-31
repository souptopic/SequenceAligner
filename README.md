# Sequence Aligner

A sequence alignment tool for academic purposes. Not accepting contributions.

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [File Formats](#default-file-formats)
- [Performance](#performance-benchmarks)
- [Development](#development)

## Overview
Sequence Aligner provides efficient DNA/protein sequence alignment using memory-mapped I/O and AVX optimizations. Supports both single and multi-threaded operations.

## Features
- Needleman-Wunsch algorithm
- Memory mapped file I/O
- Multithreading support
- Cross-platform (Windows/Linux)
- AVX-optimized code
- Simple Python launcher interface
- Dark mode

## Requirements

<details open>
<summary>Windows (MSYS2/UCRT64)</summary>

1. Install [Python](https://www.python.org/downloads/)
2. Install [MSYS2](https://www.msys2.org)
3. Run in MSYS2 UCRT64:
```sh
# Core dependencies
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make
```

4. Optional Dark mode support
```sh
# In Windows Terminal
pip install customtkinter
```
</details>

<details>
<summary>Linux</summary>

```sh 
# Core dependencies (use distro equivalent package manager and package)
sudo apt install build-essential python3-tk

# Optional: Windows build support
sudo apt install gcc-mingw-w64

# Optional: Dark mode
pip install customtkinter
```
</details>

## Installation

```sh
# Clone repository
git clone https://github.com/souptopic/SequenceAligner.git
cd SequenceAligner

# Run the launcher
python start.py
```

## Usage

1. Launch [start.py](start.py)
2. Select options:
   - **Run**: Choose `Singlethreaded` or `Multithreaded`
   - **Settings**: Modify format, I/O files, and parameters
3. Click `Save` to apply changes or `Reset` for defaults
4. Use `Tuning` for batch size optimization

## Default File Formats

<details>
<summary>Input CSV Format</summary>

```csv
sequence,label
KPVSLS,0
LNNSRA,0
```
- Standard dataset: [avpdb.csv](testing/datasets/avpdb.csv) (1042 lines, 27KB)
- Large dataset: Auto-generated (4,001,280 lines, 97.6MB)
</details>

<details>
<summary>Output CSV Format</summary>

```csv
sequence1,sequence2,label1,label2,score,alignment
KPVSLS,LNNSRA,0,0,-5,"('KPVSLS', 'LNNSRA')"
```
- Single-threaded output: [results.csv](results/results.csv)
- Multi-threaded output: `results_mt.csv`
</details>

## Performance Benchmarks

| Mode | Platform | Without Write | With Write |
|------|----------|--------------|------------|
| Singlethreaded* | WSL2 | ~400µs | ~550µs |
| Singlethreaded* | Windows | ~600µs | ~750µs |
| Multithreaded** | Windows | ~450ms | ~900ms |
| Multithreaded** | WSL2 | 1-2s | 1-2s |

\* Using standard dataset
\** Using large dataset

*Note: Timing excludes initialization overhead*

## [Development](TODO.md)

## License

This project is licensed under the GNU Affero General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

This software is intended for academic purposes only. If you use this software in your research, please cite:

```bibtex
@software{SequenceAligner,
  author = {souptopic},
  title = {Sequence Aligner},
  year = {2025},
  url = {https://github.com/souptopic/SequenceAligner}
}
```
