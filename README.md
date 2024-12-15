# Sequence Aligner

A sequence alignment tool.

NOTE: This is a University Project.
  - Please, do not contribute or create issues with this repository.

## Table of Contents
- [Requirements](#requirements)
  - [Windows (MSYS2 -> UCRT64)](#windows-msys2---ucrt64)
  - [Linux or WSL2](#linux-or-wsl2)
  - [Windows Support on Linux or WSL2](#windows-support-on-linux-or-wsl2)
- [Building](#building)
  - [Windows Build](#windows-build)
  - [Linux or WSL2 Build](#linux-or-wsl2-build)
  - [Cross-Platform Build on Linux or WSL2](#cross-platform-build-on-linux-or-wsl2)
  - [Creating large datasets (important)](#creating-large-datasets) 
  - [Tuning for potential faster runtime](#tuning-for-potential-faster-runtime)
- [Cleaning](#cleaning)
- [Usage](#usage)
- [Format](#format)
- [Features](#features)
- [Development](#development)
- [Benchmark Times](#benchmark-times)

## Requirements
- CPU features: AVX2, SSE4.2, BMI2

### Windows (MSYS2 -> UCRT64)
- MSYS2 installed from https://www.msys2.org in UCRT64 Environment
- GCC toolchain: `pacman -S mingw-w64-ucrt-x86_64-gcc`
- Python: `pacman -S mingw-w64-ucrt-x86_64-python`
- Make: `pacman -S mingw-w64-ucrt-x86_64-make`
	- UCRT64 Packages are for building Windows projects, while GNU versions (`gcc`, `make`) and `python` are for native MSYS2

### Linux or WSL2
- Build essentials: `sudo apt install build-essential` 
	- or distro equivalent

### Windows Support on Linux or WSL2
- MinGW-w64 cross-compiler: `sudo apt install gcc-mingw-w64`
	- or distro equivalent

## Building
- Clone this repository and change directory to project root
```sh
git clone https://github.com/souptopic/SequenceAligner.git
cd SequenceAligner
```

### Windows Build
```sh
mingw32-make
```

### Linux or WSL2 Build 
```sh
make linux
```

### Cross-Platform Build on Linux or WSL2
```sh
# Builds both Linux and Windows executables
make
```

### Creating large datasets
- Create large datasets from the provided standard dataset with
```sh
mingw32-make dataset # MSYS2 UCRT64 (Windows) Make
make dataset
```
- Required for running `mt(.exe)` in default mode (no arguments)

### Tuning for potential faster runtime
- Build tuners with 
```sh
mingw32-make tune
make tune
``` 
- Run the binaries `batch(.exe)` and `buffer(.exe)`
- Results will be displayed and you can modify the variables they tune
- The tuners will print out an explanation on how to do so

## Cleaning
```sh
# Removes bin/ directory
mingw32-make clean # MSYS2 UCRT64 (Windows) Make
make clean
```
Or just manually remove the `bin/` directory

## Usage
The program reads sequences from CSV files and performs alignment:
- Single-threaded version: `.bin/ms(.exe)`
- Multi-threaded version: `.bin/mt(.exe)`
- Optional arguments: `[input_csv_path] [output_csv_path]`, `-h` or `--help`

## Format:
- Note: Format will be easy to change with a script later

### Input Files
- Standard dataset: [avpdb.csv](testing/datasets/avpdb.csv) | 1042 lines | 27KB
- Large dataset: [created with script](scripts/create_mega_dataset.py) | 4001280 lines | 97.6MB

#### Format:
```csv
sequence, label
KPVSLS,0
LNNSRA,0
```

### Output Files
- Single-threaded: [results.csv](results/results.csv)
- Multi-threaded: `results_mega.csv`

#### Format:
```csv
sequence1,sequence2,label1,label2,score,alignment
KPVSLS,LNNSRA,0,0,-5,"('KPVSLS', 'LNNSRA')"
LNNSRA,HCKFWF,0,1,-14,"('LNNSRA', 'HCKFWF')"
```

## Features
- Needleman-Wunsch Sequence Alignment
- Memory mapped file reading and parsing
- Single and Multi threaded versions
- Cross-platform support (Windows/Linux)

## Development
- See [TODO](TODO.md)

## Benchmark Times

### Methodology
- Timing starts on first non-header csv line parsed
- Timing ends on last line aligned

#### Why not from program start to end?
- Things such as file creation add fluctuations between runs 
- The point isn't optimizing the Windows File System, but the alignment algorithm and (maybe) csv parsing and writing which actually allow for some optimizations.
- Right now, things like file creation and writes add a bit of time, sometimes no change, sometimes stalling for ~100-200µs or up to 2 seconds, probably from stuff like Windows Defender or File Explorer background services, making it impossible to notice improvements from micro-optimizations
- To see stable results, comment the line 21 in [Makefile](Makefile), as then there will be little to no fluctuations in final time. This should be the case until I make writing consistent and include it in the benchmark time. See: [TODO](TODO.md)

### Singe-threaded Standard dataset
- ~400µs on WSL2 Ubuntu without writing, ~550µs with
- ~200µs extra on Windows for both cases (for some reason?)
- ~0-800µs extra for whole code (File creation and opening, writing, freeing, closing; sometimes stalling for up to a whole second or two)

### Multithreaded Large dataset
- ~450ms on Windows without writing, ~900ms with writing (~350ms less with writev instead of write: [TODO](TODO.md), needs further testing and optimizing)
