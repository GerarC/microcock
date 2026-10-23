# rtc

The clock server. It is the only holder of the CMOS ports, reads the real time clock and answers date and time requests. The file system servers use it to stamp the files they create or change, so they need no port capability of their own.

## Where it sits

```text
fat, ext --RTC_READ--> rtc --port I/O--> CMOS index and data ports
```

- Capability, from `initrd/manifest.conf`: `io:0x70+2`, the index and data ports of the CMOS (`rtc path=bin/rtc.elf`). The test manifest adds nothing for it.
- `init` starts it and sends `CAPABILITY_GRANTED`. Requests that arrive before the grant are kept (up to 16) and answered once the ports are open. If the grant is missing it prints `rtc: no clock ports` and exits; otherwise it prints `rtc: ready`.
- Clients are `fat` and `ext` (through `cock::user::Clock`, `userland/lib/include/cock/user/clock.hpp`). They look the server up by name `rtc` (`cock::abi::rtc::SERVER_NAME`) and fall back to a fixed date when it is missing or gives a bad value.

## Protocol

Header: `shared/include/cock/abi/rtc.hpp`; conversions in `shared/include/cock/abi/datetime.hpp`. Messages are asynchronous IPC messages, not synchronous calls.

| Message | Arguments | Notes |
|---------|-----------|-------|
| `RTC_READ` | `arg0` request id | Sent by a client; messages from `KERNEL_SENDER` are ignored |
| `RTC_REPLY` | `arg0` the id, `arg1` status, `arg2` packed date, `arg3` packed time | Sent to the requester |

Statuses: `OK` and `UNAVAILABLE` (the clock could not give a stable, valid reading). A packed date is year, month and day in 16, 8 and 8 bits; a packed time is hour, minute and second in the low three bytes. `cock::abi::rtc::unpack` turns both words back into a `DateTime` and checks that it is valid. The clock is taken as UTC: there is no time zone.

## How the clock is read

`cock::user::Cmos` (`userland/lib/include/cock/user/cmos.hpp`) waits until the clock is not updating, reads the registers twice and accepts the value only when two reads agree. It handles BCD and binary registers, 12 and 24 hour modes and the century register, and rejects a value that is not a valid date. The server holds no state between requests.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: `waitForCapabilities`, opens the clock, drains the kept messages, then the request loop |
| `src/grant.cpp`, `include/rtc/grant.hpp` | `waitForCapabilities`: blocks until `CAPABILITY_GRANTED`, keeping every other message |
| `src/requests.cpp`, `include/rtc/requests.hpp` | `initClock` and `handle`, which answers `RTC_READ`; owns the `Cmos` reader |
| `src/pending.cpp`, `include/rtc/pending.hpp` | `keepPending` and `handlePending`: requests that arrive before the grant |
| `src/reply.cpp`, `include/rtc/reply.hpp` | `reply`: sends `RTC_REPLY` |
| `Makefile` | `NAME = rtc`, `SRCS` under `src/` and the small `LIBC_SRCS` list |
| `../lib/include/cock/user/cmos.hpp` | `Cmos`, the register reader used only by this server |
| `../lib/include/cock/user/clock.hpp` | `Clock`, the client other programs use |
| `../../shared/include/cock/abi/rtc.hpp`, `datetime.hpp` | Protocol, packing and date arithmetic (Unix seconds, FAT date and time) |

## Build, run, test

```sh
make userland/rtc
make run-headless
bash scripts/qemu_userland_test.sh
bash scripts/host_test.sh
```

Use `HOST_TOOLCHAIN=1` without the cross compiler. The userland script requires `rtc: ready` on the serial log and, after the guest wrote files, checks with `mdir` and `debugfs` that the FAT and ext2 files carry the current year and month. `test/host/test_datetime.cpp` covers the date arithmetic and the FAT conversion.

## Limits and known gaps

- Read only: the clock cannot be set.
- No capability check on who may ask; any process can send `RTC_READ` until the IPC capability of stage 9.
- The pending list before the grant holds 16 messages; more are dropped.
- Resolution is one second and there is no time zone. Dates before 1970 are invalid, and FAT dates are clamped to 1980 through 2107.
- There is no timer or alarm service: clock ticks and sleeping belong to the kernel timers.

## Documentation

- [FAT32 server, dates](../../documentation/userland/fat32.md)
- [ext2 server, times](../../documentation/userland/ext2.md)
- [File system protocol](../../documentation/userland/filesystem.md)
- [Capabilities](../../documentation/core/capabilities.md)
