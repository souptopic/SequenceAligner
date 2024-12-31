ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
else
    DETECTED_OS := $(shell uname -s)
    MINGW_AVAILABLE := $(shell which x86_64-w64-mingw32-gcc 2>/dev/null)
endif

ifeq ($(DETECTED_OS),Windows)
    CC = gcc
else
    CC = gcc
    CROSS_CC = x86_64-w64-mingw32-gcc
endif

BASE_FLAGS = -march=native -mavx2 -msse4.2 -pthread

TUNE_FLAGS = $(BASE_FLAGS) -O3 -flto -ffast-math -funroll-loops \
    -fno-strict-aliasing -fprefetch-loop-arrays -Wl,--gc-sections \
    -DNDEBUG -mbmi2 -DBATCHMODE

ifeq ($(MAKECMDGOALS),debug)
    COMMON_FLAGS = $(BASE_FLAGS) -g -O0 -Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wformat-security -Wstack-protector -Wstrict-overflow=5 -fstack-protector-strong
else
    COMMON_FLAGS = $(BASE_FLAGS) -O3 -flto -ffast-math -funroll-loops \
    -fno-strict-aliasing \
    -fprefetch-loop-arrays -Wl,--gc-sections \
    -DNDEBUG -mbmi2
endif

WINDOWS_FLAGS = $(COMMON_FLAGS) -mconsole -D_WIN32_WINNT=0x0601
CROSS_FLAGS = $(WINDOWS_FLAGS) -DCROSS_COMPILE

SRC_DIR = src
TUNER_DIR = src/tuners
INC_DIR = include
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
TUNER_FILES = $(wildcard $(TUNER_DIR)/*.c)

SOURCES = $(addprefix $(SRC_DIR)/, $(notdir $(SRC_FILES)))
TUNER_SOURCES = $(addprefix $(TUNER_DIR)/, $(notdir $(TUNER_FILES)))
NATIVE_TARGETS = $(addprefix bin/, $(notdir $(SOURCES:.c=)))
TUNER_NATIVE_TARGETS = $(addprefix bin/, $(notdir $(TUNER_SOURCES:.c=)))
WINDOWS_TARGETS = $(addprefix bin/, $(notdir $(SOURCES:.c=.exe)))
TUNER_WINDOWS_TARGETS = $(addprefix bin/, $(notdir $(TUNER_SOURCES:.c=.exe)))

DATASET_DIR = testing/datasets
MEGA_DATASET = $(DATASET_DIR)/avpdb_mega.csv
BASE_DATASET = $(DATASET_DIR)/avpdb.csv

all: debug

done:
	@echo "Done"

tune:
	$(MAKE) debug SRC_DIR=$(TUNER_DIR) \
        NATIVE_TARGETS="$(TUNER_NATIVE_TARGETS)" \
        WINDOWS_TARGETS="$(TUNER_WINDOWS_TARGETS)" \
        COMMON_FLAGS="$(TUNE_FLAGS)"

dataset: $(MEGA_DATASET) done

$(MEGA_DATASET): $(BASE_DATASET)
	python3 scripts/create_mega_dataset.py -sc

ifeq ($(DETECTED_OS),Windows)
debug: clean windows done
else
ifeq ($(MINGW_AVAILABLE),)
debug: clean linux done
else
debug: clean linux windows done
endif
endif

linux: $(NATIVE_TARGETS)

windows: $(WINDOWS_TARGETS)

bin/%: $(SRC_DIR)/%.c | bin
	$(CC) $(COMMON_FLAGS) -I$(INC_DIR) -o $@ $< -lm -lpthread

bin/%.exe: $(SRC_DIR)/%.c | bin
ifeq ($(DETECTED_OS),Windows)
	$(CC) $(WINDOWS_FLAGS) -I$(INC_DIR) -o $@ $< -lm -lpthread -lcomdlg32 -lShlwapi
else
	$(CROSS_CC) $(CROSS_FLAGS) -I$(INC_DIR) -o $@ $< -lm -lpthread -lcomdlg32 -lshlwapi
endif

bin:
ifeq ($(DETECTED_OS),Windows)
	-powershell -Command "if (-not (Test-Path bin)) { New-Item -ItemType Directory -Path bin | Out-Null }"
else
	mkdir -p bin
endif

clean:
ifeq ($(DETECTED_OS),Windows)
	-powershell -Command "if (Test-Path bin) { Remove-Item -Path bin -Recurse -Force -ErrorAction SilentlyContinue }"
else
	rm -rf bin
endif

.PHONY: all linux windows debug clean dataset tune done