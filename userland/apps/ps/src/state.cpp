#include <ps/state.hpp>

using cock::abi::pm::State;

namespace cock::ps {

namespace {

constexpr char STATE_RUNNING = 'R';
constexpr char STATE_SLEEPING = 'S';
constexpr char STATE_STOPPED = 'T';
constexpr char STATE_ZOMBIE = 'Z';
constexpr char STATE_EXITED = 'X';
constexpr char STATE_UNINTERRUPTIBLE = 'D';
constexpr char STATE_UNKNOWN = '?';

} // namespace

char stateLetter(State state) {
	switch (state) {
	case State::RUNNING:
		return STATE_RUNNING;
	case State::SLEEPING:
		return STATE_SLEEPING;
	case State::STOPPED:
		return STATE_STOPPED;
	case State::ZOMBIE:
		return STATE_ZOMBIE;
	case State::EXITED:
		return STATE_EXITED;
	case State::UNINTERRUPTIBLE:
		return STATE_UNINTERRUPTIBLE;
	case State::FREE:
		return STATE_UNKNOWN;
	}
	return STATE_UNKNOWN;
}

} // namespace cock::ps
