#include <cock/user/stream.hpp>
#include <cock/user/tool.hpp>
#include <grep/command.hpp>
#include <grep/matcher.hpp>

using namespace cock::user::tool;

namespace cock::grep {

namespace {

constexpr int TEXT_ARGUMENT = 1;
constexpr int FIRST_PATH = 2;

int searchInput(const char *program, const char *text, const char *path, bool label, bool &matched) {
	LineMatcher matcher(program, text, label ? path : nullptr);
	int status = readPath(program, path, [&](const char *bytes, size_t count) { return matcher.add(bytes, count); });
	if (status == STATUS_OK && matcher.finish() == Flow::ERROR) status = STATUS_FAILURE;
	if (matcher.matched()) matched = true;
	return status;
}

} // namespace

int run(int argc, char **argv) {
	if (argumentCount(argc) == NO_ARGUMENTS) {
		usage(argv[0], "text [path...]");
		return STATUS_ERROR;
	}

	const char *text = argv[TEXT_ARGUMENT];
	bool matched = false;
	bool failed = false;
	if (argc == FIRST_PATH) {
		failed = searchInput(argv[0], text, nullptr, false, matched) != STATUS_OK;
	} else {
		bool label = argc - FIRST_PATH > ONE_ARGUMENT;
		for (int index = FIRST_PATH; index < argc; index++)
			if (searchInput(argv[0], text, argv[index], label, matched) != STATUS_OK) failed = true;
	}

	if (failed) return STATUS_ERROR;
	return matched ? STATUS_MATCHED : STATUS_NO_MATCH;
}

} // namespace cock::grep
