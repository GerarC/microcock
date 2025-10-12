#include <cxa/cxa_constructors.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

constexpr const char *USING_CTORS_MESSAGE = "[ctors] using ctors";
constexpr const char *USING_INIT_ARRAY_MESSAGE = "[ctors] using init_array";

typedef void (**CtorPtr)();
typedef void (*Ctor)();

extern "C" void (*__init_array_start[])();
extern "C" void (*__init_array_end[])();
extern "C" void (*__ctors_start[])();
extern "C" void (*__ctors_end[])();

static inline int is_valid_text_address(void *p) {
	uintptr_t addr = (uintptr_t)p;
	if (addr >= 0x00100000 && addr < 0x01000000) return 1;
	return 0;
}

extern "C" void call_global_constructors() {
	const char *using_ctors_message = USING_INIT_ARRAY_MESSAGE;
	CtorPtr start = __init_array_start;
	CtorPtr end = __init_array_end;

	if (start == end) {
		using_ctors_message = USING_CTORS_MESSAGE;
		start = __ctors_start, end = __ctors_end;
	}
	puts(using_ctors_message);
	for (CtorPtr ctor = start; ctor < end; ctor++) {
		if (is_valid_text_address((void *)(*ctor))) {
			Ctor builder = *ctor;
			builder();
		}
	}
}
