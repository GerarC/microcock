OUT_DIR ?= .
USER_AS ?= nasm -felf32
USER_LD ?= i686-elf-ld

TARGET = $(OUT_DIR)/hello.elf
OBJ    = $(OUT_DIR)/hello.o

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo "  [LD ] $@"
	@$(USER_LD) -m elf_i386 $< -o $@

$(OUT_DIR)/hello.o: main.s
	@echo "  [ASM] $<"
	@$(USER_AS) $< -o $@
