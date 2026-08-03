CC = gcc

STD = -std=gnu99
WARNINGS = -Wall -Wextra -Wconversion -Wsign-conversion
DEBUG = -pg -O0 -ggdb3 -fno-omit-frame-pointer -fno-optimize-sibling-calls -fasynchronous-unwind-tables
DEPFLAGS = -MMD -MP

CFLAGS = $(STD) $(WARNINGS) $(DEBUG) $(DEPFLAGS) -I.
LDFLAGS = -pthread

BIN_DIR = bin
LIB_DIRS = helper
SRC_DIRS = user process thread memory io network ipc signal time error

LIB_SRCS = $(wildcard $(addsuffix /*.c, $(LIB_DIRS)))
LIB_OBJS = $(patsubst %.c, $(BIN_DIR)/%.o, $(LIB_SRCS))

EXE_SRCS = $(wildcard $(addsuffix /*.c, $(SRC_DIRS)))
EXES = $(patsubst %.c, $(BIN_DIR)/%, $(EXE_SRCS))

DEPS = $(LIB_OBJS:.o=.d) $(addsuffix .d, $(EXES))

.PHONY: all clean

all: $(EXES)

$(BIN_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/%: %.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

-include $(DEPS)
