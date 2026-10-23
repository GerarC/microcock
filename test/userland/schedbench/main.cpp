#include <cock/abi/sched_metrics.hpp>
#include <cock/user/api.hpp>
#include <cock/user/thread.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::abi::sched::Metrics;
using cock::user::NO_PROCESS;

namespace {

constexpr size_t STACK_BYTES = 0x4000;
constexpr uint32_t CPU_BOUND_TICKS = 30;
constexpr uint32_t IO_BOUND_ROUNDS = 5;
constexpr uint32_t IO_BOUND_SLEEP_MS = 20;
constexpr uint32_t WORKER_COUNT = 2;
constexpr uint32_t WAKE_ALL = 0xFFFFFFFF;
constexpr uintptr_t EXIT_PASSED = 0;
constexpr uintptr_t EXIT_FAILED = 1;

alignas(0x1000) uint8_t cpuStack[STACK_BYTES];
alignas(0x1000) uint8_t ioStack[STACK_BYTES];

Metrics cpuFigures = {};
Metrics ioFigures = {};
bool cpuRead = false;
bool ioRead = false;
uint32_t finished = 0;

void finish() {
	__atomic_add_fetch(&finished, 1, __ATOMIC_SEQ_CST);
	cock::user::futex_wake(&finished, WAKE_ALL);
}

void cpuBound(void *) {
	Metrics now = {};
	do {
		cpuRead = cock::user::sched_metrics(now);
	} while (cpuRead && now.turnaroundTicks < CPU_BOUND_TICKS);
	cpuFigures = now;
	finish();
}

void ioBound(void *) {
	for (uint32_t round = 0; round < IO_BOUND_ROUNDS; round++)
		cock::user::sleep_ms(IO_BOUND_SLEEP_MS);
	ioRead = cock::user::sched_metrics(ioFigures);
	finish();
}

void waitForWorkers() {
	uint32_t seen = __atomic_load_n(&finished, __ATOMIC_SEQ_CST);
	while (seen < WORKER_COUNT) {
		cock::user::futex_wait(&finished, seen);
		seen = __atomic_load_n(&finished, __ATOMIC_SEQ_CST);
	}
}

} // namespace

extern "C" int main() {
	uint32_t cpuThread =
		cock::user::thread_start(cpuBound, nullptr, cpuStack, STACK_BYTES);
	uint32_t ioThread = cock::user::thread_start(ioBound, nullptr, ioStack, STACK_BYTES);
	if (cpuThread == NO_PROCESS || ioThread == NO_PROCESS) {
		printf("schedbench: failed to start the threads\n");
		cock::user::exit_process(EXIT_FAILED);
	}

	waitForWorkers();

	if (!cpuRead || !ioRead) {
		printf("schedbench: done metrics off\n");
		cock::user::exit_process(EXIT_PASSED);
	}

	printf("schedbench: done cpu response %u cpu %u turnaround %u\n", cpuFigures.responseTicks,
		   cpuFigures.cpuTicks, cpuFigures.turnaroundTicks);
	printf("schedbench: done io response %u cpu %u turnaround %u\n", ioFigures.responseTicks,
		   ioFigures.cpuTicks, ioFigures.turnaroundTicks);

	if (ioFigures.cpuTicks >= cpuFigures.cpuTicks) {
		printf("schedbench: the I/O bound thread used as much CPU as the CPU bound one\n");
		cock::user::exit_process(EXIT_FAILED);
	}
	cock::user::exit_process(EXIT_PASSED);
}
