#include <cock/user/stream.hpp>
#include <cp/copy.hpp>
#include <fcntl.h>
#include <unistd.h>

using namespace cock::user::tool;

namespace cock::cp {

int copyFile(const char *program, const char *from, const char *to) {
	int source = ::open(from, O_RDONLY);
	if (source < 0) return fail(program, from);

	int target = ::open(to, O_WRONLY | O_CREAT | O_TRUNC);
	if (target < 0) {
		int status = fail(program, to);
		::close(source);
		return status;
	}

	int status = readDescriptor(program, from, source, [&](const char *bytes, size_t count) {
		if (writeAll(target, bytes, count)) return Flow::MORE;
		fail(program, to);
		return Flow::ERROR;
	});

	::close(source);
	if (::close(target) < 0 && status == STATUS_OK) status = fail(program, to);
	return status;
}

} // namespace cock::cp
