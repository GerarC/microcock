#include <tty/output.hpp>
#include <tty/terminal.hpp>

using cock::abi::console::CHUNK_BYTES;
using cock::abi::console::FIRST_TERMINAL;
using cock::abi::console::TERMINAL_COUNT;
using cock::user::NO_PROCESS;

namespace cock::tty {

namespace {

constexpr size_t NO_ITEMS = 0;

uint32_t active = FIRST_TERMINAL;
bool ownersReady = false;

void prepareOwners() {
	if (ownersReady) return;
	for (Terminal &terminal : terminals) terminal.owner = NO_PROCESS;
	ownersReady = true;
}

void clear(Terminal &terminal) {
	char sink[CHUNK_BYTES];
	while (terminal.discipline.hasInput()) terminal.discipline.take(sink, sizeof(sink));
	terminal.readerCount = NO_ITEMS;
	terminal.returnedLength = NO_ITEMS;
}

} // namespace

Terminal terminals[TERMINAL_COUNT];

uint32_t activeTerminal() {
	return active;
}

void activate(uint32_t terminal) {
	if (terminal >= TERMINAL_COUNT || terminal == active) return;
	active = terminal;
	show(terminal);
}

bool attach(uint32_t owner, uint32_t &terminal) {
	prepareOwners();
	for (uint32_t index = FIRST_TERMINAL; index < TERMINAL_COUNT; index++) {
		Terminal &candidate = terminals[index];
		if (candidate.owner != NO_PROCESS && cock::user::process_alive(candidate.owner)) continue;

		clear(candidate);
		candidate.owner = owner;
		terminal = index;
		return true;
	}
	return false;
}

} // namespace cock::tty
