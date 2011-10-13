/* stag.h */
/* 
 * This file is public domain as declared by Sturm Mabie.
 */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <locale.h>
#include <regex.h>
#include <signal.h>
#include <unistd.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curses.h>
#include <form.h>
#include <menu.h>

#include <taglib/tag_c.h>

#define SAFE(s) (strlen(s) != 0 ? (s) : " ")
#define INT_SAFE(s) ((*s) != '0' ? (s) : " ")

#define PROG_NAME	"stag"
#define PROG_VERSION	"1.0"
#define INFO_LEN 9		/* Height of the bottom info window. */

/* 
 * These flags are used by the make_active function and control its behavior
 * and what files to add to the list of active files. Active files are files
 * that can be viewed and edited. It might be a dubious design to group the
 * recurse, clear, and hide functions with the rest.
 */
#define AFLG_REC 0x0001		/* recursively add files? */
#define AFLG_CLR 0x0002		/* clear active structure? */
#define AFLG_HID 0x0004		/* include hidden files and directories? */
#define AFLG_MP3 0x0008		/* mp3 files */
#define AFLG_OGG 0x0010		/* ogg vorbis */
#define AFLG_FLC 0x0020		/* flac */
#define AFLG_MPC 0x0040		/* musepack */
#define AFLG_OGF 0x0080		/* ogg flac */
#define AFLG_WPK 0x0100		/* wavpack */
#define AFLG_SPX 0x0200		/* speex */
#define AFLG_TAO 0x0400		/* true audio */
#define AFLG_MP4 0x0800		/* mpeg4 */
#define AFLG_ASF 0x1000		/* ms asf */

LIST_HEAD(h, entry);

extern struct h active;
extern char wtfbuf[]; 
extern const char *ext[];

struct entry {	
 	char			dir[PATH_MAX];  /* set by dirname(3) */
	char			name[NAME_MAX]; /* set by basename(3) */
	int			mark;		/* is the file selected? */
	int			mod;		/* the tags been changed? */
	TagLib_File	       *filep;			
	TagLib_Tag	       *tagp;
	LIST_ENTRY(entry)	entries;
};

/* 
 * Suck my dick Ulrich Drepper.
 */
#ifdef __GLIBC__
#undef basename
#undef dirname
#define basename bsd_basename
#define dirname bsd_dirname

#if defined(QUEUE_MACRO_DEBUG) || (defined(_KERNEL) && defined(DIAGNOSTIC))
#define _Q_INVALIDATE(a) (a) = ((void *)-1)
#else
#define _Q_INVALIDATE(a)
#endif

#define LIST_REPLACE(elm, elm2, field) do {                             \
        if (((elm2)->field.le_next = (elm)->field.le_next) != NULL)     \
                (elm2)->field.le_next->field.le_prev =                  \
			&(elm2)->field.le_next;				\
        (elm2)->field.le_prev = (elm)->field.le_prev;                   \
	*(elm2)->field.le_prev = (elm2);                                \
	_Q_INVALIDATE((elm)->field.le_prev);                            \
	_Q_INVALIDATE((elm)->field.le_next);                            \
	} while (0)

char *bsd_basename(const char *);
char *bsd_dirname(const char *);
size_t strlcat(char *, const char *, size_t);
size_t strlcpy(char *, const char *, size_t);
#endif	/* __GLIC__ */

ITEM **path_make_items(const char *, int);
ITEM **list_make_items(void);
ITEM **info_make_items(const struct entry *, int);
void free_items(ITEM **);
void free_item_strings(ITEM **);
void draw_info(const struct entry *, WINDOW *);

struct entry *make_entry(const char *);
void free_entry(struct entry *);
void clear_active(void);
int populate_active(const char *, unsigned int);

void set_title(struct entry *, const char *);
void set_artist(struct entry *, const char *);
void set_album(struct entry *, const char *);
void set_comment(struct entry *, const char *);
void set_genre(struct entry *, const char *);
void set_year(struct entry *, unsigned int);
void set_track(struct entry *, unsigned int);

void write_entry(struct entry *);
void revert_entry(struct entry *);
void remove_entry(struct entry *);

void write_marked(void);
void revert_marked(void);
void remove_marked(void);
void set_marked_title(const char *);
void set_marked_artist(const char *);
void set_marked_album(const char *);
void set_marked_comment(const char *);
void set_marked_genre(const char *);
void set_marked_year(unsigned int);
void set_marked_track(unsigned int);

