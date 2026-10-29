#include <cock/abi/pm.hpp>
#include <cock/user/pm.hpp>
#include <cock/user/tool.hpp>
#include <ps/columns.hpp>
#include <ps/command.hpp>
#include <ps/manager.hpp>
#include <stdint.h>
#include <stdio.h>

using namespace cock::user::tool;

namespace cock::ps {

namespace {

constexpr uint32_t FIRST_ROW = 0;

int unreachable(const char *program) {
	printf("%s: pm does not answer\n", program);
	return STATUS_FAILURE;
}

} // namespace

int run(int, char **argv) {
	cock::user::pm::Client client;
	if (!connectManager(client)) return unreachable(argv[0]);

	const cock::abi::pm::Snapshot *snapshot = client.snapshot();
	if (snapshot == nullptr) return unreachable(argv[0]);

	printHeader();
	uint32_t count = snapshot->count < cock::abi::pm::MAX_PROCESSES ? snapshot->count : cock::abi::pm::MAX_PROCESSES;
	for (uint32_t index = FIRST_ROW; index < count; index++) printRow(snapshot->records[index]);

	client.release();
	return STATUS_OK;
}

} // namespace cock::ps
