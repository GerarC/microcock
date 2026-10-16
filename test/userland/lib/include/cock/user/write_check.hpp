/**
 * @file write_check.hpp
 * @brief Write path test shared by the programs that exercise a writable file server (`ramfs`, `fat`, ext2 through the `vfs`).
 *
 * `WriteCheck::run` makes fifteen steps in a directory of the file system under test: create and read back, reopen without
 * truncating, write a multi page file, overwrite and append, protection of open files, truncate, delete, refusal of bad
 * requests, listing, directories (create, remove, rename, move), long names, short name aliases, replacing names and hard
 * links. A step that fails prints `writecheck: step N failed`. The test leaves `keep.txt` and `keepdir` behind so a later boot can
 * check persistence. The check runs in Ring 3 through the file client and needs the server to be reachable.
 */
#ifndef COCK_USER_WRITE_CHECK_HPP
#define COCK_USER_WRITE_CHECK_HPP

#include <cock/abi/fs.hpp>
#include <cock/user/fs.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace cock::user::fs {

/** Runs the write tests against one file system through a connected file client. */
class WriteCheck {
  public:
	/**
	 * @brief Prepares a check.
	 * @param client Connected file client; must outlive the check.
	 * @param directory Absolute path of an existing writable directory where the files are made (no trailing slash, short enough for the path limit).
	 * @param bigBytes Size in bytes of the large file of step 3; sets how many clusters or blocks are used. Must be at least 4100 so that step 4 can overwrite and append past a page.
	 * @param windowsNames True for a Windows style file system (FAT): names with forbidden characters are refused and hard links are unsupported. False for ext2.
	 */
	WriteCheck(Client &client, const char *directory, uint32_t bigBytes, bool windowsNames)
		: client(client), directory(directory), bigBytes(bigBytes), windowsNames(windowsNames) {}

	/**
	 * @brief Runs all steps in order and stops at the first failure.
	 * @return True when all fifteen steps passed.
	 */
	bool run() {
		return step(1, createsAndReadsBack()) && step(2, reopensWithoutTruncating()) &&
			   step(3, writesALargeFile()) && step(4, overwritesAndAppends()) &&
			   step(5, protectsOpenFiles()) && step(6, truncates()) &&
			   step(7, deletes()) && step(8, keepsAFile()) && step(9, refusesBadRequests()) &&
			   step(10, lists()) && step(11, managesDirectories()) &&
			   step(12, usesLongNames()) && step(13, aliasesDoNotCollide()) &&
				   step(14, replacesNames()) && step(15, linksFiles());
	}

	/**
	 * @brief Checks that an open fails with an exact status.
	 * @param path Absolute path to open.
	 * @param flags Open flags.
	 * @param expected Status the server must report.
	 * @return True when no handle was returned and `lastStatus` equals `expected`.
	 */
	bool refuses(const char *path, uint32_t flags, abi::fs::Status expected) {
		return client.open(path, flags) == abi::fs::NO_HANDLE &&
			   client.lastStatus() == expected;
	}

	/**
	 * @brief Creates a file in the test directory and writes a text into it.
	 * @param name Name relative to the test directory.
	 * @param text Null terminated content.
	 * @return True when the file was created, fully written and closed.
	 */
	bool creates(const char *name, const char *text) {
		char path[PATH_BYTES];
		if (!join(path, name)) return false;
		uint32_t handle = client.open(path, WRITE_CREATE);
		size_t length = strlen(text);
		bool ok = handle != abi::fs::NO_HANDLE &&
				  client.write(handle, START, text, length) == static_cast<long>(length);
		return handle != abi::fs::NO_HANDLE && client.close(handle) && ok;
	}

	/**
	 * @brief Checks that a file holds exactly a text.
	 * @param name Name relative to the test directory.
	 * @param text Expected null terminated content, shorter than 64 bytes.
	 * @return True when the file opens, is a regular file of that size, reads back the same bytes and closes.
	 */
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

	/**
	 * @brief Deletes a file of the test directory.
	 * @param name Name relative to the test directory.
	 * @return True when the server accepted the unlink.
	 */
	bool removes(const char *name) {
		char path[PATH_BYTES];
		return join(path, name) && client.unlink(path);
	}

	/**
	 * @brief Checks that a file does not exist.
	 * @param name Name relative to the test directory.
	 * @return True when opening it fails with `NOT_FOUND`.
	 */
	bool missing(const char *name) {
		char path[PATH_BYTES];
		return join(path, name) &&
			   refuses(path, abi::fs::OPEN_READ_ONLY, abi::fs::Status::NOT_FOUND);
	}

  private:
	/** Size of the path buffers, equal to the protocol path limit. */
	static constexpr size_t PATH_BYTES = abi::fs::MAX_PATH_BYTES;
	/** Size of the buffers that hold short texts and names. */
	static constexpr size_t TEXT_BYTES = 64;
	/** Bytes moved per read or write of the large file: one window. */
	static constexpr size_t CHUNK_BYTES = abi::fs::WINDOW_BYTES;
	/** Offset of the start of a file. */
	static constexpr uint32_t START = 0;
	/** Open flags for writing and creating. */
	static constexpr uint32_t WRITE_CREATE = abi::fs::OPEN_WRITE | abi::fs::OPEN_CREATE;
	/** Open flags for writing and truncating. */
	static constexpr uint32_t WRITE_TRUNCATE = abi::fs::OPEN_WRITE | abi::fs::OPEN_TRUNCATE;
	/** An open flag that the protocol does not define. */
	static constexpr uint32_t UNKNOWN_FLAG = 0x80;
	/** Letters in the repeating pattern of the large file. */
	static constexpr uint32_t PATTERN_LETTERS = 26;
	/** First letter of the pattern. */
	static constexpr char PATTERN_FIRST = 'A';
	/** Offset of the overwrite of step 4; it crosses the 4096 byte page boundary. */
	static constexpr uint32_t OVERWRITE_OFFSET = 4090;
	/** Length of the overwrite. */
	static constexpr size_t OVERWRITE_BYTES = 20;
	/** Byte written by the overwrite. */
	static constexpr char OVERWRITE_CHAR = 'x';
	/** Length of the append. */
	static constexpr size_t APPEND_BYTES = 10;
	/** Byte written by the append. */
	static constexpr char APPEND_CHAR = 'z';
	/** Distance past the end of the file of the write that must be refused as `OUT_OF_RANGE`. */
	static constexpr uint32_t GAP_BYTES = 2;
	/** Bytes of untouched pattern checked on each side of the overwrite. */
	static constexpr size_t WINDOW_MARGIN = 10;
	/** Path separator. */
	static constexpr char SEPARATOR = '/';
	/** Name of the first test file. */
	static constexpr char NOTE_NAME[] = "note.txt";
	/** Content of the first test file. */
	static constexpr char NOTE_TEXT[] = "written by cock\n";
	/** Name of the large test file. */
	static constexpr char BIG_NAME[] = "big.bin";
	/** Content written after truncating the large file. */
	static constexpr char SHORT_TEXT[] = "after truncate\n";
	/** Name of the file that is created and deleted. */
	static constexpr char GONE_NAME[] = "gone.txt";
	/** Content of the deleted file. */
	static constexpr char GONE_TEXT[] = "short lived\n";
	/** Name of the file left on the volume for the persistence check. */
	static constexpr char KEEP_NAME[] = "keep.txt";
	/** Content of the file left on the volume. */
	static constexpr char KEEP_TEXT[] = "kept on disk\n";
	/** Name that must not exist. */
	static constexpr char MISSING_NAME[] = "missing.txt";
	/** Path whose parent directory does not exist. */
	static constexpr char MISSING_DIRECTORY_NAME[] = "nodir/x.txt";
	/** Path in the root, which belongs to the read-only initrd file system. */
	static constexpr char ROOT_NEW_FILE[] = "/newfile";

	/** File client used for every request. */
	Client &client;
	/** Absolute path of the test directory. */
	const char *directory;
	/** Size of the large file. */
	uint32_t bigBytes;
	/** True for FAT rules, false for ext2 rules. */
	bool windowsNames;
	/** Buffer returned by `pathOf`; overwritten by each call. */
	char scratchPath[PATH_BYTES] = {};

	/**
	 * @brief Reports a failed step.
	 * @param number Step number, 1 to 15.
	 * @param passed Result of the step.
	 * @return `passed`.
	 */
	static bool step(unsigned number, bool passed) {
		if (!passed) printf("writecheck: step %u failed\n", number);
		return passed;
	}

	/**
	 * @brief Builds the absolute path of a name in the test directory.
	 * @param path Output buffer of `PATH_BYTES` bytes.
	 * @param name Name relative to the test directory (may contain subdirectories).
	 * @return False when the path does not fit.
	 */
	bool join(char *path, const char *name) const {
		size_t base = strlen(directory);
		size_t length = strlen(name);
		if (base + sizeof(char) + length + sizeof(char) > PATH_BYTES) return false;
		memcpy(path, directory, base);
		path[base] = SEPARATOR;
		memcpy(path + base + sizeof(char), name, length + sizeof(char));
		return true;
	}

	/**
	 * @brief Byte expected at a position of the large file.
	 * @param position Offset in the file.
	 * @return A letter that cycles A to Z.
	 */
	static char pattern(uint32_t position) {
		return static_cast<char>(PATTERN_FIRST + position % PATTERN_LETTERS);
	}

	/**
	 * @brief Opens a file of the test directory.
	 * @param name Name relative to the test directory.
	 * @param flags Open flags.
	 * @return The handle, or `NO_HANDLE` when the path is too long or the open failed.
	 */
	uint32_t openNamed(const char *name, uint32_t flags) {
		char path[PATH_BYTES];
		return join(path, name) ? client.open(path, flags) : abi::fs::NO_HANDLE;
	}

	/** Step 1: creates a file and reads it back. */
	bool createsAndReadsBack() {
		if (!creates(NOTE_NAME, NOTE_TEXT)) return false;
		return reads(NOTE_NAME, NOTE_TEXT);
	}

	/** Step 2: reopening an existing file with write and create keeps its content. */
	bool reopensWithoutTruncating() {
		uint32_t handle = openNamed(NOTE_NAME, WRITE_CREATE);
		if (handle == abi::fs::NO_HANDLE) return false;
		FileInfo info = {};
		bool ok = client.stat(handle, info) && info.size == strlen(NOTE_TEXT);
		return client.close(handle) && ok;
	}

	/** Step 3: writes the large file window by window, checks the size and reads the whole pattern back. */
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

	/**
	 * @brief Reads the large file and compares it with the pattern.
	 * @param from First offset to check.
	 * @param to Offset where checking stops.
	 * @return True when every byte matches and the file is long enough.
	 */
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

	/** Step 4: overwrites across a page boundary, appends at the end and checks that a write past the end is refused with `OUT_OF_RANGE`; verifies the bytes around the changes. */
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

	/** Step 5: a read only handle cannot write, and a file that is open cannot be opened for writing, truncated or deleted (`BUSY`); several readers are allowed, a writer excludes readers. */
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

	/** Step 6: opening with truncate empties the file; new content can be written and read. */
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

	/** Step 7: deleting a file makes it disappear and a second delete fails with `NOT_FOUND`. */
	bool deletes() {
		if (!removes(BIG_NAME) || !missing(BIG_NAME) || removes(BIG_NAME)) return false;
		if (client.lastStatus() != abi::fs::Status::NOT_FOUND) return false;
		return creates(GONE_NAME, GONE_TEXT) && reads(GONE_NAME, GONE_TEXT) &&
			   removes(GONE_NAME) && missing(GONE_NAME);
	}

	/** Step 8: leaves a file on the volume so a later boot can check that it persisted. */
	bool keepsAFile() { return creates(KEEP_NAME, KEEP_TEXT) && reads(KEEP_NAME, KEEP_TEXT); }

	/** Step 9: bad flag combinations, missing parents, opening a directory for writing, unlinking a directory and creating in the read-only root get the exact error status. */
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

	/**
	 * @brief Creates a directory in the test directory.
	 * @param name Name relative to the test directory.
	 * @return True when the server accepted the request.
	 */
	bool makes(const char *name) {
		char path[PATH_BYTES];
		return join(path, name) && client.mkdir(path);
	}

	/**
	 * @brief Removes a directory of the test directory.
	 * @param name Name relative to the test directory.
	 * @return True when the server accepted the request.
	 */
	bool removesDirectory(const char *name) {
		char path[PATH_BYTES];
		return join(path, name) && client.rmdir(path);
	}

	/**
	 * @brief Renames or moves inside the test directory.
	 * @param from Existing name relative to the test directory.
	 * @param to New name relative to the test directory.
	 * @return True when the server accepted the request.
	 */
	bool renames(const char *from, const char *to) {
		char first[PATH_BYTES];
		char second[PATH_BYTES];
		return join(first, from) && join(second, to) && client.rename(first, second);
	}

	/**
	 * @brief Checks that an operation failed with an exact status.
	 * @param succeeded Result of the operation just made.
	 * @param expected Status the server must have reported.
	 * @return True when the operation failed and `lastStatus` equals `expected`.
	 */
	bool failsWith(bool succeeded, abi::fs::Status expected) {
		return !succeeded && client.lastStatus() == expected;
	}

	/**
	 * @brief Checks that a directory lists a name and never lists `.` or `..`.
	 * @param relative Directory relative to the test directory; empty for the test directory itself.
	 * @param wanted Name that must appear.
	 * @return True when the name is found, the dot entries are absent and the handle closes.
	 */
	bool listed(const char *relative, const char *wanted) {
		char path[PATH_BYTES];
		bool joined = relative[0] == '\0' ? true : join(path, relative);
		uint32_t handle = client.open(relative[0] == '\0' ? directory : path);
		if (!joined || handle == abi::fs::NO_HANDLE) return false;

		bool found = false;
		bool sawDots = false;
		uint32_t index = START;
		char name[TEXT_BYTES * 2] = {};
		abi::fs::FileType type = abi::fs::FileType::REGULAR;
		while (client.readdir(handle, index, name, sizeof(name), type)) {
			index++;
			if (strcmp(name, wanted) == 0) found = true;
			if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) sawDots = true;
		}
		return client.close(handle) && found && !sawDots;
	}

	/** Step 11: creates, nests, renames, moves and removes directories, with the errors for existing names, missing parents, non empty directories, moving a directory into itself, open files and moves between file systems (`CROSS_DEVICE`). */
	bool managesDirectories() {
		constexpr char INNER_TEXT[] = "inner file\n";
		constexpr char TREE_TEXT[] = "tree survives\n";
		constexpr char CROSSED[] = "/crossed.txt";
		bool ok = makes("sub") && failsWith(makes("sub"), abi::fs::Status::EXISTS) &&
				  failsWith(makes("nodir/deep"), abi::fs::Status::NOT_FOUND) &&
				  failsWith(makes("note.txt/under"), abi::fs::Status::NOT_FOUND);
		ok = ok && creates("sub/inner.txt", INNER_TEXT) && reads("sub/inner.txt", INNER_TEXT) &&
			 listed("sub", "inner.txt") && listed("", "sub");
		ok = ok && failsWith(removesDirectory("sub"), abi::fs::Status::NOT_EMPTY) &&
			 failsWith(removesDirectory(NOTE_NAME), abi::fs::Status::UNSUPPORTED) &&
			 failsWith(removesDirectory("absent"), abi::fs::Status::NOT_FOUND);
		ok = ok && renames("sub/inner.txt", "sub/moved.txt") &&
			 reads("sub/moved.txt", INNER_TEXT) && missing("sub/inner.txt");
		ok = ok && makes("sub2") && renames("sub/moved.txt", "sub2/final.txt") &&
			 reads("sub2/final.txt", INNER_TEXT);
		ok = ok && failsWith(renames(NOTE_NAME, "sub"), abi::fs::Status::EXISTS) &&
			 failsWith(renames("absent.txt", "other.txt"), abi::fs::Status::NOT_FOUND) &&
			 failsWith(renames(NOTE_NAME, "nodir/x.txt"), abi::fs::Status::NOT_FOUND);

		ok = ok && renames("sub2", "sub/inside") && reads("sub/inside/final.txt", INNER_TEXT) &&
			 missing("sub2/final.txt") && listed("sub", "inside");
		ok = ok && failsWith(renames("sub", "sub/inside/x"), abi::fs::Status::BAD_REQUEST) &&
			 failsWith(renames("sub", "sub/again"), abi::fs::Status::BAD_REQUEST);

		uint32_t reader = openNamed("sub/inside/final.txt", abi::fs::OPEN_READ_ONLY);
		ok = ok && reader != abi::fs::NO_HANDLE &&
			 failsWith(renames("sub/inside/final.txt", "sub/inside/other.txt"),
					   abi::fs::Status::BUSY);
		if (reader != abi::fs::NO_HANDLE) ok = client.close(reader) && ok;
		uint32_t directoryHandle = openNamed("sub", abi::fs::OPEN_READ_ONLY);
		ok = ok && directoryHandle != abi::fs::NO_HANDLE &&
			 failsWith(removesDirectory("sub"), abi::fs::Status::BUSY);
		if (directoryHandle != abi::fs::NO_HANDLE) ok = client.close(directoryHandle) && ok;

		ok = ok && removes("sub/inside/final.txt") && removesDirectory("sub/inside") &&
			 removesDirectory("sub") && missing("sub");
		char first[PATH_BYTES];
		ok = ok && join(first, KEEP_NAME) &&
			 failsWith(client.rename(first, CROSSED), abi::fs::Status::CROSS_DEVICE);

		ok = ok && makes("keepdir") && creates("keepdir/tmp.txt", TREE_TEXT) &&
			 renames("keepdir/tmp.txt", "keepdir/stay.txt") && reads("keepdir/stay.txt", TREE_TEXT);
		return ok;
	}

	/**
	 * @brief Makes a hard link inside the test directory.
	 * @param existing Existing name relative to the test directory.
	 * @param name New name relative to the test directory.
	 * @return True when the server accepted the request.
	 */
	bool links(const char *existing, const char *name) {
		char first[PATH_BYTES];
		char second[PATH_BYTES];
		return join(first, existing) && join(second, name) && client.link(first, second);
	}

	/** Step 15: hard links share the content and survive deleting the other name; links of directories, to existing names, to missing files and of open files are refused. With Windows style names the link must be `UNSUPPORTED`. */
	bool linksFiles() {
		constexpr char ORIGINAL[] = "lnk-a.txt";
		constexpr char ALIAS[] = "lnk-b.txt";
		constexpr char OTHER[] = "lnk-c.txt";
		constexpr char DIRECTORY[] = "lnk-dir";
		constexpr char LINK_TEXT[] = "one body, two names\n";
		constexpr char OTHER_TEXT[] = "a different file\n";
		bool ok = creates(ORIGINAL, LINK_TEXT);
		if (windowsNames)
			return ok && failsWith(links(ORIGINAL, ALIAS), abi::fs::Status::UNSUPPORTED) &&
				   missing(ALIAS) && removes(ORIGINAL);

		ok = ok && links(ORIGINAL, ALIAS) && reads(ALIAS, LINK_TEXT) && listed("", ALIAS);
		ok = ok && creates(OTHER, OTHER_TEXT) &&
			 failsWith(links(ORIGINAL, OTHER), abi::fs::Status::EXISTS) &&
			 failsWith(links("absent.txt", "lnk-d.txt"), abi::fs::Status::NOT_FOUND) &&
			 failsWith(links(ORIGINAL, "nodir/x.txt"), abi::fs::Status::NOT_FOUND);
		ok = ok && makes(DIRECTORY) &&
			 failsWith(links(DIRECTORY, "lnk-e"), abi::fs::Status::UNSUPPORTED) &&
			 removesDirectory(DIRECTORY);

		uint32_t reader = openNamed(ORIGINAL, abi::fs::OPEN_READ_ONLY);
		ok = ok && reader != abi::fs::NO_HANDLE &&
			 failsWith(links(ORIGINAL, "lnk-f.txt"), abi::fs::Status::BUSY);
		if (reader != abi::fs::NO_HANDLE) ok = client.close(reader) && ok;

		ok = ok && removes(ORIGINAL) && missing(ORIGINAL) && reads(ALIAS, LINK_TEXT);
		return ok && removes(ALIAS) && removes(OTHER) && missing(ALIAS);
	}

	/** Step 14: renaming onto an existing file replaces it, a directory replaces an empty directory only, a name cannot swap between file and directory, a busy target is refused and a rename that only changes the case works. */
	bool replacesNames() {
		constexpr char FIRST_FILE[] = "rep-a.txt";
		constexpr char SECOND_FILE[] = "rep-b.txt";
		constexpr char BUSY_FILE[] = "rep-c.txt";
		constexpr char MOVED_DIRECTORY[] = "rep-d1";
		constexpr char TARGET_DIRECTORY[] = "rep-d2";
		constexpr char KEPT_DIRECTORY[] = "rep-d3";
		constexpr char FULL_DIRECTORY[] = "rep-d4";
		constexpr char MOVED_CHILD[] = "rep-d1/x.txt";
		constexpr char TARGET_CHILD[] = "rep-d2/x.txt";
		constexpr char FULL_CHILD[] = "rep-d4/y.txt";
		constexpr char CASE_FROM[] = "Case.txt";
		constexpr char CASE_TO[] = "CASE.TXT";
		constexpr char FIRST_TEXT[] = "first body\n";
		constexpr char SECOND_TEXT[] = "second body\n";
		bool ok = creates(FIRST_FILE, FIRST_TEXT) && creates(SECOND_FILE, SECOND_TEXT) &&
				  renames(FIRST_FILE, SECOND_FILE) && reads(SECOND_FILE, FIRST_TEXT) &&
				  missing(FIRST_FILE);

		ok = ok && makes(MOVED_DIRECTORY) && makes(TARGET_DIRECTORY) &&
			 creates(MOVED_CHILD, FIRST_TEXT) && renames(MOVED_DIRECTORY, TARGET_DIRECTORY) &&
			 reads(TARGET_CHILD, FIRST_TEXT) && missing(MOVED_DIRECTORY);

		ok = ok && makes(KEPT_DIRECTORY) && makes(FULL_DIRECTORY) &&
			 creates(FULL_CHILD, SECOND_TEXT) &&
			 failsWith(renames(KEPT_DIRECTORY, FULL_DIRECTORY), abi::fs::Status::NOT_EMPTY) &&
			 reads(FULL_CHILD, SECOND_TEXT);

		ok = ok && failsWith(renames(SECOND_FILE, KEPT_DIRECTORY), abi::fs::Status::EXISTS) &&
			 failsWith(renames(KEPT_DIRECTORY, SECOND_FILE), abi::fs::Status::EXISTS);

		uint32_t reader = openNamed(SECOND_FILE, abi::fs::OPEN_READ_ONLY);
		ok = ok && creates(BUSY_FILE, FIRST_TEXT) && reader != abi::fs::NO_HANDLE &&
			 failsWith(renames(BUSY_FILE, SECOND_FILE), abi::fs::Status::BUSY);
		if (reader != abi::fs::NO_HANDLE) ok = client.close(reader) && ok;

		ok = ok && creates(CASE_FROM, FIRST_TEXT) && renames(CASE_FROM, CASE_TO) &&
			 reads(CASE_TO, FIRST_TEXT) && listed("", CASE_TO);

		return ok && removes(SECOND_FILE) && removes(BUSY_FILE) && removes(TARGET_CHILD) &&
			   removesDirectory(TARGET_DIRECTORY) && removesDirectory(KEPT_DIRECTORY) &&
			   removes(FULL_CHILD) && removesDirectory(FULL_DIRECTORY) && removes(CASE_TO);
	}

	/** Step 12: long, mixed case and non ASCII names are created, read, listed and renamed, many long names spread over directory entries work, and with Windows style names forbidden characters and a trailing dot or space are refused. */
	bool usesLongNames() {
		constexpr char LONG_NAME[] = "A long file name.txt";
		constexpr char MIXED_NAME[] = "Mixed Case.TXT";
		constexpr char UNICODE_NAME[] = "caf\xC3\xA9 \xC3\xB1.txt";
		constexpr char RENAMED_NAME[] = "renamed long name.txt";
		constexpr char PERSIST_NAME[] = "Persist Long Name.txt";
		constexpr char LONG_TEXT[] = "long names work\n";
		constexpr char PERSIST_TEXT[] = "long name on disk\n";
		constexpr char SPREAD_PREFIX[] = "a-rather-long-file-name-for-testing-";
		constexpr uint32_t SPREAD_FILES = 4;
		constexpr char DIGIT_FIRST = '0';
		constexpr char SPREAD_SUFFIX[] = ".txt";
		bool ok = creates(LONG_NAME, LONG_TEXT) && reads(LONG_NAME, LONG_TEXT) &&
				  listed("", LONG_NAME);
		ok = ok && creates(MIXED_NAME, LONG_TEXT) && reads(MIXED_NAME, LONG_TEXT) &&
			 creates(UNICODE_NAME, LONG_TEXT) && reads(UNICODE_NAME, LONG_TEXT) &&
			 listed("", UNICODE_NAME);
		ok = ok && renames(MIXED_NAME, RENAMED_NAME) && reads(RENAMED_NAME, LONG_TEXT) &&
			 missing(MIXED_NAME) && listed("", RENAMED_NAME);
		ok = ok && removes(LONG_NAME) && removes(UNICODE_NAME) && removes(RENAMED_NAME) &&
			 missing(LONG_NAME);

		char name[PATH_BYTES];
		size_t prefix = strlen(SPREAD_PREFIX);
		memcpy(name, SPREAD_PREFIX, prefix);
		memcpy(name + prefix + sizeof(char), SPREAD_SUFFIX, sizeof(SPREAD_SUFFIX));
		for (uint32_t index = 0; ok && index < SPREAD_FILES; index++) {
			name[prefix] = static_cast<char>(DIGIT_FIRST + index);
			ok = creates(name, LONG_TEXT);
		}
		for (uint32_t index = 0; ok && index < SPREAD_FILES; index++) {
			name[prefix] = static_cast<char>(DIGIT_FIRST + index);
			ok = reads(name, LONG_TEXT) && listed("", name);
		}
		for (uint32_t index = 0; ok && index < SPREAD_FILES; index++) {
			name[prefix] = static_cast<char>(DIGIT_FIRST + index);
			ok = removes(name);
		}

		if (windowsNames) {
			ok = ok && refuses(pathOf("bad*name.txt"), WRITE_CREATE, abi::fs::Status::BAD_REQUEST) &&
				 refuses(pathOf("trailing dot."), WRITE_CREATE, abi::fs::Status::BAD_REQUEST) &&
				 refuses(pathOf("trailing space "), WRITE_CREATE, abi::fs::Status::BAD_REQUEST) &&
				 refuses(pathOf("quote\"d.txt"), WRITE_CREATE, abi::fs::Status::BAD_REQUEST);
		}
		return ok && creates(PERSIST_NAME, PERSIST_TEXT) && reads(PERSIST_NAME, PERSIST_TEXT);
	}

	/**
	 * @brief Builds a path in the scratch buffer.
	 * @param name Name relative to the test directory.
	 * @return Pointer to the internal buffer, valid until the next call.
	 */
	const char *pathOf(const char *name) {
		join(scratchPath, name);
		return scratchPath;
	}

	/** Step 13: long names that share their first characters get distinct short aliases, and a freed alias can be reused. */
	bool aliasesDoNotCollide() {
		constexpr char FIRST_NAME[] = "longname_one.txt";
		constexpr char SECOND_NAME[] = "longname_two.txt";
		constexpr char THIRD_NAME[] = "longname_three.txt";
		constexpr char TEXT[] = "alias\n";
		bool ok = creates(FIRST_NAME, TEXT) && creates(SECOND_NAME, TEXT) &&
				  reads(FIRST_NAME, TEXT) && reads(SECOND_NAME, TEXT);
		ok = ok && removes(FIRST_NAME) && creates(THIRD_NAME, TEXT) && reads(THIRD_NAME, TEXT) &&
			 listed("", SECOND_NAME) && listed("", THIRD_NAME);
		return ok && removes(SECOND_NAME) && removes(THIRD_NAME) && missing(FIRST_NAME);
	}

	/** Step 10: listing the test directory shows the files that exist and none of the deleted ones. */
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
