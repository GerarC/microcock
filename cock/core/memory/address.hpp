/** @file
 * @brief Strongly typed physical and virtual addresses, so the two cannot be
 * mixed by accident. Both are zero cost wrappers over the HAL raw types.
 */
#ifndef CORE_MEMORY_ADDRESS_HPP
#define CORE_MEMORY_ADDRESS_HPP

#include <hal/address_space.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::memory {

/**
 * @brief A physical address. Zero means null.
 *
 * Arithmetic wraps like the underlying integer, callers check ranges before
 * adding. Comparisons are by numeric value.
 */
class PhysAddr {
  public:
	/** @brief Creates the null address. */
	constexpr PhysAddr() = default;
	/**
	 * @brief Wraps a raw physical address.
	 * @param raw Physical address value.
	 */
	constexpr explicit PhysAddr(hal::PhysicalRaw raw) : value(raw) {}

	/** @return The raw physical address. */
	constexpr hal::PhysicalRaw raw() const { return value; }
	/** @return True when the address is zero. */
	constexpr bool isNull() const { return value == 0; }

	/** @return The address rounded down to the start of its page. */
	constexpr PhysAddr pageBase() const {
		return PhysAddr(value & ~static_cast<hal::PhysicalRaw>(hal::PAGE_MASK));
	}
	/** @return The offset of the address inside its page. */
	constexpr size_t pageOffset() const { return value & hal::PAGE_MASK; }
	/** @return The page frame number, the address shifted by the page size. */
	constexpr size_t frameNumber() const { return value >> hal::PAGE_SHIFT; }

	/**
	 * @brief Advances the address.
	 * @param bytes Distance in bytes.
	 * @return The address @p bytes further.
	 */
	constexpr PhysAddr operator+(size_t bytes) const {
		return PhysAddr(static_cast<hal::PhysicalRaw>(value + bytes));
	}

	/** @brief Equality by value. */
	constexpr bool operator==(PhysAddr other) const { return value == other.value; }
	/** @brief Inequality by value. */
	constexpr bool operator!=(PhysAddr other) const { return value != other.value; }
	/** @brief Numeric less than. */
	constexpr bool operator<(PhysAddr other) const { return value < other.value; }
	/** @brief Numeric less than or equal. */
	constexpr bool operator<=(PhysAddr other) const { return value <= other.value; }
	/** @brief Numeric greater than. */
	constexpr bool operator>(PhysAddr other) const { return value > other.value; }
	/** @brief Numeric greater than or equal. */
	constexpr bool operator>=(PhysAddr other) const { return value >= other.value; }

  private:
	hal::PhysicalRaw value = 0; /**< Raw physical address. */
};

/**
 * @brief A virtual address in some address space. Zero means null.
 *
 * Holding a VirtAddr says nothing about whether it is mapped or whether it
 * belongs to user or kernel space: user supplied values must be validated
 * before use.
 */
class VirtAddr {
  public:
	/** @brief Creates the null address. */
	constexpr VirtAddr() = default;
	/**
	 * @brief Wraps a raw virtual address.
	 * @param raw Virtual address value.
	 */
	constexpr explicit VirtAddr(hal::VirtualRaw raw) : value(raw) {}

	/**
	 * @brief Captures the address held by a pointer.
	 * @param pointer Any pointer, may be null.
	 * @return The pointer value as a VirtAddr.
	 */
	static VirtAddr of(const void *pointer) {
		return VirtAddr(reinterpret_cast<hal::VirtualRaw>(pointer));
	}

	/** @return The raw virtual address. */
	constexpr hal::VirtualRaw raw() const { return value; }
	/** @return True when the address is zero. */
	constexpr bool isNull() const { return value == 0; }

	/**
	 * @brief Reinterprets the address as a pointer.
	 * @tparam T Pointee type.
	 * @return A pointer with the same value. Dereferencing it is only valid
	 * when the address is mapped in the active address space.
	 */
	template <typename T> T *as() const { return reinterpret_cast<T *>(value); }

	/** @return The address rounded down to the start of its page. */
	constexpr VirtAddr pageBase() const {
		return VirtAddr(value & ~static_cast<hal::VirtualRaw>(hal::PAGE_MASK));
	}
	/** @return The offset of the address inside its page. */
	constexpr size_t pageOffset() const { return value & hal::PAGE_MASK; }

	/**
	 * @brief Advances the address.
	 * @param bytes Distance in bytes.
	 * @return The address @p bytes further. Wraps on overflow.
	 */
	constexpr VirtAddr operator+(size_t bytes) const { return VirtAddr(value + bytes); }
	/**
	 * @brief Distance between two addresses.
	 * @param other Address to subtract.
	 * @return The difference in bytes. Meaningful only when this address is
	 * not below @p other.
	 */
	constexpr size_t operator-(VirtAddr other) const { return value - other.value; }

	/** @brief Equality by value. */
	constexpr bool operator==(VirtAddr other) const { return value == other.value; }
	/** @brief Inequality by value. */
	constexpr bool operator!=(VirtAddr other) const { return value != other.value; }
	/** @brief Numeric less than. */
	constexpr bool operator<(VirtAddr other) const { return value < other.value; }
	/** @brief Numeric less than or equal. */
	constexpr bool operator<=(VirtAddr other) const { return value <= other.value; }
	/** @brief Numeric greater than. */
	constexpr bool operator>(VirtAddr other) const { return value > other.value; }
	/** @brief Numeric greater than or equal. */
	constexpr bool operator>=(VirtAddr other) const { return value >= other.value; }

  private:
	hal::VirtualRaw value = 0; /**< Raw virtual address. */
};

/** @brief First address a user process may not touch, from the HAL. */
constexpr VirtAddr USER_SPACE_LIMIT_ADDRESS(hal::USER_SPACE_LIMIT);
/** @brief First address of the kernel half of every address space. */
constexpr VirtAddr KERNEL_SPACE_BASE_ADDRESS(hal::KERNEL_SPACE_BASE);

} // namespace cock::core::memory

#endif // !CORE_MEMORY_ADDRESS_HPP
