# Futex

`SYS_FUTEX_WAIT(word, expected, milliseconds)` and `SYS_FUTEX_WAKE(word, count)` are the blocking primitive of [threads](README.md). `word` must be a 4 byte aligned, readable user address.

The wait compares the word with `expected` with interrupts off: if it differs it returns `1` at once; if it is equal the thread joins the wait list (`Futex`, a FIFO keyed by process and address) and blocks. It returns `0` when woken and `2` when the time ran out (`0xFFFFFFFF` means no limit, `0` never waits).

The wake takes up to `count` waiters of the **same process** on that address and returns how many it woke; a wake also cancels the timeout of each (see [timeouts](../scheduler/timeouts.md)).

The restart pattern of [blocking calls](../scheduler/blocking_calls.md) applies: a thread that wakes runs the call again and finds the wake mark on the thread, not the word, so the word does not have to change. `~Thread` removes a dead waiter from the list.
