#include <cock/abi/fs.hpp>
#include <init/manifest.hpp>
#include <stdio.h>

using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

constexpr char MANIFEST_PATH[] = "/manifest.conf";
constexpr size_t NO_BYTES = 0;

char manifestText[cock::abi::manifest::MAX_FILE_BYTES];

bool readText(Client &files, size_t &length) {
	uint32_t handle = files.open(MANIFEST_PATH);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	bool loaded = files.stat(handle, info) && info.size > NO_BYTES && info.size <= sizeof(manifestText) &&
				  files.readAll(handle, manifestText, info.size);
	files.close(handle);
	length = info.size;
	return loaded;
}

} // namespace

namespace cock::init {

cock::abi::manifest::Manifest manifest;

bool loadManifest(Client &files) {
	size_t length = NO_BYTES;
	if (!readText(files, length)) {
		printf("init: cannot read %s\n", MANIFEST_PATH);
		return false;
	}
	if (!manifest.parse(manifestText, length)) {
		printf("init: manifest line %u rejected, error %u\n", static_cast<unsigned>(manifest.errorLine),
			   static_cast<unsigned>(manifest.error));
		return false;
	}
	return true;
}

} // namespace cock::init
