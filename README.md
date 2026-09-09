# linux-system-programming

Example programs for Linux system programming. Each `.c` in a topic directory builds to
`bin/<dir>/<name>`; `helper/` is the exception, compiled as an object library and linked into every
target. Sources are collected by wildcard, so a new example is just a `.c` dropped into an existing
topic directory. A new directory needs adding to `SRC_DIRS`. `bin/` is untracked.

## Layout

| Path | Contents |
|---|---|
| `helper/` | `log.c` `log.h` -- logging object library |
| `process/` | `fork.c` `fork_and_waitpid.c` `fork_thread_locals.c` `posix_spawnp.c` |
| `signal/` | `sigaction_basics.c` `siginfo_and_ucontext.c` `mutex_in_signal_handler.c` |
| `thread/` | `pthread_create.c` |
| `error/ io/ ipc/ memory/ time/ user/` | empty; listed in `SRC_DIRS`, held by `.gitkeep` |

## Build

Requires GCC, GNU Make and glibc, plus clang-format for the format targets and Universal Ctags and
cscope for the index targets. The glibc floor is 2.30, for the `gettid()` wrapper; the GNU flavour
of `strerror_r()` and the `uc_mcontext` layout `signal/siginfo_and_ucontext.c` reads are glibc-only
at any version. That file also `#error`s outside x86_64, aarch64 and rv64.

```sh
make
make format-check
make clean
```

| Target | Effect |
|---|---|
| `make` | build every `bin/<dir>/<name>`, plus `tags`, `cscope.out` and `compile_commands.json` |
| `make symbols` | the symbol index on its own |
| `make tags` | ctags only |
| `make cscope` | cscope only |
| `make compdb` | `compile_commands.json` only |
| `make format` | `clang-format -i` over every source and header |
| `make format-check` | `clang-format --dry-run --Werror`; nonzero on drift |
| `make clean` | remove `bin/`, the symbol index and `gmon.out` |

Flags live in the `Makefile` (`STD`, `WARNINGS`, `DEBUG`, `DEPFLAGS`, `CFLAGS`, `LDFLAGS`). What it
does not state:

- `-std=gnu99` is pinned so the build does not shift with the compiler's default.
- `-O0 -ggdb3` with frame pointers kept: these binaries are for GDB, Valgrind and perf, not for
  timing.
- `-MMD -MP` puts `.d` files beside the objects and executables in `bin/`, so editing
  `helper/log.h` rebuilds everything that includes it.
- `-I.` is what makes `#include "helper/log.h"` resolve from the project root. clangd reads it from
  `compile_commands.json`, a file target of the default build rather than a command to remember;
  left to a separate command it went ungenerated and clangd reported 40 errors over the tree.
- `.ctags.d/default.ctags` holds the ctags settings, so an editor invoking ctags itself indexes the
  tree the way `make tags` does. `cscope -bkqu` skips `/usr/include`, so a query answers about this
  tree instead of libc; the `u` forces a full rebuild past cscope's whole-second mtime comparison.
- `-pg`: every program writes its profile as `gmon.out` in the directory it was run from, under
  that one name. Run them from separate directories or set `GMON_OUT_PREFIX` to keep more than one.
  A child leaving through `_exit()` writes no profile, so the fork examples produce one for the
  parent only.
- The `-Wunused-parameter` warnings come from signal handlers and thread routines whose signatures
  are fixed by the API. The build keeps every `-Wunused-*` warning; `.clangd` suppresses them so
  the editor stays quiet.

`.clang-format` is LLVM base, 4-space, 100 col. `Cpp11BracedListStyle: false` is what makes braced
initializers `{ content }` rather than `{content}`; `make format-check` enforces it.

## Output

| Context | Call | Destination |
|---|---|---|
| Narrative | `printf()` | stdout |
| Diagnostics | `LOG_*` (`helper/log.h`) | stderr |
| Signal handler, post-`fork()` child | `write()` | stdout |

`LOG_PERROR(rc, ...)` and `LOG_PWARN(rc, ...)` take the error number as an argument, covering both C
conventions in one call shape: pass `errno` after a call that sets it, or the return value of a
`pthread_*` or `posix_spawn*` function, which return the number and leave `errno` alone.
`log_emit()` renders it with the reentrant `strerror_r()`. `LOG_PWARN` marks a failure an example
provokes on purpose, such as registering a handler for SIGKILL. The macros supply the timestamp,
pid/tid, source location and function name; do not repeat those in the message.

## Signal safety

- `LOG_*` is thread-safe -- each record is formatted into automatic buffers and handed to one
  `write()` of at most 1023 bytes -- but not async-signal-safe. Never call it from a handler. Only
  the short-write retry loop in `write_all()` can split a record.
- In a handler, write with `write()` directly. `signal/sigaction_basics.c` emits fixed strings;
  `signal/siginfo_and_ucontext.c` formats decimal and hex by hand. Both call only functions listed
  in `signal-safety(7)`.
- Handlers save `errno` on entry and restore it on exit. Otherwise the handler's `write()`
  overwrites the value the interrupted code is about to read, and the next diagnostic reports the
  wrong reason.
- A handler's `write()` bypasses stdio, so every example that mixes the two makes stdout line
  buffered with `setvbuf()` in `main()`. Without it, buffered `printf()` output appears after the
  handler's writes whenever stdout is redirected.
- `signal/mutex_in_signal_handler.c` is the deliberate counter-example, not a broken build. Its last
  stage relocks `g_mutex` from `handler_lock()` on the thread already holding it. POSIX leaves that
  undefined for a default mutex and glibc deadlocks, so the run hangs and never reaches a flush,
  which is why this example needs `setvbuf()` most. Its header comment covers the near miss:
  `pthread_mutex_trylock()` and `pthread_mutex_timedlock()` bound the wait but are no safer.
- After `fork()` the child path uses only `write()` and `_exit()`. `process/fork.c` shows the shape;
  `process/fork_thread_locals.c` is where it is mandatory, forking from the main thread of a process
  with three live workers. `_exit()` also skips the stdio flush, so the child never re-emits the
  inherited copy of the parent's output buffer.
