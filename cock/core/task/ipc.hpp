#ifndef CORE_IPC_HPP
#define CORE_IPC_HPP

#include <cock/ipc/message.hpp>
#include <stdint.h>

namespace cock::core::task {

using ipc::Message;

class IPC {
  public:
	static void send(uint32_t to_pid, Message message);
	static void receive(Message *message);
};

} // namespace cock::core::task

#endif // !CORE_IPC_HPP
