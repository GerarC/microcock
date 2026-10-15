#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/heap.hpp>
#include <stddef.h>
#include <string.h>

namespace cock::core::hal {

constexpr size_t IO_PERMISSION_MAP_SIZE = arch::x86::IO_PERMISSION_MAP_SIZE;
constexpr size_t MAX_RESOURCE = 0x10000;
constexpr uint8_t IO_DENIED = 0xFF;

struct PortRange {
	uint32_t first;
	uint32_t last;
};

constexpr PortRange RESERVED_PORTS[] = {
	{0x20, 0x21},
	{0x40, 0x43},
	{0xA0, 0xA1},
};

struct X86Context {
	uint8_t iopm[IO_PERMISSION_MAP_SIZE];
};

static bool overlaps_reserved_ports(uint32_t first, uint32_t size) {
	uint32_t last = first + size - 1;
	for (const PortRange &range : RESERVED_PORTS) {
		if (first <= range.last && last >= range.first) return true;
	}
	return false;
}

ArchThreadContext create_thread_context() {
	X86Context *ctx = new X86Context();
	memset(ctx->iopm, IO_DENIED, sizeof(ctx->iopm));
	return static_cast<ArchThreadContext>(ctx);
}

void destroy_thread_context(ArchThreadContext context) {
	if (!context) return;
	X86Context *ctx = static_cast<X86Context *>(context);
	delete ctx;
}

bool grant_hardware_resource(ArchThreadContext context, uint32_t resource_id,
							 uint32_t size) {
	if (!context) return false;

	if (size == 0 || resource_id >= MAX_RESOURCE ||
		size > MAX_RESOURCE - resource_id)
		return false;

	if (overlaps_reserved_ports(resource_id, size)) return false;

	X86Context *ctx = static_cast<X86Context *>(context);

	for (uint32_t i = 0; i < size; i++) {
		uint32_t port = resource_id + i;
		ctx->iopm[port / 8] &= ~(1 << (port % 8));
	}
	return true;
}

void apply_thread_context(ArchThreadContext context) {
	if (context) {
		X86Context *ctx = static_cast<X86Context *>(context);
		arch::x86::GDT::setIOPM(ctx->iopm);
	} else {
		arch::x86::GDT::setIOPM(nullptr);
	}
}

} // namespace cock::core::hal
