# linux-system-programming

Example programs for Linux system programming, organized by topic.
Each source file compiles into a standalone executable.

## Directory Structure

```
.
├── helper                     Shared utility code (object library)
├── process                    Process creation and management
├── signal                     Signal handling
├── thread                     POSIX threads
├── CMakeLists.txt             CMake build configuration
├── CMakePresets.json          CMake presets for WSL environments
├── CMakeSettings.json         Visual Studio CMake integration
├── Makefile                   GNU Make build configuration
└── README.md
```

Additional topic directories (`error`, `io`, `ipc`, `memory`, `network`,
`time`, `user`) are pre-registered in the build system for future
examples. Build output is written to `bin/` (generated, not tracked).

Each `.c` file in a topic directory produces an executable at
`bin/<dir>/<name>`. For instance, `process/fork_and_waitpid.c`
compiles to `bin/process/fork_and_waitpid`.

Shared code placed in `helper/` is compiled as an object library
and linked to all targets automatically.

## Build

### Prerequisites

- GCC
- GNU Make or CMake (>= 3.10)
- pthreads

### Make

```sh
make
make clean
```

### CMake

```sh
cmake -B build -G Ninja
cmake --build build
```

A preset for WSL environments is provided:

```sh
cmake --preset wsl-debug
cmake --build out/build/wsl-debug
```

### Compiler Flags

Both build systems apply identical flags:

```
-Wall -Wextra -Wconversion -Wsign-conversion
-pg -O0 -ggdb3
-fno-omit-frame-pointer -fno-optimize-sibling-calls -fasynchronous-unwind-tables
```

Optimization is disabled. Debug symbols and frame pointers are
preserved for accurate stack traces under GDB, Valgrind, and perf.

## Adding Programs

Place a `.c` file in the appropriate topic directory. The build
system discovers sources via file globbing; no manual registration
is required.

## Thread and Signal Safety

The examples are written so that code built on top of them stays
correct under threads and signals:

- The `LOG_*` macros in `helper/log.h` are thread-safe: each record is
  formatted into one buffer and written as a single unit, so concurrent
  threads never interleave a line. They are not async-signal-safe and
  must not be called from a signal handler.
- Inside a signal handler, write directly with `write()` from static
  buffers and format integers by hand, as shown in `signal/`. Only
  async-signal-safe functions (see `signal-safety(7)`) are used there.
- After `fork()`, the child path uses only async-signal-safe calls
  (`write()`, `_exit()`), as shown in `process/fork.c`, so the pattern
  stays correct even when the parent is multithreaded.