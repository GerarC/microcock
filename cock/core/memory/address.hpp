#ifndef CORE_MEMORY_ADDRESS_HPP
#define CORE_MEMORY_ADDRESS_HPP

#include <hal/address_space.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

class PhysAddr {
  public:
	constexpr PhysAddr() = default;
	constexpr explicit PhysAddr(hal::PhysicalRaw raw) : value(raw) {}

	constexpr hal::PhysicalRaw raw() const { return value; }
	constexpr bool isNull() const { return value == 0; }

	constexpr PhysAddr pageBase() const {
		return PhysAddr(value & ~static_cast<hal::PhysicalRaw>(hal::PAGE_MASK));
	}
	constexpr size_t pageOffset() const { return value & hal::PAGE_MASK; }
	constexpr size_t frameNumber() const { return value >> hal::PAGE_SHIFT; }

	constexpr PhysAddr operator+(size_t bytes) const {
		return PhysAddr(static_cast<hal::PhysicalRaw>(value + bytes));
	}

	constexpr bool operator==(PhysAddr other) const { return value == other.value; }
	constexpr bool operator!=(PhysAddr other) const { return value != other.value; }
	constexpr bool operator<(PhysAddr other) const { return value < other.value; }
	constexpr bool operator<=(PhysAddr other) const { return value <= other.value; }
	constexpr bool operator>(PhysAddr other) const { return value > other.value; }
	constexpr bool operator>=(PhysAddr other) const { return value >= other.value; }

  private:
	hal::PhysicalRaw value = 0;
};

class VirtAddr {
  public:
	constexpr VirtAddr() = default;
	constexpr explicit VirtAddr(hal::VirtualRaw raw) : value(raw) {}

	static VirtAddr of(const void *pointer) {
		return VirtAddr(reinterpret_cast<hal::VirtualRaw>(pointer));
	}

	constexpr hal::VirtualRaw raw() const { return value; }
	constexpr bool isNull() const { return value == 0; }

	template <typename T> T *as() const { return reinterpret_cast<T *>(value); }

	constexpr VirtAddr pageBase() const {
		return VirtAddr(value & ~static_cast<hal::VirtualRaw>(hal::PAGE_MASK));
	}
	constexpr size_t pageOffset() const { return value & hal::PAGE_MASK; }

	constexpr VirtAddr operator+(size_t bytes) const { return VirtAddr(value + bytes); }
	constexpr size_t operator-(VirtAddr other) const { return value - other.value; }

	constexpr bool operator==(VirtAddr other) const { return value == other.value; }
	constexpr bool operator!=(VirtAddr other) const { return value != other.value; }
	constexpr bool operator<(VirtAddr other) const { return value < other.value; }
	constexpr bool operator<=(VirtAddr other) const { return value <= other.value; }
	constexpr bool operator>(VirtAddr other) const { return value > other.value; }
	constexpr bool operator>=(VirtAddr other) const { return value >= other.value; }

  private:
	hal::VirtualRaw value = 0;
};

constexpr VirtAddr USER_SPACE_LIMIT_ADDRESS(hal::USER_SPACE_LIMIT);
constexpr VirtAddr KERNEL_SPACE_BASE_ADDRESS(hal::KERNEL_SPACE_BASE);

} // namespace cock::core::memory

#endif // !CORE_MEMORY_ADDRESS_HPP
