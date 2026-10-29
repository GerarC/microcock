# Context of the architecture

`ArchProcessContext` holds the I/O permission bitmap of the process. Copying a bitmap into the TSS at every switch would be the most expensive thing the kernel did, so it is lazy:

* each context has a **version** that changes when its permissions change,
* the TSS remembers the version it holds, and copies nothing when it already matches,
* only the bytes in use are copied, and a process with no ports turns the bitmap off instead of copying anything,
* kernel threads never reload it.

The context is applied by the scheduler when it switches threads (see [switching](../scheduler/switching.md)).
