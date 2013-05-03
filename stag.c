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
	ITEM **items;
	struct entry *p;
	char *s;
	int c, hidden, tmp, many, d, regexp, idx;
	int metap;

	idx  = regexp = many = hidden = 0;
	metap = 0;
	state = DIR_MODE;
	info.menu = NULL;
	p = NULL;

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
		if (state == DIR_MODE)
		switch (c) {
  		case ' ':       kb_add();                               break;
                case 13:        kb_enter(hidden);                       break;
                case 'h':       kb_hide(&hidden);                       break;
		case KEY_DOWN:
		case 'n':       menu_driver(dir.menu, REQ_DOWN_ITEM);   break;
		case KEY_NPAGE:
		case 14:        menu_driver(dir.menu, REQ_SCR_DPAGE);   break;
		case KEY_RIGHT:
		case 'o':       kb_file_mode();                      continue;
		case KEY_UP:
		case 'p':       menu_driver(dir.menu, REQ_UP_ITEM);     break;
		case KEY_PPAGE:
		case 16: 	menu_driver(dir.menu, REQ_SCR_UPAGE);	break;
		}

		if (state == FILE_MODE)
		switch (c) {
		case ' ':       kb_toggle();                    	break;
		case 13:	kb_edit(&many);                      continue;
		case '/':	/* regex select */
			regexp = 1;
			post_form(edit.form);
			wrefresh(edit.win);
			state = EDIT_MODE;
			continue;
		rjmp:
			if (regcomp(&reg, s, REG_EXTENDED | 
				    REG_ICASE | REG_NOSUB) != 0)
				break;
			menu_driver(file.menu, REQ_FIRST_ITEM);
			for (d = 0; d < item_count(file.menu); d++, 
				     menu_driver(file.menu, REQ_DOWN_ITEM)) {
				if (regexec(&reg, make_regex_str
					    (ENTRY(file.menu)->tagp), 
					    0, NULL, 0)
				    == 0) {
					ENTRY(file.menu)->mark = 
						!ENTRY(file.menu)->mark;
					menu_driver(file.menu, REQ_TOGGLE_ITEM);
				}
			}
			menu_driver(file.menu, REQ_FIRST_ITEM);
			break;
		case 'a':	/* toggle all */
			idx = item_index(current_item((const MENU *)file.menu));
			menu_driver(file.menu, REQ_FIRST_ITEM);
			for (d = 0; d < item_count(file.menu); d++, 
				     menu_driver(file.menu, REQ_DOWN_ITEM)) {
				ENTRY(file.menu)->mark = 
					!ENTRY(file.menu)->mark;
				menu_driver(file.menu, REQ_TOGGLE_ITEM);
			}
			nth_item(file.menu, idx);
			break;
		case 'c':	/* clear */
			unpost_menu(file.menu);

			items = menu_items(file.menu);
			clear_active();
			set_menu_items(file.menu, NULL);
			if (items != NULL) {
				free_items(items);
				free(items);
			}

			post_menu(file.menu);
			wrefresh(file.win);
			state = DIR_MODE;
			continue;
		case 'd':	/* remove selected from active list */
			unpost_menu(file.menu);
			
			s = ENTRY(file.menu)->name;
			idx = item_index(current_item((const MENU *)file.menu));
			items = menu_items(file.menu);
			remove_marked();
			set_menu_items(file.menu, list_make_items());
			if (items != NULL) {
				free_items(items);
				free(items);
			}

			post_menu(file.menu);
			for (d = 0; d < item_count(file.menu) && 
				     s != ENTRY(file.menu)->name; d++) {
				menu_driver(file.menu, REQ_DOWN_ITEM);
			}
			/* The selected item was one of the items deleted. */
			if (d == item_count(file.menu))
				nth_item(file.menu, idx);
			wrefresh(file.win);
			break;
		case 'e':	/* multi edit */
			tmp = 1;
			LIST_FOREACH(p, &active, entries) {
				if (p->mark)
					tmp = 0;
			}
			if (tmp)
				break;
			set_menu_items(info.menu,
				       info_make_items(ENTRY(file.menu), 1));
			post_menu(info.menu);
			many = 1;
			state = INFO_MODE;
			continue;
		case KEY_LEFT:	/* switch focus to directory list */
		case 'o':
			state = DIR_MODE;
			continue;

		case KEY_DOWN:
		case 'n':
			menu_driver(file.menu, REQ_DOWN_ITEM);
			draw_info(ENTRY(file.menu), info.win);
			wrefresh(info.win);
			break;
		case KEY_NPAGE:
		case 14:	/* C-n */
			menu_driver(file.menu, REQ_SCR_DPAGE);
			draw_info(ENTRY(file.menu), info.win);
			wrefresh(info.win);
			break;
		case KEY_UP:
		case 'p':
			menu_driver(file.menu, REQ_UP_ITEM);
			draw_info(ENTRY(file.menu), info.win);
			wrefresh(info.win);
			break;
		case KEY_PPAGE:
		case 16:	/* C-p */
			menu_driver(file.menu, REQ_SCR_UPAGE);
			draw_info(ENTRY(file.menu), info.win);
			wrefresh(info.win);
			break;
		case 'r':	/* reload marked */
			revert_marked();
			draw_info(ENTRY(file.menu), info.win);
			wrefresh(info.win);
			break;
		case 's':	/* write file */
			write_entry(ENTRY(file.menu));
			draw_info(ENTRY(file.menu), info.win);
			wrefresh(info.win);
			break;
		case 'u':	/* unmark all */
			idx = item_index(current_item((const MENU *)file.menu));
			menu_driver(file.menu, REQ_FIRST_ITEM);
			for (d = 0; d < item_count(file.menu); d++, 
				     menu_driver(file.menu, REQ_DOWN_ITEM)) {
				if (ENTRY(file.menu)->mark) {
					menu_driver(file.menu, REQ_TOGGLE_ITEM);
					ENTRY(file.menu)->mark = 0;
				}
			}
			nth_item(file.menu, idx);
			break;
		case 'w':	/* write marked */
			write_marked();
			draw_info(ENTRY(file.menu), info.win);
			wrefresh(info.win);
			break;
		}

		if (state == INFO_MODE)
		switch (c) {
		case 13:	/* LF: edit entry */
			set_field_buffer(edit.field[0], 0, 
					 item_name(current_item(info.menu)));
			post_form(edit.form);
			form_driver(edit.form, REQ_END_FIELD);

			/*
			 * XXX this doesn't make any sense ncurses is fucked up.
			 */
			form_driver(edit.form, ' ');
			form_driver(edit.form, REQ_DEL_PREV);

			wrefresh(edit.win);
			state = EDIT_MODE;
			continue;
		case KEY_RIGHT:
		case 'o':	/* switch focus to file list */
			unpost_menu(info.menu);
			free_items(menu_items(info.menu));
			draw_info(ENTRY(file.menu), info.win);

			wrefresh(info.win);
			state = FILE_MODE;
			continue;
		case KEY_LEFT:
			unpost_menu(info.menu);
			free_items(menu_items(info.menu));
			draw_info(ENTRY(file.menu), info.win);

			wrefresh(info.win);
			state = DIR_MODE;
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

		if (state == EDIT_MODE) 
		switch (c) {
		case 13:	/* LF */
			form_driver(edit.form, REQ_NEXT_FIELD);

			s = str_cleanup(field_buffer(edit.field[0], 0));

			set_field_buffer(edit.field[0], 0, "");

			if (regexp) {
				regexp = 0;
				state = FILE_MODE;
				goto rjmp;
			}
			/* 
			 * This whole scheme is pretty fucking stupid and should
			 * probably be fixed to use function pointers.
			 */
			switch (item_index(current_item(info.menu))) {
			case 0:	CHOOSE(track, atoi(s));		break;
			case 1:	CHOOSE(title, s);		break;
			case 2:	CHOOSE(artist, s);		break;
			case 3: CHOOSE(album, s);		break;
			case 4:	CHOOSE(genre, s);		break;
			case 5:	CHOOSE(year, atoi(s));		break;
			case 6:	CHOOSE(comment, s);		break;
			}
			idx = item_index(current_item((const MENU *)info.menu));

			unpost_menu(info.menu);
			set_menu_items(info.menu,
				       info_make_items(ENTRY(file.menu), many));
			post_menu(info.menu);
			nth_item(info.menu, idx);

			wrefresh(edit.win);
			state = INFO_MODE;
			continue;
		case 127:	/* backspace */
			form_driver(edit.form, REQ_DEL_PREV);
			break;
		case 1:		/* C-a */
			form_driver(edit.form, REQ_BEG_FIELD);
			break;
		case KEY_LEFT:
		case 2:		/* C-b */
			form_driver(edit.form, REQ_PREV_CHAR);
			break;
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
resize(int s)
{
	resizep = 1;
}
