#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/abi/service.hpp>
#include <pm/state.hpp>

namespace {

constexpr size_t NO_LABELS = 0;

uint32_t permittedLabels[cock::abi::pm::MAX_PERMITTED_LABELS];
size_t permittedCount = NO_LABELS;

} // namespace

namespace cock::pm {

Table table;
uint32_t rootHandle = cock::abi::NO_CAPABILITY_NODE;
uint32_t namesEndpoint = cock::abi::endpoint::NO_ENDPOINT;

Caller callerOf(uint32_t kernelId) {
	if (kernelId == cock::abi::service::ROOT_PID) return Caller::ROOT;
	Row *row = table.findKernel(kernelId);
	if (row != nullptr && row->pid == cock::abi::pm::INIT_PID &&
		row->state == cock::abi::pm::State::RUNNING)
		return Caller::INIT;
	return Caller::OTHER;
}

bool permit(uint32_t label) {
	uint32_t first = cock::abi::service::PROGRAM_SERVICE_BASE;
	if (label < first || label - first >= cock::abi::pm::PROGRAM_LABEL_COUNT) return false;
	if (permitted(label)) return true;
	if (permittedCount == cock::abi::pm::MAX_PERMITTED_LABELS) return false;
	permittedLabels[permittedCount++] = label;
	return true;
}

bool permitted(uint32_t label) {
	for (size_t index = 0; index < permittedCount; index++)
		if (permittedLabels[index] == label) return true;
	return false;
}

} // namespace cock::pm
