OUT_DIR ?= .

TARGET = $(OUT_DIR)/hello.elf
OBJ    = $(OUT_DIR)/hello.o

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo "  [LD ] $@"
	@i686-elf-ld -m elf_i386 $< -o $@

$(OUT_DIR)/hello.o: main.s
	@echo "  [ASM] $<"
	@nasm -f elf32 $< -o $@
