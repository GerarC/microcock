#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr char EXT_FILE[] = "/ext/scratch/cfile.txt";
constexpr char EXT_GONE[] = "/ext/scratch/cgone.txt";
constexpr char FAT_FILE[] = "/fat/SCRATCH/cfile.txt";
constexpr char FAT_GONE[] = "/fat/SCRATCH/cgone.txt";
constexpr char MISSING_FILE[] = "/ext/scratch/cmissing.txt";
constexpr char FIRST_TEXT[] = "C library wrote this";
constexpr char SECOND_TEXT[] = ", then appended";
constexpr char REPLACED_TEXT[] = "wrote";
constexpr char FINAL_TEXT[] = "C library wrote this, then appended";
constexpr char CHECKED_TEXT[] = "C library wrote this";
constexpr size_t FIRST_BYTES = sizeof(FIRST_TEXT) - sizeof(char);
constexpr size_t SECOND_BYTES = sizeof(SECOND_TEXT) - sizeof(char);
constexpr size_t FINAL_BYTES = sizeof(FINAL_TEXT) - sizeof(char);
constexpr size_t REPLACED_BYTES = sizeof(REPLACED_TEXT) - sizeof(char);
constexpr size_t REPLACE_AT = 2;
constexpr size_t BUFFER_BYTES = 64;
constexpr int INVALID_DESCRIPTOR = 99;
constexpr int NO_OFFSET = 0;
constexpr int EXIT_SERVED = 0;
constexpr int EXIT_REFUSED = 1;
constexpr ssize_t FAILED = -1;

bool writesAll(int fd, const char *text, size_t length) {
	return write(fd, text, length) == static_cast<ssize_t>(length);
}

bool readsBack(const char *path, const char *text, size_t length) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) return false;

	char buffer[BUFFER_BYTES] = {};
	bool ok = read(fd, buffer, sizeof(buffer)) == static_cast<ssize_t>(length) &&
			  memcmp(buffer, text, length) == 0 && read(fd, buffer, sizeof(buffer)) == 0;
	return close(fd) == 0 && ok;
}

bool worksOn(const char *file, const char *gone) {
	int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0) return false;

	bool ok = writesAll(fd, FIRST_TEXT, FIRST_BYTES);
	char buffer[BUFFER_BYTES] = {};
	ok = ok && read(fd, buffer, sizeof(buffer)) == FAILED;
	ok = close(fd) == 0 && ok;
	ok = ok && readsBack(file, CHECKED_TEXT, FIRST_BYTES);

	fd = open(file, O_WRONLY | O_APPEND);
	ok = ok && fd >= 0 && writesAll(fd, SECOND_TEXT, SECOND_BYTES);
	ok = ok && close(fd) == 0 && readsBack(file, FINAL_TEXT, FINAL_BYTES);

	fd = open(file, O_RDWR);
	ok = ok && fd >= 0 && lseek(fd, REPLACE_AT, SEEK_SET) == static_cast<off_t>(REPLACE_AT);
	ok = ok && writesAll(fd, REPLACED_TEXT, REPLACED_BYTES);
	ok = ok && lseek(fd, NO_OFFSET, SEEK_CUR) == static_cast<off_t>(REPLACE_AT + REPLACED_BYTES);
	ok = ok && lseek(fd, NO_OFFSET, SEEK_END) == static_cast<off_t>(FINAL_BYTES);
	ok = ok && lseek(fd, -static_cast<off_t>(SECOND_BYTES), SEEK_END) ==
				   static_cast<off_t>(FIRST_BYTES);
	ok = ok && read(fd, buffer, sizeof(buffer)) == static_cast<ssize_t>(SECOND_BYTES) &&
		 memcmp(buffer, SECOND_TEXT, SECOND_BYTES) == 0;
	ok = ok && lseek(fd, -static_cast<off_t>(FINAL_BYTES) - 1, SEEK_END) == FAILED;
	ok = ok && close(fd) == 0;

	char expected[BUFFER_BYTES] = {};
	memcpy(expected, FINAL_TEXT, FINAL_BYTES);
	memcpy(expected + REPLACE_AT, REPLACED_TEXT, REPLACED_BYTES);
	ok = ok && readsBack(file, expected, FINAL_BYTES);

	fd = open(file, O_WRONLY | O_TRUNC);
	ok = ok && fd >= 0 && close(fd) == 0 && readsBack(file, "", 0);
	fd = open(file, O_WRONLY);
	ok = ok && fd >= 0 && writesAll(fd, FIRST_TEXT, FIRST_BYTES);
	ok = ok && close(fd) == 0;
	fd = open(file, O_WRONLY | O_APPEND);
	ok = ok && fd >= 0 && writesAll(fd, SECOND_TEXT, SECOND_BYTES) && close(fd) == 0;

	fd = open(gone, O_WRONLY | O_CREAT);
	ok = ok && fd >= 0 && writesAll(fd, FIRST_TEXT, FIRST_BYTES) && close(fd) == 0;
	ok = ok && unlink(gone) == 0 && open(gone, O_RDONLY) < 0 && unlink(gone) < 0;
	return ok;
}

bool refusesBadCalls() {
	char buffer[BUFFER_BYTES];
	return open(MISSING_FILE, O_RDONLY) < 0 && open(EXT_FILE, O_RDONLY | O_ACCMODE) < 0 &&
		   open(nullptr, O_RDONLY) < 0 && read(INVALID_DESCRIPTOR, buffer, sizeof(buffer)) < 0 &&
		   write(INVALID_DESCRIPTOR, buffer, sizeof(buffer)) < 0 && close(INVALID_DESCRIPTOR) < 0 &&
		   lseek(INVALID_DESCRIPTOR, NO_OFFSET, SEEK_SET) < 0;
}

}

extern "C" void _start() {
	bool served = worksOn(EXT_FILE, EXT_GONE);
	if (served) puts("cfile: ext ok");
	served = served && worksOn(FAT_FILE, FAT_GONE);
	if (served) puts("cfile: fat ok");
	served = served && refusesBadCalls();

	puts(served ? "cfile: ok" : "cfile: refused");
	_exit(served ? EXIT_SERVED : EXIT_REFUSED);
}
