#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <stdarg.h>

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
 */
class Logger {
  public:
	/**
	 * @brief Initializes the logger with a minimum severity threshold.
	 * @param level Messages below this level will be ignored.
	 */
	static void init(LogLevel level);

	static constexpr LogLevel COMPILE_TIME_LEVEL = COCK_LOG_LEVEL;

	static constexpr bool enabled(LogLevel level) {
		return level >= COMPILE_TIME_LEVEL;
	}

	template <typename... Args>
	[[gnu::always_inline]] static inline void log(LogLevel level, const char *fmt,
												  Args... args) {
		if (enabled(level)) write(level, fmt, args...);
	}

	template <typename... Args>
	[[gnu::always_inline]] static inline void trace(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::TRACE)) write(LogLevel::TRACE, fmt, args...);
	}

	template <typename... Args>
	[[gnu::always_inline]] static inline void debug(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::DEBUG)) write(LogLevel::DEBUG, fmt, args...);
	}

	template <typename... Args>
	[[gnu::always_inline]] static inline void info(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::INFO)) write(LogLevel::INFO, fmt, args...);
	}

	template <typename... Args>
	[[gnu::always_inline]] static inline void warn(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::WARN)) write(LogLevel::WARN, fmt, args...);
	}

	template <typename... Args>
	[[gnu::always_inline]] static inline void error(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::ERROR)) write(LogLevel::ERROR, fmt, args...);
	}

	template <typename... Args>
	[[gnu::always_inline]] static inline void fatal(const char *fmt, Args... args) {
		if constexpr (enabled(LogLevel::FATAL)) write(LogLevel::FATAL, fmt, args...);
	}

  private:
	static LogLevel minLevel;

	static void write(LogLevel level, const char *fmt, ...);

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
