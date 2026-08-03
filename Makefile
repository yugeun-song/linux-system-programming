CC = gcc

STD = -std=gnu99
WARNINGS = -Wall -Wextra -Wconversion -Wsign-conversion
DEBUG = -pg -O0 -ggdb3 -fno-omit-frame-pointer -fno-optimize-sibling-calls -fasynchronous-unwind-tables
DEPFLAGS = -MMD -MP

CFLAGS = $(STD) $(WARNINGS) $(DEBUG) $(DEPFLAGS) -I.
LDFLAGS = -pthread

DB_FLAGS = $(STD) $(WARNINGS) $(DEBUG) -I.

BIN_DIR = bin
LIB_DIRS = helper
SRC_DIRS = user process thread memory io network ipc signal time error

LIB_SRCS = $(wildcard $(addsuffix /*.c, $(LIB_DIRS)))
LIB_OBJS = $(patsubst %.c, $(BIN_DIR)/%.o, $(LIB_SRCS))

EXE_SRCS = $(wildcard $(addsuffix /*.c, $(SRC_DIRS)))
EXES = $(patsubst %.c, $(BIN_DIR)/%, $(EXE_SRCS))

DEPS = $(LIB_OBJS:.o=.d) $(addsuffix .d, $(EXES))

.PHONY: all clean compile_commands

all: $(EXES)

$(BIN_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/%: %.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDFLAGS)

compile_commands:
	@printf '[\n' > compile_commands.json
	@sep=" "; for src in $(LIB_SRCS) $(EXE_SRCS); do \
		printf '%s{ "directory": "%s", "file": "%s", "command": "%s %s -c %s" }\n' \
			"$$sep" "$(CURDIR)" "$$src" "$(CC)" "$(DB_FLAGS)" "$$src" \
			>> compile_commands.json; \
		sep=","; \
	done
	@printf ']\n' >> compile_commands.json

clean:
	rm -rf $(BIN_DIR)
	rm -f gmon.out compile_commands.json

-include $(DEPS)
