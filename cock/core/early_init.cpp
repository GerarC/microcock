#include <cock/core/arch.hpp>
#include <cock/core/cock.hpp>

#if defined(__linux__)
#error "You are not using a  cross-compiler"
#endif

namespace cock {

extern "C" void call_global_constructors();

extern "C" void init_cock(void) {
	arch_video_init();
	call_global_constructors();
	arch_core_init();

	cock_main();
}

} // namespace cock
