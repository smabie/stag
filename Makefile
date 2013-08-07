# Makefile

# 
# This file is public domain as declared by Sturm Mabie.
# 

CC=gcc
PROG=stag
CURSES=-lncurses -lmenu -lform 
CURSESW=-lncursesw -lmenuw -lformw -D_CURSESW_WIDE
FILES=ui.c stag.c tagger.c strlcpy.c strlcat.c basename.c dirname.c
CFLAGS+=-ansi -pedantic -Wall -D_BSD_SOURCE
CPPFLAGS+=-I/usr/local/include `taglib-config --cflags`
LDFLAGS+=-L/usr/local/lib `taglib-config --libs` -ltag_c -lstdc++ 

wide:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(CURSESW) $(FILES) -o $(PROG)
all:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(CURSES) $(FILES) -o $(PROG)
