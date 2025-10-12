# MicroCOCK
Micro Colombian Open Code Kernel is a project where the quid of the matter is learning about kernels as the project progesses.

### Index
This is the index of the components in order of creation 

1. **[Multiboot Init](documentation/multiboot.md):** to start an operative System an existing software is needed, in this case: *GRUB*.
2. **[Kernel Main](documentation/kernel_main.md):** this is the process that bootloader shold be calling.
3. **[VGA](documentation/vga.md):** once the kernel is up, you can access to all the available I/O stuff, the very first driver should be VGA one to be able to write in the screen.
4. **[Project Structure](documentation/project_structure.md):** before moving forward in kernel developing a proper project structure should be set up;
5. **[GDT](documentation/gdt.md):** this table tells to the CPU where it has access or not when it loads a new value in segment registers.
6. **[IDT](documentation/idt.md):** an IDT is a table where interrupts are save with their handlers. There are some types of interrupts and each one has a different way to work with.
