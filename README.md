# Sequence Aligner

A "high-performance"? sequence alignment tool.

NOTE: This is a University Project.

TEMP NOTE: 
- File writing adds a lot to the final time and benchmark timing isn't standardized. Expect (relatively) massive fluctuations depending on unrelated OS things like file existing, cache etc. See: [TODO.md](TODO.md)
	- Remove / comment (`#`) `BASE_FLAGS += -DMODE_WRITE` at line 21 in [Makefile](Makefile) for consistent results until I fix writing.

## Table of Contents
- [Requirements](#requirements)
  - [Windows (MSYS2)](#windows-msys2)
  - [Linux](#linux)
  - [Windows Support on Linux](#windows-support-on-linux)
  - [WSL2](#wsl2)
- [Building](#building)
  - [Windows Build](#windows-build)
  - [Linux Build](#linux-build)
  - [Cross-Platform Build on Linux](#cross-platform-build-on-linux)
  - [WSL2 Build](#wsl2-build)
- [Usage](#usage)
- [Features](#features)
- [Development](#development)

## Requirements

### Windows (MSYS2)
- MSYS2 installed from https://www.msys2.org
- GCC toolchain: `pacman -S mingw-w64-x86_64-gcc`
- Make (choose one):
  - MinGW Make: `pacman -S mingw-w64-x86_64-make` (I used this)
  - GNU Make: `pacman -S make` (Didn't test)
- Required CPU features: AVX2, SSE4.2, BMI2

### Linux
- GCC toolchain: `sudo apt install build-essential`
- Required CPU features: AVX2, SSE4.2, BMI2

### Windows Support on Linux
- MinGW-w64 cross-compiler: `sudo apt install gcc-mingw-w64`
- Required CPU features: AVX2, SSE4.2, BMI2

### WSL2
Same requirements as Linux or Windows Support on Linux depending on target platform.

## Building

### Windows Build
```sh
# In MSYS2 MinGW64 shell
mingw32-make
# If using GNU Make
make
```

### Linux Build
```sh
make linux
```

### Cross-Platform Build on Linux
```sh
# Builds both Linux and Windows executables
make
```

### WSL2 Build
Same commands as [Linux Build](#linux-build) or [Cross-Platform Build on Linux](#cross-platform-build-on-linux)

## Cleaning
```sh
# Removes bin/ directory
make clean
mingw32-make clean # MSYS2 MinGW64 Make
```

Or just manually remove the `bin/` directory

## Usage
The program reads sequences from CSV files and performs alignment:

Note: do `cd bin` first

- Single-threaded version: `./ms` or `.\ms.exe`


- Multi-threaded version: `./mt` or `.\mt.exe`


## Format:

### Input Files
- Standard dataset: [avpdb.csv](testing/datasets/avpdb.csv) | 1042 lines | 27KB
- Large dataset: [avpdb_mega.csv](testing/datasets/avpdb_mega.csv) | 4001280 lines | 97.6MB

```csv
sequence, label
KPVSLS,0
LNNSRA,0
```

### Output Files
- Single-threaded: [results.csv](results/results.csv)
- Multi-threaded: [results_mega.csv](results/results_mega.csv)

Format:
```csv
sequence1,sequence2,label1,label2,score,alignment
KPVSLS,LNNSRA,0,0,-5,"('KPVSLS', 'LNNSRA')"
LNNSRA,HCKFWF,0,1,-14,"('LNNSRA', 'HCKFWF')"
```

## Features
- AVX2 SIMD optimized sequence alignment
- Memory mapped file reading
- Support for both single and multi-threaded processing
- Cross-platform support (Windows/Linux)

## Development
See [TODO.md](TODO.md)

## Benchmark Times

### Temporary methodology
- Timing starts on first non-header csv line parsed
- Timing ends on last line aligned
- Undecided about including other things such as file creation as they add very large fluctuations between runs and make it impossible to notice improvements from micro-optimizations - The point isn't optimizing Windows File System, but the alignment algorithm and (maybe) csv parsing, and seeing the effects of number of total lines.
	- Right now, things like frees, file creation and writes add a bit of time, sometimes no change, sometimes stalling for 2 seconds, probably from stuff like Windows Defender...
	- Writing can be optimized with buffered writing and will be later properly included in benchmark time if it gets consistent
	- Right now, to best see micro-optimizations from the alignment algorithm comment the line 21 in [Makefile](Makefile), as then there will be little to no fluctuations in final time.

### Singe-threaded Standard dataset
- ~400µs on WSL2 Ubuntu without writing, ~550µs with writing (formatting).
- ~200µs extra on Windows (for some reason?)
- ~0-800µs extra for whole code (File creation and opening, writing, freeing, closing... wild fluctuations, sometimes stalling for a whole second or two for some reason)

### Multithreaded Large dataset
- ~450ms on Windows without writing, ~900ms with writing (~300ms less on writev instead of write: [TODO.md](TODO.md), needs further testing and optimizing)