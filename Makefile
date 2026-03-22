# Makefile for GameCube / devkitPPC (C++)
# Target: SupremacyProject

# --- Environment Setup ---
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/c/devkitPro")
endif
DEVKITPPC := $(DEVKITPRO)/devkitPPC
LIBOGC    := $(DEVKITPRO)/libogc

# --- Tools ---
CXX      := $(DEVKITPPC)/bin/powerpc-eabi-g++
LD       := $(DEVKITPPC)/bin/powerpc-eabi-g++
ELF2DOL  := $(DEVKITPRO)/tools/bin/elf2dol
BIN2S    := $(DEVKITPRO)/tools/bin/bin2s
AS       := $(DEVKITPPC)/bin/powerpc-eabi-as

# --- Project Files ---
TARGET  := SupremacyProject
BUILD   := build
SOURCES := $(wildcard src/*.cpp)
INCLUDE := include

# --- Data / Asset Embedding ---
# Any file placed in data/ will be embedded into the binary.
# A file like data/textures/grass.png becomes:
#   extern const u8 grass_png[];
#   extern const u32 grass_png_size;
DATADIR   := data
# Generate a .s and .o for each data file in build/data/

# --- Compiler & Linker Flags ---
PORTLIBS  := $(DEVKITPRO)/portlibs/ppc

CXXFLAGS := -Wall -O2 -I$(INCLUDE) -I$(LIBOGC)/include -I$(BUILD)/data -I$(PORTLIBS)/include \
            -DGEKKO -D_GC -mrvl -mcpu=750 -meabi -mhard-float
LDFLAGS  := -L$(LIBOGC)/lib/cube -L$(PORTLIBS)/lib -logc -lpng -lz -lm

# --- Build Rules ---
# Auto-generate texture atlas before building
ATLAS     := data/textures/atlas.png
ATLAS_H   := include/atlas_regions.h
ATLAS_SRC := $(filter-out data/textures/atlas.png,$(wildcard data/textures/*.png))
DATAFILES := $(ATLAS)  # only embed the atlas
DATAOBJS  := $(patsubst $(DATADIR)/%,$(BUILD)/data/%.o,$(DATAFILES))
GCNTARGET := $(BUILD)/$(TARGET).dol
ELF       := $(BUILD)/$(TARGET).elf

all: $(GCNTARGET)

$(GCNTARGET): $(ELF)
	@echo "Converting ELF to DOL..."
	$(ELF2DOL) $(ELF) $(GCNTARGET)
	@echo "Build Complete: $(GCNTARGET)"

# Link: include data object files alongside source objects
$(ATLAS) $(ATLAS_H): $(ATLAS_SRC)
	@echo "Generating texture atlas..."
	@python3 tools/gen_atlas.py

$(ELF): $(SOURCES) $(DATAOBJS)
	@mkdir -p $(BUILD)
	@echo "Compiling..."
	$(CXX) $(CXXFLAGS) $(SOURCES) $(DATAOBJS) -o $(ELF) $(LDFLAGS)

# Rule: convert any data file → .s via bin2s → .o via as
# e.g. data/textures/grass.png → build/data/textures/grass.png.o
$(BUILD)/data/%.o: $(DATADIR)/% $(ATLAS_H)
	@mkdir -p $(dir $@)
	@echo Embedding $<...
	$(BIN2S) $< > $(BUILD)/data/$*.s
	$(AS) $(BUILD)/data/$*.s -o $@

# Dummy rule to avoid duplicate
clean:
	@echo "Cleaning..."
	rm -rf $(BUILD)

.PHONY: all clean