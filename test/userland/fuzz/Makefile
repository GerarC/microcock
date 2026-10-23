NAME = fuzz
SRCS = main.cpp scenarios.cpp
CONSOLE_CLIENT = no
LIBC_SRCS = string/memcpy.cpp string/memset.cpp string/strlen.cpp string/strcmp.cpp stdio/printf.cpp stdio/putchar.cpp stdio/puts.cpp unistd/write.cpp sys/mman/mmap.cpp sys/mman/munmap.cpp

include ../../../userland/common.mk

ifdef FUZZ_SEED
CXX_FLAGS += -DFUZZ_SEED=$(FUZZ_SEED)
endif
ifdef FUZZ_ROUNDS
CXX_FLAGS += -DFUZZ_ROUNDS=$(FUZZ_ROUNDS)
endif
ifdef FUZZ_TRACE
CXX_FLAGS += -DFUZZ_TRACE
endif
ifeq ($(FUZZ),root)
CXX_FLAGS += -DFUZZ_ROOT
endif
