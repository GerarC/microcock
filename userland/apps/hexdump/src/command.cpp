#include <cock/user/stream.hpp>
#include <cock/user/tool.hpp>
#include <hexdump/command.hpp>
#include <hexdump/row.hpp>

using namespace cock::user::tool;

namespace cock::hexdump {

namespace {

constexpr char OUTPUT_NAME[] = "standard output";

}

int run(int argc, char **argv) {
	if (argumentCount(argc) > ONE_ARGUMENT) return usage(argv[0], "[path]");

	const char *path = argumentCount(argc) == ONE_ARGUMENT ? argv[FIRST_ARGUMENT] : nullptr;
	RowWriter rows;
	int status = readPath(argv[0], path, [&](const char *bytes, size_t count) {
		for (size_t index = 0; index < count; index++) {
			if (rows.add(static_cast<uint8_t>(bytes[index]))) continue;
			fail(argv[0], OUTPUT_NAME);
			return Flow::ERROR;
		}
		return Flow::MORE;
	});
	if (status != STATUS_OK) return status;
	return rows.finish() ? STATUS_OK : fail(argv[0], OUTPUT_NAME);
}

} // namespace cock::hexdump
