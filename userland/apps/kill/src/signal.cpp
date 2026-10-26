#include <cock/abi/process.hpp>
#include <cock/user/number.hpp>
#include <kill/signal.hpp>
#include <stddef.h>
#include <string.h>

using namespace cock::user::tool;

namespace cock::kill {

namespace {

struct Named {
	const char *name;
	uint32_t number;
};

constexpr Named SIGNALS[] = {
	{"INT", cock::abi::process::SIGNAL_INTERRUPT},
	{"KILL", cock::abi::process::SIGNAL_KILL},
	{"TERM", cock::abi::process::SIGNAL_TERMINATE},
	{"CONT", cock::abi::process::SIGNAL_CONTINUE},
	{"STOP", cock::abi::process::SIGNAL_STOP},
};

} // namespace

bool parseSignal(const char *text, uint32_t &signal) {
	uint32_t number = 0;
	bool typedAsNumber = parseUnsigned(text, number);
	for (const Named &known : SIGNALS) {
		if ((typedAsNumber && known.number == number) || (!typedAsNumber && strcmp(known.name, text) == 0)) {
			signal = known.number;
			return true;
		}
	}
	return false;
}

} // namespace cock::kill
