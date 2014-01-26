/* stag.c */
/* 
 * This file is public domain as declared by Sturm Mabie.
 */

#include "stag.h"

/* 
 * Access entry from list item.
 */
#define ENTRY(x) ((struct entry *)					\
		  (item_userptr(current_item((const MENU *)(x)))))

#define CHOOSE(t, s) do {			\
	if (many)			        \
		set_marked_##t (s);		\
	else	 				\
		set_##t (ENTRY(file.menu), s);	\
	} while (0)					

struct frame dir, file, info;
struct textbox edit;

int resizep;
char cwd[PATH_MAX];
enum mode state;

jmp_buf env;

/* 
 * I don't catch curses functions with (void). It's too annoying. In fact all of
 * this code does mysterious things on errors. Moreover, this code is just
 * horrible. ncurses is horrible and riddled with bugs, my code does weird state
 * shit with gotos, there's too many variables and everything in general is just
 * sort of confusing.
 */
int
main(int argc, char **argv)
{
	struct winsize w;
	regex_t reg;
	char *s;
	int c, hidden, many, d, regexp, idx;
	int metap;

	idx  = regexp = many = hidden = 0;
	metap = 0;
	state = DIR_MODE;
	info.menu = NULL;
        
	signal(SIGWINCH, resize);
	(void)memset(wtfbuf, ' ', 1023);
	
	(void)setlocale(LC_ALL, "");

        if (getcwd(cwd, PATH_MAX) == NULL)
		err(1, "getcwd");

	if (getopt(argc, argv, "") != -1) {
		(void)fprintf(stderr, "usage: %s [directory ...]\n", PROG_NAME);
		exit(1);
	}

	for (d = 1; d < argc; d++) {
		if (access(argv[d], R_OK | W_OK | X_OK) == -1) {
			warn("access: %s", argv[d]);
			continue;
		}
		if (populate_active(argv[d], AFLG_REC))
			return 1;
	}
	/* 
	 * This jump is awful and makes things initially difficult to reason
	 * about.
	 */
resize:
        init_screen();

        dir.idx = file.idx = info.idx = 0;

	dir.win = make_win(LINES - INFO_LEN - 1, COLS / 2, 1, 0);
	file.win = make_win(LINES - INFO_LEN - 1, COLS / 2, 1, COLS / 2 + 1);
	info.win = make_win(INFO_LEN - 1, COLS, LINES - INFO_LEN + 1, 9);
	edit.win = make_win(1, COLS, LINES - 1, 0);

        dir.menu = make_menu(path_make_items(cwd, hidden), dir.win);
        file.menu = make_menu(NULL, file.win);
        info.menu = make_menu(NULL, info.win);

	menu_opts_off(file.menu, O_ONEVALUE);

	edit.field[0] = new_field(1, COLS, 0, 0, 0, 0);
	edit.field[1] = NULL;

	field_opts_off(edit.field[0], O_AUTOSKIP | O_STATIC);
	edit.form = new_form(edit.field);

	set_form_win(edit.form, edit.win);
	set_form_sub(edit.form, edit.win);
	
	set_menu_items(file.menu, list_make_items());
	post_menu(file.menu);
 	post_menu(dir.menu);

	nth_item(dir.menu, dir.idx);
	nth_item(file.menu, file.idx);

	if (state == INFO_MODE) {
		set_menu_items(info.menu, 
			       info_make_items(ENTRY(file.menu), 0));
		post_menu(info.menu);		
		nth_item(info.menu, info.idx);
	}

	refresh();
	wrefresh(info.win);
	wrefresh(file.win);
	wrefresh(dir.win);

	while ((c = wgetch(state == DIR_MODE ? 
			   dir.win : (state == FILE_MODE ? 
				      file.win : info.win))) != EOF) {
		if (c == 'q' && state != EDIT_MODE)
			break;
		if (resizep) {
			dir.idx = item_index(current_item
					     ((const MENU *)dir.menu));
			file.idx = item_index(current_item
					      ((const MENU *)file.menu));
			info.idx = item_index(current_item
					      ((const MENU *)info.menu));

			resizep = 0;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			resize_term(w.ws_row, w.ws_col);			
                        destroy_screen();

			goto resize;
		}
                
		if (state == DIR_MODE) {
                        switch (c) {
                        case ' ':
                                kb_add();
                                break;
                        case 13:
                                kb_enter(hidden);
                                break;
                        case 'h':
                                kb_hide(&hidden);
                                break;
                        case KEY_DOWN:
                        case 'n':
                                menu_driver(dir.menu, REQ_DOWN_ITEM);
                                break;
                        case KEY_NPAGE:
                        case 14:
                                menu_driver(dir.menu, REQ_SCR_DPAGE);
                                break;
                        case 9:
                        case KEY_RIGHT:
                        case 'o':
                                kb_file_mode();
                                print_state();
                                continue;
                        case KEY_UP:
                        case 'p':
                                menu_driver(dir.menu, REQ_UP_ITEM);
                                break;
                        case KEY_PPAGE:
                        case 16:
                                menu_driver(dir.menu, REQ_SCR_UPAGE);
                                break;
                        }
                }
                
		if (state == FILE_MODE) {
                        switch (c) {
                        case 9:
                                if (any_marked())
                                        kb_multi_edit(&many);
                                else
                                        state = DIR_MODE;
                                print_state();
                                continue;
                        case ' ':
                                kb_toggle();
                                break;
                        case 13:
                                kb_edit(&many);
                                print_state();
                                continue;
                        case '/':
                                kb_regex1(&regexp);
                                continue;
                        rjmp:
                                kb_regex2(&reg, s);
                                break;
                        case 'a':
                                kb_toggle_all();
                                break;
                        case 'c':
                                kb_clear();
                                continue;
                        case 'd':
                                kb_remove();
                                break;
                        case 'e':
                                kb_multi_edit(&many);
                                continue;
                        case KEY_LEFT:
                        case 'o':
                                state = DIR_MODE;
                                print_state();
                                continue;
                        case KEY_DOWN:
                        case 'n':
                                kb_move(REQ_DOWN_ITEM);
                                break;
                        case KEY_NPAGE:
                        case 14:
                                kb_move(REQ_SCR_DPAGE);
                                break;
                        case KEY_UP:
                        case 'p':
                                kb_move(REQ_UP_ITEM);
                                break;
                        case KEY_PPAGE:
                        case 16:
                                kb_move(REQ_SCR_UPAGE);
                                break;
                        case 'r':
                                kb_reload();
                                break;
                        case 's':
                                kb_write();
                                break;
                        case 'u':
                                kb_unmark();
                                break;
                        case 'w':
                                kb_write_marked();
                                break;
                        }
                }
                
		if (state == INFO_MODE) {
                        switch (c) {
                        case 9:
                                kb_other();
                                state = DIR_MODE;
                                print_state();
                                continue;
                        case 13:
                                kb_edit_field();
                                print_state();
                                continue;
                        case KEY_RIGHT:
                        case 'o':
                                kb_other();
                                print_state();
                                continue;
                        case KEY_LEFT:
                                kb_left();
                                continue;
                        case KEY_DOWN:
                        case 'n':
                                menu_driver(info.menu, REQ_DOWN_ITEM);
                                break;
                        case KEY_UP:
                        case 'p':
                                menu_driver(info.menu, REQ_UP_ITEM);
                                break;
                        }
                }
                
		if (state == EDIT_MODE) {
                        switch (c) {
                        case 13:	/* LF */
                                form_driver(edit.form, REQ_NEXT_FIELD);
                                
                                s = str_cleanup(field_buffer(edit.field[0], 0));
                                
                                set_field_buffer(edit.field[0], 0, "");
                                
                                if (regexp) {
                                        regexp = 0;
                                        state = FILE_MODE;
                                        print_state();
                                        goto rjmp;
                                }
                                /* 
                                 * This whole scheme is pretty fucking stupid 
                                 * and should probably be fixed to use function 
                                 * pointers.
                                 */
                                switch (item_index(current_item(info.menu))) {
                                case 0:
                                        CHOOSE(track, atoi(s));
                                        break;
                                case 1:
                                        CHOOSE(title, s);
                                        break;
                                case 2:
                                        CHOOSE(artist, s);
                                        break;
                                case 3:
                                        CHOOSE(album, s);
                                        break;
                                case 4:
                                        CHOOSE(genre, s);
                                        break;
                                case 5:
                                        CHOOSE(year, atoi(s));
                                        break;
                                case 6:	CHOOSE(comment, s);
                                        break;
                                }
                                idx = item_index(current_item
                                                 ((const MENU *)info.menu));
                                
                                unpost_menu(info.menu);
                                set_menu_items(info.menu,
                                               info_make_items(ENTRY(file.menu),
                                                               many));
                                post_menu(info.menu);
                                nth_item(info.menu, idx);
                                
                                wrefresh(edit.win);
                                state = INFO_MODE;
                                print_state();
                                continue;
                        case KEY_BACKSPACE:	/* backspace */
                                form_driver(edit.form, REQ_DEL_PREV);
                                break;
                        case 1:		/* C-a */
                                form_driver(edit.form, REQ_BEG_FIELD);
                                break;
                        case KEY_LEFT:
                        case 2:		/* C-b */
                                form_driver(edit.form, REQ_PREV_CHAR);
                                break;
                        case KEY_DC:
                        case 4:		/* C-d */
                                form_driver(edit.form, REQ_DEL_CHAR);
                                break;
                        case 5:		/* C-e */
                                form_driver(edit.form, REQ_END_FIELD);
                                break;
                        case KEY_RIGHT:
                        case 6:		/* C-f */
                                form_driver(edit.form, REQ_NEXT_CHAR);
                                break;
                        case 11:	/* C-k */
                                form_driver(edit.form, REQ_CLR_EOF);
                                break;
                        case 27:	/* meta key */
                                metap = 1;
                                break;
                        case 'b':	/* M-b */
                                if (metap) {
                                        metap = 0;
                                        form_driver(edit.form, REQ_PREV_WORD);
                                        break;
                                }
                        case 'f':	/* M-f */
                                if (metap) {
                                        metap = 0;
                                        form_driver(edit.form, REQ_NEXT_WORD);
                                        break;
                                }
                        default:
                                form_driver(edit.form, c);
                                break;
                        }
                }
		if (state == EDIT_MODE) 
			wrefresh(edit.win);
	}

        destroy_screen();

	return 0;
}

void
init_screen()
{
        char hostname[MAXHOSTNAMELEN];

        (void)gethostname(hostname, MAXHOSTNAMELEN);
        
        initscr();
	raw();
	noecho();
	nonl();
                
	mvvline(1, COLS / 2, 0, LINES - INFO_LEN - 1);
	mvhline(LINES - INFO_LEN, 0, 0, COLS);
	mvprintw(0, COLS - sizeof(PROG_NAME) - strlen(hostname) - 1,
		 "%s@%s", PROG_NAME, hostname);
        print_state();
	mvprintw(LINES - INFO_LEN + 1, 0, "track:\t ");
	mvprintw(LINES - INFO_LEN + 2, 0, "title:\t ");
	mvprintw(LINES - INFO_LEN + 3, 0, "artist:\t ");
	mvprintw(LINES - INFO_LEN + 4, 0, "album:\t ");
	mvprintw(LINES - INFO_LEN + 5, 0, "genre:\t ");
	mvprintw(LINES - INFO_LEN + 6, 0, "year:\t ");
	mvprintw(LINES - INFO_LEN + 7, 0, "comment:\t ");

	intrflush(stdscr, FALSE);
	meta(stdscr, TRUE);

	set_menu_format(NULL, LINES - INFO_LEN - 1, 0);
}

void
destroy_screen()
{
        erase();
        werase(file.win);
        werase(dir.win);
        werase(info.win);
	
        delwin(dir.win);
        delwin(file.win);
        delwin(info.win);
        delwin(edit.win);
        endwin();
}

WINDOW *
make_win(int nrow, int ncol, int y, int x)
{
        WINDOW *ret;
        
        ret = newwin(nrow, ncol, y, x);
        keypad(ret, TRUE);
        intrflush(ret, FALSE);
        meta(ret, TRUE);
        
        return ret;
}

MENU *
make_menu(ITEM **items, WINDOW *win)
{
        MENU *ret;

        ret = new_menu(items);
        set_menu_mark(ret, " ");
        set_menu_win(ret, win);
        set_menu_sub(ret, win);
        
        return ret;
}

/* 
 * Remove leading and trailing blanks from the string.
 */
char *
str_cleanup(char *s)
{
	static char buf[1024];
	char *ret;
	int c;
	
	(void)strlcpy(buf, s, 1024);

	for (ret = buf; isspace(*ret); ret++)
		;
	for (c = strlen(buf) - 1; isspace(buf[c]); c--)
		;
	buf[c + 1] = '\0';

	return ret;
}

/* 
 * generate string to match against.
 */
const char *
make_regex_str(TagLib_Tag *p)
{
	static char buf[2048];	/* arbitrary */

	(void)snprintf(buf, 2048, "track: %d title: %s artist: %s album: %s"
		       " genre: %s year: %d comment: %s",
		       taglib_tag_year(p),
		       taglib_tag_title(p),
		       taglib_tag_artist(p),
		       taglib_tag_album(p),
		       taglib_tag_genre(p),
		       taglib_tag_year(p),
		       taglib_tag_comment(p));
	return buf;
}

void
print_state()
{
        char *s;
        
        mvprintw(0, 0, "               ");
        switch (state) {
        case DIR_MODE:
                s = "directory-mode";
                break;
        case FILE_MODE:
                s = "file-mode";
                break;
        case INFO_MODE:
                s = "info-mode";
                break;
        case EDIT_MODE:
                s = "edit-mode";
                break;
        }
        mvprintw(0, 0, " %s", s);
        refresh();
}
void
resize(int s)
{
	resizep = 1;
}
