#ifndef COCK_HPP
#define COCK_HPP

#include <cock/core/boot/module.hpp>
#include <stddef.h>

namespace cock {
#define FOR_ETERNAL for (;;)

using core::boot::BootModule;
extern uint32_t vga_server_pid;

extern "C" {
void cock_main(const BootModule *modules, size_t mod_count);
}

} // namespace cock

#endif // !COCK_HPP
