# Logger

`cock::utils::Logger` (`cock/utils/logger.hpp`) writes one line per message to the kernel console: a level prefix such as `[INFO] `, the formatted text and a newline.

## Levels

| Level | Use |
|-------|-----|
| `TRACE` | Fine grained events, such as an unknown system call |
| `DEBUG` | Diagnostic information for developers |
| `INFO` | General operational messages |
| `WARN` | A problem that does not stop the operation |
| `ERROR` | An operation failed, such as an exception from a program |
| `FATAL` | A critical failure. It only logs: it does not halt, use [panic](panic.md) for that |

Each level has a function (`Logger::trace`, `debug`, `info`, `warn`, `error`, `fatal`) that takes a `printf` style format. `Logger::log(level, ...)` takes the level as a value.

## Two filters

A message must pass two filters, and the first one costs nothing:

1. **Compile time.** The build sets `COCK_LOG_LEVEL` from `make LOG_LEVEL=<level>` (`TRACE` by default). Each helper is a template whose body is behind `if constexpr (enabled(level))`, so a call below the level is removed by the compiler: no code, no format string, no arguments evaluated at run time. `Logger::log` takes a level that is not a constant, so there the check is a plain branch.
2. **Run time.** `Logger::init(level)` sets a threshold, and `vlog` drops anything below it. The kernel sets it to `DEBUG` during boot (`cock_main`, see [kernel main](../kernel_main.md)). The initial value, before `init`, is `TRACE`.

A release kernel built with `LOG_LEVEL=WARN` therefore contains no trace, debug or info call at all.

## Limits

The logger is not safe for concurrent use from several CPUs. The kernel runs on one CPU, so it needs no lock, and a message is printed with several `printf` calls that interrupts could in principle separate, so a handler that logs from inside an interrupt may interleave its line with another.
