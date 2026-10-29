/** @file
 * @brief The terminals: for each one, its line being edited, its finished lines, its waiting readers and its owner.
 *
 * Nothing is shared between terminals, so the input typed on one never reaches a program of another. The terminal that
 * is on the screen is the active one: the keys go to it.
 */
#ifndef TTY_TERMINAL_HPP
#define TTY_TERMINAL_HPP

#include <cock/abi/console.hpp>
#include <cock/abi/console_line.hpp>
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::tty {

/** @brief Most readers that wait for a line on one terminal. */
constexpr size_t MAX_READERS = 8;

/** @brief A reader held with a deferred reply. */
struct Reader {
	/** Ticket of the deferred call. */
	uint32_t ticket;
	/** Most bytes the reader asked for. */
	size_t length;
};

/** @brief The state of one terminal. */
struct Terminal {
	/** The line being edited and the finished lines. */
	cock::abi::console::LineDiscipline discipline;
	/** The readers that wait for a finished line, oldest first. */
	Reader readers[MAX_READERS];
	/** Number of valid entries in `readers`. */
	size_t readerCount;
	/** Bytes taken for a reader that died, to give to the next one. */
	char returned[cock::abi::console::CHUNK_BYTES];
	/** Number of valid bytes in `returned`. */
	size_t returnedLength;
	/** Process id of the program attached to the terminal, or `NO_PROCESS`. */
	uint32_t owner;
};

/** @brief The terminals, numbered from `FIRST_TERMINAL`. */
extern Terminal terminals[cock::abi::console::TERMINAL_COUNT];

/**
 * @brief Gives the terminal that is on the screen.
 * @return Its number.
 */
uint32_t activeTerminal();

/**
 * @brief Puts a terminal on the screen: the keys go to it from now on, and the console driver is asked to show it.
 * @param terminal Terminal number; one that does not exist is ignored.
 */
void activate(uint32_t terminal);

/**
 * @brief Attaches a program to the lowest terminal that has no living owner.
 *
 * A terminal whose owner died is cleared first: its waiting readers, the finished lines nobody read and the bytes kept
 * for a dead reader are dropped, so the new owner does not receive what the old one left.
 *
 * @param owner Process id of the program.
 * @param terminal Receives the terminal number.
 * @return True when a terminal was given; false when every terminal has a living owner.
 */
bool attach(uint32_t owner, uint32_t &terminal);

} // namespace cock::tty

#endif // !TTY_TERMINAL_HPP
