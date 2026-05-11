CC = gcc
CFLAGS = -Wall -Wextra -Wno-format-truncation -O3 -I/usr/include/freetype2
LIBS = -lm -lX11 -lXrandr -lXrender -lXft

.PHONY: run
run: limebar
	./limebar

limebar: limebar.c config.h
	$(CC) $(CFLAGS) $(LIBS) -o limebar limebar.c
