#include <names/paths.hpp>
#include <string.h>

namespace cock::names {

void normalize(const char *name, char (&path)[MAX_PATH_BYTES + 1]) {
	size_t length = strlen(name);
	size_t prefix = 0;
	if (length != 0 && name[0] != abi::names::PATH_SEPARATOR) {
		memcpy(path, abi::names::RUN_PREFIX, abi::names::RUN_PREFIX_BYTES);
		prefix = abi::names::RUN_PREFIX_BYTES;
	}
	memcpy(path + prefix, name, length + 1);
}

} // namespace cock::names
