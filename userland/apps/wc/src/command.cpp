#include <cock/user/stream.hpp>
#include <cock/user/tool.hpp>
#include <stdio.h>
#include <wc/command.hpp>
#include <wc/counts.hpp>

using namespace cock::user::tool;

namespace cock::wc {

namespace {

constexpr char TOTAL_NAME[] = "total";

int countInput(const char *program, const char *path, Counts &counts) {
	return readPath(program, path, [&](const char *bytes, size_t count) {
		counts.add(bytes, count);
		return Flow::MORE;
	});
}

void printCounts(const Counts &counts, const char *name) {
	if (name == nullptr) printf("%u %u %u\n", counts.lines, counts.words, counts.bytes);
	else printf("%u %u %u %s\n", counts.lines, counts.words, counts.bytes, name);
}

} // namespace

int run(int argc, char **argv) {
	if (argumentCount(argc) == NO_ARGUMENTS) {
		Counts counts;
		int status = countInput(argv[0], nullptr, counts);
		if (status == STATUS_OK) printCounts(counts, nullptr);
		return status;
	}

	Counts total;
	int status = STATUS_OK;
	for (int index = FIRST_ARGUMENT; index < argc; index++) {
		Counts counts;
		if (countInput(argv[0], argv[index], counts) != STATUS_OK) {
			status = STATUS_FAILURE;
			continue;
		}
		printCounts(counts, argv[index]);
		total.merge(counts);
	}
	if (argumentCount(argc) > ONE_ARGUMENT) printCounts(total, TOTAL_NAME);
	return status;
}

} // namespace cock::wc
