CC = gcc

STD = -std=gnu99
WARNINGS = -Wall -Wextra -Wconversion -Wsign-conversion
DEBUG = -pg -O0 -ggdb3 -fno-omit-frame-pointer -fno-optimize-sibling-calls -fasynchronous-unwind-tables
DEPFLAGS = -MMD -MP

CFLAGS = $(STD) $(WARNINGS) $(DEBUG) $(DEPFLAGS) -I.
LDFLAGS = -pthread

DB_FLAGS = $(STD) $(WARNINGS) $(DEBUG) -I.

CTAGSFLAGS = --kinds-C=+px
CSCOPEFLAGS = -b -q

BIN_DIR = bin
LIB_DIRS = helper
SRC_DIRS = user process thread memory io ipc signal time error
DIRS = $(LIB_DIRS) $(SRC_DIRS)

LIB_SRCS = $(wildcard $(addsuffix /*.c, $(LIB_DIRS)))
LIB_OBJS = $(patsubst %.c, $(BIN_DIR)/%.o, $(LIB_SRCS))

EXE_SRCS = $(wildcard $(addsuffix /*.c, $(SRC_DIRS)))
EXES = $(patsubst %.c, $(BIN_DIR)/%, $(EXE_SRCS))

DEPS = $(LIB_OBJS:.o=.d) $(addsuffix .d, $(EXES))

HDRS = $(wildcard $(addsuffix /*.h, $(LIB_DIRS) $(SRC_DIRS)))
SRCS = $(LIB_SRCS) $(EXE_SRCS) $(HDRS)

.PHONY: all clean compile_commands cscope format format-check

all: $(EXES) compile_commands.json

$(BIN_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/%: %.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDFLAGS)

compile_commands: compile_commands.json

compile_commands.json: $(LIB_SRCS) $(EXE_SRCS) $(DIRS) Makefile
	@printf '[\n' > $@
	@sep=" "; for src in $(LIB_SRCS) $(EXE_SRCS); do \
		printf '%s{ "directory": "%s", "file": "%s", "command": "%s %s -c %s" }\n' \
			"$$sep" "$(CURDIR)" "$$src" "$(CC)" "$(DB_FLAGS)" "$$src" \
			>> $@; \
		sep=","; \
	done
	@printf ']\n' >> $@

tags: $(SRCS) $(DIRS)
	ctags $(CTAGSFLAGS) -f $@ $(SRCS)

cscope: cscope.out

cscope.out: $(SRCS) $(DIRS)
	@printf '%s\n' $(SRCS) > cscope.files
	cscope $(CSCOPEFLAGS) -i cscope.files

format:
	clang-format -i $(SRCS)

format-check:
	clang-format --dry-run --Werror $(SRCS)

clean:
	rm -rf $(BIN_DIR)
	rm -f gmon.out compile_commands.json
	rm -f tags cscope.files cscope.out cscope.in.out cscope.po.out

-include $(DEPS)
