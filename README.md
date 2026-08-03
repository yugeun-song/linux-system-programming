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
├── .clang-format              Formatting rules (LLVM base, 4-space, 100 col)
├── .editorconfig              Editor defaults
├── .gitignore
├── Makefile                   Build configuration
└── README.md
```

Additional topic directories (`error`, `io`, `ipc`, `memory`, `network`,
`time`, `user`) are listed in the Makefile's `SRC_DIRS` and each holds a
`.gitkeep`, so the layout survives a clone and a new example only needs
a `.c` file dropped in. Build output is written to `bin/` (generated,
not tracked).

Each `.c` file in a topic directory produces an executable at
`bin/<dir>/<name>`. For instance, `process/fork_and_waitpid.c`
compiles to `bin/process/fork_and_waitpid`.

Shared code placed in `helper/` is compiled as an object library
and linked to all targets automatically.

## Build

### Prerequisites

- GCC
- GNU Make
- pthreads

```sh
make
make clean
```

`clean` removes `bin/`, the generated `compile_commands.json`, and the
`gmon.out` left in the project root by a profiling run.

### Compiler Flags

```
-std=gnu99
-Wall -Wextra -Wconversion -Wsign-conversion
-pg -O0 -ggdb3
-fno-omit-frame-pointer -fno-optimize-sibling-calls -fasynchronous-unwind-tables
-MMD -MP
```

The standard is pinned so the build does not shift with the compiler's
default. Optimization is disabled, and debug symbols and frame pointers
are preserved for accurate stack traces under GDB, Valgrind, and perf.
`-MMD -MP` records header dependencies alongside the objects in `bin/`,
so editing `helper/log.h` rebuilds everything that includes it.

`-pg` links the gprof instrumentation. Each program writes its profile
to a file named `gmon.out` in whatever directory it was run from, and
every program uses that same name, so run them from separate
directories or set `GMON_OUT_PREFIX` to keep more than one. A child
that leaves through `_exit()` writes no profile, which is why the fork
examples produce one for the parent only.

The unused-parameter warnings from the signal handlers and thread
routines are expected. Those signatures are fixed by the API, and the
warnings are left visible rather than silenced.

### Editor Support

```sh
make compile_commands
```

writes `compile_commands.json` for clangd and other libclang-based
tools. It is regenerated on demand and removed by `make clean`.

## Adding Programs

Place a `.c` file in the appropriate topic directory. The Makefile
discovers sources by wildcard; no manual registration is required.

## Output

Three paths, picked by what the calling context allows:

- Narrative goes to stdout with `printf()`. That output is what the
  examples are read for, so it stays plain and stays on stdout.
- Diagnostics go to stderr through the `LOG_*` macros in
  `helper/log.h`. `LOG_PERROR(rc, ...)` takes the error number as an
  argument, which covers both C conventions in one call shape: pass
  `errno` after a call that sets it, or the return value of a
  `pthread_*` or `posix_spawn*` function, which return the number and
  leave `errno` alone. The macros supply the timestamp, pid and tid,
  source location and function name, so the message itself carries none
  of that. `LOG_PWARN` marks a failure an example provokes on purpose,
  such as registering a handler for SIGKILL.
- Signal handlers and the post-`fork()` child write to stdout with
  `write()` directly. Neither `printf()` nor `LOG_*` is
  async-signal-safe, so neither is usable there.

## Thread and Signal Safety

The examples are written so that code built on top of them stays
correct under threads and signals:

- The `LOG_*` macros in `helper/log.h` are thread-safe: each record is
  formatted into one buffer and written as a single unit, so concurrent
  threads never interleave a line. They are not async-signal-safe and
  must not be called from a signal handler.
- Inside a signal handler, write directly with `write()` and format
  integers by hand, as shown in `signal/sigaction_basics.c` and
  `signal/siginfo_and_ucontext.c`. Those handlers call only functions
  listed in `signal-safety(7)`. `signal/mutex_in_signal_handler.c` is the
  deliberate counter-example: it takes a mutex from a handler to show
  what goes wrong, and says so in its header comment.
- Every handler saves `errno` on entry and restores it before returning.
  A handler that calls `write()` and leaves `errno` behind overwrites the
  value the interrupted code is about to read, so a `perror()` further
  down reports the wrong reason.
- A handler's `write()` bypasses stdio, so buffered `printf()` output
  would otherwise appear after it whenever stdout is redirected. Every
  example that mixes the two makes stdout line buffered with `setvbuf()`
  at the top of `main()`, so a redirected transcript reads in the order
  the program actually ran. `signal/mutex_in_signal_handler.c` needs it
  most: it self-deadlocks by design and never reaches a flush, so
  without it a redirected run keeps only the handlers' raw writes and
  loses every line `main()` printed.
- `pthread_*` functions return an error number and leave `errno` alone,
  so their failures are reported with `LOG_PERROR(rc, ...)` and never
  with the ambient `errno`. `posix_spawnp()` behaves the same way.
  `log_emit()` formats the number with the reentrant `strerror_r()`, so
  the diagnostic path is safe to use from any thread.
- After `fork()`, the child path uses only async-signal-safe calls
  (`write()`, `_exit()`), as shown in `process/fork.c`, so the pattern
  stays correct even when the parent is multithreaded. `_exit()` also
  skips the stdio flush, so the child never re-emits the copy of the
  parent's output buffer it inherited.