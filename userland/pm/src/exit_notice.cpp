#include <cock/user/api.hpp>
#include <pm/exit_notice.hpp>
#include <pm/state.hpp>
#include <pm/wait.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

bool sendExit(const cock::pm::Row &row) {
	Message notice = {};
	notice.type = MessageType::PM_EXITED;
	notice.payload.args.arg0 = row.kernelId;
	notice.payload.args.arg1 = row.status;
	return cock::user::ep_send(cock::pm::rootHandle, notice);
}

bool sendSpawned(const cock::pm::Row &row) {
	Message notice = {};
	notice.type = MessageType::PM_SPAWNED;
	notice.payload.args.arg0 = row.kernelId;
	notice.payload.args.arg1 = row.label;
	return cock::user::ep_send(cock::pm::rootHandle, notice);
}

bool sendForked(const cock::pm::Row &row, uint32_t parentKernelId) {
	Message notice = {};
	notice.type = MessageType::PM_FORKED;
	notice.payload.args.arg0 = row.kernelId;
	notice.payload.args.arg1 = parentKernelId;
	return cock::user::ep_send(cock::pm::rootHandle, notice);
}

} // namespace

namespace cock::pm {

void takeMessage(const Message &message) {
	if (message.type != MessageType::PROCESS_EXITED) return;
	Row *row = table.markExited(message.payload.args.arg0, message.payload.args.arg1);
	if (row == nullptr) return;
	if (row->forward && sendExit(*row)) row->forwardPending = false;
	answerWaiter(*row);
}

bool flushNotices() {
	for (Row *row = table.pendingAnnounce(); row != nullptr; row = table.pendingAnnounce()) {
		if (!sendSpawned(*row)) return true;
		row->announcePending = false;
	}
	for (Row *row = table.pendingFork(); row != nullptr; row = table.pendingFork()) {
		const Row *parent = table.find(row->parent);
		if (parent != nullptr && !sendForked(*row, parent->kernelId)) return true;
		row->forkPending = false;
	}
	for (Row *row = table.pendingForward(); row != nullptr; row = table.pendingForward()) {
		if (!sendExit(*row)) return true;
		row->forwardPending = false;
	}
	return false;
}

} // namespace cock::pm
