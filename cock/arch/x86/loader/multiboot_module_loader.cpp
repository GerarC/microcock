#include <cock/arch/x86/utils/helpers.hpp>
#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/arch/x86/loader/multiboot_module_loader.hpp>
#include <cock/core/boot/module.hpp>
#include <stddef.h>

namespace cock::arch::x86 {

using core::boot::BootModule;

BootModule MultibootModuleLoader::modules[MAX_MODULE_NUM] = {};
size_t MultibootModuleLoader::modCount = 0;

void MultibootModuleLoader::init(MBInfo *boot_info) {
    modCount = 0;

    if (boot_info && (boot_info->flags & (1 << 3)) && boot_info->mods_count > 0) {
        
        MBModuleList *mod_list = reinterpret_cast<MBModuleList *>(
            boot_info->mods_address + utils::KERNEL_START);

        modCount = boot_info->mods_count;

        for (size_t i = 0; i < modCount && i < MAX_MODULE_NUM; i++) {
            modules[i].start_address =
                reinterpret_cast<void *>(mod_list[i].mod_start + utils::KERNEL_START);
            modules[i].size = mod_list[i].mod_end - mod_list[i].mod_start;
            modules[i].name = reinterpret_cast<const char *>(
                mod_list[i].cmdline + utils::KERNEL_START);
        }
    }
}


} // namespace cock::arch::x86
