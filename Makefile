CFLAGS = -Wall -Wextra -ggdb

ifeq ($(san),1)
	CFLAGS += -fsanitize=undefined,address
endif

main: main.c
	clang $(CFLAGS) main.c -o main
