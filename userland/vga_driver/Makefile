OUT_DIR ?= .
USER_CXX ?= i686-elf-g++
USER_LD ?= i686-elf-ld

TARGET = $(OUT_DIR)/vga_driver.elf

LIBC = ../../libc
SRCS = vga.cpp main.cpp
LIBC_SRCS = $(LIBC)/sys/mman/mmap.cpp $(LIBC)/string/memcpy.cpp $(LIBC)/string/memset.cpp

OBJ = $(addprefix $(OUT_DIR)/vga_driver_,$(addsuffix .o,$(notdir $(basename $(SRCS) $(LIBC_SRCS)))))

INCLUDE_FLAGS = -I$(LIBC)/include -I../../shared/include -I.
CXX_FLAGS = -std=gnu++20 -ffreestanding -nostdlib -fno-exceptions -fno-rtti -fno-stack-protector -fno-pie -fno-asynchronous-unwind-tables $(INCLUDE_FLAGS) -O2

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo "  [LD ] $@"
	@$(USER_LD) -m elf_i386 -T linker.ld $(OBJ) -o $@

$(OUT_DIR)/vga_driver_%.o: %.cpp
	@echo "  [CXX] $<"
	@$(USER_CXX) $(CXX_FLAGS) -c $< -o $@

$(OUT_DIR)/vga_driver_%.o: $(LIBC)/sys/mman/%.cpp
	@$(USER_CXX) $(CXX_FLAGS) -c $< -o $@

$(OUT_DIR)/vga_driver_%.o: $(LIBC)/string/%.cpp
	@$(USER_CXX) $(CXX_FLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
