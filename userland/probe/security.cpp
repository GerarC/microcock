#include "probe.hpp"
#include <cock/abi/capability.hpp>
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

using cock::abi::CapabilityType;

namespace probe {

namespace {

constexpr uint16_t PS2_DATA_PORT = 0x60;
constexpr uint32_t SINGLE_RESOURCE = 1;
constexpr uint32_t KEYBOARD_IRQ_LINE = 1;
constexpr uint32_t ROOT_SERVER_PID = 1;
constexpr uintptr_t VIDEO_PHYSICAL = 0xB8000;
constexpr uintptr_t RAM_PHYSICAL = 0x100000;
constexpr uintptr_t MAP_TARGET = 0x80000000;
constexpr long WRITE_FAILED = -1;
constexpr int MUNMAP_FAILED = -1;
constexpr const char INITRD_MODULE[] = "initrd";
constexpr const char MISSING_MODULE[] = "no-such-module";
constexpr size_t OVERSIZED_MODULE_NAME = 4096;
constexpr char SPAWN_NAME[] = "intruder";
constexpr size_t SPAWN_IMAGE_BYTES = 64;

bool mapPhysicalIsDenied(uintptr_t physical) {
	return mmap(reinterpret_cast<void *>(MAP_TARGET), PAGE_BYTES,
				MAPPING_PROTECTION, MAP_PHYS | MAP_FIXED, NO_FILE,
				physical) == MAP_FAILED;
}

} // namespace

bool portsAreDenied() {
	return !cock::user::request_ports(PS2_DATA_PORT, SINGLE_RESOURCE);
}

bool irqIsDenied() { return !cock::user::irq_bind(KEYBOARD_IRQ_LINE); }

bool grantIsDenied() {
	return !cock::user::cap_grant(ROOT_SERVER_PID, CapabilityType::IO_PORTS,
								  PS2_DATA_PORT, SINGLE_RESOURCE);
}

bool physicalVideoMapIsDenied() { return mapPhysicalIsDenied(VIDEO_PHYSICAL); }

bool physicalRamMapIsDenied() { return mapPhysicalIsDenied(RAM_PHYSICAL); }

bool kernelWriteIsDenied() {
	return cock::user::write_text(reinterpret_cast<const char *>(KERNEL_ADDRESS),
								  PROBE_LENGTH) == WRITE_FAILED;
}

bool unmappedWriteIsDenied() {
	return cock::user::write_text(
			   reinterpret_cast<const char *>(UNMAPPED_USER_ADDRESS),
			   PROBE_LENGTH) == WRITE_FAILED;
}

bool kernelUnmapIsDenied() {
	return munmap(reinterpret_cast<void *>(KERNEL_ADDRESS), PAGE_BYTES) ==
		   MUNMAP_FAILED;
}

bool bootModulesAreDenied() {
	return cock::user::module_open(INITRD_MODULE) == cock::user::NO_SHM &&
		   cock::user::module_open(MISSING_MODULE) == cock::user::NO_SHM &&
		   syscall(SYS_MODULE_OPEN, KERNEL_ADDRESS, PROBE_LENGTH, 0, 0, 0, 0) ==
			   cock::user::FAILURE &&
		   syscall(SYS_MODULE_OPEN, UNMAPPED_USER_ADDRESS, PROBE_LENGTH, 0, 0, 0,
				   0) == cock::user::FAILURE &&
		   syscall(SYS_MODULE_OPEN, reinterpret_cast<uintptr_t>(INITRD_MODULE),
				   NO_PAGES, 0, 0, 0, 0) == cock::user::FAILURE &&
		   syscall(SYS_MODULE_OPEN, reinterpret_cast<uintptr_t>(INITRD_MODULE),
				   OVERSIZED_MODULE_NAME, 0, 0, 0, 0) == cock::user::FAILURE;
}

bool spawnIsDenied() {
	char image[SPAWN_IMAGE_BYTES] = {};
	return cock::user::spawn(image, sizeof(image), SPAWN_NAME) == cock::user::NO_PROCESS &&
		   syscall(SYS_SPAWN, KERNEL_ADDRESS, PROBE_LENGTH,
				   reinterpret_cast<uintptr_t>(SPAWN_NAME), sizeof(SPAWN_NAME), 0,
				   0) == cock::user::FAILURE &&
		   syscall(SYS_SPAWN, reinterpret_cast<uintptr_t>(image), NO_PAGES,
				   reinterpret_cast<uintptr_t>(SPAWN_NAME), sizeof(SPAWN_NAME), 0,
				   0) == cock::user::FAILURE;
}

} // namespace probe
