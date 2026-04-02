
#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/heap.hpp>
#include <stddef.h>
#include <string.h>

namespace cock::core::hal {

constexpr size_t IO_PERMISSION_MAP_SIZE = 0x2001;
constexpr size_t MAX_RESOURCE = 0x10000;

struct X86Context {
	uint8_t iopm[IO_PERMISSION_MAP_SIZE];
};

ArchThreadContext create_thread_context() {
	X86Context *ctx = new X86Context();
	memset(ctx->iopm, 0xFF, sizeof(ctx->iopm));
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

	if (resource_id + size > MAX_RESOURCE) return false;

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
