# VGA Display Drivers

Due to the microkernel architecture, MicroCOCK utilizes two distinct implementations of the VGA text-mode driver during its lifecycle.

## Early Kernel VGA Driver
Provides a fallback text output interface used exclusively during the early boot sequence. It writes directly to the standard VGA framebuffer mapped at physical address `0xC00B8000`.

```cpp
void VGA::putChar(char c) {
    if (c == NEWLINE) {
        newline();
        return;
    }
    putEntryAt(c, color, column, row);
    if (++column == VGA_WIDTH) {
        column = INITIAL_COL;
        newline();
    }
}
```

## Userland VGA Server
The fully isolated Ring 3 display server handles standard output for the operating system. During initialization, it uses the `mmap` syscall to map the physical hardware VGA buffer (`0xB8000`) securely into its own virtual address space at `0xA0000000`.

```cpp
void VgaServer::init() {
    buffer = (uint16_t*)mmap((void*)VGA_VIRTUAL_TARGET, PAGE_SIZE, 
                             PROT_READ | PROT_WRITE, 
                             MAP_PHYS | MAP_FIXED, -1, 
                             VGA_PHYSICAL_ADDRESS);
}
```

It executes an infinite loop waiting for `sys_ipc_recv`. The kernel routes text data to it via IPC messages containing types like `VIDEO_WRITE_CHAR` or `VIDEO_WRITE_STR`.

```cpp
extern "C" void _start() {
    VgaServer::init();
    Message msg = {};
    
    while (true) {
        ipc_recv(&msg);
        switch (msg.type) {
            case MessageType::VIDEO_WRITE_CHAR:
                VgaServer::putChar(static_cast<char>(msg.payload.args.arg0));
                break;
            case MessageType::VIDEO_WRITE_STR: {
                size_t length = msg.payload.args.arg0;
                const char *str_chunk = reinterpret_cast<const char *>(&msg.payload.args.arg1);
                VgaServer::write(str_chunk, length);
                break;
            }
            case MessageType::NONE:
                VgaServer::clear();
                break;
        }
    }
}
```
