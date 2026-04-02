#ifndef CORE_BOOT_MODULE_HPP
#define CORE_BOOT_MODULE_HPP

#include <stdint.h>
#include <stddef.h>

namespace cock::core::boot {
    struct BootModule {
        void* start_address;
        size_t size;
        const char* name;
    };
}

#endif
