#ifndef COCK_HPP
#define COCK_HPP

#include <cock/core/boot/module.hpp>
#include <cock/utils/macros.hpp>
#include <stddef.h>

namespace cock {

using core::boot::BootModule;

extern "C" {
void cock_main(const BootModule *modules, size_t mod_count);
}

} // namespace cock

#endif // !COCK_HPP
