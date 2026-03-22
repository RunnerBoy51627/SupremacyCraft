# ── My3DSCraft — Universal Makefile ──────────────────────────────────────────
# Usage:
#   make                          → GameCube build (default)
#   make PLATFORM=pc              → PC build
#   make PLATFORM=pc GLM_INCLUDE=./glm
#   make clean                    → clean all builds
# ─────────────────────────────────────────────────────────────────────────────

PLATFORM ?= gc
TARGET    := My3DSCraft
BUILD     := build/$(PLATFORM)
SHADERS   := shaders/

GAME_SRCS := $(wildcard src/*.cpp)
ATLAS     := data/textures/atlas.png

# Default goal must be declared before any file rules
.DEFAULT_GOAL := all
ATLAS_H   := include/atlas_regions.h
ATLAS_SRC := $(filter-out $(ATLAS),$(wildcard data/textures/*.png))

# ── Atlas generation ─────────────────────────────────────────────────────────
.PHONY: all clean hbc pack

# Switch resource pack: make pack PACK=mypack
pack:
	@python3 tools/gen_atlas.py --set $(PACK)
	@echo "Run make to rebuild with the new pack"

# List available packs
packs:
	@python3 tools/gen_atlas.py --list


gen_atlas: $(ATLAS_SRC)
	@echo "Generating texture atlas..."
	@python3 tools/gen_atlas.py $(if $(PACK),--pack $(PACK))

$(ATLAS_H): gen_atlas

# ═════════════════════════════════════════════════════════════════════════════
ifeq ($(PLATFORM), pc)
# ── PC Build ─────────────────────────────────────────────────────────────────

PLATFORM_DIR := src/platform/pc
SOURCES      := $(GAME_SRCS) \
                $(PLATFORM_DIR)/pc_window.cpp \
                $(PLATFORM_DIR)/pc_input.cpp

GLM_INCLUDE ?= /usr/include
INCLUDE     := -Iinclude -I$(BUILD)/data -I$(PLATFORM_DIR) \
               -Isrc/platform -I$(GLM_INCLUDE)

UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
    SDL_CFLAGS := $(shell sdl2-config --cflags)
    SDL_LIBS   := $(shell sdl2-config --libs) -framework OpenGL
    CXX        := g++
else ifeq ($(UNAME), Linux)
    SDL_CFLAGS := $(shell sdl2-config --cflags)
    SDL_LIBS   := $(shell sdl2-config --libs) -lGL -lGLEW -lGLU
    CXX        := g++
else
    MSYS2_PREFIX := $(if $(MSYSTEM_PREFIX),$(MSYSTEM_PREFIX),/mingw64)
    SDL_CFLAGS   := -I$(MSYS2_PREFIX)/include -I$(MSYS2_PREFIX)/include/SDL2
    SDL_LIBS     := -L$(MSYS2_PREFIX)/lib -lmingw32 -lSDL2main -lSDL2 \
                    -lglew32 -lopengl32 -lglu32 -lSDL2_mixer
    CXX          := $(MSYS2_PREFIX)/bin/g++
endif

CXXFLAGS := -Wall -O2 -std=c++11 -fext-numeric-literals \
            $(INCLUDE) $(SDL_CFLAGS) -D_PC -DPLATFORM_PC
LDFLAGS  := $(SDL_LIBS) -lpng -lz -lm

ATLAS_C   := $(BUILD)/data/atlas_png.cpp
ATLAS_OBJ := $(BUILD)/data/atlas_png.o

all: $(ATLAS_H) $(BUILD)/$(TARGET)

$(ATLAS_C): $(ATLAS_H)
	@mkdir -p $(BUILD)/data
	@echo "Embedding atlas (PC)..."
	@python3 tools/embed_packs.py $(BUILD)/data

$(ATLAS_OBJ): $(ATLAS_C)
	$(CXX) -c $(ATLAS_C) -o $(ATLAS_OBJ)

$(BUILD)/$(TARGET): $(SOURCES) $(ATLAS_OBJ)
	@mkdir -p $(BUILD)
	@echo "Compiling PC..."
	$(CXX) $(CXXFLAGS) -O1 src/itemdrop.cpp -c -o $(BUILD)/itemdrop.o
	$(CXX) $(CXXFLAGS) $(filter-out src/itemdrop.cpp,$(SOURCES)) $(ATLAS_OBJ) $(BUILD)/itemdrop.o -o $@ $(LDFLAGS)
	@cp -r $(SHADERS) $(BUILD)/shaders
	@echo "Done: $@"

# ═════════════════════════════════════════════════════════════════════════════
else
# ── GameCube Build ────────────────────────────────────────────────────────────

ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO not set. export DEVKITPRO=/c/devkitPro")
endif

DEVKITPPC := $(DEVKITPRO)/devkitPPC
LIBOGC    := $(DEVKITPRO)/libogc
PORTLIBS  := $(DEVKITPRO)/portlibs/ppc

CXX     := $(DEVKITPPC)/bin/powerpc-eabi-g++
ELF2DOL := $(DEVKITPRO)/tools/bin/elf2dol
BIN2S   := $(DEVKITPRO)/tools/bin/bin2s
AS      := $(DEVKITPPC)/bin/powerpc-eabi-as

SOURCES  := $(GAME_SRCS)
CXXFLAGS := -Wall -O2 \
            -Iinclude -I$(LIBOGC)/include -I$(BUILD)/data \
            -I$(PORTLIBS)/include -Isrc/platform \
            -DGEKKO -D_GC -DHAVE_WIIMOTE -mrvl -mcpu=750 -meabi -mhard-float
WII_LIBS := $(if $(wildcard $(LIBOGC)/lib/wii/libwiiuse.a),-lwiiuse -lbte)
LDFLAGS  := -L$(LIBOGC)/lib/wii -L$(LIBOGC)/lib/cube -L$(PORTLIBS)/lib \
            $(WII_LIBS) -lasnd -logc -lpng -lz -lm

ELF       := $(BUILD)/$(TARGET).elf
DOL       := $(BUILD)/$(TARGET).dol
# Pack objects built by embed_packs_gc.py
PACK_OBJS := $(wildcard $(BUILD)/data/packs/*.o)

# Sound raw PCM files
SOUND_RAWS := $(wildcard data/sounds/*.raw)
SOUND_OBJS := $(patsubst data/sounds/%.raw,$(BUILD)/data/sounds/%.raw.o,$(SOUND_RAWS))

all: $(ATLAS_H) include/atlas_packs.h $(DOL)
	@echo "GC build complete: $(DOL)"

include/atlas_packs.h: $(ATLAS_H)
	@mkdir -p $(BUILD)/data/packs
	@echo "Generating atlas_packs.h..."
	@python3 tools/embed_packs_gc.py $(BUILD)/data $(BIN2S) $(AS)

# Convert sounds then re-invoke make so SOUND_RAWS wildcard picks up new .raw files
.PHONY: convert_sounds
convert_sounds:
	@python3 tools/sound2raw.py
	@$(MAKE) --no-print-directory

$(DOL): $(ELF)
	@echo "Converting to DOL..."
	$(ELF2DOL) $(ELF) $(DOL)
	@echo "Done: $@"

$(ELF): $(SOURCES) $(SOUND_OBJS)
	@mkdir -p $(BUILD)
	@echo "Compiling GC..."
	$(eval PACK_OBJS := $(wildcard $(BUILD)/data/packs/*.o))
	$(CXX) $(CXXFLAGS) $(SOURCES) $(PACK_OBJS) $(SOUND_OBJS) -o $(ELF) $(LDFLAGS)

$(BUILD)/data/sounds/%.raw.o: data/sounds/%.raw
	@mkdir -p $(dir $@)
	@echo "Embedding sound $<..."
	cd data/sounds && $(BIN2S) $*.raw > $(abspath $(BUILD)/data/sounds/$*.raw.s)
	$(AS) $(BUILD)/data/sounds/$*.raw.s -o $@

endif
# ═════════════════════════════════════════════════════════════════════════════

clean:
	@echo "Cleaning all builds..."
	rm -rf build

# ── Homebrew Channel package ─────────────────────────────────────────────────
# Creates apps/My3DSCraft/ folder ready to copy to SD card
# Usage: make hbc
HBC_DIR := apps/My3DSCraft

hbc: $(DOL)
	@mkdir -p $(HBC_DIR)
	@cp $(DOL) $(HBC_DIR)/boot.dol
	@cp hbc/meta.xml $(HBC_DIR)/meta.xml
	@if [ -f hbc/icon.png ]; then cp hbc/icon.png $(HBC_DIR)/icon.png; fi
	@echo "HBC package ready: $(HBC_DIR)/"
	@echo "Copy the apps/ folder to the root of your SD card"