# linux-system-programming

Example programs for Linux system programming, organized by topic.
Each source file compiles into a standalone executable.

## Directory Structure

```
.
├── bin                       Build output (generated)
├── error                     Error handling
├── helper                    Shared utility code (object library)
├── io                        File and device I/O
├── ipc                       Inter-process communication
├── memory                    Memory management
├── network                   Socket programming
├── process                   Process creation and management
├── signal                    Signal handling
├── thread                    POSIX threads
├── time                      Timers and clocks
├── user                      User and group operations
├── CMakeLists.txt            CMake build configuration
├── CMakePresets.json         CMake presets for WSL environments
├── CMakeSettings.json        Visual Studio CMake integration
├── Makefile                  GNU Make build configuration
└── README.md
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