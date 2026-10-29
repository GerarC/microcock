#include <cock/abi/names.hpp>
#include <cock/abi/pm.hpp>
#include <cock/abi/process.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <cock/user/pm.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::abi::pm::Status;
using cock::abi::process::KILLED_STATUS;
using cock::abi::process::SIGNAL_CONTINUE;
using cock::abi::process::SIGNAL_INTERRUPT;
using cock::abi::process::SIGNAL_KILL;
using cock::abi::process::SIGNAL_STOP;
using cock::abi::process::SIGNAL_TERMINATE;
using cock::user::pm::Client;
using cock::user::pm::Forked;
using cock::user::pm::Listing;
using cock::user::pm::Waited;

namespace {

constexpr char SERVER_NAME[] = "pm";
constexpr int PASSED = 0;
constexpr int FAILED = 1;
constexpr int CAUGHT_STATUS = 21;
constexpr int TIMED_OUT_STATUS = 22;
constexpr int DENIER_PASSED = 33;
constexpr int DENIER_NO_ROW = 34;
constexpr int DENIER_WAS_SERVED = 35;
constexpr uint32_t UNKNOWN_SIGNAL = 3;
constexpr uint32_t CATCH_TIMEOUT_MS = 5000;
constexpr uint32_t IGNORE_STEP_MS = 50;
constexpr uint32_t IGNORE_ROUNDS = 200;
constexpr uint32_t SETTLE_MS = 100;
constexpr uint32_t NO_KERNEL_ID = 0xFFFFFFFF;
constexpr int SLEEPER_DONE_STATUS = 41;
constexpr uint32_t SLEEPER_MS = 300;
constexpr uint32_t STOP_WAIT_MS = 600;

enum class Step : uint32_t {
	CONNECT = 1,
	FORGED_SIGNAL_SENT,
	FORK_CATCHER,
	TERMINATE_REFUSED,
	CATCHER_STATUS,
	ENDED_NOT_REFUSED,
	FORK_IGNORER,
	IGNORER_UNKNOWN,
	DIRECT_CALL_ALLOWED,
	UNKNOWN_SIGNAL_ACCEPTED,
	SOFT_SIGNALS_REFUSED,
	SOFT_SIGNALS_ENDED_IT,
	KILL_REFUSED,
	IGNORER_STATUS,
	KILLED_STILL_ALIVE,
	FORK_DENIER,
	DENIER_STATUS,
	FORK_SLEEPER,
	SLEEPER_UNKNOWN,
	STOP_REFUSED,
	STOPPED_NOT_REPORTED,
	STOPPED_STILL_RAN,
	CONTINUE_REFUSED,
	CONTINUED_NOT_REPORTED,
	SLEEPER_STATUS,
	FORK_VICTIM,
	VICTIM_UNKNOWN,
	STOPPED_NOT_KILLED,
	CONTINUE_OF_RUNNING_REFUSED,
};

uint32_t kernelIdOf(Client &client, uint32_t pid) {
	for (uint32_t index = 0;; index++) {
		Listing row = client.list(index);
		if (row.status != Status::OK) return NO_KERNEL_ID;
		if (row.pid == pid) return row.kernelId;
	}
}

uint32_t parentOf(Client &client, uint32_t kernelId) {
	for (uint32_t index = 0;; index++) {
		Listing row = client.list(index);
		if (row.status != Status::OK) return cock::abi::pm::NO_PID;
		if (row.kernelId == kernelId) return row.parent;
	}
}

int runCatcher() {
	cock::ipc::Message message = {};
	for (;;) {
		if (cock::user::ipc_recv_timeout(message, CATCH_TIMEOUT_MS) != cock::user::Received::MESSAGE)
			return TIMED_OUT_STATUS;
		if (message.type == cock::ipc::MessageType::SIGNAL && message.payload.args.arg0 == SIGNAL_TERMINATE)
			return CAUGHT_STATUS;
	}
}

int runIgnorer() {
	for (uint32_t round = 0; round < IGNORE_ROUNDS; round++) cock::user::sleep_ms(IGNORE_STEP_MS);
	return TIMED_OUT_STATUS;
}

int runDenier(Client &client) {
	uint32_t parent = parentOf(client, cock::user::self());
	if (parent == cock::abi::pm::NO_PID) return DENIER_NO_ROW;
	if (client.kill(parent, SIGNAL_TERMINATE) != Status::DENIED) return DENIER_WAS_SERVED;
	return DENIER_PASSED;
}

cock::abi::pm::State stateOf(Client &client, uint32_t pid) {
	for (uint32_t index = 0;; index++) {
		Listing row = client.list(index);
		if (row.status != Status::OK) return cock::abi::pm::State::FREE;
		if (row.pid == pid) return row.state;
	}
}

int runSleeper() {
	cock::user::sleep_ms(SLEEPER_MS);
	return SLEEPER_DONE_STATUS;
}

int fail(Step step) {
	printf("sigprobe: wrong at step %u\n", static_cast<unsigned>(step));
	return FAILED;
}

bool forgedSignalIsRefused() {
	cock::ipc::Message forged = {};
	forged.type = cock::ipc::MessageType::SIGNAL;
	forged.payload.args.arg0 = SIGNAL_KILL;
	return !cock::user::ipc_send(cock::user::self(), forged);
}

int checkCatcher(Client &client) {
	Forked forked = client.fork();
	if (forked.status != Status::OK) return static_cast<int>(Step::FORK_CATCHER);
	if (forked.isChild()) cock::user::exit_process(runCatcher());

	if (client.kill(forked.pid, SIGNAL_TERMINATE) != Status::OK) return static_cast<int>(Step::TERMINATE_REFUSED);
	Waited waited = client.wait(forked.pid);
	bool caught = waited.status == Status::OK && waited.exitStatus == static_cast<uint32_t>(CAUGHT_STATUS);
	if (!caught) return static_cast<int>(Step::CATCHER_STATUS);
	if (client.kill(forked.pid, SIGNAL_TERMINATE) != Status::NOT_FOUND) return static_cast<int>(Step::ENDED_NOT_REFUSED);
	return PASSED;
}

int checkIgnorer(Client &client) {
	Forked forked = client.fork();
	if (forked.status != Status::OK) return static_cast<int>(Step::FORK_IGNORER);
	if (forked.isChild()) cock::user::exit_process(runIgnorer());

	uint32_t kernelId = kernelIdOf(client, forked.pid);
	if (kernelId == NO_KERNEL_ID) return static_cast<int>(Step::IGNORER_UNKNOWN);
	if (cock::user::proc_signal(kernelId, SIGNAL_KILL) || !cock::user::process_alive(kernelId))
		return static_cast<int>(Step::DIRECT_CALL_ALLOWED);
	if (client.kill(forked.pid, UNKNOWN_SIGNAL) != Status::BAD_REQUEST)
		return static_cast<int>(Step::UNKNOWN_SIGNAL_ACCEPTED);

	if (client.kill(forked.pid, SIGNAL_TERMINATE) != Status::OK ||
		client.kill(forked.pid, SIGNAL_INTERRUPT) != Status::OK)
		return static_cast<int>(Step::SOFT_SIGNALS_REFUSED);
	cock::user::sleep_ms(SETTLE_MS);
	if (!cock::user::process_alive(kernelId)) return static_cast<int>(Step::SOFT_SIGNALS_ENDED_IT);

	if (client.kill(forked.pid, SIGNAL_KILL) != Status::OK) return static_cast<int>(Step::KILL_REFUSED);
	Waited waited = client.wait(forked.pid);
	if (waited.status != Status::OK || waited.exitStatus != KILLED_STATUS)
		return static_cast<int>(Step::IGNORER_STATUS);
	if (cock::user::process_alive(kernelId)) return static_cast<int>(Step::KILLED_STILL_ALIVE);
	return PASSED;
}

int checkDenier(Client &client) {
	Forked forked = client.fork();
	if (forked.status != Status::OK) return static_cast<int>(Step::FORK_DENIER);
	if (forked.isChild()) cock::user::exit_process(runDenier(client));

	Waited waited = client.wait(forked.pid);
	bool denied = waited.status == Status::OK && waited.exitStatus == static_cast<uint32_t>(DENIER_PASSED);
	return denied ? PASSED : static_cast<int>(Step::DENIER_STATUS);
}

int checkStopAndContinue(Client &client) {
	Forked forked = client.fork();
	if (forked.status != Status::OK) return static_cast<int>(Step::FORK_SLEEPER);
	if (forked.isChild()) cock::user::exit_process(runSleeper());

	uint32_t kernelId = kernelIdOf(client, forked.pid);
	if (kernelId == NO_KERNEL_ID) return static_cast<int>(Step::SLEEPER_UNKNOWN);
	if (client.kill(forked.pid, SIGNAL_CONTINUE) != Status::OK)
		return static_cast<int>(Step::CONTINUE_OF_RUNNING_REFUSED);
	if (client.kill(forked.pid, SIGNAL_STOP) != Status::OK) return static_cast<int>(Step::STOP_REFUSED);
	if (stateOf(client, forked.pid) != cock::abi::pm::State::STOPPED)
		return static_cast<int>(Step::STOPPED_NOT_REPORTED);

	cock::user::sleep_ms(STOP_WAIT_MS);
	if (!cock::user::process_alive(kernelId)) return static_cast<int>(Step::STOPPED_STILL_RAN);
	if (stateOf(client, forked.pid) != cock::abi::pm::State::STOPPED)
		return static_cast<int>(Step::STOPPED_NOT_REPORTED);

	if (client.kill(forked.pid, SIGNAL_CONTINUE) != Status::OK) return static_cast<int>(Step::CONTINUE_REFUSED);
	if (stateOf(client, forked.pid) == cock::abi::pm::State::STOPPED)
		return static_cast<int>(Step::CONTINUED_NOT_REPORTED);
	Waited waited = client.wait(forked.pid);
	bool finished = waited.status == Status::OK && waited.exitStatus == static_cast<uint32_t>(SLEEPER_DONE_STATUS);
	return finished ? PASSED : static_cast<int>(Step::SLEEPER_STATUS);
}

int checkStoppedProcessCanBeKilled(Client &client) {
	Forked forked = client.fork();
	if (forked.status != Status::OK) return static_cast<int>(Step::FORK_VICTIM);
	if (forked.isChild()) cock::user::exit_process(runSleeper());

	if (kernelIdOf(client, forked.pid) == NO_KERNEL_ID) return static_cast<int>(Step::VICTIM_UNKNOWN);
	if (client.kill(forked.pid, SIGNAL_STOP) != Status::OK) return static_cast<int>(Step::STOP_REFUSED);
	if (client.kill(forked.pid, SIGNAL_KILL) != Status::OK) return static_cast<int>(Step::STOPPED_NOT_KILLED);
	Waited waited = client.wait(forked.pid);
	bool killed = waited.status == Status::OK && waited.exitStatus == KILLED_STATUS;
	return killed ? PASSED : static_cast<int>(Step::STOPPED_NOT_KILLED);
}

} // namespace

extern "C" int main() {
	cock::user::names::Connection manager = cock::user::names::connect(SERVER_NAME);
	if (manager.status != cock::abi::names::Status::OK) return fail(Step::CONNECT);
	Client client(manager.handle);

	if (!forgedSignalIsRefused()) return fail(Step::FORGED_SIGNAL_SENT);
	int result = checkCatcher(client);
	if (result != PASSED) return fail(static_cast<Step>(result));
	result = checkIgnorer(client);
	if (result != PASSED) return fail(static_cast<Step>(result));
	result = checkDenier(client);
	if (result != PASSED) return fail(static_cast<Step>(result));
	result = checkStopAndContinue(client);
	if (result != PASSED) return fail(static_cast<Step>(result));
	result = checkStoppedProcessCanBeKilled(client);
	if (result != PASSED) return fail(static_cast<Step>(result));

	puts("sigprobe: ok");
	return PASSED;
}
