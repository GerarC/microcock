#include <cock/abi/console.hpp>
#include <cock/user/console.hpp>
#include <tty/output.hpp>

using cock::abi::console::Echo;

namespace proto = cock::abi::console;

namespace cock::tty {

namespace {

cock::user::console::Client driver;

void draw(char character) {
	if (driver.connect()) driver.echo(&character, sizeof(character));
}

} // namespace

void echo(Echo kind, char character) {
	switch (kind) {
	case Echo::NONE:
		return;
	case Echo::CHARACTER:
		return draw(character);
	case Echo::ERASE:
		return draw(proto::CHAR_BACKSPACE);
	case Echo::NEWLINE:
		return draw(proto::CHAR_NEWLINE);
	}
}

} // namespace cock::tty
