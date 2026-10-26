NAME = uname
SRCS = src/main.cpp src/command.cpp tool.cpp
LIBC_SRCS = string/memcpy.cpp string/memset.cpp string/memcmp.cpp string/strlen.cpp stdio/printf.cpp stdio/putchar.cpp stdio/puts.cpp unistd/write.cpp

STREAM_CLIENT = yes

include ../../common.mk

ifdef VERSION_MAJOR
CXX_FLAGS += -DUNAME_VERSION_MAJOR=$(VERSION_MAJOR) -DUNAME_VERSION_MINOR=$(VERSION_MINOR) -DUNAME_VERSION_PATCH=$(VERSION_PATCH)
endif
