# Diagnostics

What the kernel uses to report that something happened, and to stop when it cannot continue (`cock/utils/`).

| Page | Concept |
|------|---------|
| [logger.md](logger.md) | `Logger`: levels, the compile time filter and the run time threshold |
| [panic.md](panic.md) | `panic`: the unrecoverable error |
| [stack_protector.md](stack_protector.md) | Stack canaries and `__stack_chk_fail` |

All three write through `printf` to the kernel console, which goes to the serial port and, while it is enabled, to the VGA screen (see the console header in [the HAL](../hal.md)).
