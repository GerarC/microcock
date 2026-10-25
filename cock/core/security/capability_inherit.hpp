/** @file
 * @brief The capabilities a process created by `fork` is born with: copies of the
 * `IPC` and `ENDPOINT` capabilities of its parent, recorded in the derivation tree.
 */
#ifndef CORE_SECURITY_CAPABILITY_INHERIT_HPP
#define CORE_SECURITY_CAPABILITY_INHERIT_HPP

#include <cock/core/security/capability.hpp>
#include <cock/core/task/process.hpp>
#include <stddef.h>

namespace cock::core::security {

/**
 * @brief Tells whether a forked child gets a copy of a capability.
 * @param capability An entry of the parent.
 * @return True for an `IPC` capability and for an `ENDPOINT` handle that does not carry
 * the bind right. Hardware capabilities (ports, interrupt lines, device memory, boot
 * modules, `SPAWN`) stay with the parent: two processes must not share a device, and a
 * child must not create processes or label them. The owner entry of an endpoint (the one
 * with the bind right) stays too: the child does not become the server of its parent.
 */
bool is_inheritable(const Capability &capability);

/**
 * @brief Tells whether `inherit_capabilities` will copy every inheritable entry of a process.
 * @param parent The process that forks.
 * @return True when the derivation tree has a free node for each entry `is_inheritable`
 * accepts and every such entry still has its node in the tree. Nothing changes between this
 * call and `inherit_capabilities` in the same system call, so the second one then succeeds.
 */
bool can_inherit_capabilities(const task::Process &parent);

/**
 * @brief Gives a child a copy of every inheritable capability of its parent.
 *
 * Each copy is a new node of the derivation tree, a child of the node of the entry it comes
 * from, so whoever may revoke the parent's capability may revoke the copy too. The copy has
 * no flags: it cannot be passed on, minted or bound, so the child never holds more than the
 * parent held and cannot give it away. An `ENDPOINT` copy keeps the handle number of its
 * source, which is what the program stored in its memory. It keeps the badge too, with one
 * exception: a badge equal to the id of the parent is the identity servers know the parent by
 * (the initial handle of a process carries it), so the copy gets the id of the child and a
 * server tells the two apart. A server that has never heard of the child, such as the name
 * server, does not answer it until it learns of it.
 *
 * @param parent The process that forks; its table is not changed.
 * @param child The new process, which already has an id and holds no capability.
 * @return True when every entry was copied. False when the derivation tree or the table of
 * the child is full; the child holds nothing and the tree has no node of the child then.
 */
bool inherit_capabilities(const task::Process &parent, task::Process &child);

} // namespace cock::core::security

#endif // !CORE_SECURITY_CAPABILITY_INHERIT_HPP
