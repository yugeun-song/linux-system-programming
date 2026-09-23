# linux-system-programming

Example programs for Linux system programming. Each `.c` in a topic directory builds to
`bin/<dir>/<name>`; `utils/` is the exception, compiled as an object library and linked into every
target. Sources are collected by wildcard, so a new example is just a `.c` dropped into an existing
topic directory. A new directory needs adding to `SRC_DIRS`. `bin/` is untracked.

## Layout

| Path | Contents |
|---|---|
| `utils/` | `log.c` `log.h` -- async-signal-safe logging, an object library |
| `process/` | `fork.c` `fork_thread_locals.c` `posix_spawnp.c` |
| `signal/` | `sigaction_basics.c` `siginfo_and_ucontext.c` `mutex_in_signal_handler.c` |
| `thread/` | `pthread_create.c` |
| `io/ ipc/ memory/ time/ user/` | empty; listed in `SRC_DIRS`, held by `.gitkeep` |

## Build

Requires GCC, GNU Make and glibc, plus clang-format for the format targets and Universal Ctags and
cscope for the index targets. The glibc floor is 2.32, for `strerrordesc_np()`; `gettid()` and
the `uc_mcontext` layout `signal/siginfo_and_ucontext.c` reads are GNU extensions.
That file also `#error`s outside x86_64, aarch64 and rv64.

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
- `-Wl,-z,now` binds every PLT entry before `main()`. With lazy binding the first call of a libc
  function from a signal handler would run the dynamic linker's resolver inside the handler, which
  is not async-signal-safe; anything else that links `utils/log.c` needs the same flag.
- `-O0 -ggdb3` with frame pointers kept: these binaries are for GDB, Valgrind and perf, not for
  timing.
- `-MMD -MP` puts `.d` files beside the objects and executables in `bin/`, so editing
  `utils/log.h` rebuilds everything that includes it.
- `-I.` is what makes `#include "utils/log.h"` resolve from the project root. clangd reads it from
  `compile_commands.json`, a file target of the default build rather than a command to remember;
  left to a separate command it went ungenerated and clangd reported 40 errors over the tree. A
  source added before the next `make` borrows the flags of its nearest neighbour in the database,
  so it parses cleanly too.
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
initializers `{ content }` rather than `{content}`. `BinPackArguments: false` keeps a call on one
line while it fits in 100 columns and otherwise gives every argument a line of its own.
`make format-check` enforces all of it.

## Output

Every line an example prints goes through `PRINT_*()` (`utils/log.h`) to descriptor 2, whatever it
is at the time, one `write()` per call; the examples use no stdio. glibc's `fprintf(stderr, ...)`
issues one `write()` per conversion, so its lines tear under contention where the logger's stay
whole. The `PRINT_` prefix stays clear of `<syslog.h>`, which owns the `LOG_*` names. A record reads

```text
HH:MM:SS.mmm [LEVEL] [pid/tid] file:line func(): message: description (errno=N)
```

from the raw `CLOCK_REALTIME` value with no zone applied, with `LEVEL` one of `INFO`, `WARN` and
`ERR` padded to four columns, `tid` the kernel thread id from `gettid()`, and the errno tail present
only when a number is passed.

`PRINT_INFO()` carries the narrative and `PRINT_ERR()` a failure without an error number.
`PRINT_PERROR(errnum, ...)` and `PRINT_PWARN(errnum, ...)` take the number as an argument, covering
both C conventions in one call shape: pass `errno` after a call that sets it, or the return value of
a `pthread_*` or `posix_spawn*` function, which return the number and leave `errno` alone.
`PRINT_PWARN()` marks a failure an example provokes on purpose, such as registering a handler for
SIGKILL. The macros supply everything before the message; do not repeat it there.

A message may span lines: after each `\n` the next line is padded to the width of that record's
prefix, and a blank line gets none. The `PRINT_*_NO_PADDING()` forms leave continuation lines at
column 0. All ten macros expand to `PRINT_EMIT(level, errnum, padding, ...)`.

## Signal safety

- `log_emit()` is async-signal-safe: no stdio, no lock, no `malloc`, no static state, one `write()`
  per call, `errno` and the signal mask preserved. It works in a signal handler, in the child of a
  multithreaded `fork()` and from any thread; only the short-write retry loop in `write_all()` can
  split a record between threads. `nm -u bin/utils/log.o` lists the call surface; a record costs six
  system calls.
- `utils/log.c` formats with its own `printf` subset: the C99 integer, character, string and pointer
  conversions with flags, width, precision and length modifiers (`L` and `q` read as `ll`, as glibc
  does), plus `%m` and `%#m`, matching glibc's `snprintf()` byte for byte; the `'` and `I` flags are
  accepted and ignored. Floating-point conversions, `%lc`, `%ls`, `%n` and anything unknown consume
  their argument and print the specifier verbatim, so later arguments stay aligned; log a duration
  as an integer count of ns rather than a double. A NULL format prints `(null)`.
- Nothing can block or crash the caller: a record is cut at 1023 bytes with its errno suffix and
  newline kept, width and precision are clamped at 65535, an unknown errno reads `Unknown error`, a
  closed or non-blocking stderr drops the record, and so do a broken pipe and a file at
  `RLIMIT_FSIZE`: `write_all()` blocks SIGPIPE and SIGXFSZ around the `write()` and consumes the one
  its own `write()` raised, leaving any the program had pending, so logging never kills the process.
  Under a seccomp filter that denies `rt_sigtimedwait` it leaves that signal blocked instead; only a
  filter that denies `rt_sigprocmask` leaves nothing to do, as for any program. On a pipe or a unix
  stream socket a record of at most 1023 bytes is delivered whole or not at all, even with
  `O_NONBLOCK`; a tty can split one when a signal interrupts a partial write, and a non-blocking TCP
  socket when its send buffer fills mid-record.
- The error text comes from `strerrordesc_np()`, a table lookup, and the time of day is
  `CLOCK_REALTIME` reduced modulo one day with no zone handling, so the logger never calls `tzset()`
  or `localtime_r()`. A call needs 2.4 KB of stack at `-O0`, 2.7 KB at `-O2` and 5.6 KB under
  ASan+UBSan, libc callees included, measured on a guard-paged `clone()` stack; on an alternate
  stack add the kernel's signal frame per nesting level, `getauxval(AT_MINSIGSTKSZ)`, 3.6 KB on
  AVX-512.
- Handlers still save `errno` on entry and restore it on exit, so whatever call they gain later
  cannot overwrite the value the interrupted code is about to read.
- `signal/mutex_in_signal_handler.c` is the deliberate counter-example, not a broken build. Its last
  stage relocks `g_mutex` from `handler_lock()` on the thread already holding it. POSIX leaves that
  undefined for a default mutex and glibc deadlocks, so the run hangs after its last line. Its
  header comment covers the near miss: `pthread_mutex_trylock()` and `pthread_mutex_timedlock()`
  bound the wait but are no safer.
- After `fork()` the child path uses only `PRINT_*()` and `_exit()`; `process/fork.c` and
  `process/fork_thread_locals.c` both show the shape. `_exit()` keeps the child from running the
  parent's `atexit()` handlers or flushing stdio it inherited, which matters as soon as a program
  does use stdio.
