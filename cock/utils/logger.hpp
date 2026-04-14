#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <stdarg.h>

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
 */
class Logger {
  public:
	/**
	 * @brief Initializes the logger with a minimum severity threshold.
	 * @param level Messages below this level will be ignored.
	 */
	static void init(LogLevel level);

	static void log(LogLevel level, const char *fmt, ...);
	static void trace(const char *fmt, ...);
	static void debug(const char *fmt, ...);
	static void info(const char *fmt, ...);
	static void warn(const char *fmt, ...);
	static void error(const char *fmt, ...);
	static void fatal(const char *fmt, ...);

  private:
	static LogLevel minLevel;

	/**
	 * @brief Converts a LogLevel enum to its string representation.
	 */
	static const char *levelToString(LogLevel level);

	/**
	 * @brief Internal variadic print routine for log formatting.
	 */
	static void vlog(LogLevel level, const char *fmt, va_list args);
};

} // namespace cock::utils

#endif
