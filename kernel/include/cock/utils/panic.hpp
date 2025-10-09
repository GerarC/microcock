
#ifndef PANIC_HPP
#define PANIC_HPP

extern "C" {
__attribute__((noreturn)) void panic(const char *msg);
}

#endif // !PANIC_HPP
