#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <stdarg.h>

namespace cock::utils {

enum class LogLevel {
	TRACE = 0,
	DEBUG = 1,
	INFO = 2,
	WARN = 3,
	ERROR = 4,
	FATAL = 5
};

class Logger {
  public:
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
	static const char *levelToString(LogLevel level);
	static void vlog(LogLevel level, const char *fmt, va_list args);
};

} // namespace cock::utils

#endif
