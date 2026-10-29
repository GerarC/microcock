# Documentation

The pages are split by layer. Each folder has an index.

| Folder | What it covers |
|--------|----------------|
| [core/](core/README.md) | The architecture agnostic kernel: boot sequence, `memory/`, `process/`, `scheduler/`, `threads/`, IPC, capabilities, `diagnostics/` |
| [arch/](arch/README.md) | The x86 implementation under `cock/arch/x86`: `boot/`, `cpu/`, `interrupts/`, `devices/` |
| [userland/](userland/README.md) | Ring 3 programs: `system/` (manifest, boot, build), `drivers/`, `servers/`, `apps/`, `testing/` |
| [shared/](shared/README.md) | Headers included by both the kernel and the programs: ABI, message format, data structures |
| [libs/](libs/README.md) | The freestanding C and C++ libraries and the `cock::user` library |

New to the code? Follow the [learning path](learning_path.md): a reading order from the first instruction to the userland.

[Project structure](project_structure.md) shows the directory tree and the HAL, the contract between the core and the architecture.

Every program in `userland/` also has a `README.md` next to its sources.

Public headers carry their own reference as Doxygen comments: each function, class and constant states what it does, what it expects, what it returns and how it fails. These pages explain how the pieces fit together; the headers describe each piece.
