/**
 * @file initializer.hpp
 * @brief Entry point of the in-kernel test suites.
 */
#ifndef TEST_INITIALIZER_HPP
#define TEST_INITIALIZER_HPP

namespace cock::test {
    /**
     * @brief Runs every kernel test suite (PMM, scheduler, IPC, reaper, synchronous IPC and so on) and prints the summary.
     *
     * Called from `kernel_main` in test builds, after the subsystems are initialized. The result is reported through the
     * exit code of the emulator (`EXIT_CODE_SUCCESS` or `EXIT_CODE_FAILURE`).
     */
    void run_all_tests();
}

#endif
