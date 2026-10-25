#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/names.hpp>
#include <cock/user/pm.hpp>
#include <init/keep.hpp>
#include <init/manifest.hpp>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr char PM_NAME[] = "pm";
constexpr uintptr_t EXIT_SUCCESS_STATUS = 0;
constexpr uintptr_t EXIT_FAILED_STATUS = 1;

} // namespace

extern "C" int main() {
	cock::user::names::Connection manager = cock::user::names::connect(PM_NAME);
	if (manager.status != cock::abi::names::Status::OK) {
		puts("init: cannot reach pm");
		return EXIT_FAILED_STATUS;
	}

	cock::user::fs::Client files;
	if (!files.connect()) {
		puts("init: cannot reach the file server");
		return EXIT_FAILED_STATUS;
	}
	if (!cock::init::loadManifest(files)) return EXIT_FAILED_STATUS;

	cock::user::pm::Client process(manager.handle);
	puts("init: ready");
	cock::init::keepRunning(files, process);
	return EXIT_SUCCESS_STATUS;
}
