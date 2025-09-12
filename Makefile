AS       = nasm

BUILD 		= build
PROJECT		= cock

CPP_SOURCES  = $(shell find * -name '*.cpp')
ASM_SOURCES  = $(shell find * -name '*.s')

# Convert to objects in build/
OBJ_CPP = $(patsubst %.cpp,$(BUILD)/%.o,$(CPP_SOURCES))
OBJ_ASM = $(patsubst %.s,$(BUILD)/%.o,$(ASM_SOURCES))
OBJ     = $(OBJ_CPP) $(OBJ_ASM)
BIN		= $(patsubst %.s,$(BUILD)/%.bin,$(ASM_SOURCES))

all: $(BUILD)/$(PROJECT).img

# Ensure build dir exists
$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.bin: %.s | $(BUILD)
	@mkdir -p $(dir $@)
	$(AS) $< -f bin -o $@

$(BUILD)/$(PROJECT).img: $(BIN) 
	dd if=/dev/zero of=$@ bs=512 count=2880
	mkfs.fat -F 12 -n "NBOS" $@
	dd if=$(BUILD)/arch/i386/boot/entry.bin of=$@ conv=notrunc
	mcopy -i $@ $(BUILD)/kernel/core/cock.bin "::cock.bin"

run: $(BUILD)/cock.img
	qemu-system-i386 -fda $<

clean:
	rm -r $(BUILD)
