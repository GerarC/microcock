#ifndef COCK_USER_WRITE_CHECK_HPP
#define COCK_USER_WRITE_CHECK_HPP

#include <cock/abi/fs.hpp>
#include <cock/user/fs.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace cock::user::fs {

class WriteCheck {
  public:
	WriteCheck(Client &client, const char *directory, uint32_t bigBytes)
		: client(client), directory(directory), bigBytes(bigBytes) {}

	bool run() {
		return step(1, createsAndReadsBack()) && step(2, reopensWithoutTruncating()) &&
			   step(3, writesALargeFile()) && step(4, overwritesAndAppends()) &&
			   step(5, protectsOpenFiles()) && step(6, truncates()) &&
			   step(7, deletes()) && step(8, keepsAFile()) && step(9, refusesBadRequests()) &&
			   step(10, lists());
	}

	bool refuses(const char *path, uint32_t flags, abi::fs::Status expected) {
		return client.open(path, flags) == abi::fs::NO_HANDLE &&
			   client.lastStatus() == expected;
	}

	bool creates(const char *name, const char *text) {
		char path[PATH_BYTES];
		if (!join(path, name)) return false;
		uint32_t handle = client.open(path, WRITE_CREATE);
		size_t length = strlen(text);
		bool ok = handle != abi::fs::NO_HANDLE &&
				  client.write(handle, START, text, length) == static_cast<long>(length);
		return handle != abi::fs::NO_HANDLE && client.close(handle) && ok;
	}

	bool reads(const char *name, const char *text) {
		char path[PATH_BYTES];
		if (!join(path, name)) return false;
		uint32_t handle = client.open(path);
		if (handle == abi::fs::NO_HANDLE) return false;

		char buffer[TEXT_BYTES] = {};
		FileInfo info = {};
		size_t length = strlen(text);
		bool ok = client.stat(handle, info) && info.size == length &&
				  info.type == abi::fs::FileType::REGULAR &&
				  client.read(handle, START, buffer, sizeof(buffer)) ==
					  static_cast<long>(length) &&
				  memcmp(buffer, text, length) == 0;
		return client.close(handle) && ok;
	}

	bool removes(const char *name) {
		char path[PATH_BYTES];
		return join(path, name) && client.unlink(path);
	}

	bool missing(const char *name) {
		char path[PATH_BYTES];
		return join(path, name) &&
			   refuses(path, abi::fs::OPEN_READ_ONLY, abi::fs::Status::NOT_FOUND);
	}

  private:
	static constexpr size_t PATH_BYTES = abi::fs::MAX_PATH_BYTES;
	static constexpr size_t TEXT_BYTES = 64;
	static constexpr size_t CHUNK_BYTES = abi::fs::WINDOW_BYTES;
	static constexpr uint32_t START = 0;
	static constexpr uint32_t WRITE_CREATE = abi::fs::OPEN_WRITE | abi::fs::OPEN_CREATE;
	static constexpr uint32_t WRITE_TRUNCATE = abi::fs::OPEN_WRITE | abi::fs::OPEN_TRUNCATE;
	static constexpr uint32_t UNKNOWN_FLAG = 0x80;
	static constexpr uint32_t PATTERN_LETTERS = 26;
	static constexpr char PATTERN_FIRST = 'A';
	static constexpr uint32_t OVERWRITE_OFFSET = 4090;
	static constexpr size_t OVERWRITE_BYTES = 20;
	static constexpr char OVERWRITE_CHAR = 'x';
	static constexpr size_t APPEND_BYTES = 10;
	static constexpr char APPEND_CHAR = 'z';
	static constexpr uint32_t GAP_BYTES = 2;
	static constexpr size_t WINDOW_MARGIN = 10;
	static constexpr char SEPARATOR = '/';
	static constexpr char NOTE_NAME[] = "note.txt";
	static constexpr char NOTE_TEXT[] = "written by cock\n";
	static constexpr char BIG_NAME[] = "big.bin";
	static constexpr char SHORT_TEXT[] = "after truncate\n";
	static constexpr char GONE_NAME[] = "gone.txt";
	static constexpr char GONE_TEXT[] = "short lived\n";
	static constexpr char KEEP_NAME[] = "keep.txt";
	static constexpr char KEEP_TEXT[] = "kept on disk\n";
	static constexpr char MISSING_NAME[] = "missing.txt";
	static constexpr char MISSING_DIRECTORY_NAME[] = "nodir/x.txt";
	static constexpr char ROOT_NEW_FILE[] = "/newfile";

	Client &client;
	const char *directory;
	uint32_t bigBytes;

	static bool step(unsigned number, bool passed) {
		if (!passed) printf("writecheck: step %u failed\n", number);
		return passed;
	}

	bool join(char *path, const char *name) const {
		size_t base = strlen(directory);
		size_t length = strlen(name);
		if (base + sizeof(char) + length + sizeof(char) > PATH_BYTES) return false;
		memcpy(path, directory, base);
		path[base] = SEPARATOR;
		memcpy(path + base + sizeof(char), name, length + sizeof(char));
		return true;
	}

	static char pattern(uint32_t position) {
		return static_cast<char>(PATTERN_FIRST + position % PATTERN_LETTERS);
	}

	uint32_t openNamed(const char *name, uint32_t flags) {
		char path[PATH_BYTES];
		return join(path, name) ? client.open(path, flags) : abi::fs::NO_HANDLE;
	}

	bool createsAndReadsBack() {
		if (!creates(NOTE_NAME, NOTE_TEXT)) return false;
		return reads(NOTE_NAME, NOTE_TEXT);
	}

	bool reopensWithoutTruncating() {
		uint32_t handle = openNamed(NOTE_NAME, WRITE_CREATE);
		if (handle == abi::fs::NO_HANDLE) return false;
		FileInfo info = {};
		bool ok = client.stat(handle, info) && info.size == strlen(NOTE_TEXT);
		return client.close(handle) && ok;
	}

	bool writesALargeFile() {
		uint32_t handle = openNamed(BIG_NAME, WRITE_CREATE);
		if (handle == abi::fs::NO_HANDLE) return false;

		char chunk[CHUNK_BYTES];
		uint32_t position = START;
		bool ok = true;
		while (ok && position < bigBytes) {
			size_t count = bigBytes - position < CHUNK_BYTES ? bigBytes - position : CHUNK_BYTES;
			for (size_t index = 0; index < count; index++)
				chunk[index] = pattern(position + static_cast<uint32_t>(index));
			ok = client.write(handle, position, chunk, count) == static_cast<long>(count);
			if (ok) position += static_cast<uint32_t>(count);
		}

		FileInfo info = {};
		ok = ok && client.stat(handle, info) && info.size == bigBytes;
		ok = client.close(handle) && ok;
		return ok && patternReads(START, bigBytes);
	}

	bool patternReads(uint32_t from, uint32_t to) {
		uint32_t handle = openNamed(BIG_NAME, abi::fs::OPEN_READ_ONLY);
		if (handle == abi::fs::NO_HANDLE) return false;

		char chunk[CHUNK_BYTES];
		uint32_t position = from;
		bool ok = true;
		while (ok && position < to) {
			long count = client.read(handle, position, chunk, CHUNK_BYTES);
			ok = count > 0;
			for (long index = 0; ok && index < count; index++)
				ok = chunk[index] == pattern(position + static_cast<uint32_t>(index));
			if (ok) position += static_cast<uint32_t>(count);
		}
		return client.close(handle) && ok && position >= to;
	}

	bool overwritesAndAppends() {
		uint32_t handle = openNamed(BIG_NAME, abi::fs::OPEN_WRITE);
		if (handle == abi::fs::NO_HANDLE) return false;

		char block[OVERWRITE_BYTES];
		memset(block, OVERWRITE_CHAR, sizeof(block));
		bool ok = client.write(handle, OVERWRITE_OFFSET, block, sizeof(block)) ==
				  static_cast<long>(sizeof(block));

		char tail[APPEND_BYTES];
		memset(tail, APPEND_CHAR, sizeof(tail));
		ok = ok && client.write(handle, bigBytes, tail, sizeof(tail)) ==
					   static_cast<long>(sizeof(tail));
		ok = ok && client.write(handle, bigBytes + sizeof(tail) + GAP_BYTES, tail,
								sizeof(tail)) == WRITE_FAILED &&
			 client.lastStatus() == abi::fs::Status::OUT_OF_RANGE;

		FileInfo info = {};
		ok = ok && client.stat(handle, info) && info.size == bigBytes + sizeof(tail);
		ok = client.close(handle) && ok;
		if (!ok) return false;

		char chunk[CHUNK_BYTES];
		handle = openNamed(BIG_NAME, abi::fs::OPEN_READ_ONLY);
		if (handle == abi::fs::NO_HANDLE) return false;

		size_t window = OVERWRITE_BYTES + 2 * WINDOW_MARGIN;
		ok = client.read(handle, OVERWRITE_OFFSET - WINDOW_MARGIN, chunk, window) ==
			 static_cast<long>(window);
		for (size_t index = 0; ok && index < window; index++) {
			uint32_t position = OVERWRITE_OFFSET - WINDOW_MARGIN + static_cast<uint32_t>(index);
			bool inside = index >= WINDOW_MARGIN && index < WINDOW_MARGIN + OVERWRITE_BYTES;
			ok = chunk[index] == (inside ? OVERWRITE_CHAR : pattern(position));
		}

		ok = ok && client.read(handle, bigBytes, chunk, CHUNK_BYTES) ==
					   static_cast<long>(sizeof(tail));
		for (size_t index = 0; ok && index < sizeof(tail); index++)
			ok = chunk[index] == APPEND_CHAR;
		return client.close(handle) && ok;
	}

	bool protectsOpenFiles() {
		char path[PATH_BYTES];
		if (!join(path, BIG_NAME)) return false;

		uint32_t reader = client.open(path);
		if (reader == abi::fs::NO_HANDLE) return false;

		char byte = 0;
		bool ok = client.write(reader, START, &byte, sizeof(byte)) == WRITE_FAILED &&
				  client.lastStatus() == abi::fs::Status::READ_ONLY;
		ok = ok && refuses(path, abi::fs::OPEN_WRITE, abi::fs::Status::BUSY);
		ok = ok && !client.unlink(path) && client.lastStatus() == abi::fs::Status::BUSY;
		ok = ok && refuses(path, WRITE_TRUNCATE, abi::fs::Status::BUSY);
		uint32_t second = client.open(path);
		ok = ok && second != abi::fs::NO_HANDLE && client.close(second);
		ok = client.close(reader) && ok;

		uint32_t writer = client.open(path, abi::fs::OPEN_WRITE);
		ok = ok && writer != abi::fs::NO_HANDLE &&
			 refuses(path, abi::fs::OPEN_READ_ONLY, abi::fs::Status::BUSY);
		if (writer != abi::fs::NO_HANDLE) ok = client.close(writer) && ok;
		return ok;
	}

	bool truncates() {
		uint32_t handle = openNamed(BIG_NAME, WRITE_TRUNCATE);
		if (handle == abi::fs::NO_HANDLE) return false;

		FileInfo info = {};
		bool ok = client.stat(handle, info) && info.size == START;
		size_t length = strlen(SHORT_TEXT);
		ok = ok && client.write(handle, START, SHORT_TEXT, length) == static_cast<long>(length);
		ok = client.close(handle) && ok;
		return ok && reads(BIG_NAME, SHORT_TEXT);
	}

	bool deletes() {
		if (!removes(BIG_NAME) || !missing(BIG_NAME) || removes(BIG_NAME)) return false;
		if (client.lastStatus() != abi::fs::Status::NOT_FOUND) return false;
		return creates(GONE_NAME, GONE_TEXT) && reads(GONE_NAME, GONE_TEXT) &&
			   removes(GONE_NAME) && missing(GONE_NAME);
	}

	bool keepsAFile() { return creates(KEEP_NAME, KEEP_TEXT) && reads(KEEP_NAME, KEEP_TEXT); }

	bool refusesBadRequests() {
		char path[PATH_BYTES];
		if (!join(path, MISSING_NAME)) return false;
		bool ok = refuses(path, abi::fs::OPEN_WRITE, abi::fs::Status::NOT_FOUND) &&
				  refuses(path, abi::fs::OPEN_CREATE, abi::fs::Status::BAD_REQUEST) &&
				  refuses(path, abi::fs::OPEN_TRUNCATE, abi::fs::Status::BAD_REQUEST) &&
				  refuses(path, UNKNOWN_FLAG, abi::fs::Status::UNSUPPORTED);

		ok = ok && join(path, MISSING_DIRECTORY_NAME) &&
			 refuses(path, WRITE_CREATE, abi::fs::Status::NOT_FOUND);
		ok = ok && refuses(directory, abi::fs::OPEN_WRITE, abi::fs::Status::UNSUPPORTED);
		ok = ok && !client.unlink(directory) && client.lastStatus() == abi::fs::Status::UNSUPPORTED;
		ok = ok && refuses(ROOT_NEW_FILE, WRITE_CREATE, abi::fs::Status::READ_ONLY);
		return ok;
	}

	bool lists() {
		uint32_t handle = client.open(directory);
		if (handle == abi::fs::NO_HANDLE) return false;

		bool sawNote = false;
		bool sawKeep = false;
		bool sawOthers = false;
		uint32_t index = START;
		char name[TEXT_BYTES] = {};
		abi::fs::FileType type = abi::fs::FileType::REGULAR;
		while (client.readdir(handle, index, name, sizeof(name), type)) {
			index++;
			if (strcmp(name, NOTE_NAME) == 0) sawNote = true;
			if (strcmp(name, KEEP_NAME) == 0) sawKeep = true;
			if (strcmp(name, BIG_NAME) == 0 || strcmp(name, GONE_NAME) == 0) sawOthers = true;
		}
		return client.close(handle) && sawNote && sawKeep && !sawOthers;
	}
};

} // namespace cock::user::fs

#endif // !COCK_USER_WRITE_CHECK_HPP
