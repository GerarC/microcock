#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr char FILE_PATH[] = "hello.txt";
constexpr size_t BUFFER_BYTES = 64;
constexpr uint32_t START_OFFSET = 0;
constexpr uintptr_t EXIT_SERVED = 0;
constexpr uintptr_t EXIT_REFUSED = 1;

}

extern "C" void _start() {
	cock::user::fs::Client client;
	char buffer[BUFFER_BYTES];

	bool served = client.connect();
	uint32_t handle = served ? client.open(FILE_PATH) : cock::abi::fs::NO_HANDLE;
	served = handle != cock::abi::fs::NO_HANDLE &&
			 client.read(handle, START_OFFSET, buffer, sizeof(buffer)) > 0 &&
			 client.close(handle);

	puts(served ? "fsclient: ok" : "fsclient: refused");
	cock::user::exit_process(served ? EXIT_SERVED : EXIT_REFUSED);
}
