#include <cock/utils/logger.hpp>
#include <cxa/cxa_constructors.hpp>
#include <hal/address_space.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::utils::Logger;

constexpr const char *USING_CTORS_MESSAGE = "[ctors] using ctors";
constexpr const char *USING_INIT_ARRAY_MESSAGE = "[ctors] using init_array";

typedef void (**ConstructorPointer)();
typedef void (*Constructor)();

extern "C" void (*__init_array_start[])();
extern "C" void (*__init_array_end[])();
extern "C" void (*__ctors_start[])();
extern "C" void (*__ctors_end[])();
extern "C" char _kernel_end[];

static inline int is_valid_text_address(void *p) {
    uintptr_t addr = (uintptr_t)p;
    return (addr >= cock::core::hal::KERNEL_SPACE_BASE &&
            addr < (uintptr_t)_kernel_end);
}

extern "C" void call_global_constructors() {
	const char *using_ctors_message = USING_INIT_ARRAY_MESSAGE;
	ConstructorPointer start = __init_array_start;
	ConstructorPointer end = __init_array_end;

	if (start == end) {
		using_ctors_message = USING_CTORS_MESSAGE;
		start = __ctors_start, end = __ctors_end;
	}
	Logger::trace(using_ctors_message);
	for (ConstructorPointer ctor = start; ctor < end; ctor++) {
		if (is_valid_text_address((void *)(*ctor))) {
			Constructor constructor = *ctor;
			constructor();
		}
	}
}
