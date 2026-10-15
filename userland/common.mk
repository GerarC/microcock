OUT_DIR ?= .
USER_CXX ?= i686-elf-g++
USER_LD ?= i686-elf-ld

USERLAND_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))
LIBC := $(USERLAND_ROOT)../libc
LINKER_SCRIPT := $(USERLAND_ROOT)linker.ld

INCLUDE_FLAGS = -I$(LIBC)/include -I$(USERLAND_ROOT)../shared/include -I$(USERLAND_ROOT)lib/include -I.
CXX_FLAGS = -std=gnu++20 -ffreestanding -nostdlib -fno-exceptions -fno-rtti \
            -fno-stack-protector -fno-pie -fno-tree-loop-distribute-patterns \
            -fno-asynchronous-unwind-tables -Wall -Wextra $(INCLUDE_FLAGS) -O2

TARGET = $(OUT_DIR)/$(NAME).elf
OBJ = $(addprefix $(OUT_DIR)/$(NAME)_,$(addsuffix .o,$(notdir $(basename $(SRCS) $(LIBC_SRCS)))))

vpath %.cpp . $(addprefix $(LIBC)/,$(dir $(LIBC_SRCS)))

all: $(TARGET)

$(TARGET): $(OBJ) $(LINKER_SCRIPT)
	@echo "  [LD ] $@"
	@$(USER_LD) -m elf_i386 -T $(LINKER_SCRIPT) $(OBJ) -o $@

$(OUT_DIR)/$(NAME)_%.o: %.cpp
	@echo "  [CXX] $<"
	@$(USER_CXX) $(CXX_FLAGS) -MMD -MP -c $< -o $@

clean:
	rm -f $(OBJ) $(OBJ:.o=.d) $(TARGET)

.PHONY: all clean

-include $(OBJ:.o=.d)
