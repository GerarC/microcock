#include <cock/abi/fs.hpp>
#include <cock/abi/service.hpp>
#include <init/endpoints.hpp>
#include <init/files.hpp>
#include <init/manifest.hpp>
#include <stdio.h>

using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

char manifestText[cock::abi::manifest::MAX_FILE_BYTES];

bool readManifestText(Client &client, size_t &length) {
	uint32_t handle = client.open(cock::abi::manifest::FILE_PATH);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	bool loaded = client.stat(handle, info) && info.size > cock::init::NO_BYTES &&
				  info.size <= sizeof(manifestText) &&
				  cock::init::readInto(client, handle,
									   reinterpret_cast<uint8_t *>(manifestText), info.size);
	client.close(handle);
	length = info.size;
	return loaded;
}

} // namespace

namespace cock::init {

cock::abi::manifest::Manifest manifest;

bool loadManifest(Client &client, uint32_t files) {
	size_t length = NO_BYTES;
	uint32_t handle = handleOf(cock::abi::service::RAMFS_SERVICE);
	if (!client.connectTo(files, handle) || !readManifestText(client, length)) {
		printf("init: cannot read %s\n", cock::abi::manifest::FILE_PATH);
		return false;
	}

	if (!manifest.parse(manifestText, length)) {
		printf("init: manifest line %u rejected, error %u\n",
			   static_cast<unsigned>(manifest.errorLine), static_cast<unsigned>(manifest.error));
		return false;
	}
	printf("init: manifest has %u entries\n", static_cast<unsigned>(manifest.count));
	return true;
}

} // namespace cock::init
