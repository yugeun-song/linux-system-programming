# linux-system-programming

Example programs for Linux system programming, organized by topic.
Each source file compiles into a standalone executable.

## Directory Structure

```
├── process/        Process creation and management
├── thread/         POSIX threads
├── memory/         Memory management
├── io/             File and device I/O
├── network/        Socket programming
├── ipc/            Inter-process communication
├── signal/         Signal handling
├── time/           Timers and clocks
├── error/          Error handling
├── user/           User and group operations
├── helper/         Shared utility code (object library)
├── bin/            Build output (generated)
├── CMakeLists.txt
└── Makefile
```

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