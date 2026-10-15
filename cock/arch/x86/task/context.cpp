#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/core/hal/tasking.hpp>
#include <stddef.h>
#include <string.h>

namespace cock::core::hal {

constexpr size_t MAX_RESOURCE = 0x10000;
constexpr uint8_t IO_DENIED = 0xFF;
constexpr uint32_t BITS_PER_BYTE = 8;
constexpr uint32_t FIRST_CONTEXT_VERSION = 1;

struct PortRange {
	uint32_t first;
	uint32_t last;
};

constexpr PortRange RESERVED_PORTS[] = {
	{0x20, 0x21},
	{0x40, 0x43},
	{0xA0, 0xA1},
};

static uint32_t nextContextVersion = FIRST_CONTEXT_VERSION;

static bool overlaps_reserved_ports(uint32_t first, uint32_t size) {
	uint32_t last = first + size - 1;
	for (const PortRange &range : RESERVED_PORTS) {
		if (first <= range.last && last >= range.first) return true;
	}
	return false;
}

void init_process_context(ArchProcessContext &context) {
	memset(context.iopm, IO_DENIED, sizeof(context.iopm));
	context.iopm_bytes = 0;
	context.version = nextContextVersion++;
}

bool grant_hardware_resource(ArchProcessContext &context, uint32_t resource_id,
							 uint32_t size) {
	if (size == 0 || resource_id >= MAX_RESOURCE ||
		size > MAX_RESOURCE - resource_id)
		return false;

	if (overlaps_reserved_ports(resource_id, size)) return false;

	for (uint32_t i = 0; i < size; i++) {
		uint32_t port = resource_id + i;
		context.iopm[port / BITS_PER_BYTE] &= ~(1 << (port % BITS_PER_BYTE));
	}

	uint32_t bytes_in_use = (resource_id + size - 1) / BITS_PER_BYTE + 1;
	if (bytes_in_use > context.iopm_bytes) context.iopm_bytes = bytes_in_use;
	context.version = nextContextVersion++;
	return true;
}

void apply_process_context(const ArchProcessContext &context) {
	arch::x86::GDT::applyIoPermissions(context.iopm, context.iopm_bytes,
									   context.version);
}

} // namespace cock::core::hal
