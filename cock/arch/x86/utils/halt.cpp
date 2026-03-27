#include <cock/core/hal/utils.hpp>

namespace cock::core::hal {

void halt() { __asm__ volatile("hlt"); }

} // namespace cock::core::hal
