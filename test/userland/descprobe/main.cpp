#include <cock/abi/fs.hpp>
#include <cock/abi/names.hpp>
#include <cock/abi/pm.hpp>
#include <cock/user/api.hpp>
#include <cock/user/exec.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/names.hpp>
#include <cock/user/pm.hpp>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using cock::abi::pm::Status;
using cock::user::pm::Client;
using cock::user::pm::Forked;
using cock::user::pm::Waited;

namespace {

constexpr char PM_NAME[] = "pm";
constexpr char HELLO_PATH[] = "/hello.txt";
constexpr char CHILD_PATH[] = "/bin/descchild.elf";
constexpr char CHILD_NAME[] = "descchild";
constexpr char CHILD_TEXT[] = "from child";
constexpr char REDIRECTED_TEXT[] = "redirected child";
constexpr char EXEC_REPORT[] = "exec ok";
constexpr char FILE_START[] = "Hello";
constexpr char FILE_NEXT[] = " from";
constexpr size_t PART_BYTES = 5;
constexpr size_t BUFFER_BYTES = 64;
constexpr size_t NUMBER_CHARACTERS = 11;
constexpr uint32_t DECIMAL_BASE = 10;
constexpr uint32_t CHILD_ARGUMENTS = 4;
constexpr int STDOUT_DESCRIPTOR = 1;
constexpr int PAIR_SIZE = 2;
constexpr int READ_SIDE = 0;
constexpr int WRITE_SIDE = 1;
constexpr int EXIT_OK = 0;
constexpr int EXIT_CHILD_FAILED = 1;
constexpr int EXIT_MAIN_FAILED = 1;
constexpr ssize_t FAILED = -1;

struct Reading {
	char text[BUFFER_BYTES];
	size_t length;
};

bool reachesPm(cock::user::names::Connection &manager) {
	manager = cock::user::names::connect(PM_NAME);
	return manager.status == cock::abi::names::Status::OK;
}

bool collects(Client &client, uint32_t child) {
	Waited waited = client.wait(child);
	bool collected = waited.status == Status::OK && waited.pid == child &&
					 waited.exitStatus == static_cast<uint32_t>(EXIT_OK);
	if (!collected)
		printf("descprobe: child %u ended with status %u, wait said %u\n", static_cast<unsigned>(child),
			   static_cast<unsigned>(waited.exitStatus), static_cast<unsigned>(waited.status));
	return collected;
}

ssize_t readsUntilEnd(int descriptor, Reading &reading) {
	reading.length = 0;
	for (;;) {
		ssize_t got = read(descriptor, reading.text + reading.length, sizeof(reading.text) - reading.length - 1);
		if (got < 0) return FAILED;
		if (got == 0) break;
		reading.length += static_cast<size_t>(got);
	}
	reading.text[reading.length] = '\0';
	return static_cast<ssize_t>(reading.length);
}

bool says(const Reading &reading, const char *expected) {
	bool same = reading.length == strlen(expected) && memcmp(reading.text, expected, reading.length) == 0;
	if (!same) printf("descprobe: read \"%s\", wanted \"%s\"\n", reading.text, expected);
	return same;
}

bool readsPart(int descriptor, const char *expected) {
	char buffer[BUFFER_BYTES] = {};
	return read(descriptor, buffer, PART_BYTES) == static_cast<ssize_t>(PART_BYTES) &&
		   memcmp(buffer, expected, PART_BYTES) == 0;
}

void format(int value, char (&text)[NUMBER_CHARACTERS + 1]) {
	char reversed[NUMBER_CHARACTERS];
	size_t length = 0;
	uint32_t rest = static_cast<uint32_t>(value);
	do {
		reversed[length++] = static_cast<char>('0' + rest % DECIMAL_BASE);
		rest /= DECIMAL_BASE;
	} while (rest != 0);
	for (size_t index = 0; index < length; index++) text[index] = reversed[length - 1 - index];
	text[length] = '\0';
}

bool inheritsPipe(Client &client) {
	int ends[PAIR_SIZE];
	if (pipe(ends) != 0) return false;

	Forked forked = client.fork();
	if (forked.status != Status::OK) return false;
	if (forked.isChild())
		_exit(write(ends[WRITE_SIDE], CHILD_TEXT, strlen(CHILD_TEXT)) == static_cast<ssize_t>(strlen(CHILD_TEXT))
				  ? EXIT_OK
				  : EXIT_CHILD_FAILED);

	Reading reading = {};
	bool ok = close(ends[WRITE_SIDE]) == 0 && readsUntilEnd(ends[READ_SIDE], reading) >= 0 && says(reading, CHILD_TEXT);
	ok = collects(client, forked.pid) && ok;
	return close(ends[READ_SIDE]) == 0 && ok;
}

bool inheritsFile(Client &client) {
	int descriptor = open(HELLO_PATH, O_RDONLY);
	if (descriptor < 0 || !readsPart(descriptor, FILE_START)) return false;

	Forked forked = client.fork();
	if (forked.status != Status::OK) return false;
	if (forked.isChild()) _exit(readsPart(descriptor, FILE_NEXT) && close(descriptor) == 0 ? EXIT_OK : EXIT_CHILD_FAILED);

	bool ok = collects(client, forked.pid);
	ok = ok && readsPart(descriptor, FILE_NEXT);
	return close(descriptor) == 0 && ok;
}

bool inheritsRedirectedOutput(Client &client) {
	int ends[PAIR_SIZE];
	if (pipe(ends) != 0) return false;

	int saved = dup(STDOUT_DESCRIPTOR);
	bool ok = saved >= 0 && dup2(ends[WRITE_SIDE], STDOUT_DESCRIPTOR) == STDOUT_DESCRIPTOR;
	Forked forked = client.fork();
	if (forked.status != Status::OK) return false;
	if (forked.isChild())
		_exit(write(STDOUT_DESCRIPTOR, REDIRECTED_TEXT, strlen(REDIRECTED_TEXT)) ==
					  static_cast<ssize_t>(strlen(REDIRECTED_TEXT))
				  ? EXIT_OK
				  : EXIT_CHILD_FAILED);

	if (saved >= 0) ok = dup2(saved, STDOUT_DESCRIPTOR) == STDOUT_DESCRIPTOR && ok;
	if (saved >= 0) ok = close(saved) == 0 && ok;
	Reading reading = {};
	ok = close(ends[WRITE_SIDE]) == 0 && readsUntilEnd(ends[READ_SIDE], reading) >= 0 && says(reading, REDIRECTED_TEXT) && ok;
	ok = collects(client, forked.pid) && ok;
	return close(ends[READ_SIDE]) == 0 && ok;
}

[[noreturn]] void replaceWithChild(Client &client, int writeEnd, int fileDescriptor) {
	cock::user::fs::Client files;
	if (files.connect(cock::abi::fs::SERVER_NAME, true)) {
		char pipeText[NUMBER_CHARACTERS + 1];
		char fileText[NUMBER_CHARACTERS + 1];
		format(writeEnd, pipeText);
		format(fileDescriptor, fileText);
		const char *argv[CHILD_ARGUMENTS] = {CHILD_NAME, pipeText, fileText, FILE_NEXT};
		cock::user::exec(client, files, CHILD_PATH, argv, CHILD_ARGUMENTS);
	}
	_exit(EXIT_CHILD_FAILED);
}

bool keepsDescriptorsThroughExec(Client &client) {
	int ends[PAIR_SIZE];
	if (pipe(ends) != 0) return false;
	int descriptor = open(HELLO_PATH, O_RDONLY);
	if (descriptor < 0 || !readsPart(descriptor, FILE_START)) return false;

	Forked forked = client.fork();
	if (forked.status != Status::OK) return false;
	if (forked.isChild()) replaceWithChild(client, ends[WRITE_SIDE], descriptor);

	Reading reading = {};
	bool ok = close(ends[WRITE_SIDE]) == 0 && readsUntilEnd(ends[READ_SIDE], reading) >= 0 && says(reading, EXEC_REPORT);
	ok = collects(client, forked.pid) && ok;
	ok = close(descriptor) == 0 && ok;
	return close(ends[READ_SIDE]) == 0 && ok;
}

}

extern "C" int main() {
	cock::user::names::Connection manager = {};
	if (!reachesPm(manager)) {
		puts("descprobe: cannot reach pm");
		return EXIT_MAIN_FAILED;
	}
	Client client(manager.handle);

	bool passed = inheritsPipe(client);
	if (passed) puts("descprobe: pipe inherited");
	passed = passed && inheritsFile(client);
	if (passed) puts("descprobe: file inherited");
	passed = passed && inheritsRedirectedOutput(client);
	if (passed) puts("descprobe: output redirected");
	passed = passed && keepsDescriptorsThroughExec(client);
	if (passed) puts("descprobe: exec kept descriptors");

	puts(passed ? "descprobe: ok" : "descprobe: refused");
	return passed ? EXIT_OK : EXIT_MAIN_FAILED;
}
