CFLAGS = -Wall -Wextra

ifeq ($(r),1)
	CFLAGS += -O3 -DNDEBUG
else
	CFLAGS += -O0 -ggdb
endif

ifeq ($(san),1)
	CFLAGS += -fsanitize=undefined,address
endif

main: main.c
	clang $(CFLAGS) main.c -o main
