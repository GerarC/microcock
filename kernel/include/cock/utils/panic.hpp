
#ifndef PANIC_HPP
#define PANIC_HPP

namespace cock::utils {

extern "C" {
__attribute__((noreturn)) void panic(const char *msg, ...);
}

} // namespace cock::utils

#endif // !PANIC_HPP
