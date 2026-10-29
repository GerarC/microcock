#include <cock/path/resolve.hpp>
#include <errno.h>
#include <ls/location.hpp>
#include <unistd.h>

namespace cock::ls {

bool resolveLocation(const char *path, char (&out)[PATH_BYTES]) {
	char directory[PATH_BYTES] = {};
	if (getcwd(directory, sizeof(directory)) != nullptr && cock::path::resolve(directory, path, out, sizeof(out)))
		return true;

	errno = ENAMETOOLONG;
	return false;
}

} // namespace cock::ls
