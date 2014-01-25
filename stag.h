/* stag.h */
/* 
 * This file is public domain as declared by Sturm Mabie.
 */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
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
#include <setjmp.h>

#ifdef NCURSESW_WIDE
#include <ncursesw/curses.h>
#include <ncursesw/form.h>
#include <ncursesw/menu.h>
#else
#include <curses.h>
#include <form.h>
#include <menu.h>
#endif /* NCURSESW_WIDE */

#include <taglib/tag_c.h>

/* 
 * sys/queue from OpenBSD. We use our own because not all queue's are created
 * equal.
 */
#include "queue.h"		

#define ENTRY(x) ((struct entry *)					\
		  (item_userptr(current_item((const MENU *)(x)))))

#define PROG_NAME	"stag"
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
extern char cwd[];
extern const char *ext[];
extern struct textbox edit;
extern struct frame dir, file, info;
extern enum mode { DIR_MODE, FILE_MODE, INFO_MODE, EDIT_MODE } state;
extern jmp_buf env;

struct entry {	
 	char			dir[PATH_MAX];  /* set by dirname(3) */
	char			name[NAME_MAX]; /* set by basename(3) */
	int			mark;		/* is the file selected? */
	int			mod;		/* the tags been changed? */
	TagLib_File	       *filep;			
	TagLib_Tag	       *tagp;
	LIST_ENTRY(entry)	entries;
};

struct frame {
        WINDOW *win;
        MENU   *menu;
        int     idx;
};

struct textbox {
        WINDOW  *win;
        FORM    *form;
        FIELD   *field[2];
};

#ifdef __GLIBC__
#undef basename
#undef dirname
#define basename bsd_basename
#define dirname bsd_dirname

char *bsd_basename(const char *);
char *bsd_dirname(const char *);
size_t strlcat(char *, const char *, size_t);
size_t strlcpy(char *, const char *, size_t);
#endif	/* __GLIBC__ */

WINDOW *make_win(int, int, int, int);
MENU *make_menu(ITEM **, WINDOW *);

void init_screen();
void destroy_screen();

void print_state();
int any_marked();
void nth_item(MENU *, int);
unsigned int all_equal_int(unsigned int (*)(const TagLib_Tag *));
char *all_equal_str(char *(*)(const TagLib_Tag *));
char *str_cleanup(char *);
char *clean_xstrdup(char *);
const char *make_regex_str(TagLib_Tag *);
void resize(int);

void stag_warnx(const char *, ...);
void stag_warn(const char *, ...);

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

void kb_add();
void kb_enter(int);
void kb_hide(int *);
void kb_file_mode();
void kb_toggle();
void kb_edit(int *);
void kb_regex1(int *);
void kb_regex2(regex_t *, char *);
void kb_toggle_all();
void kb_clear();
void kb_remove();
void kb_multi_edit(int *);
void kb_move(int);
void kb_reload();
void kb_write();
void kb_unmark();
void kb_write_marked();
void kb_edit_field();
void kb_other();
void kb_left();


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

