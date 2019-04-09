# Makefile for nexporter

CC = gcc
CFLAGS = -DANSI_TERM_COLOR -std=c99 -pg
LIBS = 

.PHONY: exe clean

exe: nexporter

nexporter: nexporter.o httpserver.o exporters.o cmdline.o config.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

config: config.o
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@


# Should we write "rm" of different of "level" of target files separately?
clean:
	-rm *.o
	-rm nexporter
