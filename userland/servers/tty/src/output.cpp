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

void drawRepeated(char character, size_t count, uint32_t terminal) {
	char chunk[proto::CHUNK_BYTES];
	for (size_t index = 0; index < proto::CHUNK_BYTES; index++)
		chunk[index] = character;

	while (count > 0) {
		size_t length = count < proto::CHUNK_BYTES ? count : proto::CHUNK_BYTES;
		if (!driver.connect() || !driver.echo(chunk, length, terminal)) return;
		count -= length;
	}
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
	case Echo::REPLACE:
		return;
	}
}

void redraw(size_t erased, const char *text, size_t length, uint32_t terminal) {
	drawRepeated(proto::CHAR_BACKSPACE, erased, terminal);

	for (size_t offset = 0; offset < length; offset += proto::CHUNK_BYTES) {
		size_t left = length - offset;
		size_t count = left < proto::CHUNK_BYTES ? left : proto::CHUNK_BYTES;
		if (!driver.connect() || !driver.echo(text + offset, count, terminal)) return;
	}
}

void show(uint32_t terminal) {
	if (driver.connect()) driver.show(terminal);
}

} // namespace cock::tty
