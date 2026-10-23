#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <init/boot.hpp>
#include <init/manifest.hpp>
#include <init/run.hpp>
#include <stddef.h>
#include <stdint.h>

namespace {

constexpr uintptr_t EXIT_SUCCESS_STATUS = 0;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;

} // namespace

extern "C" void _start() {
	if (!cock::init::grantInitrd()) cock::user::exit_process(EXIT_FAILED_STATUS);

	cock::user::fs::Client client;
	if (!cock::init::loadManifest(client)) cock::user::exit_process(EXIT_FAILED_STATUS);

	for (size_t index = 0; index < cock::init::manifest.count; index++)
		cock::init::runEntry(client, cock::init::manifest.entries[index]);

	cock::user::exit_process(EXIT_SUCCESS_STATUS);
}
