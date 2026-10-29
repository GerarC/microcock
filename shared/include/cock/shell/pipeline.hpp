/**
 * @file pipeline.hpp
 * @brief Groups the tokens of a line into the commands of a pipeline and their redirections. No I/O, tested on the host.
 */
#ifndef COCK_SHELL_PIPELINE_HPP
#define COCK_SHELL_PIPELINE_HPP

#include <cock/shell/words.hpp>
#include <stddef.h>

namespace cock::shell {

/** Most commands joined by `|` in one line. */
constexpr size_t MAX_STAGES = 4;
/** Number of commands of a line that has no pipe. */
constexpr size_t SINGLE_STAGE = 1;
/** Index of the first command of a pipeline. */
constexpr size_t FIRST_STAGE = 0;
/** Number of arguments a command has when it names no program. */
constexpr size_t NO_ARGUMENTS = 0;

/** Outcome of `parsePipeline`. */
enum class PipelineResult {
	/** The line was grouped. */
	OK,
	/** A command has no program: the line starts or ends with `|`, has `| |`, or only redirections. */
	MISSING_COMMAND,
	/** A redirection is not followed by a file name (a word). */
	MISSING_FILE,
	/** More than `MAX_STAGES` commands. */
	TOO_MANY_STAGES,
	/** `<` on a command that is not the first, or `>` and `>>` on a command that is not the last. */
	MISPLACED_REDIRECTION,
};

/** One command of a pipeline. */
struct Stage {
	/** The program name and its arguments, in order, with the redirections taken out. */
	const char *argv[MAX_WORDS];
	/** Number of entries of `argv`, at least 1 after a successful `parsePipeline`. */
	size_t argc;
	/** File that becomes the standard input, or null. */
	const char *input;
	/** File that becomes the standard output, or null. */
	const char *output;
	/** True when `output` is appended to instead of replaced. */
	bool append;
};

/** The commands of a line, in the order they were typed. */
struct Pipeline {
	/** The commands. */
	Stage stages[MAX_STAGES];
	/** Number of commands, at least 1 after a successful `parsePipeline`. */
	size_t count;
};

/**
 * @brief Tells whether a command has a redirection.
 * @param stage The command.
 * @return True when it reads from or writes to a file.
 */
inline bool isRedirected(const Stage &stage) {
	return stage.input != nullptr || stage.output != nullptr;
}

/**
 * @brief Groups the tokens of a split line into commands.
 *
 * `|` ends a command. A redirection takes the word after it as the file; a second redirection of the same kind in one
 * command replaces the first. Words that are not file names are the program and its arguments, wherever the
 * redirections sit among them (`echo a > f b` has the arguments `echo`, `a`, `b`). Input may be redirected only on the
 * first command and output only on the last: the middle of a pipe is already connected.
 * @param words The tokens of `split`.
 * @param pipeline Receives the commands; its content is undefined unless the result is `OK`.
 * @return `OK`, or the reason the line is not a pipeline.
 */
inline PipelineResult parsePipeline(const Words &words, Pipeline &pipeline) {
	pipeline = {};
	pipeline.count = SINGLE_STAGE;
	Stage *stage = &pipeline.stages[pipeline.count - SINGLE_STAGE];

	for (size_t index = 0; index < words.count; index++) {
		TokenKind kind = words.kinds[index];
		if (kind == TokenKind::WORD) {
			stage->argv[stage->argc++] = words.items[index];
			continue;
		}
		if (kind == TokenKind::PIPE) {
			if (stage->argc == NO_ARGUMENTS) return PipelineResult::MISSING_COMMAND;
			if (pipeline.count == MAX_STAGES) return PipelineResult::TOO_MANY_STAGES;
			stage = &pipeline.stages[pipeline.count++];
			continue;
		}

		index++;
		if (index == words.count || words.kinds[index] != TokenKind::WORD) return PipelineResult::MISSING_FILE;
		if (kind == TokenKind::REDIRECT_INPUT) {
			stage->input = words.items[index];
		} else {
			stage->output = words.items[index];
			stage->append = kind == TokenKind::REDIRECT_APPEND;
		}
	}

	for (size_t index = 0; index < pipeline.count; index++) {
		const Stage &each = pipeline.stages[index];
		if (each.argc == NO_ARGUMENTS) return PipelineResult::MISSING_COMMAND;
		if (each.input != nullptr && index != FIRST_STAGE) return PipelineResult::MISPLACED_REDIRECTION;
		if (each.output != nullptr && index != pipeline.count - SINGLE_STAGE)
			return PipelineResult::MISPLACED_REDIRECTION;
	}
	return PipelineResult::OK;
}

} // namespace cock::shell

#endif // !COCK_SHELL_PIPELINE_HPP
