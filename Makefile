AS			= nasm -felf32
CXX			= i686-elf-g++
CPP_FLAGS   = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti \
              -D__is_cock_kernel -D__is_libc -D__is_libk \
              -Ilibc/include -Ilibcxx/include -I. \
              -fstack-protector-all -fno-use-cxa-atexit \
              -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
              -MMD -MP
LN_FLAGS  	= -ffreestanding -O2 -nostdlib -lgcc
BUILD 		= build
PROJECT		= cock

VERSION_MAJOR := 0
VERSION_MINOR := 0
VERSION_PATCH := 3
VERSION_STAGE := "\"Pre-alfa\""
GIT_HASH := "\"$(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)\""
BUILD_DATE := "\"$(shell date +%Y-%m-%d)\""

CPP_FLAGS += -DCOCK_VERSION_MAJOR=$(VERSION_MAJOR)
CPP_FLAGS += -DCOCK_VERSION_MINOR=$(VERSION_MINOR)
CPP_FLAGS += -DCOCK_VERSION_PATCH=$(VERSION_PATCH)
CPP_FLAGS += -DCOCK_VERSION_STAGE=$(VERSION_STAGE)
CPP_FLAGS += -DCOCK_GIT_HASH=$(GIT_HASH)
CPP_FLAGS += -DCOCK_BUILD_DATE=$(BUILD_DATE)

CPP_SOURCES  = $(shell find * -name '*.cpp')
ASM_SOURCES  = $(shell find * -name '*.s')
CRTBEGIN_OBJ:= $(shell $(CXX) $(CPP_FLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ	:= $(shell $(CXX) $(CPP_FLAGS) -print-file-name=crtend.o)

# Convert to objects in build/
OBJ_CPP		= $(patsubst %.cpp,$(BUILD)/%.occ,$(CPP_SOURCES))
OBJ_ASM		= $(patsubst %.s,$(BUILD)/%.os,$(ASM_SOURCES))
OBJ_CORE	= $(OBJ_CPP) $(OBJ_ASM) 
OBJ    		= $(OBJ_CORE) $(CRTBEGIN_OBJ) $(CRTEND_OBJ)
LINKER_I686 = cock/arch/x86/linker.ld

all: $(BUILD)/$(PROJECT).bin

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

$(BUILD)/$(PROJECT).iso: $(BUILD)/$(PROJECT).bin
	@echo "[ISO] creating ISO"
	@mkdir -p build/isodir/boot/grub
	@cp $< build/isodir/boot/$(PROJECT).bin
	@echo menuentry "cock" { > build/isodir/boot/grub/grub.cfg
	@echo "    multiboot /boot/$(PROJECT).bin" >> build/isodir/boot/grub/grub.cfg
	@echo } >> build/isodir/boot/grub/grub.cfg
	@grub-mkrescue -o build/cock.iso build/isodir

iso: $(BUILD)/$(PROJECT).iso

run: $(BUILD)/$(PROJECT).iso
	@echo "[RUN] Launching QEMU"
	@qemu-system-i386 -cdrom $<


clean:
	@echo "[CLN] Cleaning builder folder" 
	@rm -fr $(BUILD)

.PHONY: all clean iso run
 
-include $(OBJ_CPP:.occ=.d)
