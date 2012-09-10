#!/bin/sh -ex
rm -fv spil{.o,}
clang -Wall -Wextra -Wno-unused-parameter -Wfatal-errors -g3 -o kmsconiface.o -I ~/kmscon2/src -I ~/kmscon2/tests kmsconiface.c -c `pkg-config --cflags  pixman-1`
clang -Wall -Wextra -o game.o game.c -c `pkg-config --cflags  pixman-1`
clang -o spil game.o kmsconiface.o -L ~/kmscon2/.libs -luterm -leloop -Wl,-rpath=$(echo ~/kmscon2/.libs) `pkg-config --libs  pixman-1`
