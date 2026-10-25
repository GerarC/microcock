#include <cock/abi/exec.hpp>
#include <cock/exec/args.hpp>
#include <string.h>
#include <test/host/check.hpp>

namespace host_test {

namespace {

using cock::abi::exec::ArgsBlock;
using cock::abi::exec::MAX_ARG_BYTES;
using cock::abi::exec::MAX_ARGS;

constexpr uint32_t SOME_HANDLE = 5;
constexpr uint32_t NO_ARGS = 0;
constexpr uint32_t TWO_ARGS = 2;
constexpr uint32_t FIRST_INDEX = 0;
constexpr uint32_t SECOND_INDEX = 1;
constexpr uint32_t BAD_OFFSET_ADD = 1;
constexpr char FIRST_ARG[] = "prog";
constexpr char SECOND_ARG[] = "-v";
constexpr char LETTER[] = "x";

void checkBuildAndRead() {
	const char *argv[] = {FIRST_ARG, SECOND_ARG};
	ArgsBlock block;
	CHECK_TRUE(cock::exec::build(block, SOME_HANDLE, argv, TWO_ARGS));
	CHECK_TRUE(cock::exec::valid(block));
	CHECK_EQ(SOME_HANDLE, block.initialHandle);
	CHECK_EQ(TWO_ARGS, block.count);
	CHECK_TRUE(strcmp(cock::exec::argument(block, FIRST_INDEX), FIRST_ARG) == 0);
	CHECK_TRUE(strcmp(cock::exec::argument(block, SECOND_INDEX), SECOND_ARG) == 0);
	CHECK_TRUE(cock::exec::argument(block, TWO_ARGS) == nullptr);

	ArgsBlock none;
	CHECK_TRUE(cock::exec::build(none, cock::abi::exec::NO_INITIAL_HANDLE, argv, NO_ARGS));
	CHECK_TRUE(cock::exec::valid(none));
	CHECK_TRUE(cock::exec::argument(none, FIRST_INDEX) == nullptr);
}

void checkLimits() {
	const char *many[MAX_ARGS + 1];
	for (const char *&entry : many) entry = LETTER;
	ArgsBlock block;
	CHECK_TRUE(cock::exec::build(block, SOME_HANDLE, many, MAX_ARGS));
	CHECK_FALSE(cock::exec::build(block, SOME_HANDLE, many, MAX_ARGS + 1));
	CHECK_FALSE(cock::exec::valid(block));

	char longest[MAX_ARG_BYTES + 1];
	memset(longest, 'a', sizeof(longest));
	longest[MAX_ARG_BYTES] = '\0';
	const char *one[] = {longest};
	CHECK_FALSE(cock::exec::build(block, SOME_HANDLE, one, 1));
	longest[MAX_ARG_BYTES - 1] = '\0';
	CHECK_TRUE(cock::exec::build(block, SOME_HANDLE, one, 1));
}

void checkValid() {
	const char *argv[] = {FIRST_ARG, SECOND_ARG};
	ArgsBlock block;
	cock::exec::build(block, SOME_HANDLE, argv, TWO_ARGS);

	ArgsBlock copy = block;
	copy.magic = 0;
	CHECK_FALSE(cock::exec::valid(copy));

	copy = block;
	copy.count = MAX_ARGS + 1;
	CHECK_FALSE(cock::exec::valid(copy));

	copy = block;
	copy.bytes = MAX_ARG_BYTES + 1;
	CHECK_FALSE(cock::exec::valid(copy));

	copy = block;
	copy.offsets[SECOND_INDEX] = copy.bytes + BAD_OFFSET_ADD;
	CHECK_FALSE(cock::exec::valid(copy));

	copy = block;
	copy.strings[copy.bytes - 1] = 'z';
	CHECK_FALSE(cock::exec::valid(copy));

	copy = block;
	copy.bytes = 0;
	CHECK_FALSE(cock::exec::valid(copy));
}

} // namespace

void testExecArgs() {
	checkBuildAndRead();
	checkLimits();
	checkValid();
}

} // namespace host_test
