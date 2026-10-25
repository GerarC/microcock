#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr char EXT_FILE[] = "/ext/scratch/cfile.txt";
constexpr char EXT_GONE[] = "/ext/scratch/cgone.txt";
constexpr char EXT_REDIRECTED[] = "/ext/scratch/credirected.txt";
constexpr char FAT_FILE[] = "/fat/SCRATCH/cfile.txt";
constexpr char FAT_GONE[] = "/fat/SCRATCH/cgone.txt";
constexpr char MISSING_FILE[] = "/ext/scratch/cmissing.txt";
constexpr char ROOT_FILE[] = "/hello.txt";
constexpr char CROSSED_FILE[] = "/crossed.txt";
constexpr char EXT_SCRATCH[] = "/ext/scratch";
constexpr char FAT_SCRATCH[] = "/fat/SCRATCH";
constexpr char DIRECTORY_NAME[] = "cdir";
constexpr char RENAMED_DIRECTORY_NAME[] = "cdir2";
constexpr char INNER_NAME[] = "inner.txt";
constexpr char MOVED_NAME[] = "moved.txt";
constexpr uint32_t PING_MARK = 0x70696E67;
constexpr mode_t NO_MODE = 0;
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
constexpr char REDIRECTED_TEXT[] = "redirected";
constexpr size_t REDIRECTED_BYTES = sizeof(REDIRECTED_TEXT) - sizeof(char);
constexpr size_t PREFIX_BYTES = 5;
constexpr size_t SECOND_PREFIX_END = PREFIX_BYTES + PREFIX_BYTES;
constexpr off_t SEEK_TARGET = 3;
constexpr int STDIN_DESCRIPTOR = 0;
constexpr int STDOUT_DESCRIPTOR = 1;
constexpr int HIGH_DESCRIPTOR = 20;
constexpr int BEYOND_TABLE_DESCRIPTOR = 1000;
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


bool failsWith(int result, int expected) { return result < 0 && errno == expected; }

bool refusesBadCalls() {
	char buffer[BUFFER_BYTES];
	return failsWith(open(MISSING_FILE, O_RDONLY), ENOENT) &&
		   failsWith(open(EXT_FILE, O_RDONLY | O_ACCMODE), EINVAL) &&
		   failsWith(open(nullptr, O_RDONLY), EINVAL) &&
		   failsWith(static_cast<int>(read(INVALID_DESCRIPTOR, buffer, sizeof(buffer))), EBADF) &&
		   failsWith(static_cast<int>(write(INVALID_DESCRIPTOR, buffer, sizeof(buffer))), EBADF) &&
		   failsWith(close(INVALID_DESCRIPTOR), EBADF) &&
		   failsWith(lseek(INVALID_DESCRIPTOR, NO_OFFSET, SEEK_SET), EBADF) &&
		   failsWith(open(ROOT_FILE, O_WRONLY), EROFS) && failsWith(unlink(MISSING_FILE), ENOENT) &&
		   failsWith(rename(EXT_FILE, CROSSED_FILE), EXDEV);
}

void join(char *path, const char *base, const char *name) {
	size_t length = strlen(base);
	memcpy(path, base, length);
	path[length] = '/';
	memcpy(path + length + sizeof(char), name, strlen(name) + sizeof(char));
}

bool worksOnDirectories(const char *base) {
	char directory[BUFFER_BYTES];
	char inner[BUFFER_BYTES];
	char moved[BUFFER_BYTES];
	char renamed[BUFFER_BYTES];
	join(directory, base, DIRECTORY_NAME);
	join(inner, directory, INNER_NAME);
	join(moved, directory, MOVED_NAME);
	join(renamed, base, RENAMED_DIRECTORY_NAME);

	bool ok = mkdir(directory, NO_MODE) == 0 && failsWith(mkdir(directory, NO_MODE), EEXIST);
	int fd = open(inner, O_WRONLY | O_CREAT);
	ok = ok && fd >= 0 && writesAll(fd, FIRST_TEXT, FIRST_BYTES) && close(fd) == 0;
	ok = ok && failsWith(rmdir(directory), ENOTEMPTY) && rename(inner, moved) == 0 &&
		 readsBack(moved, CHECKED_TEXT, FIRST_BYTES) && failsWith(open(inner, O_RDONLY), ENOENT);
	ok = ok && rename(directory, renamed) == 0 && failsWith(rmdir(directory), ENOENT);

	char movedAgain[BUFFER_BYTES];
	join(movedAgain, renamed, MOVED_NAME);
	ok = ok && readsBack(movedAgain, CHECKED_TEXT, FIRST_BYTES) && unlink(movedAgain) == 0 &&
		 rmdir(renamed) == 0 && failsWith(rmdir(renamed), ENOENT);
	return ok;
}

bool sharesOffset(const char *file) {
	char buffer[BUFFER_BYTES] = {};
	int fd = open(file, O_RDONLY);
	if (fd < 0) return false;

	int copy = dup(fd);
	bool ok = copy >= 0 && copy != fd;
	ok = ok && read(fd, buffer, PREFIX_BYTES) == static_cast<ssize_t>(PREFIX_BYTES) &&
		 memcmp(buffer, FIRST_TEXT, PREFIX_BYTES) == 0;
	ok = ok && lseek(copy, NO_OFFSET, SEEK_CUR) == static_cast<off_t>(PREFIX_BYTES);
	ok = ok && read(copy, buffer, PREFIX_BYTES) == static_cast<ssize_t>(PREFIX_BYTES) &&
		 memcmp(buffer, FIRST_TEXT + PREFIX_BYTES, PREFIX_BYTES) == 0;
	ok = ok && close(fd) == 0;
	ok = ok && lseek(copy, NO_OFFSET, SEEK_CUR) == static_cast<off_t>(SECOND_PREFIX_END);
	ok = ok && close(copy) == 0 && failsWith(close(copy), EBADF);
	return ok && failsWith(static_cast<int>(read(copy, buffer, sizeof(buffer))), EBADF);
}

bool replacesDescriptor(const char *file) {
	int fd = open(file, O_RDONLY);
	bool ok = fd >= 0 && dup2(fd, HIGH_DESCRIPTOR) == HIGH_DESCRIPTOR;
	ok = ok && lseek(HIGH_DESCRIPTOR, SEEK_TARGET, SEEK_SET) == SEEK_TARGET &&
		 lseek(fd, NO_OFFSET, SEEK_CUR) == SEEK_TARGET;
	ok = ok && dup2(fd, fd) == fd && failsWith(dup2(fd, BEYOND_TABLE_DESCRIPTOR), EBADF) &&
		 failsWith(dup2(BEYOND_TABLE_DESCRIPTOR, fd), EBADF) && failsWith(dup(BEYOND_TABLE_DESCRIPTOR), EBADF);
	return ok && close(fd) == 0 && close(HIGH_DESCRIPTOR) == 0;
}

bool keepsConsoleModes() {
	char buffer[BUFFER_BYTES];
	return failsWith(lseek(STDOUT_DESCRIPTOR, NO_OFFSET, SEEK_CUR), ESPIPE) &&
		   failsWith(static_cast<int>(read(STDOUT_DESCRIPTOR, buffer, sizeof(buffer))), EBADF) &&
		   failsWith(static_cast<int>(write(STDIN_DESCRIPTOR, buffer, sizeof(buffer))), EBADF);
}

bool redirectsOutput(const char *file) {
	int saved = dup(STDOUT_DESCRIPTOR);
	int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC);
	bool ok = saved >= 0 && fd >= 0 && dup2(fd, STDOUT_DESCRIPTOR) == STDOUT_DESCRIPTOR;
	ok = ok && writesAll(STDOUT_DESCRIPTOR, REDIRECTED_TEXT, REDIRECTED_BYTES);
	if (saved >= 0) ok = dup2(saved, STDOUT_DESCRIPTOR) == STDOUT_DESCRIPTOR && ok;
	if (saved >= 0) ok = close(saved) == 0 && ok;
	if (fd >= 0) ok = close(fd) == 0 && ok;
	ok = ok && readsBack(file, REDIRECTED_TEXT, REDIRECTED_BYTES);
	return unlink(file) == 0 && ok;
}

bool keepsForeignMessages() {
	cock::ipc::Message ping = {};
	ping.type = cock::ipc::MessageType::SYS_PING;
	ping.payload.args.arg0 = PING_MARK;
	if (!cock::user::ipc_send(cock::user::self(), ping)) return false;

	int fd = open(EXT_FILE, O_RDONLY);
	if (fd < 0 || close(fd) != 0) return false;

	cock::ipc::Message received = {};
	return cock::user::ipc_recv(received) && received.type == cock::ipc::MessageType::SYS_PING &&
		   received.payload.args.arg0 == PING_MARK;
}

}

extern "C" int main() {
	bool served = worksOn(EXT_FILE, EXT_GONE);
	if (served) puts("cfile: ext ok");
	served = served && worksOn(FAT_FILE, FAT_GONE);
	if (served) puts("cfile: fat ok");
	served = served && worksOnDirectories(EXT_SCRATCH) && worksOnDirectories(FAT_SCRATCH);
	if (served) puts("cfile: directories ok");
	served = served && refusesBadCalls();
	if (served) puts("cfile: errors ok");
	served = served && sharesOffset(EXT_FILE) && replacesDescriptor(EXT_FILE) && keepsConsoleModes() &&
			 redirectsOutput(EXT_REDIRECTED);
	if (served) puts("cfile: descriptors ok");
	served = served && keepsForeignMessages();

	puts(served ? "cfile: ok" : "cfile: refused");
	_exit(served ? EXIT_SERVED : EXIT_REFUSED);
}
