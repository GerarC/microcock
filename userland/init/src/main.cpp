#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <init/boot.hpp>
#include <init/directory.hpp>
#include <init/manifest.hpp>
#include <init/run.hpp>
#include <init/supervise.hpp>
#include <init/watch.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr uintptr_t EXIT_SUCCESS_STATUS = 0;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;
constexpr const char INCOMPLETE_POLICY_MESSAGE[] = "init: the name server has an incomplete policy";

} // namespace

extern "C" int main() {
	if (!cock::init::startNames()) cock::user::exit_process(EXIT_FAILED_STATUS);

	uint32_t files = cock::init::grantInitrd();
	if (files == cock::user::NO_PROCESS) cock::user::exit_process(EXIT_FAILED_STATUS);

	cock::user::fs::Client client;
	client.onForeign = cock::init::keepExitNotice;
	if (!cock::init::loadManifest(client, files)) cock::user::exit_process(EXIT_FAILED_STATUS);
	if (!cock::init::describeSystem()) puts(INCOMPLETE_POLICY_MESSAGE);

	for (size_t index = 0; index < cock::init::manifest.count; index++)
		cock::init::runEntry(client, cock::init::manifest.entries[index]);

	if (cock::init::anyWatched()) cock::init::superviseServers(client);

	cock::user::exit_process(EXIT_SUCCESS_STATUS);
}
