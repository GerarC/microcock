/**
 * @file mounts.hpp
 * @brief Parser of `mounts.conf`, the table that tells the `vfs` which file system server serves each path prefix.
 *
 * One mount per line: `<prefix> <server>`, for example `/ ramfs` and `/disk fat`. Blank lines and lines that
 * start with `#` are ignored. The prefix starts with `/`, has no trailing `/` (except the root itself) and no
 * empty components. The table must contain the root `/`. The parser has no dependencies and is covered by
 * host tests.
 */
#ifndef SHARED_COCK_ABI_MOUNTS_HPP
#define SHARED_COCK_ABI_MOUNTS_HPP

#include <cock/abi/manifest.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::mounts {

/** Path of the mount table inside the initrd, in its `etc` directory. */
constexpr const char *FILE_PATH = "etc/mounts.conf";
/** Largest file the `vfs` reads, in bytes. */
constexpr size_t MAX_FILE_BYTES = 512;
/** Most mounts in the table. */
constexpr size_t MAX_MOUNTS = 8;
/** Longest prefix including the terminator. */
constexpr size_t MAX_PREFIX_BYTES = 64;
/** Longest server name including the terminator. */
constexpr size_t MAX_SERVER_BYTES = 32;
/** Prefix of the root mount, which every table must have. */
constexpr char ROOT_PREFIX[] = "/";
/** Path component separator. */
constexpr char SEPARATOR = '/';

/** One mount: a path prefix and the server that serves it. */
struct Mount {
	/** Mount point, null terminated. */
	char prefix[MAX_PREFIX_BYTES];
	/** Name of the file system server, null terminated. */
	char server[MAX_SERVER_BYTES];
};

/** Reason why parsing failed; see `Table::errorLine` for the line. */
enum class Error : uint32_t {
	/** No error. */
	NONE = 0,
	/** More than `MAX_MOUNTS` lines. */
	TOO_MANY_MOUNTS,
	/** The prefix is malformed or too long. */
	BAD_PREFIX,
	/** The server name is too long. */
	BAD_SERVER,
	/** A line does not have exactly two fields. */
	WRONG_FIELDS,
	/** The prefix appears twice. */
	DUPLICATE_PREFIX,
	/** The table has no `/` mount. */
	NO_ROOT,
};

namespace detail {

using manifest::detail::Slice;

/**
 * @brief Checks the shape of a mount prefix.
 * @param prefix Candidate prefix.
 * @return True for `/`, or for a path that starts with `/`, does not end with `/` and has no `//`.
 */
inline bool validPrefix(Slice prefix) {
	if (prefix.empty() || *prefix.begin != SEPARATOR) return false;
	if (prefix.length() == sizeof(ROOT_PREFIX) - sizeof(char)) return true;
	if (*(prefix.end - 1) == SEPARATOR) return false;
	for (const char *cursor = prefix.begin + 1; cursor != prefix.end; cursor++)
		if (*cursor == SEPARATOR && *(cursor - 1) == SEPARATOR) return false;
	return true;
}

} // namespace detail

/** Parsed mount table. */
struct Table {
	/** Mounts in file order; only the first `count` are valid. */
	Mount mounts[MAX_MOUNTS];
	/** Number of mounts. */
	size_t count;
	/** One based number of the line that failed; 0 for none (also 0 for `NO_ROOT`). */
	size_t errorLine;
	/** Result of the last `parse`. */
	Error error;

	/**
	 * @brief Parses a whole file, replacing the previous content.
	 * @param text File content; not required to be null terminated.
	 * @param length Size in bytes.
	 * @return True when the file is valid and has the root mount; false with `error` (and `errorLine`) set otherwise.
	 */
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

	/**
	 * @brief Finds a mount by exact prefix.
	 * @param prefix Null terminated prefix.
	 * @return The mount, or null when there is none. It is not a longest match search.
	 */
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
	/**
	 * @brief Parses a line and appends the mount.
	 * @param line Trimmed non blank line.
	 * @return `Error::NONE` or the reason the line was refused.
	 */
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
