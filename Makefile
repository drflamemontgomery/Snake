##
# Snake
#
# @file
# @version 0.1

SNAKE_SRCS := src/main.c
CFLAGS := -lncurses

default: build

build:
	gcc -o snake $(SNAKE_SRCS) $(CFLAGS)

run:
	./snake

# end
