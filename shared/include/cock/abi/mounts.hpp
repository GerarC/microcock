#ifndef SHARED_COCK_ABI_MOUNTS_HPP
#define SHARED_COCK_ABI_MOUNTS_HPP

#include <cock/abi/manifest.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::mounts {

constexpr const char *FILE_PATH = "mounts.conf";
constexpr size_t MAX_FILE_BYTES = 512;
constexpr size_t MAX_MOUNTS = 8;
constexpr size_t MAX_PREFIX_BYTES = 64;
constexpr size_t MAX_SERVER_BYTES = 32;
constexpr char ROOT_PREFIX[] = "/";
constexpr char SEPARATOR = '/';

struct Mount {
	char prefix[MAX_PREFIX_BYTES];
	char server[MAX_SERVER_BYTES];
};

enum class Error : uint32_t {
	NONE = 0,
	TOO_MANY_MOUNTS,
	BAD_PREFIX,
	BAD_SERVER,
	WRONG_FIELDS,
	DUPLICATE_PREFIX,
	NO_ROOT,
};

namespace detail {

using manifest::detail::Slice;

inline bool validPrefix(Slice prefix) {
	if (prefix.empty() || *prefix.begin != SEPARATOR) return false;
	if (prefix.length() == sizeof(ROOT_PREFIX) - sizeof(char)) return true;
	if (*(prefix.end - 1) == SEPARATOR) return false;
	for (const char *cursor = prefix.begin + 1; cursor != prefix.end; cursor++)
		if (*cursor == SEPARATOR && *(cursor - 1) == SEPARATOR) return false;
	return true;
}

} // namespace detail

struct Table {
	Mount mounts[MAX_MOUNTS];
	size_t count;
	size_t errorLine;
	Error error;

	bool parse(const char *text, size_t length) {
		count = 0;
		errorLine = 0;
		error = Error::NONE;

		const char *cursor = text;
		const char *end = text + length;
		size_t lineNumber = 0;
		while (cursor != end) {
			const char *lineEnd = cursor;
			while (lineEnd != end && *lineEnd != manifest::detail::NEWLINE)
				lineEnd++;
			lineNumber++;

			detail::Slice line = manifest::detail::trim({cursor, lineEnd});
			cursor = lineEnd == end ? end : lineEnd + 1;
			if (line.empty() || *line.begin == manifest::detail::COMMENT_START) continue;

			Error result = addMount(line);
			if (result != Error::NONE) {
				error = result;
				errorLine = lineNumber;
				return false;
			}
		}

		if (find(ROOT_PREFIX) == nullptr) {
			error = Error::NO_ROOT;
			return false;
		}
		return true;
	}

	const Mount *find(const char *prefix) const {
		for (size_t index = 0; index < count; index++) {
			detail::Slice known = {mounts[index].prefix, mounts[index].prefix};
			while (*known.end != '\0')
				known.end++;
			if (known.equals(prefix)) return &mounts[index];
		}
		return nullptr;
	}

  private:
	Error addMount(detail::Slice line) {
		if (count == MAX_MOUNTS) return Error::TOO_MANY_MOUNTS;

		detail::Slice rest = line;
		detail::Slice prefix = manifest::detail::nextToken(rest);
		detail::Slice server = manifest::detail::nextToken(rest);
		if (prefix.empty() || server.empty() || !manifest::detail::nextToken(rest).empty())
			return Error::WRONG_FIELDS;
		if (!detail::validPrefix(prefix)) return Error::BAD_PREFIX;

		Mount mount = {};
		if (!manifest::detail::copyText(prefix, mount.prefix, MAX_PREFIX_BYTES))
			return Error::BAD_PREFIX;
		if (!manifest::detail::copyText(server, mount.server, MAX_SERVER_BYTES))
			return Error::BAD_SERVER;
		if (find(mount.prefix) != nullptr) return Error::DUPLICATE_PREFIX;

		mounts[count++] = mount;
		return Error::NONE;
	}
};

} // namespace cock::abi::mounts

#endif // !SHARED_COCK_ABI_MOUNTS_HPP
