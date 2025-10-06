AS			= nasm -felf32
CXX			= i686-elf-g++
CPP_FLAGS 	= -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -D__is_kernel -D__is_libc -D__is_libk -Ilibc/include -Icock/include
LN_FLAGS  	= -ffreestanding -O2 -nostdlib 
BUILD 		= build
PROJECT		= cock

CPP_SOURCES  = $(shell find * -name '*.cpp')
ASM_SOURCES  = $(shell find * -name '*.s')

# Convert to objects in build/
OBJ_CPP		= $(patsubst %.cpp,$(BUILD)/%.occ,$(CPP_SOURCES))
OBJ_ASM		= $(patsubst %.s,$(BUILD)/%.os,$(ASM_SOURCES))
OBJ    		= $(OBJ_CPP) $(OBJ_ASM)
LINKER_I686 = cock/arch/i386/linker.ld

all: $(BUILD)/$(PROJECT).bin

# Ensure build dir exists
$(BUILD):
	mkdir -p $(BUILD)


$(BUILD)/%.os: %.s | $(BUILD)
	@mkdir -p $(dir $@)
	$(AS) $< -o $@

$(BUILD)/%.occ: %.cpp | $(BUILD)
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CPP_FLAGS)

$(BUILD)/$(PROJECT).bin: $(OBJ) 
	$(CXX) -T $(LINKER_I686) -o $@ $(LN_FLAGS) $(OBJ) -lgcc

$(BUILD)/$(PROJECT).iso: $(BUILD)/$(PROJECT).bin
	sh cock/arch/i386/make_iso.sh
	

run: $(BUILD)/$(PROJECT).iso
	qemu-system-i386 -cdrom $<

clean:
	rm -r $(BUILD)
