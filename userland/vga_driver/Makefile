OUT_DIR ?= .
TARGET = $(OUT_DIR)/vga_driver.elf

SRCS = vga.cpp main.cpp
LIBC_SRCS = ../../libc/sys/mman/mmap.cpp ../../libc/string/memcpy.cpp ../../libc/string/memset.cpp

OBJ = $(patsubst %.cpp,$(OUT_DIR)/%.o,$(notdir $(SRCS) $(LIBC_SRCS)))

INCLUDE_FLAGS = -I../../libc/include -I../../shared/include -I.
CXX = i686-elf-g++
LD  = i686-elf-ld
CXX_FLAGS = -m32 -ffreestanding -nostdlib -fno-exceptions -fno-rtti $(INCLUDE_FLAGS) -O2

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo "  [LD ] $@"
	@$(LD) -m elf_i386 -T linker.ld $(OBJ) -o $@

$(OUT_DIR)/%.o: %.cpp
	@echo "  [CXX] $<"
	@$(CXX) $(CXX_FLAGS) -c $< -o $@

$(OUT_DIR)/mmap.o: ../../libc/sys/mman/mmap.cpp
	@$(CXX) $(CXX_FLAGS) -c $< -o $@

$(OUT_DIR)/memcpy.o: ../../libc/string/memcpy.cpp
	@$(CXX) $(CXX_FLAGS) -c $< -o $@

$(OUT_DIR)/memset.o: ../../libc/string/memset.cpp
	@$(CXX) $(CXX_FLAGS) -c $< -o $@

clean:
	rm -f $(OUT_DIR)/*.o $(TARGET)
