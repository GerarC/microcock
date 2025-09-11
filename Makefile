AS       = nasm

BUILD = ./build

CPP_SOURCES  = $(shell find * -name '*.cpp')
ASM_SOURCES  = $(shell find * -name '*.s')

# Convert to objects in build/
OBJ_CPP = $(patsubst %.cpp,$(BUILD)/%.o,$(CPP_SOURCES))
OBJ_ASM = $(patsubst %.s,$(BUILD)/%.o,$(ASM_SOURCES))
OBJ     = $(OBJ_CPP) $(OBJ_ASM)

all: $(BUILD)/cock.elf

# Ensure build dir exists
$(BUILD):
	mkdir -p $(BUILD)

# Compile C++ sources
$(BUILD)/%.o: %.cpp | $(BUILD)
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -c $< -o $@

# Assemble .s files
$(BUILD)/%.o: %.s | $(BUILD)
	@mkdir -p $(dir $@)
	$(AS) $< -o $@

# Link final ELF
$(BUILD)/cock.elf: $(OBJ)
	$(LD) $(LDFLAGS) $^ -o $@

# Run in QEMU
run: $(BUILD)/cock.elf
	qemu-system-x86_64 -cdrom $< -serial stdio -display gtk

debug: $(BUILD)/cock.elf
	qemu-system-x86_64 -cdrom $< -S -s

# Build ISO with GRUB
$(BUILD)/cock.iso: $(BUILD)/cock.elf
	mkdir -p iso/boot/grub
	cp $(BUILD)/cock.elf iso/boot/
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "Microcock Kernel" { multiboot2 /boot/cock.elf; boot }' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/cock.iso iso

purge:
	rm -r build/*

clean:
	rm -rf $(BUILD) iso

