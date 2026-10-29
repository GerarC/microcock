# Metrics

A kernel built with `SCHED_METRICS=1` keeps three figures per thread, in timer ticks (10 ms):

* **Response time:** from the moment the thread is first queued as ready until it first runs.
* **CPU time:** the ticks that fired while the thread was running, so it is rounded to ticks.
* **Turnaround time:** from the creation of the thread until it is read, or until it dies.

The hooks are in `SchedMetrics` (`sched_metrics.hpp`): `onCreate` in `Thread::initBase`, `onReady` in `Scheduler::enqueue`, `onTick` in `Scheduler::onTick`, `onDispatch` in `switchFrom` and `onExit` in `TaskManager::notifyThreadDeath`, which logs the three figures at trace level. Each hook is a template whose body sits behind `if constexpr (SCHED_METRICS_ENABLED)`, and `Thread::metrics` is an empty `[[no_unique_address]]` struct without the flag. Without it there is no extra code and no extra byte in a thread, and `SYS_SCHED_METRICS` is not compiled or registered. With it, a thread reads its own figures into an `abi::sched::Metrics` (`cock/abi/sched_metrics.hpp`) through `cock::user::sched_metrics`. The test program `schedbench` runs a CPU bound and an I/O bound thread and prints both sets; the I/O bound thread must use less CPU time.
