BUILD_DIR=build/emulator
T3D_INST=$(N64_INST)

include $(N64_INST)/include/n64.mk
include $(N64_INST)/include/t3d.mk

# libdragon's n64.mk defines a generic `%.v64: %.z64` pattern rule (byteswap a
# freshly-linked ROM into a .v64). Our assets/sd/*.v64 files are INPUT assets,
# not build outputs. Because they're prerequisites of the filesystem copy rule,
# make walks that implicit chain -- v64 <- z64 <- $(BUILD_DIR)/%.elf -- and,
# since `%.z64`'s directory is split off, tries to LINK a bogus
# assets/sd/build/emulator/NNN.elf with no object files ([LD] error). Cancel
# the rule (redefine the pattern with no recipe) so these stay plain sources.
%.v64: %.z64

EXTRA_DEFINES ?=
# -Wmissing-field-initializers is excluded: the codebase partially designated-
# initializes libdragon/Tiny3D structs in ~97 places, which is well-defined
# (omitted members are value-initialised) and intentional. Enabling it would mean
# either listing third-party members we don't set, or rewriting every site.
N64_CXXFLAGS += -Wall -Wextra -Wno-missing-field-initializers -std=gnu++20 -Os -fno-exceptions -fno-rtti -fno-threadsafe-statics -Isrc $(EXTRA_DEFINES)

MKSPRITE_FLAGS=

PROJECT_NAME=WonderMenu

src = $(wildcard src/*.cpp) \
      $(wildcard src/animation/*.cpp) \
      $(wildcard src/boot/*.cpp) \
      $(wildcard src/boot/*.c) \
      $(wildcard src/boot/*.S) \
      $(wildcard src/flashcart/*.c) \
      $(wildcard src/flashcart/*/*.c) \
      $(wildcard src/general/*.cpp) \
      $(wildcard src/renderer/*.cpp) \
	  $(wildcard src/popovers/*.cpp) \
      $(wildcard src/scene/*.cpp) \
      $(wildcard src/scene/scenes/*.cpp) \
      $(wildcard src/scene/views/*.cpp) \
	  $(wildcard src/scene/views/drawables/*.cpp) \
      $(wildcard src/scene/views/rows/*.cpp) \
      $(wildcard src/ui/*.cpp) \
	  $(wildcard src/ui/views/*.cpp) \
	  $(wildcard src/ui/scenes/*.cpp) \
	  $(wildcard src/ui/popovers/*.cpp) \
	  $(wildcard src/ui/views/drawables/*.cpp) \
	  $(wildcard src/ui/views/rows/*.cpp) \
      $(wildcard src/util/*.cpp) \
      $(wildcard src/utils/*.c)

assets_png =. $(wildcard assets/ui/*.png) \
			  $(wildcard assets/ui/tab-control/*.png) \
			  $(wildcard assets/ui/games/*.png)

assets_gltf = $(wildcard assets/models/*.glb)

assets_ttf =  $(wildcard assets/fonts/*.ttf)

assets_otf =  $(wildcard assets/fonts/*.otf)

assets_n64 =  $(wildcard assets/sd/*.n64) \
			  $(wildcard assets/sd/*.v64)

assets_bin =  $(wildcard assets/sd/*.db)

assets_txt =  $(wildcard assets/*.txt)

assets_elf =  $(wildcard assets/*.elf.stripped)

assets_conv = $(patsubst assets/%,filesystem/%,$(assets_png:%.png=%.sprite)) \
			  $(patsubst assets/%,filesystem/%,$(assets_ttf:%.ttf=%.font64)) \
			  $(patsubst assets/%,filesystem/%,$(assets_otf:%.otf=%.font64)) \
			  $(patsubst assets/%,filesystem/%,$(assets_gltf:%.glb=%.t3dm)) \
			  $(patsubst assets/%,filesystem/%,$(assets_n64)) \
			  $(patsubst assets/%,filesystem/%,$(assets_bin)) \
			  $(patsubst assets/%,filesystem/%,$(assets_txt)) \
			  $(patsubst assets/%,filesystem/%,$(assets_elf))

all: $(PROJECT_NAME).z64
all: rom-with-size

filesystem/%.I8.sprite: MKSPRITE_FLAGS = -f I8
filesystem/%.IA8.sprite: MKSPRITE_FLAGS = -f IA8
filesystem/%.IA16.sprite: MKSPRITE_FLAGS = -f IA16
filesystem/%.RGBA16.sprite: MKSPRITE_FLAGS = -f RGBA16
filesystem/%.RGBA32.sprite: MKSPRITE_FLAGS = -f RGBA32

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o $(dir $@) "$<"

filesystem/fonts/InterDisplay-SemiBold-12.font64: MKFONT_FLAGS = -s 12
filesystem/fonts/InterDisplay-Bold-14.font64: MKFONT_FLAGS = -s 14
filesystem/fonts/InterDisplay-Medium-14.font64: MKFONT_FLAGS = -s 14
filesystem/fonts/InterDisplay-SemiBold-15.font64: MKFONT_FLAGS = -s 15
filesystem/fonts/InterDisplay-SemiBold-digits-15.font64: MKFONT_FLAGS = -s 15
filesystem/fonts/Unbounded-Black-14.font64: MKFONT_FLAGS = -s 14
filesystem/fonts/Unbounded-Black-16.font64: MKFONT_FLAGS = -s 16
filesystem/fonts/Unbounded-Black95-16.font64: MKFONT_FLAGS = -s 16

filesystem/fonts/%.font64: assets/fonts/%.ttf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -o $(dir $@) "$<"

filesystem/fonts/%.font64: assets/fonts/%.otf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -o $(dir $@) "$<"

filesystem/%.t3dm: assets/%.glb
	@mkdir -p $(dir $@)
	@echo "    [T3D-MODEL] $@"
	$(T3D_GLTF_TO_3D) "$<" $@ $(T3DM_FLAGS)
	$(N64_BINDIR)/mkasset -c 1 -o $(dir $@) $@

filesystem/sd/%.n64: assets/sd/%.n64
	@mkdir -p $(dir $@)
	@echo "    [ROM] $@"
	@dd if="$<" of="$@" bs=8192 count=1 2>/dev/null

filesystem/sd/%.v64: assets/sd/%.v64
	@mkdir -p $(dir $@)
	@echo "    [ROM] $@"
	@dd if="$<" of="$@" bs=8192 count=1 2>/dev/null

filesystem/%.db: assets/%.db
	@mkdir -p $(dir $@)
	@echo "    [BIN] $@"
	@cp "$<" "$@"

filesystem/%.txt: assets/%.txt
	@mkdir -p $(dir $@)
	@echo "    [TEXT] $@"
	@cp "$<" "$@"

filesystem/%.elf.stripped: assets/%.elf.stripped
	@mkdir -p $(dir $@)
	@echo "    [ELF] $@"
	@cp "$<" "$@"

objs = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(filter %.cpp,$(src))) \
       $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter %.c,$(src))) \
       $(patsubst %.S,$(BUILD_DIR)/%.o,$(filter %.S,$(src)))

$(BUILD_DIR)/$(PROJECT_NAME).dfs: $(assets_conv)
$(BUILD_DIR)/$(PROJECT_NAME).elf: $(objs)

$(PROJECT_NAME).z64: N64_ROM_TITLE="WonderMenu"
$(PROJECT_NAME).z64: $(BUILD_DIR)/$(PROJECT_NAME).dfs

rom-with-size: $(PROJECT_NAME).z64
	@python3 -c "import os,struct; p='$<'; s=os.path.getsize(p); \
	f=open(p,'r+b'); f.seek(0x18); f.write(struct.pack('>I', s)); f.close(); \
	print(f'    [SIZE] {s} bytes -> {p}')"

clean:
	rm -rf $(BUILD_DIR) *.z64
	rm -rf filesystem

release:
	rm -rf filesystem/sd
	$(MAKE) all assets_n64= assets_bin= BUILD_DIR=build/console EXTRA_DEFINES=-DRELEASE_BUILD PROJECT_NAME=$(PROJECT_NAME)-release

release-bundle:
	rm -rf filesystem/*.n64 filesystem/*.v64
	$(MAKE) all BUILD_DIR=build/console EXTRA_DEFINES=-DRELEASE_BUILD PROJECT_NAME=$(PROJECT_NAME)-release-bundle

build_lib:
	rm -rf $(BUILD_DIR) *.z64
	make -C $(T3D_INST)
	make all -j4

# Stage the prebuilt SC64 menu -- no compilation, just copies the WonderMenu ROM
# and its database into build/sc64 where the SC64 expects them.
SC64_DIR      = build/sc64
SC64_MENU_SRC = WonderMenu-release.z64
SC64_DB_SRC   = assets/sd/WonderMenu.db

sc64:
	mkdir -p $(SC64_DIR)
	cp -f $(SC64_MENU_SRC) $(SC64_DIR)/sc64menu.n64
	cp -f $(SC64_DB_SRC) $(SC64_DIR)/WonderMenu.db
	@echo "    [SC64] $(SC64_MENU_SRC) + $(SC64_DB_SRC) -> $(SC64_DIR)/"

-include $(objs:.o=.d)

.PHONY: all clean release release-bundle emu rom-with-size sc64
