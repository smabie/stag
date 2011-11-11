# Makefile

# 
# This file is public domain as declared by Sturm Mabie.
# 

CC=gcc
PROG=stag
FILES=ui.c stag.c tagger.c strlcpy.c strlcat.c basename.c dirname.c
CFLAGS+=-Wall -ansi -pedantic -D_BSD_SOURCE
CPPFLAGS+=-I/usr/local/include
LDFLAGS+=-L/usr/local/lib -lmenu -lform -lncursesw -ltag -ltag_c

all:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(FILES) -o $(PROG)
