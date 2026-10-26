#include <cock/abi/console.hpp>
#include <cock/user/console.hpp>
#include <tty/output.hpp>

using cock::abi::console::Echo;

namespace proto = cock::abi::console;

namespace cock::tty {

namespace {

cock::user::console::Client driver;

void draw(char character, uint32_t terminal) {
	if (driver.connect()) driver.echo(&character, sizeof(character), terminal);
}

} // namespace

void echo(Echo kind, char character, uint32_t terminal) {
	switch (kind) {
	case Echo::NONE:
		return;
	case Echo::CHARACTER:
		return draw(character, terminal);
	case Echo::ERASE:
		return draw(proto::CHAR_BACKSPACE, terminal);
	case Echo::NEWLINE:
		return draw(proto::CHAR_NEWLINE, terminal);
	}
}

void show(uint32_t terminal) {
	if (driver.connect()) driver.show(terminal);
}

} // namespace cock::tty
