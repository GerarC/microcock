# Address types

Physical and virtual addresses are different types, defined in `cock/core/memory/address.hpp`.

```cpp
class PhysAddr {
  public:
	constexpr explicit PhysAddr(hal::PhysicalRaw raw);
	constexpr hal::PhysicalRaw raw() const;
	constexpr PhysAddr pageBase() const;
	constexpr size_t frameNumber() const;
};

class VirtAddr {
  public:
	constexpr explicit VirtAddr(hal::VirtualRaw raw);
	constexpr hal::VirtualRaw raw() const;
	template <typename T> T *as() const;
	constexpr VirtAddr pageBase() const;
};
```

The conversion is always explicit, so a physical address cannot be passed where a virtual one is expected. The underlying integer types come from `hal/address_space.hpp` (`PhysicalRaw` is 32 bits on x86, and could be wider on another architecture; see [the HAL](../hal.md)). Both are plain wrappers around an integer: the compiler produces the same code as for a raw `uint32_t`.
