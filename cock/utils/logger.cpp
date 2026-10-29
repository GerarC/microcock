#include <cock/utils/logger.hpp>
#include <stdio.h>

namespace cock::utils {

LogLevel Logger::minLevel = LogLevel::TRACE;

void Logger::init(LogLevel level) { minLevel = level; }

constexpr const char *LOG_LEVEL_TEMPLATE = "[%s] ";

const char *Logger::levelToString(LogLevel level) {
	switch (level) {
		case LogLevel::TRACE:
			return "TRACE";
		case LogLevel::DEBUG:
			return "DEBUG";
		case LogLevel::INFO:
			return "INFO";
		case LogLevel::WARN:
			return "WARN";
		case LogLevel::ERROR:
			return "ERROR";
		case LogLevel::FATAL:
			return "FATAL";
		default:
			return "UNK";
	}
}

void Logger::vlog(LogLevel level, const char *fmt, va_list args) {
	if (level < minLevel) return;

	printf(LOG_LEVEL_TEMPLATE, levelToString(level));
	vprintf(fmt, args);
	printf("\n");
}

void Logger::write(LogLevel level, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vlog(level, fmt, args);
	va_end(args);
}

} // namespace cock::utils
