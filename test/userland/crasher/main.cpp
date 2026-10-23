#include <cock/user/api.hpp>

namespace {

constexpr uintptr_t EXIT_CRASHED = 7;

}

extern "C" int main() {
	cock::user::exit_process(EXIT_CRASHED);
}
