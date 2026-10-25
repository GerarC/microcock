#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <pipe/ends.hpp>
#include <pipe/limits.hpp>
#include <pipe/requests.hpp>
#include <pipe/waiters.hpp>

namespace cock::pipe {

namespace {

struct Waiter {
	bool used;
	uint32_t ticket;
	uint32_t owner;
	uint32_t request[CALL_WORDS];
};

Waiter waiters[MAX_WAITERS];

} // namespace

bool holdCall(uint32_t sender, const uint32_t (&request)[CALL_WORDS]) {
	for (Waiter &waiter : waiters) {
		if (waiter.used) continue;

		uint32_t ticket = cock::user::ipc_defer();
		if (ticket == cock::user::NO_TICKET) return false;

		waiter = {true, ticket, sender, {}};
		for (size_t index = 0; index < CALL_WORDS; index++)
			waiter.request[index] = request[index];
		return true;
	}
	return false;
}

void retryWaiters() {
	bool answered = true;
	while (answered) {
		answered = false;
		for (Waiter &waiter : waiters) {
			if (!waiter.used) continue;

			uint32_t reply[CALL_WORDS] = {};
			if (attempt(waiter.owner, waiter.request, reply) == Attempt::BLOCKED) continue;

			cock::user::ipc_reply_to(waiter.ticket, reply);
			waiter.used = false;
			answered = true;
		}
	}
}

void sweepDead() {
	releaseDeadOwners();
	for (Waiter &waiter : waiters)
		if (waiter.used && !cock::user::process_alive(waiter.owner)) waiter.used = false;
	retryWaiters();
}

} // namespace cock::pipe
