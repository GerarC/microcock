#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr char DEAD_PATH[] = "/mute/anything";
constexpr char LIVE_PATH[] = "/hello.txt";
constexpr char SERVER_NAME[] = "mute";
constexpr uintptr_t EXIT_SERVED = 0;
constexpr uintptr_t EXIT_REFUSED = 1;

}

extern "C" int main() {
	cock::user::fs::Client client;
	bool served = client.connect();
	served = served && client.open(DEAD_PATH) == cock::abi::fs::NO_HANDLE;
	if (served) puts("muteclient: dead server did not hang the vfs");

	uint32_t handle = served ? client.open(LIVE_PATH) : cock::abi::fs::NO_HANDLE;
	served = handle != cock::abi::fs::NO_HANDLE && client.close(handle);

	cock::user::fs::Client direct;
	served = served && !direct.connect(SERVER_NAME);

	puts(served ? "muteclient: ok" : "muteclient: refused");
	cock::user::exit_process(served ? EXIT_SERVED : EXIT_REFUSED);
}
