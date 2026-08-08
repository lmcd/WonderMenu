# WonderMenu -- top-level build
#
# Builds the two sub-projects in dependency order:
#
#   payload/  -- the Expansion Pak resident payload (payload.z64 + a stripped
#                ELF that the menu loads in place at runtime)
#   menu/     -- the menu ROM itself (WonderMenu.z64)
#
# The menu embeds the payload's stripped ELF as a filesystem asset (see
# ELFFile / IntroScene), so the payload must be built first and its output
# copied into menu/assets/ before the menu's DFS is packed.

N64_INST ?= /opt/libdragon
export N64_INST

PAYLOAD_ELF      = payload/build/payload.elf.stripped
MENU_PAYLOAD_ELF = menu/assets/payload.elf.stripped

# The payload is built from a sibling checkout that isn't always present. When
# it's missing, build the menu against whatever payload.elf.stripped is already
# in menu/assets/ rather than failing on a directory we can't enter.
HAVE_PAYLOAD := $(wildcard payload/Makefile)

ifeq ($(HAVE_PAYLOAD),)
MENU_PAYLOAD_DEP =
else
MENU_PAYLOAD_DEP = $(MENU_PAYLOAD_ELF)
endif

all: menu
.PHONY: all

# --- payload ---------------------------------------------------------------

payload:
ifeq ($(HAVE_PAYLOAD),)
	@echo "==> payload (skipped -- payload/ not found)"
else
	@echo "==> payload"
	$(MAKE) -C payload
endif
.PHONY: payload

# Phony prerequisite, so the payload's own Makefile always gets a chance to
# decide whether the ELF is out of date -- we don't second-guess it here.
$(PAYLOAD_ELF): payload

# Only touch the copy in menu/assets when the bytes actually differ. Otherwise
# every top-level build would give the menu a newer asset mtime and force a
# full DFS repack for no reason.
$(MENU_PAYLOAD_ELF): $(PAYLOAD_ELF)
	@cmp -s "$<" "$@" || { cp -f "$<" "$@" && echo "    [PAYLOAD] $< -> $@"; }

# --- menu ------------------------------------------------------------------

menu: $(MENU_PAYLOAD_DEP)
	@echo "==> menu"
	$(MAKE) -C menu
.PHONY: menu

# Pass-throughs to menu/, but gated on a current payload asset first, so a
# release ROM can never embed a stale payload.elf.stripped. `deploy` picks
# this up transitively via its `release` prerequisite.
release release-bundle sc64: $(MENU_PAYLOAD_DEP)
	$(MAKE) -C menu $@
.PHONY: release release-bundle sc64

# --- hardware --------------------------------------------------------------

SC64DEPLOYER     ?= sc64deployer
MENU_RELEASE_ROM  = menu/WonderMenu-release.z64

# Build the release ROM, then push it to a connected SummerCart64.
deploy: release
	@command -v $(SC64DEPLOYER) >/dev/null || { echo "deploy: $(SC64DEPLOYER) not found in PATH"; exit 1; }
	@echo "    [SC64] upload $(MENU_RELEASE_ROM)"
	$(SC64DEPLOYER) upload "$(CURDIR)/$(MENU_RELEASE_ROM)"
	$(SC64DEPLOYER) debug
.PHONY: deploy

# --- emulator --------------------------------------------------------------

ARES    ?= /Applications/ares.app/Contents/MacOS/ares
MENU_ROM = menu/WonderMenu.z64

# Build the ROM, then boot it in ares. Invoked via the binary inside the bundle
# rather than `open -a` so that ares takes the ROM path as an argument and its
# stdout/stderr stay attached to this terminal.
launch-emu: all
	@test -x "$(ARES)" || { echo "launch-emu: ares not found at $(ARES)"; exit 1; }
	@echo "    [ARES] $(MENU_ROM)"
	"$(ARES)" "$(CURDIR)/$(MENU_ROM)"
.PHONY: launch-emu

# --- housekeeping ----------------------------------------------------------

clean:
ifneq ($(HAVE_PAYLOAD),)
	$(MAKE) -C payload clean
endif
	$(MAKE) -C menu clean
.PHONY: clean
