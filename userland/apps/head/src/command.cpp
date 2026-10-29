#include <cock/user/stream.hpp>
#include <cock/user/tool.hpp>
#include <head/command.hpp>
#include <head/options.hpp>
#include <head/window.hpp>
#include <string.h>

using namespace cock::user::tool;

namespace cock::head {

namespace {

constexpr char BANNER_OPEN[] = "==> ";
constexpr char BANNER_CLOSE[] = " <==\n";
constexpr char BLANK_LINE[] = "\n";

void writeText(const char *text) {
	writeAll(STDOUT_DESCRIPTOR, text, strlen(text));
}

void printBanner(const char *path, bool first) {
	if (!first) writeText(BLANK_LINE);
	writeText(BANNER_OPEN);
	writeText(path);
	writeText(BANNER_CLOSE);
}

int printHead(const char *program, const char *path, uint32_t lines) {
	Window window(lines);
	if (window.closed()) return STATUS_OK;

	return readPath(program, path, [&](const char *bytes, size_t count) {
		size_t inside = window.take(bytes, count);
		if (inside != 0 && !writeAll(STDOUT_DESCRIPTOR, bytes, inside)) return Flow::ERROR;
		return window.closed() ? Flow::DONE : Flow::MORE;
	});
}

} // namespace

int run(int argc, char **argv) {
	Options options;
	if (!parseOptions(argc, argv, options)) return usage(argv[0], "[-n lines] [path...]");

	if (options.firstPath >= argc) return printHead(argv[0], nullptr, options.lines);

	bool several = argc - options.firstPath > ONE_ARGUMENT;
	int status = STATUS_OK;
	for (int index = options.firstPath; index < argc; index++) {
		if (several) printBanner(argv[index], index == options.firstPath);
		if (printHead(argv[0], argv[index], options.lines) != STATUS_OK) status = STATUS_FAILURE;
	}
	return status;
}

} // namespace cock::head
