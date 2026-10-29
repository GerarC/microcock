#include <cock/abi/pm.hpp>
#include <ps/columns.hpp>
#include <ps/line.hpp>
#include <ps/palette.hpp>
#include <ps/state.hpp>
#include <stddef.h>
#include <stdint.h>

using cock::abi::pm::NO_PID;
using cock::abi::pm::ProcessRecord;
using cock::abi::pm::State;

namespace cock::ps {

namespace {

constexpr size_t PID_WIDTH = 4;
constexpr size_t PARENT_WIDTH = 4;
constexpr size_t STATE_WIDTH = 4;
constexpr size_t STATE_LETTER_WIDTH = 1;
constexpr size_t FIRST_CHARACTER = 0;
constexpr char NO_CHARACTER = '\0';
constexpr char SEPARATOR = ' ';
constexpr char MISSING[] = "-";
constexpr char PID_TITLE[] = "PID";
constexpr char PARENT_TITLE[] = "PPID";
constexpr char STATE_TITLE[] = "STAT";
constexpr char NAME_TITLE[] = "COMMAND";

bool hasExited(State state) {
	return state == State::EXITED;
}

} // namespace

void printHeader() {
	Line line;
	line.text(HEADER);
	line.rightAligned(PID_TITLE, PID_WIDTH);
	line.character(SEPARATOR);
	line.rightAligned(PARENT_TITLE, PARENT_WIDTH);
	line.character(SEPARATOR);
	line.text(STATE_TITLE);
	line.character(SEPARATOR);
	line.text(NAME_TITLE);
	line.text(RESET);
	line.finish();
}

void printRow(const ProcessRecord &record) {
	Line line;
	line.text(NUMBER);
	line.number(record.pid, PID_WIDTH);
	line.character(SEPARATOR);
	if (record.parent == NO_PID) {
		line.text(MUTED);
		line.rightAligned(MISSING, PARENT_WIDTH);
	} else {
		line.number(record.parent, PARENT_WIDTH);
	}
	line.character(SEPARATOR);

	line.text(stateColor(record.state));
	line.character(stateLetter(record.state));
	line.text(RESET);
	line.spaces(STATE_WIDTH - STATE_LETTER_WIDTH);
	line.character(SEPARATOR);

	bool named = record.name[FIRST_CHARACTER] != NO_CHARACTER;
	line.text(named && !hasExited(record.state) ? NAME : MUTED);
	line.text(named ? record.name : MISSING);
	line.text(RESET);
	line.finish();
}

} // namespace cock::ps
