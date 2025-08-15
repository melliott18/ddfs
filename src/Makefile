# Makefile for makefs-ddfs

EXECBIN = makefs-ddfs
SOURCES = ddfs.c ddfs_inode.c ddfs_bitmap.c $(EXECBIN).c
OBJECTS=$(SOURCES:.c=.o)
DEPS=$(SOURCES:.c=.d)
CFLAGS  = -Wall -Wextra -Wpedantic -Wshadow -std=c99 -O2

all : $(EXECBIN)

$(EXECBIN) : $(OBJECTS)
	cc -o $@ $(OBJECTS)

%.o: %.c
	cc -c $(CFLAGS) $<

clean:
	-rm -rf $(DEPS) $(OBJECTS)

spotless:
	rm -rf $(EXECBIN)

-include $(DEPS)

.PHONY: all clean spotless
