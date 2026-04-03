#ifndef CORE_SERIAL_HPP
#define CORE_SERIAL_HPP

#include <stddef.h>

namespace cock::driver {

class Serial {
public:
    static void init();
    static void putChar(char c);
    static void write(const char* data, size_t size);
    static void writeString(const char* data);
private:
    static int isTransmitEmpty();
};

} // namespace cock::core::serial

#endif // !CORE_SERIAL_HPP
