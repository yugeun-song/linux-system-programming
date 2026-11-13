STD := gnu99
WARNINGS_EXTRA := -Wconversion -Wsign-conversion
CPPFLAGS_EXTRA := -I.
LDFLAGS_EXTRA := -Wl,-z,now
LDLIBS := -pthread

SRC_DIRS := user process thread memory io ipc signal time
LIB_SRCS := $(wildcard utils/*.c)
PROGRAMS := $(patsubst %.c,%,$(wildcard $(addsuffix /*.c,$(SRC_DIRS))))

include mk/c.mk
