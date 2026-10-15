#ifndef COCK_DATA_STRUCTURE_RANGE_POOL_HPP
#define COCK_DATA_STRUCTURE_RANGE_POOL_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::data_structure {

template <size_t MAX_FREE_RANGES> class RangePool {
  public:
	constexpr RangePool(uintptr_t base, uintptr_t limit, uintptr_t pageBytes)
		: base(base), limit(limit), pageBytes(pageBytes), cursor(base) {}

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

	uintptr_t end() const { return cursor; }

	size_t freeRanges() const { return count; }

	size_t freePages() const {
		size_t total = 0;
		for (size_t index = 0; index < count; index++)
			total += ranges[index].pages;
		return total;
	}

  private:
	struct Range {
		uintptr_t start;
		size_t pages;
	};

	uintptr_t base;
	uintptr_t limit;
	uintptr_t pageBytes;
	uintptr_t cursor;
	Range ranges[MAX_FREE_RANGES] = {};
	size_t count = 0;

	uintptr_t ends(const Range &range) const { return range.start + range.pages * pageBytes; }

	void remove(size_t index) {
		for (size_t next = index + 1; next < count; next++)
			ranges[next - 1] = ranges[next];
		count--;
	}

	void insert(size_t index, uintptr_t start, size_t pages) {
		for (size_t next = count; next > index; next--)
			ranges[next] = ranges[next - 1];
		ranges[index] = {start, pages};
		count++;
	}
};

} // namespace cock::data_structure

#endif
