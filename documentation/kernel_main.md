## Kernel Main

This is the main function of the kernel, it will be called by bootloader after it sets up all its stuff.

at the moment it just print some messages through an custom implementation of printf that uses the [VGA](vga.md) driver. It must be extern, so it can be seen from the booloader.

*core/cock.c*
~~~ c 
extern "C" void cock_main(void) {
    puts("Hello Worldo!");
}
~~~

at this moment of developing proceess another function that can be called `early_init`. This one sets up all the stuff needed for the kernel before it starts. At the moment it inits the vga driver, global constructors, GDT and IDT. after this process it call the kernel main.

*core/early_init.c*
~~~ c 
extern "C" void init_cock(void) {
	arch_video_init();
	call_global_constructors();
	arch_core_init();

	cock_main();
}
~~~
