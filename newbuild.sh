#!/bin/sh
gcc -Wall -Wextra -o game.o game.c -c `pkg-config --cflags  pixman-1`
gcc -shared -Wl,-soname,game.so -o game.so game.o
