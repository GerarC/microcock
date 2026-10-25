#include <names/forked.hpp>

namespace cock::names {

namespace {

struct Copy {
	bool used = false;
	uint32_t pid = NOT_RUNNING;
	uint32_t label = abi::service::NO_SERVICE;
};

Copy copies[MAX_FORKED];

Copy *slotFor(uint32_t pid, AliveCheck alive) {
	Copy *free = nullptr;
	for (Copy &copy : copies) {
		if (copy.used && copy.pid == pid) return &copy;
		if (free == nullptr && (!copy.used || !alive(copy.pid))) free = &copy;
	}
	return free;
}

} // namespace

Service *findForked(uint32_t pid, AliveCheck alive) {
	if (pid == NOT_RUNNING || !alive(pid)) return nullptr;
	for (Copy &copy : copies)
		if (copy.used && copy.pid == pid) return findByLabel(copy.label);
	return nullptr;
}

Service *findClient(uint32_t pid, AliveCheck alive) {
	Service *service = findByPid(pid);
	return service != nullptr ? service : findForked(pid, alive);
}

bool addForked(uint32_t child, uint32_t parent, AliveCheck alive) {
	if (child == NOT_RUNNING || child == parent || !alive(child) || findByPid(child) != nullptr)
		return false;

	Service *service = findClient(parent, alive);
	if (service == nullptr) return false;

	Copy *slot = slotFor(child, alive);
	if (slot == nullptr) return false;
	slot->used = true;
	slot->pid = child;
	slot->label = service->label;
	return true;
}

} // namespace cock::names
