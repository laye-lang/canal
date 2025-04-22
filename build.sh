#!/usr/bin/env sh
set -xe

CFLAGS="-Wall -Wextra -pedantic -ggdb -std=c11"
CLIBS=""

gcc $CFLAGS -o canal src/main.c src/str.c $CLIBS
