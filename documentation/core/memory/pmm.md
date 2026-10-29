# Physical Memory Manager (PMM)

The PMM manages the raw hardware RAM in 4KB chunks called "frames". It uses a bitmap that covers the whole 4 GiB range, plus a second bitmap that remembers which frames are real RAM (the rest are devices or holes).

During initialization it reads the Multiboot memory map and marks as free only the regions reported as available. It then reserves:

* the first MiB,
* the kernel image, up to `_kernel_end`,
* the Multiboot information structure, the memory map, the command lines,
* the module list and every boot module.

Allocation does not scan from zero: it remembers the last word where it found a free frame and uses `__builtin_ctz` to pick the bit.

```cpp
PhysAddr PhysicalMemoryManager::allocFrame() {
	InterruptGuard guard;

	uint32_t frame = findFreeFrame();
	if (frame == NO_FRAME) return PhysAddr();

	setFrame(frame);
	usedFrames++;
	freeFrames--;
	return PhysAddr(frame * PAGE_SIZE);
}
```

`allocFrame` returns a null address when memory runs out and `freeFrame` only releases frames that `allocFrame` handed out (a separate bitmap records them), so a double free, a reserved frame of the kernel or of the boot structures, or an address outside RAM is ignored, and the callers decide what an out of memory condition means.

## Reference counts

Every frame has a **reference count**, one byte per frame (1 MiB of `.bss` for the 4 GiB range). `allocFrame` sets it to 1, `shareFrame` adds a reference and `freeFrame` drops one: the frame goes back to the pool only with its last reference. `shareFrame` fails for a frame that is not allocated and for one that already has `MAX_FRAME_REFERENCES` (255), and `frameReferences` reads the count. Only [copy on write](copy_on_write.md) shares frames, so a frame that is not part of a forked address space always has exactly one reference.

## Queries

`isRam` answers whether a physical address belongs to RAM; the [VMM](vmm.md) uses it to tell devices from memory. `freeFrameCount` returns the number of free frames, kept as a counter, so a caller can check before starting a large mapping.
