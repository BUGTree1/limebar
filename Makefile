CC = gcc
CFLAGS = -Wall -Wextra -O3 -I/usr/include/freetype2
LIBS = -lX11 -lXrandr -lXrender -lXft

.PHONY: run
run: limebar
	./limebar

limebar: limebar.c config.h
	$(CC) $(CFLAGS) $(LIBS) -o limebar limebar.c
