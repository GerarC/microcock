#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <rs/boot.hpp>
#include <rs/directory.hpp>
#include <rs/manifest.hpp>
#include <rs/pm.hpp>
#include <rs/run.hpp>
#include <rs/supervise.hpp>
#include <rs/watch.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr uintptr_t EXIT_SUCCESS_STATUS = 0;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;
constexpr const char INCOMPLETE_POLICY_MESSAGE[] = "rs: the name server has an incomplete policy";

} // namespace

extern "C" int main() {
	if (!cock::rs::startNames()) cock::user::exit_process(EXIT_FAILED_STATUS);

	uint32_t files = cock::rs::grantInitrd();
	if (files == cock::user::NO_PROCESS) cock::user::exit_process(EXIT_FAILED_STATUS);

	cock::user::fs::Client client;
	client.onForeign = cock::rs::keepExitNotice;
	if (!cock::rs::loadManifest(client, files)) cock::user::exit_process(EXIT_FAILED_STATUS);
	if (!cock::rs::describeSystem()) puts(INCOMPLETE_POLICY_MESSAGE);

	if (!cock::rs::startPm(client, files)) cock::user::exit_process(EXIT_FAILED_STATUS);

	for (size_t index = 0; index < cock::rs::manifest.count; index++) {
		const cock::abi::manifest::Entry &entry = cock::rs::manifest.entries[index];
		if (&entry != cock::rs::pmEntry() && !entry.startedByInit) cock::rs::runEntry(client, entry);
	}

	cock::rs::superviseServers(client);

	cock::user::exit_process(EXIT_SUCCESS_STATUS);
}
