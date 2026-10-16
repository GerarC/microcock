NAME = sh
SRCS = main.cpp
LIBC_SRCS = string/memcpy.cpp string/memset.cpp string/strlen.cpp stdio/printf.cpp stdio/putchar.cpp stdio/puts.cpp unistd/write.cpp sys/mman/munmap.cpp

include ../common.mk

ifdef VERSION_MAJOR
CXX_FLAGS += -DSH_VERSION_MAJOR=$(VERSION_MAJOR) -DSH_VERSION_MINOR=$(VERSION_MINOR) -DSH_VERSION_PATCH=$(VERSION_PATCH)
endif
