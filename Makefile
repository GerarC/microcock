AS			= nasm -felf32
CXX			= i686-elf-g++
CPP_FLAGS   = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti \
              -D__is_cock_kernel -D__is_libc -D__is_libk \
              -Ilibc/include -Ikernel/include \
              -fstack-protector-all -fno-use-cxa-atexit \
              -mno-sse -mno-sse2 -mno-mmx -mno-80387 \
              -MMD -MP
LN_FLAGS  	= -ffreestanding -O2 -nostdlib -lgcc
BUILD 		= build
PROJECT		= cock

CPP_SOURCES  = $(shell find * -name '*.cpp')
ASM_SOURCES  = $(shell find * -name '*.s')
CRTBEGIN_OBJ:=$(shell $(CXX) $(CPP_FLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CXX) $(CPP_FLAGS) -print-file-name=crtend.o)

# Convert to objects in build/
OBJ_CPP		= $(patsubst %.cpp,$(BUILD)/%.occ,$(CPP_SOURCES))
OBJ_ASM		= $(patsubst %.s,$(BUILD)/%.os,$(ASM_SOURCES))
OBJ_CORE	= $(OBJ_CPP) $(OBJ_ASM) 
OBJ    		= $(OBJ_CORE) $(CRTBEGIN_OBJ) $(CRTEND_OBJ)
LINKER_I686 = kernel/arch/x86/linker.ld

all: $(BUILD)/$(PROJECT).bin

# Ensure build dir exists
$(BUILD):
	@echo "[BLD]  creating $@"
	@mkdir -p $(BUILD)


$(BUILD)/%.os: %.s | $(BUILD)
	@echo "[AS ]  $<"
	@mkdir -p $(dir $@)
	@$(AS) $< -o $@

$(BUILD)/%.occ: %.cpp | $(BUILD)
	@echo "[CXX]  $<"
	@mkdir -p $(dir $@)
	@$(CXX) -c $< -o $@ $(CPP_FLAGS)

$(BUILD)/$(PROJECT).bin: $(OBJ) 
	@echo "[LD ]  $@: $(OBJ)"
	@$(CXX) -T $(LINKER_I686) -o $@ $(LN_FLAGS) $(OBJ)

$(BUILD)/$(PROJECT).iso: $(BUILD)/$(PROJECT).bin
	@echo "[ISO] creating ISO"
	@sh kernel/arch/x86/make_iso.sh

iso: $(BUILD)/$(PROJECT).iso

run: $(BUILD)/$(PROJECT).iso
	@echo "[RUN] Launching QEMU"
	@qemu-system-i386 -cdrom $<


clean:
	@echo "[CLN] Cleaning builder folder" 
	@rm -fr $(BUILD)

.PHONY: all clean iso run
 
-include $(OBJ_CPP:.occ=.d)
