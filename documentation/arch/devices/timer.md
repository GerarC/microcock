# Timer (PIT)

The system tick comes from channel 0 of the 8253/8254 Programmable Interval Timer, wired to IRQ0 (`cock/arch/x86/timer/timer.cpp`). It is the only thing that drives the [scheduler](../../core/scheduler/README.md) and the [timeouts](../../core/scheduler/timeouts.md).

## Programming the chip

`Timer::init()` runs once during boot, after the [PIC](pic.md) is remapped:

1. It resets the tick counter.
2. It computes the divisor `1193180 / FREQUENCY`, where `FREQUENCY` is `hal::TIMER_FREQUENCY_HZ` (100 Hz, a tick every 10 ms).
3. It writes the command byte `0x36` (channel 0, low byte then high byte, square wave) to port `0x43`, then the low and the high byte of the divisor to port `0x40`.
4. It installs `Timer::onIrq0` on vector `0x20` with `irq_install_handler`, which also unmasks the line (see [irq](../interrupts/irq.md)).

## What a tick does

`onIrq0` only increments a 64 bit counter. It makes no scheduling decision: after the handler returns, the IRQ dispatcher sends the end of interrupt and calls the core, which sees that the line is `hal::TIMER_IRQ_LINE` and runs `Scheduler::onTick` (see [events](../../core/events.md)).

`Timer::getTicks()` reads the counter with interrupts off, so a tick cannot tear the 64 bit value. The core keeps its own tick counter for timeouts (`Timeouts`); this one is the architecture's.

The timer line is reserved: a Ring 3 driver cannot bind it (`hal::irq_line_reserved`).

## Limits

The resolution is the tick, 10 ms. The scheduler refines it for accounting with the time stamp counter (see [scheduler](../../core/scheduler/README.md)), but sleeps and timeouts are rounded up to whole ticks.
