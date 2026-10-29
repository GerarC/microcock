#include <pm/process_name.hpp>
#include <string.h>

using cock::abi::pm::Status;

namespace {

constexpr uint32_t FIRST_CHUNK = 0;

}

namespace cock::pm {

Status setProcessName(uint32_t kernelId, uint32_t chunk, uint32_t first, uint32_t second) {
	if (chunk >= cock::abi::pm::NAME_CHUNKS) return Status::BAD_REQUEST;
	Row *row = table.findKernel(kernelId);
	if (row == nullptr) return Status::NOT_FOUND;

	if (chunk == FIRST_CHUNK) copyName(row->name, nullptr);
	char *destination = row->name + chunk * cock::abi::pm::NAME_CHUNK_BYTES;
	memcpy(destination, &first, sizeof(first));
	memcpy(destination + sizeof(first), &second, sizeof(second));
	row->name[cock::abi::pm::NAME_BYTES - TERMINATOR_BYTES] = NAME_END;
	return Status::OK;
}

} // namespace cock::pm
