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
BASE_FLAGS += -DMODE_WRITE # Comment this line to disable write mode

ifeq ($(MAKECMDGOALS),debug)
    COMMON_FLAGS = $(BASE_FLAGS) -g -O0 -Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wformat-security -Wstack-protector -Wstrict-overflow=5 -fstack-protector-strong
else
    COMMON_FLAGS = $(BASE_FLAGS) -O3 -flto -ffast-math -funroll-loops \
    -fno-strict-aliasing \
    -fprefetch-loop-arrays -Wl,--gc-sections \
    -DNDEBUG -Wno-unused-result -mbmi2
endif

WINDOWS_FLAGS = $(COMMON_FLAGS) -mconsole -D_WIN32_WINNT=0x0601

SRC_DIR = src
INC_DIR = include
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

SOURCES = $(addprefix $(SRC_DIR)/, $(notdir $(SRC_FILES)))
NATIVE_TARGETS = $(addprefix bin/, $(notdir $(SOURCES:.c=)))
WINDOWS_TARGETS = $(addprefix bin/, $(notdir $(SOURCES:.c=.exe)))

DATASET_DIR = testing/datasets
MEGA_DATASET = $(DATASET_DIR)/avpdb_mega.csv
BASE_DATASET = $(DATASET_DIR)/avpdb.csv

all: $(MEGA_DATASET) debug

$(MEGA_DATASET): $(BASE_DATASET)
	python3 scripts/create_mega_dataset.py -sc

ifeq ($(DETECTED_OS),Windows)
debug: windows
else
ifeq ($(MINGW_AVAILABLE),)
debug: linux
else
debug: linux windows 
endif
endif

linux: $(NATIVE_TARGETS)

windows: $(WINDOWS_TARGETS)

bin/%: $(SRC_DIR)/%.c | bin
	$(CC) $(COMMON_FLAGS) -I$(INC_DIR) -o $@ $< -lm -lpthread

bin/%.exe: $(SRC_DIR)/%.c | bin
ifeq ($(DETECTED_OS),Windows)
	$(CC) $(WINDOWS_FLAGS) -I$(INC_DIR) -o $@ $< -lm -lpthread -lcomdlg32
else
	$(CROSS_CC) $(WINDOWS_FLAGS) -I$(INC_DIR) -o $@ $< -lm -lpthread -lcomdlg32
endif

bin:
	mkdir -p bin

clean:
	rm -rf bin

.PHONY: all linux windows debug clean