#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

namespace {

constexpr size_t ALIGNMENT = 8;
constexpr size_t ALIGN_MASK = ALIGNMENT - 1;
constexpr size_t PAGE_BYTES = 0x1000;
constexpr size_t PAGE_MASK = PAGE_BYTES - 1;
constexpr size_t INITIAL_CHUNK_PAGES = 16;
constexpr size_t MAX_CHUNK_PAGES = 256;
constexpr size_t GROWTH_FACTOR = 2;
constexpr int NO_FILE = -1;
constexpr size_t NO_OFFSET = 0;
constexpr int CHUNK_PROTECTION = PROT_READ | PROT_WRITE;
constexpr int CHUNK_FLAGS = MAP_ANONYMOUS;
constexpr int ZERO_BYTE = 0;

struct alignas(ALIGNMENT) Block {
	size_t size;
	Block *next;
};

constexpr size_t HEADER_SIZE = sizeof(Block);
constexpr size_t MIN_BLOCK_SIZE = HEADER_SIZE + ALIGNMENT;
constexpr size_t MAX_REQUEST =
	SIZE_MAX - PAGE_BYTES - HEADER_SIZE - ALIGNMENT;

static_assert(HEADER_SIZE % ALIGNMENT == 0);

Block *freeList = nullptr;
size_t nextChunkPages = INITIAL_CHUNK_PAGES;

char *bytesOf(Block *block) { return reinterpret_cast<char *>(block); }

Block *headerOf(void *payload) {
	return reinterpret_cast<Block *>(static_cast<char *>(payload) -
									 HEADER_SIZE);
}

size_t blockSizeFor(size_t request) {
	size_t size = (request + HEADER_SIZE + ALIGN_MASK) & ~ALIGN_MASK;
	return size < MIN_BLOCK_SIZE ? MIN_BLOCK_SIZE : size;
}

void linkAfter(Block *previous, Block *block) {
	if (previous)
		previous->next = block;
	else
		freeList = block;
}

void insertFree(Block *block) {
	Block *previous = nullptr;
	Block *current = freeList;
	while (current && current < block) {
		previous = current;
		current = current->next;
	}
	if (current == block) return;

	block->next = current;
	if (current && bytesOf(block) + block->size == bytesOf(current)) {
		block->size += current->size;
		block->next = current->next;
	}

	if (previous && bytesOf(previous) + previous->size == bytesOf(block)) {
		previous->size += block->size;
		previous->next = block->next;
		return;
	}
	linkAfter(previous, block);
}

Block *takeFree(size_t size) {
	Block *previous = nullptr;
	for (Block *current = freeList; current;
		 previous = current, current = current->next) {
		if (current->size < size) continue;

		size_t rest = current->size - size;
		if (rest < MIN_BLOCK_SIZE) {
			linkAfter(previous, current->next);
			return current;
		}

		Block *tail = reinterpret_cast<Block *>(bytesOf(current) + size);
		tail->size = rest;
		tail->next = current->next;
		current->size = size;
		linkAfter(previous, tail);
		return current;
	}
	return nullptr;
}

void *mapChunk(size_t bytes) {
	return mmap(nullptr, bytes, CHUNK_PROTECTION, CHUNK_FLAGS, NO_FILE,
				NO_OFFSET);
}

bool grow(size_t size) {
	size_t minimum = (size + PAGE_MASK) & ~PAGE_MASK;
	size_t bytes = nextChunkPages * PAGE_BYTES;
	if (bytes < minimum) bytes = minimum;

	void *memory = mapChunk(bytes);
	if (memory == MAP_FAILED && bytes > minimum) {
		bytes = minimum;
		memory = mapChunk(bytes);
	}
	if (memory == MAP_FAILED) return false;

	if (nextChunkPages < MAX_CHUNK_PAGES) nextChunkPages *= GROWTH_FACTOR;

	Block *chunk = static_cast<Block *>(memory);
	chunk->size = bytes;
	chunk->next = nullptr;
	insertFree(chunk);
	return true;
}

} // namespace

void *malloc(size_t size) {
	if (size > MAX_REQUEST) return nullptr;

	size_t blockSize = blockSizeFor(size);
	Block *block = takeFree(blockSize);
	if (!block) {
		if (!grow(blockSize)) return nullptr;
		block = takeFree(blockSize);
		if (!block) return nullptr;
	}
	return bytesOf(block) + HEADER_SIZE;
}

void free(void *pointer) {
	if (!pointer) return;
	insertFree(headerOf(pointer));
}

void *calloc(size_t count, size_t size) {
	if (count && size > MAX_REQUEST / count) return nullptr;

	size_t total = count * size;
	void *memory = malloc(total);
	if (memory) memset(memory, ZERO_BYTE, total);
	return memory;
}

void *realloc(void *pointer, size_t size) {
	if (!pointer) return malloc(size);
	if (!size) {
		free(pointer);
		return nullptr;
	}
	if (size > MAX_REQUEST) return nullptr;

	Block *block = headerOf(pointer);
	if (blockSizeFor(size) <= block->size) return pointer;

	void *moved = malloc(size);
	if (!moved) return nullptr;
	memcpy(moved, pointer, block->size - HEADER_SIZE);
	free(pointer);
	return moved;
}
