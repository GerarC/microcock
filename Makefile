AS            = nasm -felf32
CXX           = i686-elf-g++
CPP_FLAGS   = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti \
              -D__is_cock_kernel -D__is_libc -D__is_libk \
              -Ilibc/include -Ilibcxx/include -Ishared/include -I. \
              -fstack-protector-all -fno-use-cxa-atexit \
              -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
              -MMD -MP
TEST_FLAGS  = -DCOCK_TEST_MODE

LN_FLAGS      = -ffreestanding -O2 -nostdlib -lgcc
BUILD         = build
PROJECT        = cock

VERSION_MAJOR := 0
VERSION_MINOR := 0
VERSION_PATCH := 6
VERSION_STAGE := "\"Pre-alfa\""
GIT_HASH := "\"$(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)\""
BUILD_DATE := "\"$(shell date +%Y-%m-%d)\""

CPP_FLAGS += -DCOCK_VERSION_MAJOR=$(VERSION_MAJOR)
CPP_FLAGS += -DCOCK_VERSION_MINOR=$(VERSION_MINOR)
CPP_FLAGS += -DCOCK_VERSION_PATCH=$(VERSION_PATCH)
CPP_FLAGS += -DCOCK_VERSION_STAGE=$(VERSION_STAGE)
CPP_FLAGS += -DCOCK_GIT_HASH=$(GIT_HASH)
CPP_FLAGS += -DCOCK_BUILD_DATE=$(BUILD_DATE)

CPP_SOURCES  = $(shell find cock libc libcxx shared -name '*.cpp')
ASM_SOURCES  = $(shell find cock libc libcxx shared -name '*.s')

ifdef TEST_BUILD
CPP_FLAGS += $(TEST_FLAGS)
CPP_SOURCES += $(shell find test -name '*.cpp')
endif

CRTBEGIN_OBJ:= $(shell $(CXX) $(CPP_FLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ    := $(shell $(CXX) $(CPP_FLAGS) -print-file-name=crtend.o)

OBJ_CPP        = $(patsubst %.cpp,$(BUILD)/%.occ,$(CPP_SOURCES))
OBJ_ASM        = $(patsubst %.s,$(BUILD)/%.os,$(ASM_SOURCES))
OBJ_CORE    = $(OBJ_CPP) $(OBJ_ASM) 
OBJ            = $(OBJ_CORE) $(CRTBEGIN_OBJ) $(CRTEND_OBJ)
LINKER_I686 = cock/arch/x86/linker.ld

USERLAND_MAKEFILES := $(wildcard userland/*/Makefile)
USERLAND_PROJECTS  := $(patsubst %/Makefile,%,$(USERLAND_MAKEFILES))

all: $(BUILD)/$(PROJECT).bin

userland: $(USERLAND_PROJECTS)

$(USERLAND_PROJECTS):
	@echo "[USR] Building project: $@"
	@mkdir -p $(BUILD)/userland
	@$(MAKE) -C $@ OUT_DIR=$(abspath $(BUILD)/userland)

# Ensure build dir exists
$(BUILD):
	@echo "[BLD] creating $@"
	@mkdir -p $(BUILD)


$(BUILD)/%.os: %.s | $(BUILD)
	@echo "[AS ] $<"
	@mkdir -p $(dir $@)
	@$(AS) $< -o $@

$(BUILD)/%.occ: %.cpp | $(BUILD)
	@echo "[CXX] $<"
	@mkdir -p $(dir $@)
	@$(CXX) -c $< -o $@ $(CPP_FLAGS)

$(BUILD)/$(PROJECT).bin: $(OBJ) 
	@echo "[LD ] $@: $(OBJ)"
	@$(CXX) -T $(LINKER_I686) -o $@ $(LN_FLAGS) $(OBJ)

$(BUILD)/$(PROJECT).iso: $(BUILD)/$(PROJECT).bin userland
	@echo "[ISO] creating ISO"
	@mkdir -p build/isodir/boot/grub
	@cp $< build/isodir/boot/$(PROJECT).bin
	@echo menuentry "cock" { > build/isodir/boot/grub/grub.cfg
	@echo "    multiboot /boot/$(PROJECT).bin" >> build/isodir/boot/grub/grub.cfg
	@if ls build/userland/*.elf 1> /dev/null 2>&1; then \
		cp build/userland/*.elf build/isodir/boot/; \
		for elf in build/userland/*.elf; do \
			filename=$$(basename $$elf); \
			modname=$${filename%.*}; \
			echo "    module /boot/$$filename \"$$modname\"" >> build/isodir/boot/grub/grub.cfg; \
		done; \
	fi
	@echo } >> build/isodir/boot/grub/grub.cfg
	@grub-mkrescue -o build/cock.iso build/isodir


iso: $(BUILD)/$(PROJECT).iso

run: $(BUILD)/$(PROJECT).iso
	@echo "[RUN] Launching QEMU"
	@qemu-system-i386 -cdrom $<

test: clean
	@echo "[TST] Compiling and running in TEST MODE"
	@$(MAKE) run TEST_BUILD=1
	@$(MAKE) clean

clean:
	@echo "[CLN] Cleaning builder folder" 
	@rm -fr $(BUILD)

help:
	@echo "================================================================"
	@echo " Micro$(PROJECT) - Build System"
	@echo "================================================================"
	@echo "Available commands:"
	@echo "  make        - Compile the kernel and generate the binary file"
	@echo "  make iso    - Build the bootable ISO image with GRUB"
	@echo "  make run    - Build the ISO and launch it using QEMU"
	@echo "  make test   - Clean, compile with test flags, and run in QEMU"
	@echo "  make clean  - Remove the build directory and all compiled files"
	@echo "  make help   - Show this help message"
	@echo "================================================================"

.PHONY: all clean iso run test make userland $(USERLAND_PROJECTS)
 
-include $(OBJ_CPP:.occ=.d)
