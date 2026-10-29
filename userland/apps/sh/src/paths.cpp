#include <cock/path/resolve.hpp>
#include <sh/paths.hpp>
#include <stdio.h>
#include <unistd.h>

namespace cock::sh {

bool resolvePath(const char *command, const char *path, char (&out)[PATH_BYTES]) {
	char directory[PATH_BYTES] = {};
	if (getcwd(directory, sizeof(directory)) == nullptr || !cock::path::resolve(directory, path, out, sizeof(out))) {
		printf("sh: %s: %s: path too long\n", command, path);
		return false;
	}
	return true;
}

} // namespace cock::sh
