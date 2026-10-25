NAME = sh
SRCS = src/main.cpp src/input.cpp src/commands.cpp src/directory_commands.cpp src/exit_command.cpp src/paths.cpp src/manager.cpp src/program_commands.cpp
LIBC_SRCS = string/memcpy.cpp string/memset.cpp string/memcmp.cpp string/strlen.cpp stdio/printf.cpp stdio/putchar.cpp stdio/puts.cpp unistd/write.cpp unistd/_exit.cpp unistd/file.cpp unistd/cwd.cpp sys/mman/munmap.cpp

include ../../common.mk

ifdef VERSION_MAJOR
CXX_FLAGS += -DSH_VERSION_MAJOR=$(VERSION_MAJOR) -DSH_VERSION_MINOR=$(VERSION_MINOR) -DSH_VERSION_PATCH=$(VERSION_PATCH)
endif
