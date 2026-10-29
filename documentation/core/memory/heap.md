# Kernel Heap Allocator

For dynamic memory allocation inside Ring 0 (`kmalloc` and `kfree`, behind `new` and `delete`), the OS uses a custom Heap implementation based on a First-Fit linked list algorithm. `Thread` and `Process` do not use it: they have their own [pools](../process/pools.md).

* **Alignment:** blocks are aligned and their header is 16 bytes, so the pointer handed out is aligned on every architecture.
* **Splitting:** If a requested allocation is smaller than a free block, the allocator splits the block to reduce internal fragmentation.
* **Coalescing:** When memory is freed, the allocator merges adjacent free blocks to prevent external fragmentation.
* **Interrupt safety:** each operation runs under an `InterruptGuard`, which saves the interrupt flag, disables interrupts and restores the previous state, so guards nest (see [the HAL](../hal.md)).
* **Growth:** when no block fits, the heap asks the [VMM](vmm.md) for more pages. It does not return them.

```cpp
void *Heap::allocate(size_t size) {
	if (size == 0) return nullptr;
	if (size > SIZE_MAX - sizeof(MemoryBlock) - ALIGNMENT - hal::PAGE_SIZE)
		return nullptr;

	hal::InterruptGuard guard;
	size_t aligned_size = alignUp(size);
	MemoryBlock *prev = nullptr;
	MemoryBlock *block = findFirstAvailable(aligned_size, &prev);
	...
}
```

`kcalloc` checks for overflow in `num * size`. `operator new` stops the kernel with a [panic](../diagnostics/panic.md) when the heap cannot grow.
