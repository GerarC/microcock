ARCH          = x86
AS            = nasm -felf32
QEMU          = qemu-system-i386

ifdef HOST_TOOLCHAIN
CXX           = g++ -m32 -U__linux__ -fno-pie -no-pie -fno-asynchronous-unwind-tables -mstack-protector-guard=global
LD            = ld -m elf_i386
else
CXX           = i686-elf-g++
LD            = i686-elf-ld
endif

CPP_FLAGS   = -std=gnu++20 -ffreestanding -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
              -D__is_cock_kernel -D__is_libc -D__is_libk \
              -Ilibc/include -Ilibcxx/include -Ishared/include -I. \
              -Icock/arch/$(ARCH)/include \
              -fstack-protector-all -fno-use-cxa-atexit \
              -fno-tree-loop-distribute-patterns \
              -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
              -MMD -MP
TEST_FLAGS  = -DCOCK_TEST_MODE
LOG_LEVEL  ?= TRACE
CPP_FLAGS  += -DCOCK_LOG_LEVEL=::cock::utils::LogLevel::$(LOG_LEVEL)

ifdef USE_LTO
CPP_FLAGS += -flto
endif

ifdef SCHED_METRICS
CPP_FLAGS += -DCOCK_SCHED_METRICS
endif

LN_FLAGS      = -ffreestanding -O2 -nostdlib
LN_LIBS       = -lgcc
BUILD         = build
PROJECT        = cock

QEMU_DEBUG_EXIT = -device isa-debug-exit,iobase=0xf4,iosize=0x04
QEMU_HEADLESS   = -display none -serial stdio -no-reboot $(QEMU_DEBUG_EXIT)

VERSION_MAJOR := 0
VERSION_MINOR := 2
VERSION_PATCH := 8
VERSION_STAGE := "\"Pre-alfa\""
export VERSION_MAJOR VERSION_MINOR VERSION_PATCH
GIT_HASH := "\"$(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)\""
BUILD_DATE := "\"$(shell date +%Y-%m-%d)\""

CPP_FLAGS += -DCOCK_VERSION_MAJOR=$(VERSION_MAJOR)
CPP_FLAGS += -DCOCK_VERSION_MINOR=$(VERSION_MINOR)
CPP_FLAGS += -DCOCK_VERSION_PATCH=$(VERSION_PATCH)
CPP_FLAGS += -DCOCK_VERSION_STAGE=$(VERSION_STAGE)
CPP_FLAGS += -DCOCK_GIT_HASH=$(GIT_HASH)
CPP_FLAGS += -DCOCK_BUILD_DATE=$(BUILD_DATE)

CPP_SOURCES  = $(shell find cock libc libcxx shared -name '*.cpp' -not -path 'libc/unistd/*' -not -path 'libc/sys/*' -not -path 'libc/stdlib/malloc.cpp' -not -path 'libcxx/memory/user_new.cpp')
ASM_SOURCES  = $(shell find cock libc libcxx shared -name '*.s')

ifdef TEST_BUILD
CPP_FLAGS += $(TEST_FLAGS)
CPP_SOURCES += $(shell find test -name '*.cpp' -not -path 'test/host/*' -not -path 'test/userland/*')
endif

CRTBEGIN_OBJ:= $(shell $(CXX) $(CPP_FLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ    := $(shell $(CXX) $(CPP_FLAGS) -print-file-name=crtend.o)

OBJ_CPP        = $(patsubst %.cpp,$(BUILD)/%.occ,$(CPP_SOURCES))
OBJ_ASM        = $(patsubst %.s,$(BUILD)/%.os,$(ASM_SOURCES))
OBJ_CORE    = $(OBJ_CPP) $(OBJ_ASM)
OBJ            = $(OBJ_CORE) $(CRTBEGIN_OBJ) $(CRTEND_OBJ)
LINKER_I686 = cock/arch/x86/linker.ld

USERLAND_BOOT_ORDER = init ramfs vga_driver
INITRD_STAGE = $(BUILD)/initrd_root
INITRD_PROGRAMS = bin
INITRD_DIR = initrd
INITRD_FILES := $(shell find $(INITRD_DIR) -type f)
INITRD_ENTRIES := $(notdir $(wildcard $(INITRD_DIR)/*))
INITRD_IMAGE = $(BUILD)/initrd.tar
INITRD_MODULE = initrd
TEST_INITRD_DIR = test/initrd
ifdef TEST_INITRD
USERLAND_MAKEFILES := $(wildcard userland/*/Makefile test/userland/*/Makefile)
INITRD_FILES += $(shell find $(TEST_INITRD_DIR) -type f)
else
USERLAND_MAKEFILES := $(wildcard userland/*/Makefile)
endif
USERLAND_PROJECTS  := $(patsubst %/Makefile,%,$(USERLAND_MAKEFILES))
USERLAND_PROGRAMS  := $(notdir $(USERLAND_PROJECTS))

all: $(BUILD)/$(PROJECT).bin

userland: $(USERLAND_PROJECTS)

$(USERLAND_PROJECTS):
	@echo "[USR] Building project: $@"
	@mkdir -p $(BUILD)/userland
	@$(MAKE) -C $@ OUT_DIR=$(abspath $(BUILD)/userland) USER_CXX="$(CXX)" USER_LD="$(LD)" USER_AS="$(AS)"

# Ensure build dir exists
$(BUILD):
	@echo "[BLD] creating $@"
	@mkdir -p $(BUILD)

BUILD_FLAGS_FILE = $(BUILD)/.build_flags
BUILD_FLAGS_SIGNATURE = $(CXX) $(CPP_FLAGS)

.PHONY: FORCE
FORCE:

$(BUILD_FLAGS_FILE): FORCE | $(BUILD)
	@current="$(BUILD_FLAGS_SIGNATURE)"; \
	if [ ! -f $@ ] || [ "$$(cat $@ 2> /dev/null)" != "$$current" ]; then \
		echo "$$current" > $@; \
	fi

$(BUILD)/%.os: %.s | $(BUILD)
	@echo "[AS ] $<"
	@mkdir -p $(dir $@)
	@$(AS) $< -o $@

$(BUILD)/%.occ: %.cpp $(BUILD_FLAGS_FILE) | $(BUILD)
	@echo "[CXX] $<"
	@mkdir -p $(dir $@)
	@$(CXX) -c $< -o $@ $(CPP_FLAGS)

$(BUILD)/$(PROJECT).bin: $(OBJ)
	@echo "[LD ] $@"
	@$(CXX) -T $(LINKER_I686) -o $@ $(LN_FLAGS) $(OBJ) $(LN_LIBS)

$(INITRD_IMAGE): $(INITRD_FILES) userland | $(BUILD)
	@echo "[TAR] $@"
	@rm -rf $(INITRD_STAGE)
	@mkdir -p $(INITRD_STAGE)/$(INITRD_PROGRAMS)
	@cp -r $(INITRD_DIR)/. $(INITRD_STAGE)/
	@if [ -n "$(TEST_INITRD)" ]; then \
		cp -r $(TEST_INITRD_DIR)/. $(INITRD_STAGE)/; \
		for file in manifest.conf mounts.conf; do \
			cat $(INITRD_DIR)/$$file $(TEST_INITRD_DIR)/$$file > $(INITRD_STAGE)/$$file; \
		done; \
	fi
	@if [ "$(FUZZ)" = user ]; then \
		echo "fuzz path=bin/fuzz.elf" >> $(INITRD_STAGE)/manifest.conf; \
	fi
	@for name in $(USERLAND_PROGRAMS); do \
		case " $(USERLAND_BOOT_ORDER) " in \
			*" $$name "*) ;; \
			*) cp $(BUILD)/userland/$$name.elf $(INITRD_STAGE)/$(INITRD_PROGRAMS)/ ;; \
		esac; \
	done
	@cd $(INITRD_STAGE) && tar --format=ustar --owner=root --group=root --numeric-owner -cf $(abspath $@) *

$(BUILD)/$(PROJECT).iso: $(BUILD)/$(PROJECT).bin userland $(INITRD_IMAGE)
	@echo "[ISO] creating ISO"
	@mkdir -p build/isodir/boot/grub
	@cp $< build/isodir/boot/$(PROJECT).bin
	@echo "set timeout=0" > build/isodir/boot/grub/grub.cfg
	@echo "set default=0" >> build/isodir/boot/grub/grub.cfg
	@echo 'menuentry "cock" {' >> build/isodir/boot/grub/grub.cfg
	@echo "    multiboot /boot/$(PROJECT).bin" >> build/isodir/boot/grub/grub.cfg
	@if ls build/userland/*.elf 1> /dev/null 2>&1; then \
		for name in $(USERLAND_BOOT_ORDER); do \
			source=$$name; \
			if [ "$(FUZZ)" = root ] && [ "$$name" = init ]; then source=fuzz; fi; \
			if [ -f build/userland/$$source.elf ]; then \
				cp build/userland/$$source.elf build/isodir/boot/$$name.elf; \
				echo "    module /boot/$$name.elf \"$$name\"" >> build/isodir/boot/grub/grub.cfg; \
			fi; \
		done; \
	fi
	@cp $(INITRD_IMAGE) build/isodir/boot/$(INITRD_MODULE).tar
	@echo "    module /boot/$(INITRD_MODULE).tar \"$(INITRD_MODULE)\"" >> build/isodir/boot/grub/grub.cfg

	@echo '}' >> build/isodir/boot/grub/grub.cfg
	@grub-mkrescue -o build/cock.iso build/isodir 2> /dev/null


iso: $(BUILD)/$(PROJECT).iso

DISK_SCHEME ?= mbr
DISK_IMAGE = $(BUILD)/disk.img
ifdef DISK
QEMU_DISK = -boot d -drive file=$(DISK),format=raw,if=ide,index=0,media=disk
endif

disk: | $(BUILD)
	@echo "[DSK] $(DISK_IMAGE) ($(DISK_SCHEME))"
	@bash scripts/make_test_disk.sh $(DISK_SCHEME) $(DISK_IMAGE)

run: $(BUILD)/$(PROJECT).iso
	@echo "[RUN] Launching QEMU"
	@$(QEMU) -cdrom $< -serial stdio $(QEMU_DISK)

run-headless: $(BUILD)/$(PROJECT).iso
	@$(QEMU) -cdrom $< $(QEMU_HEADLESS) $(QEMU_DISK)

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
	@echo "  make              - Compile the kernel and generate the binary file"
	@echo "  make iso          - Build the bootable ISO image with GRUB"
	@echo "  make run          - Build the ISO and launch it using QEMU"
	@echo "  make disk         - Build build/disk.img (DISK_SCHEME=mbr|gpt), a disk with FAT32 and ext2 volumes"
	@echo "  make run-headless - Same as run, without a window (serial on stdout)"
	@echo "  make test         - Clean, compile with test flags, and run in QEMU"
	@echo "  make clean        - Remove the build directory and all compiled files"
	@echo "  make help         - Show this help message"
	@echo "Options:"
	@echo "  HOST_TOOLCHAIN=1  - Use the host g++/ld (-m32) instead of i686-elf-*"
	@echo "  TEST_BUILD=1      - Build the in-kernel test suite"
	@echo "  USE_LTO=1         - Enable link time optimization"
	@echo "  SCHED_METRICS=1   - Record response, CPU and turnaround ticks per thread (SYS_SCHED_METRICS); off by default"
	@echo "  DISK=<image>      - With run or run-headless, attach the image as the first IDE disk (see make disk)"
	@echo "  FUZZ=user|root    - With TEST_INITRD=1, boot the syscall fuzzer (FUZZ_SEED, FUZZ_ROUNDS, FUZZ_TRACE)"
	@echo "  LOG_LEVEL=<level> - Lowest kernel log level compiled in: TRACE, DEBUG, INFO, WARN, ERROR, FATAL"
	@echo "================================================================"

.PHONY: all clean iso disk run run-headless test make userland $(USERLAND_PROJECTS) FORCE

-include $(OBJ_CPP:.occ=.d)
