OUT_DIR ?= .
USER_CXX ?= i686-elf-g++
USER_LD ?= i686-elf-ld
USER_EXTRA_FLAGS ?=

USERLAND_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))
LIBC := $(USERLAND_ROOT)../libc
LIBCXX := $(USERLAND_ROOT)../libcxx
LINKER_SCRIPT := $(USERLAND_ROOT)linker.ld

INCLUDE_FLAGS = -I$(LIBC)/include -I$(USERLAND_ROOT)../shared/include -I$(USERLAND_ROOT)lib/include -I$(USERLAND_ROOT)../test/userland/lib/include -I.
CXX_FLAGS = -std=gnu++20 -ffreestanding -nostdlib -fno-exceptions -fno-rtti \
            -fno-stack-protector -fno-pie -fno-tree-loop-distribute-patterns \
            -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-80387 -Wall -Wextra -Werror $(INCLUDE_FLAGS) -O2 \
            $(USER_EXTRA_FLAGS)

CONSOLE_CLIENT ?= yes
CONSOLE_SRCS = unistd/read.cpp unistd/console.cpp
ifeq ($(CONSOLE_CLIENT),yes)
LIBC_SRCS := $(LIBC_SRCS) $(filter-out $(LIBC_SRCS),$(CONSOLE_SRCS))
endif

TARGET = $(OUT_DIR)/$(NAME).elf
OBJ = $(addprefix $(OUT_DIR)/$(NAME)_,$(addsuffix .o,$(notdir $(basename $(SRCS) $(LIBC_SRCS) $(LIBCXX_SRCS)))))

vpath %.cpp . $(addprefix $(LIBC)/,$(dir $(LIBC_SRCS))) $(addprefix $(LIBCXX)/,$(dir $(LIBCXX_SRCS)))

all: $(TARGET)

BUILD_FLAGS_FILE = $(OUT_DIR)/.$(NAME)_build_flags
BUILD_FLAGS_SIGNATURE = $(USER_CXX) $(CXX_FLAGS)

.PHONY: FORCE
FORCE:

$(BUILD_FLAGS_FILE): FORCE
	@current="$(BUILD_FLAGS_SIGNATURE)"; \
	if [ ! -f $@ ] || [ "$$(cat $@ 2> /dev/null)" != "$$current" ]; then \
		echo "$$current" > $@; \
	fi

$(TARGET): $(OBJ) $(LINKER_SCRIPT)
	@echo "  [LD ] $@"
	@$(USER_LD) -m elf_i386 -T $(LINKER_SCRIPT) $(OBJ) -o $@

$(OUT_DIR)/$(NAME)_%.o: %.cpp $(BUILD_FLAGS_FILE)
	@echo "  [CXX] $<"
	@$(USER_CXX) $(CXX_FLAGS) -MMD -MP -c $< -o $@

clean:
	rm -f $(OBJ) $(OBJ:.o=.d) $(TARGET)

.PHONY: all clean FORCE

-include $(OBJ:.o=.d)
