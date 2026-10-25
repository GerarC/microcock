/**
 * @file pm.hpp
 * @brief Client of the process manager: asks it to create a process from an image in memory, to record a
 * process that already exists, to list its table, and to collect the status of a child.
 *
 * The caller holds a handle on the endpoint of `pm` (the root server makes it when it starts `pm`). A program
 * image goes in a shared memory object that `pm` makes and sizes: `begin` asks for it and maps it, the caller
 * writes the image into the area `begin` returns, and `finish` writes the request and sends it. Only the root
 * server is served for now; any other caller gets `DENIED`.
 */
#ifndef COCK_USER_PM_HPP
#define COCK_USER_PM_HPP

#include <cock/abi/pm.hpp>
#include <cock/abi/shm.hpp>
#include <cock/exec/args.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/crt0.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

namespace cock::user::pm {

/** One row of the table of `pm`, as `list` returns it. */
struct Listing {
	/** Result of the call; the other fields are valid only for `OK`. */
	abi::pm::Status status;
	/** State of the process. */
	abi::pm::State state;
	/** Id of the process in the kernel. */
	uint32_t kernelId;
	/** Id of the process in the table of `pm`. */
	uint32_t pid;
	/** Id in the table of the parent, or `abi::pm::NO_PID`. */
	uint32_t parent;
};

/** Exit status of a `Waited` that holds none. */
constexpr uint32_t NO_EXIT_STATUS = 0;

/** What `wait` returns. */
struct Waited {
	/** Result of the call; `NOT_FOUND` means there is no such child. The other fields are valid only for `OK`. */
	abi::pm::Status status;
	/** Id in the table of the child that was collected. */
	uint32_t pid;
	/** Status the child exited with. */
	uint32_t exitStatus;
};

/** Calls to `pm` through one handle. It holds at most one object at a time. */
class Client {
public:
	/**
	 * @brief Makes a client.
	 * @param handle A handle on the endpoint of `pm`.
	 */
	explicit Client(uint32_t handle = abi::NO_CAPABILITY_NODE) : handle(handle) {}

	/**
	 * @brief Tells the client which handle reaches `pm`.
	 * @param value A handle on the endpoint of `pm`.
	 */
	void connect(uint32_t value) { handle = value; }

	/**
	 * @brief Asks `pm` for room for an image and maps it.
	 *
	 * Gives back an object held from an earlier `begin` that was never finished.
	 *
	 * @param imageBytes Size of the image, from 1 to `abi::pm::MAX_IMAGE_BYTES`.
	 * @return The writable area where the image goes, or null when the size is refused, `pm` has no room or the
	 * mapping failed.
	 */
	uint8_t *begin(size_t imageBytes) {
		release();
		if (imageBytes == 0 || imageBytes > abi::pm::MAX_IMAGE_BYTES) return nullptr;

		uint32_t words[abi::pm::CALL_WORDS] = {abi::pm::pack(abi::pm::Op::ALLOCATE),
											   static_cast<uint32_t>(abi::pm::IMAGE_OFFSET + imageBytes)};
		CallResult result = ep_call(handle, words);
		status = result.ok ? abi::pm::statusOf(result.words[abi::pm::HEADER_WORD]) : abi::pm::Status::BAD_REQUEST;
		if (status != abi::pm::Status::OK) return nullptr;

		object = result.words[abi::pm::FIRST_WORD];
		mapped = pageRound(abi::pm::IMAGE_OFFSET + imageBytes);
		base = shm_map(object, true);
		if (base == nullptr) {
			release();
			return nullptr;
		}
		return static_cast<uint8_t *>(base) + abi::pm::IMAGE_OFFSET;
	}

	/**
	 * @brief Writes the request into the object and asks `pm` to create the process.
	 *
	 * The image must already be in the area `begin` returned. The object is given back whatever the result.
	 *
	 * @param name Name of the process, at most `abi::pm::NAME_BYTES` - 1 characters.
	 * @param rights Rights of the new process.
	 * @param flags `abi::pm::FORWARD_EXIT` to have `pm` tell the root server when the process ends, `abi::pm::AS_INIT` for `init` (root only), or zero. Anything else is refused.
	 * @param imageBytes The size given to `begin`.
	 * @return The kernel process id of the new process, or `NO_PROCESS` when `begin` was not called, the name is
	 * too long, `pm` refused or the call failed.
	 */
	uint32_t finish(const char *name, const abi::service::SpawnRights &rights, uint32_t flags,
					size_t imageBytes) {
		if (base == nullptr || strlen(name) >= abi::pm::NAME_BYTES) {
			release();
			return NO_PROCESS;
		}

		abi::pm::SpawnRequest *request = static_cast<abi::pm::SpawnRequest *>(base);
		memset(request, 0, sizeof(*request));
		memcpy(request->name, name, strlen(name));
		request->rights = rights;
		request->flags = flags;
		request->imageBytes = static_cast<uint32_t>(imageBytes);

		uint32_t words[abi::pm::CALL_WORDS] = {abi::pm::pack(abi::pm::Op::SPAWN), object};
		CallResult result = ep_call(handle, words);
		release();
		status = result.ok ? abi::pm::statusOf(result.words[abi::pm::HEADER_WORD]) : abi::pm::Status::BAD_REQUEST;
		if (status != abi::pm::Status::OK) return NO_PROCESS;
		return result.words[abi::pm::FIRST_WORD];
	}

	/**
	 * @brief Writes the request into the object and asks `pm` to replace the image of the caller with it.
	 *
	 * The image must already be in the area `begin` returned. On success the call does not return: the
	 * calling process starts again in the new image, with the same process id, capabilities and endpoints,
	 * its other threads gone, the given arguments and the initial handle it had. It returns only when `pm`
	 * refused; the old image goes on. The object is given back whatever the result.
	 *
	 * @param name Name of the new image, at most `abi::pm::NAME_BYTES` - 1 characters.
	 * @param argv The arguments, `argc` strings; the first is the name the program sees.
	 * @param argc Number of arguments, at most `abi::exec::MAX_ARGS`, and their strings must fit `abi::exec::MAX_ARG_BYTES`.
	 * @param imageBytes The size given to `begin`.
	 * @return The status `pm` gave (`BAD_REQUEST` also when `begin` was not called, the name is too long or
	 * the arguments do not fit, or the call failed).
	 */
	abi::pm::Status finishExec(const char *name, const char *const *argv, uint32_t argc, size_t imageBytes) {
		if (base == nullptr || strlen(name) >= abi::pm::NAME_BYTES) {
			release();
			return status = abi::pm::Status::BAD_REQUEST;
		}

		abi::pm::ExecRequest *request = static_cast<abi::pm::ExecRequest *>(base);
		memset(request, 0, sizeof(*request));
		memcpy(request->name, name, strlen(name));
		request->imageBytes = static_cast<uint32_t>(imageBytes);
		if (!::cock::exec::build(request->args, initialHandle(), argv, argc)) {
			release();
			return status = abi::pm::Status::BAD_REQUEST;
		}

		uint32_t words[abi::pm::CALL_WORDS] = {abi::pm::pack(abi::pm::Op::EXEC), object};
		CallResult result = ep_call(handle, words);
		release();
		return status = result.ok ? abi::pm::statusOf(result.words[abi::pm::HEADER_WORD])
								  : abi::pm::Status::BAD_REQUEST;
	}

	/**
	 * @brief Status that `pm` gave in the last `begin`, `finish` or `finishExec`.
	 * @return The status, or `BAD_REQUEST` when the call itself failed or nothing was called yet.
	 */
	abi::pm::Status lastStatus() const { return status; }

	/**
	 * @brief Gives back an object that `begin` took and `finish` did not use.
	 */
	void release() {
		if (base != nullptr) munmap(base, mapped);
		if (object != NO_SHM) shm_release(object);
		base = nullptr;
		object = NO_SHM;
		mapped = 0;
	}

	/**
	 * @brief Adds a process that already exists to the table of `pm`.
	 * @param kernelId Id of the process in the kernel.
	 * @param parentKernelId Id in the kernel of its parent, or `abi::pm::NO_PARENT`.
	 * @param wantedPid Id in the table it must get (`abi::pm::INIT_PID` for `init`), or `abi::pm::NO_PID`.
	 * @param pid Receives its id in the table.
	 * @return true when `pm` recorded it.
	 */
	bool registerProcess(uint32_t kernelId, uint32_t parentKernelId, uint32_t wantedPid, uint32_t &pid) {
		uint32_t words[abi::pm::CALL_WORDS] = {abi::pm::pack(abi::pm::Op::REGISTER), kernelId,
											   parentKernelId, wantedPid};
		CallResult result = ep_call(handle, words);
		if (!result.ok || abi::pm::statusOf(result.words[abi::pm::HEADER_WORD]) != abi::pm::Status::OK)
			return false;
		pid = result.words[abi::pm::FIRST_WORD];
		return true;
	}

	/**
	 * @brief Lets `init` create processes with a service label. Root only.
	 * @param label A label of a program.
	 * @return true when `pm` accepted it.
	 */
	bool permit(uint32_t label) {
		uint32_t words[abi::pm::CALL_WORDS] = {abi::pm::pack(abi::pm::Op::PERMIT), label};
		CallResult result = ep_call(handle, words);
		return result.ok && abi::pm::statusOf(result.words[abi::pm::HEADER_WORD]) == abi::pm::Status::OK;
	}

	/**
	 * @brief Reads one row of the table of `pm`.
	 * @param index Position among the rows in use, from 0.
	 * @return The row; its `status` is `NOT_FOUND` past the last one and `BAD_REQUEST` when the call failed.
	 */
	Listing list(uint32_t index) {
		Listing listing = {abi::pm::Status::BAD_REQUEST, abi::pm::State::FREE, 0, abi::pm::NO_PID,
						   abi::pm::NO_PID};
		uint32_t words[abi::pm::CALL_WORDS] = {abi::pm::pack(abi::pm::Op::LIST), index};
		CallResult result = ep_call(handle, words);
		if (!result.ok) return listing;

		uint32_t header = result.words[abi::pm::HEADER_WORD];
		listing.status = abi::pm::statusOf(header);
		if (listing.status != abi::pm::Status::OK) return listing;
		listing.state = abi::pm::stateOf(header);
		listing.kernelId = result.words[abi::pm::FIRST_WORD];
		listing.pid = result.words[abi::pm::SECOND_WORD];
		listing.parent = result.words[abi::pm::THIRD_WORD];
		return listing;
	}

	/**
	 * @brief Collects the status of a child of the caller. Blocks while the children it matches still run.
	 * @param pid Id in the table of the child, or `abi::pm::ANY_CHILD`.
	 * @return The child and its status; `status` is `NOT_FOUND` when the caller has no such child and
	 * `BAD_REQUEST` when the call failed.
	 */
	Waited wait(uint32_t pid = abi::pm::ANY_CHILD) {
		Waited waited = {abi::pm::Status::BAD_REQUEST, abi::pm::NO_PID, NO_EXIT_STATUS};
		uint32_t words[abi::pm::CALL_WORDS] = {abi::pm::pack(abi::pm::Op::WAIT), pid};
		CallResult result = ep_call(handle, words);
		if (!result.ok) return waited;

		waited.status = abi::pm::statusOf(result.words[abi::pm::HEADER_WORD]);
		if (waited.status != abi::pm::Status::OK) return waited;
		waited.pid = result.words[abi::pm::FIRST_WORD];
		waited.exitStatus = result.words[abi::pm::SECOND_WORD];
		return waited;
	}

private:
	static size_t pageRound(size_t bytes) {
		return (bytes + abi::shm::PAGE_BYTES - 1) / abi::shm::PAGE_BYTES * abi::shm::PAGE_BYTES;
	}

	uint32_t handle;
	abi::pm::Status status = abi::pm::Status::BAD_REQUEST;
	uint32_t object = NO_SHM;
	void *base = nullptr;
	size_t mapped = 0;
};

} // namespace cock::user::pm

#endif // !COCK_USER_PM_HPP
