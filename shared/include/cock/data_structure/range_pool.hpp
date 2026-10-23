/**
 * @file range_pool.hpp
 * @brief Allocator of contiguous page ranges inside a fixed address window.
 *
 * Used by the kernel to hand out virtual address space (for example shared memory and thread stacks) and reuse
 * it after release. Pure bookkeeping: it never touches the memory it describes. Not thread safe.
 */
#ifndef COCK_DATA_STRUCTURE_RANGE_POOL_HPP
#define COCK_DATA_STRUCTURE_RANGE_POOL_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::data_structure {

/**
 * @brief Bump allocator with a sorted table of freed ranges that are reused first and merged with neighbors.
 *
 * Reservation takes the first freed range that is large enough, otherwise advances a cursor through the window.
 * Releasing at the top of the window lowers the cursor again.
 * @tparam MAX_FREE_RANGES Capacity of the table of free holes below the cursor. When it is full, a release that would need a new entry fails.
 */
template <size_t MAX_FREE_RANGES> class RangePool {
  public:
	/**
	 * @brief Creates a pool over `[base, limit)`.
	 * @param base First address of the window; must be a multiple of `pageBytes`.
	 * @param limit First address after the window.
	 * @param pageBytes Size of a page in bytes (the allocation unit, a non zero constant).
	 */
	constexpr RangePool(uintptr_t base, uintptr_t limit, uintptr_t pageBytes)
		: base(base), limit(limit), pageBytes(pageBytes), cursor(base) {}

	/**
	 * @brief Reserves a run of pages.
	 * @param pages Number of pages; must be at least 1.
	 * @param out Receives the start address of the run on success.
	 * @return True on success; false when `pages` is 0 or the window has no room.
	 */
	bool reserve(size_t pages, uintptr_t &out) {
		if (pages == 0) return false;

		for (size_t index = 0; index < count; index++) {
			if (ranges[index].pages < pages) continue;

			out = ranges[index].start;
			if (ranges[index].pages == pages) {
				remove(index);
			} else {
				ranges[index].start += pages * pageBytes;
				ranges[index].pages -= pages;
			}
			return true;
		}

		if (pages > (limit - cursor) / pageBytes) return false;
		out = cursor;
		cursor += pages * pageBytes;
		return true;
	}

	/**
	 * @brief Returns a run of pages to the pool.
	 * @param start Start address of the run, page aligned, inside the window and below the cursor.
	 * @param pages Number of pages, at least 1.
	 * @return True on success. False when the arguments are invalid, the run overlaps memory that is already free (double release), or the table of holes is full.
	 */
	bool release(uintptr_t start, size_t pages) {
		if (pages == 0 || start < base || start % pageBytes != 0) return false;
		if (start > cursor || pages > (cursor - start) / pageBytes) return false;
		uintptr_t end = start + pages * pageBytes;

		size_t at = 0;
		while (at < count && ranges[at].start < start)
			at++;
		if (at > 0 && ends(ranges[at - 1]) > start) return false;
		if (at < count && ranges[at].start < end) return false;

		bool joinsBefore = at > 0 && ends(ranges[at - 1]) == start;
		bool joinsAfter = at < count && ranges[at].start == end;
		if (end == cursor) {
			cursor = joinsBefore ? ranges[at - 1].start : start;
			if (joinsBefore) remove(at - 1);
			return true;
		}
		if (joinsBefore) {
			ranges[at - 1].pages += pages;
			if (joinsAfter) {
				ranges[at - 1].pages += ranges[at].pages;
				remove(at);
			}
		} else if (joinsAfter) {
			ranges[at].start = start;
			ranges[at].pages += pages;
		} else {
			if (count == MAX_FREE_RANGES) return false;
			insert(at, start, pages);
		}
		return true;
	}

	/**
	 * @brief Highest address ever handed out and still in use or held in a hole.
	 * @return The cursor: every reservation lies below it.
	 */
	uintptr_t end() const { return cursor; }

	/**
	 * @brief Number of free holes below the cursor.
	 * @return Entries in use in the table.
	 */
	size_t freeRanges() const { return count; }

	/**
	 * @brief Pages available in the holes below the cursor.
	 * @return Sum of the pages of all holes; does not include the untouched space above the cursor.
	 */
	size_t freePages() const {
		size_t total = 0;
		for (size_t index = 0; index < count; index++)
			total += ranges[index].pages;
		return total;
	}

  private:
	/** A free hole below the cursor. */
	struct Range {
		/** First address of the hole. */
		uintptr_t start;
		/** Length of the hole in pages. */
		size_t pages;
	};

	/** First address of the window. */
	uintptr_t base;
	/** First address after the window. */
	uintptr_t limit;
	/** Bytes per page. */
	uintptr_t pageBytes;
	/** Start of the never used space; everything above it is free. */
	uintptr_t cursor;
	/** Free holes, sorted by address and never adjacent. */
	Range ranges[MAX_FREE_RANGES] = {};
	/** Entries in use in `ranges`. */
	size_t count = 0;

	/**
	 * @brief First address after a hole.
	 * @param range Hole to measure.
	 * @return Its end address.
	 */
	uintptr_t ends(const Range &range) const { return range.start + range.pages * pageBytes; }

	/**
	 * @brief Deletes a hole from the table, shifting the rest down.
	 * @param index Position of the hole; must be below `count`.
	 */
	void remove(size_t index) {
		for (size_t next = index + 1; next < count; next++)
			ranges[next - 1] = ranges[next];
		count--;
	}

	/**
	 * @brief Inserts a hole keeping the table sorted.
	 * @param index Position for the new hole.
	 * @param start First address of the hole.
	 * @param pages Length in pages. The table must have room.
	 */
	void insert(size_t index, uintptr_t start, size_t pages) {
		for (size_t next = count; next > index; next--)
			ranges[next] = ranges[next - 1];
		ranges[index] = {start, pages};
		count++;
	}
};

} // namespace cock::data_structure

#endif
