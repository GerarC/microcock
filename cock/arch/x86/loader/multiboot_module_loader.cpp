#include <cock/arch/x86/boot/boot_memory.hpp>
#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/arch/x86/loader/multiboot_module_loader.hpp>
#include <cock/core/boot/module.hpp>
#include <cock/utils/logger.hpp>
#include <stddef.h>

namespace cock::arch::x86 {

using core::boot::BootModule;
using core::memory::PhysAddr;
using cock::utils::Logger;

constexpr uint32_t MODULE_NAME_MAP_SIZE = 0x100;
constexpr const char *UNNAMED_MODULE = "";

BootModule MultibootModuleLoader::modules[MAX_MODULE_NUM] = {};
size_t MultibootModuleLoader::modCount = 0;

void MultibootModuleLoader::init(const MBInfo *boot_info) {
    modCount = 0;

    if (!boot_info || !(boot_info->flags & MULTIBOOT_FLAG_MODULES) ||
        boot_info->mods_count == 0)
        return;

    uint32_t requested = boot_info->mods_count;
    const MBModuleList *mod_list = static_cast<const MBModuleList *>(
        BootMemory::map(PhysAddr(boot_info->mods_address),
                       requested * sizeof(MBModuleList)));
    if (!mod_list) {
        Logger::error("Module list is unreachable");
        return;
    }

    for (uint32_t i = 0; i < requested && modCount < MAX_MODULE_NUM; i++) {
        uint32_t size = mod_list[i].mod_end - mod_list[i].mod_start;
        void *start = BootMemory::map(PhysAddr(mod_list[i].mod_start), size);
        if (!start) {
            Logger::error("Module %u is unreachable", i);
            continue;
        }

        const char *name = UNNAMED_MODULE;
        if (mod_list[i].cmdline) {
            const char *mapped = static_cast<const char *>(
                BootMemory::map(PhysAddr(mod_list[i].cmdline), MODULE_NAME_MAP_SIZE));
            if (mapped) name = mapped;
        }

        modules[modCount].start_address = start;
        modules[modCount].size = size;
        modules[modCount].name = name;
        modules[modCount].physical_address = PhysAddr(mod_list[i].mod_start);
        modCount++;
    }
}


} // namespace cock::arch::x86
