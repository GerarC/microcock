/** @file
 * @brief The rights a new process is born with: its service label and IPC
 * capabilities, checked against its creator.
 */
#ifndef CORE_SYSCALL_PROC_SPAWN_RIGHTS_HPP
#define CORE_SYSCALL_PROC_SPAWN_RIGHTS_HPP

#include <cock/abi/service.hpp>
#include <cock/core/memory/address.hpp>
#include <cock/core/task/process.hpp>
#include <stdint.h>

namespace cock::core::syscall {

/**
 * @brief Tells whether a creator may give a set of rights.
 * @param creator The process that creates the new one.
 * @param rights The rights, already copied into the kernel.
 * @return True when the label is not `NO_SERVICE`, the list is within
 * `MAX_SPAWN_IPC`, the derivation tree has room for one node per label, the
 * creator may grant the label, and for every label of the list it holds a
 * grantable `IPC` capability whose node still exists.
 */
bool spawn_rights_valid(const task::Process &creator, const abi::service::SpawnRights &rights);

/**
 * @brief Copies the rights of a user program and checks them.
 * @param creator The process that creates the new one.
 * @param limit User space limit of the creator.
 * @param rights_ptr User pointer to an `abi::service::SpawnRights`, untrusted.
 * @param rights Receives the copy.
 * @return True when the pointer is readable and `spawn_rights_valid` accepts the
 * copy.
 */
bool read_spawn_rights(const task::Process &creator, memory::VirtAddr limit,
					   uintptr_t rights_ptr, abi::service::SpawnRights &rights);

/**
 * @brief Gives a process its label and derives one `IPC` capability for each label
 * of the list from a capability of the creator.
 * @param creator The process that creates the new one.
 * @param process The new process, which already has an id.
 * @param rights Rights accepted by `spawn_rights_valid` in the same system call.
 */
void give_spawn_rights(const task::Process &creator, task::Process &process,
					   const abi::service::SpawnRights &rights);

} // namespace cock::core::syscall

#endif // !CORE_SYSCALL_PROC_SPAWN_RIGHTS_HPP
