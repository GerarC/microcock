#include <names/services.hpp>
#include <string.h>

namespace cock::names {

namespace {

Service services[MAX_SERVICES];

}

Service *defineService(uint32_t label, const char *name) {
	if (label == abi::service::NO_SERVICE || label > abi::names::MAX_LABEL) return nullptr;
	if (findByLabel(label) != nullptr || findByName(name) != nullptr) return nullptr;

	for (Service &service : services) {
		if (service.defined) continue;
		service = Service();
		service.defined = true;
		service.label = label;
		memcpy(service.name, name, strlen(name) + 1);
		service.pid = NOT_RUNNING;
		service.endpoint = abi::endpoint::NO_ENDPOINT;
		service.owner = abi::service::NO_SERVICE;
		service.mode = abi::service::MODE_ALL;
		return &service;
	}
	return nullptr;
}

Service *findByLabel(uint32_t label) {
	for (Service &service : services)
		if (service.defined && service.label == label) return &service;
	return nullptr;
}

Service *findByName(const char *name) {
	if (name[0] == '\0') return nullptr;
	for (Service &service : services)
		if (service.defined && strcmp(service.name, name) == 0) return &service;
	return nullptr;
}

Service *findByPid(uint32_t pid) {
	if (pid == NOT_RUNNING) return nullptr;
	for (Service &service : services)
		if (service.defined && service.pid == pid) return &service;
	return nullptr;
}

bool lists(const Service &from, uint32_t label) {
	for (size_t index = 0; index < from.allowedCount; index++)
		if (from.allowed[index] == label) return true;
	return false;
}

bool setOwner(Service &service, uint32_t owner, uint32_t mode) {
	if ((mode & ~abi::service::MODE_ALL) != 0) return false;
	if (owner != abi::service::NO_SERVICE && findByLabel(owner) == nullptr) return false;
	service.owner = owner;
	service.mode = mode;
	return true;
}

bool allowTalk(Service &from, uint32_t label) {
	if (findByLabel(label) == nullptr) return false;
	if (lists(from, label)) return true;
	if (from.allowedCount == MAX_ALLOWED) return false;
	from.allowed[from.allowedCount++] = label;
	return true;
}

} // namespace cock::names
