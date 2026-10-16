/** @file
 * @brief Kernel logger with a compile time level, so disabled messages cost
 * nothing, and a run time threshold on top.
 */
#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <stdarg.h>

/** @brief Lowest level compiled in. Defined by the build, TRACE by default.
 * Calls below it are removed by the compiler. */
#ifndef COCK_LOG_LEVEL
#define COCK_LOG_LEVEL ::cock::utils::LogLevel::TRACE
#endif

namespace cock::utils {

/**
 * @brief Severity levels for the kernel logger.
 */
enum class LogLevel {
	TRACE = 0, /**< Fine-grained informational events. */
	DEBUG = 1, /**< Diagnostic information for developers. */
	INFO = 2,  /**< General operational messages. */
	WARN = 3,  /**< Non-critical issues or potential problems. */
	ERROR = 4, /**< Errors that prevent a specific operation. */
	FATAL = 5  /**< Critical failures leading to a system halt. */
};

/**
 * @brief System-wide logging utility.
 * * Formats and dispatches log messages based on their severity level.
 * Output goes through printf to the kernel console, one line per message
 * with a level prefix and a trailing newline. Two filters apply: the compile
 * time level removes calls entirely, the run time level set by init drops the
 * rest. Not safe for concurrent use from several CPUs.
 */
class Logger {
  public:
	/**
	 * @brief Initializes the logger with a minimum severity threshold.
	 * @param level Messages below this level will be ignored.
	 */
	static void init(LogLevel level);

	/** @brief Lowest level kept at compile time, from COCK_LOG_LEVEL. */
	static constexpr LogLevel COMPILE_TIME_LEVEL = COCK_LOG_LEVEL;

	/**
	 * @brief Tells whether a level survives the compile time filter.
	 * @param level Level to test.
	 * @return True when it is at least COMPILE_TIME_LEVEL.
	 */
	static constexpr bool enabled(LogLevel level) {
		return level >= COMPILE_TIME_LEVEL;
	}

	/**
	 * @brief Logs a message at a level chosen at run time.
	 * @tparam Args Types of the format arguments.
	 * @param level Severity of the message.
	 * @param fmt printf style format string.
	 * @param args Format arguments, passed by value.
	 * @note The compile time check is a plain branch here, since the level is
	 * not a constant. The run time threshold still applies.
	 */
	template <typename... Args>
	[[gnu::always_inline]] static inline void log(LogLevel level, const char *fmt,
												  Args... args) {
		if (enabled(level)) write(level, fmt, args...);
	}

	/**
	 * @brief Logs at TRACE level, removed at compile time when disabled.
	 * @tparam Args Types of the format arguments.
	 * @param fmt printf style format string.
	 * @param args Format arguments.
	 */
	template <typename... Args>
	[[gnu::always_inline]] static inline void trace(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::TRACE)) write(LogLevel::TRACE, fmt, args...);
	}

	/**
	 * @brief Logs at DEBUG level, removed at compile time when disabled.
	 * @tparam Args Types of the format arguments.
	 * @param fmt printf style format string.
	 * @param args Format arguments.
	 */
	template <typename... Args>
	[[gnu::always_inline]] static inline void debug(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::DEBUG)) write(LogLevel::DEBUG, fmt, args...);
	}

	/**
	 * @brief Logs at INFO level, removed at compile time when disabled.
	 * @tparam Args Types of the format arguments.
	 * @param fmt printf style format string.
	 * @param args Format arguments.
	 */
	template <typename... Args>
	[[gnu::always_inline]] static inline void info(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::INFO)) write(LogLevel::INFO, fmt, args...);
	}

	/**
	 * @brief Logs at WARN level, removed at compile time when disabled.
	 * @tparam Args Types of the format arguments.
	 * @param fmt printf style format string.
	 * @param args Format arguments.
	 */
	template <typename... Args>
	[[gnu::always_inline]] static inline void warn(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::WARN)) write(LogLevel::WARN, fmt, args...);
	}

	/**
	 * @brief Logs at ERROR level, removed at compile time when disabled.
	 * @tparam Args Types of the format arguments.
	 * @param fmt printf style format string.
	 * @param args Format arguments.
	 */
	template <typename... Args>
	[[gnu::always_inline]] static inline void error(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::ERROR)) write(LogLevel::ERROR, fmt, args...);
	}

	/**
	 * @brief Logs at FATAL level, removed at compile time when disabled.
	 * @tparam Args Types of the format arguments.
	 * @param fmt printf style format string.
	 * @param args Format arguments.
	 * @note It only logs, it does not halt. Use panic to stop the kernel.
	 */
	template <typename... Args>
	[[gnu::always_inline]] static inline void fatal(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::FATAL)) write(LogLevel::FATAL, fmt, args...);
	}

  private:
	static LogLevel minLevel; /**< Run time threshold set by init. */

	/**
	 * @brief Variadic entry the inline helpers call after the compile time
	 * filter.
	 * @param level Severity of the message.
	 * @param fmt printf style format string.
	 * @param ... Format arguments.
	 */
	static void write(LogLevel level, const char *fmt, ...);

	/**
	 * @brief Converts a LogLevel enum to its string representation.
	 * @param level Level to name.
	 * @return A static string such as INFO, or UNK for an invalid value.
	 */
	static const char *levelToString(LogLevel level);

	/**
	 * @brief Internal variadic print routine for log formatting.
	 * @param level Severity of the message, dropped when below the run time
	 * threshold.
	 * @param fmt printf style format string.
	 * @param args Format arguments as a va_list.
	 */
	static void vlog(LogLevel level, const char *fmt, va_list args);
};

} // namespace cock::utils

#endif
