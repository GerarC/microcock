/** @file
 * @brief The table of services the name server knows: name, label, running process and who may talk to it.
 */
#ifndef NAMES_SERVICES_HPP
#define NAMES_SERVICES_HPP

#include <cock/abi/endpoint.hpp>
#include <cock/abi/names.hpp>
#include <cock/abi/service.hpp>
#include <names/paths.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::names {

/** Most services the table holds: the manifest entries (32), `ramfs`, `names` and room for more. */
constexpr size_t MAX_SERVICES = 48;
/** Most labels one service may be allowed to talk to. */
constexpr size_t MAX_ALLOWED = abi::service::MAX_SPAWN_IPC;
/** Process id of a service that is not running. */
constexpr uint32_t NOT_RUNNING = 0xFFFFFFFF;

/** One program of the system, as `rs` described it. */
struct Service {
	/** True for a used slot. */
	bool defined;
	/** Service label, not `NO_SERVICE`; the same for every process that runs the program. */
	uint32_t label;
	/** NUL terminated path that `CONNECT` looks for (see `normalize`); empty when the program cannot be reached by name. */
	char name[MAX_PATH_BYTES + 1];
	/** Process id of the running program, or `NOT_RUNNING`. */
	uint32_t pid;
	/** Endpoint the running program receives on, which `connect` gives clients a handle on; `NO_ENDPOINT` when `rs` bound none. */
	uint32_t endpoint;
	/** Labels this program may talk to (its `ipc=` list). */
	uint32_t allowed[MAX_ALLOWED];
	/** Number of valid entries of `allowed`. */
	size_t allowedCount;
};

/**
 * @brief Adds a service.
 * @param label Its service label, not `NO_SERVICE`, at most `abi::names::MAX_LABEL`.
 * @param name Its path, already normalized; empty for a program that clients do not look for.
 * @return The new entry, or null when the label or a non empty name is already taken, the label is out of range
 *         or the table is full.
 */
Service *defineService(uint32_t label, const char *name);

/**
 * @brief Finds a service by label.
 * @param label Service label.
 * @return The entry, or null when none has that label.
 */
Service *findByLabel(uint32_t label);

/**
 * @brief Finds a service by name.
 * @param name NUL terminated path, already normalized; an empty name never matches.
 * @return The entry, or null when none has that name.
 */
Service *findByName(const char *name);

/**
 * @brief Finds the service a process runs.
 * @param pid Process id.
 * @return The entry bound to that process id, or null when none is.
 */
Service *findByPid(uint32_t pid);

/**
 * @brief Lets a service talk to another one.
 * @param from The service that gains the right.
 * @param label Label of the other service; it must be defined.
 * @return true when added or already present; false when the other label is unknown or `from` has `MAX_ALLOWED` entries.
 */
bool allowTalk(Service &from, uint32_t label);

/**
 * @brief Tells whether a service lists a label.
 * @param from The service whose list is read.
 * @param label Label to look for.
 * @return true when `label` is in the list of `from`.
 */
bool lists(const Service &from, uint32_t label);

} // namespace cock::names

#endif // !NAMES_SERVICES_HPP
