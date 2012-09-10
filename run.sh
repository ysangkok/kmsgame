#!/bin/sh
#sudo sh -c 'LD_LIBRARY_PATH=.libs:libxkbcommon/.libs gdb -x cmds tests/test_output'
sudo sh -c 'gdb -x cmds tests/test_output'
