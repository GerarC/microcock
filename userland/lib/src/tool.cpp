#include <cock/user/tool.hpp>
#include <errno.h>
#include <stdio.h>

namespace cock::user::tool {

int usage(const char *program, const char *arguments) {
	printf("usage: %s %s\n", program, arguments);
	return STATUS_USAGE;
}

const char *reasonFor(int code) {
	switch (code) {
	case ENOENT:
		return "not found";
	case ENOTDIR:
		return "not a directory";
	case ENAMETOOLONG:
		return "path too long";
	case EEXIST:
		return "already exists";
	case ENOTEMPTY:
		return "not empty";
	case EXDEV:
		return "different volumes";
	case EROFS:
		return "read only";
	case ENOSPC:
		return "no space";
	case EBUSY:
		return "busy";
	case EIO:
		return "i/o error";
	case EINVAL:
		return "bad request";
	case ENOTSUP:
		return "unsupported";
	case ENOTCONN:
		return "cannot reach the file server";
	case EBADF:
		return "bad descriptor";
	case EMFILE:
	case ENFILE:
		return "too many open files";
	default:
		return "failed";
	}
}

int fail(const char *program, const char *subject) {
	printf("%s: %s: %s\n", program, subject, reasonFor(errno));
	return STATUS_FAILURE;
}

} // namespace cock::user::tool
