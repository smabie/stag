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

struct {
        WINDOW  *win;
        MENU    *menu;
        
} dir, file, info;

struct {
        WINDOW  *win;
        FORM    *form;
} edit;

int resizep = 0;

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
	ITEM **items, *item;
	FIELD *edit_field[2];
	struct entry *p;
	char curdir[PATH_MAX], hostname[MAXHOSTNAMELEN], *s;
	int c, hidden, tmp, many, d, regexp, dir_idx, file_idx, info_idx, idx;
	int metap;
	enum { DIR_MODE, FILE_MODE, INFO_MODE, EDIT_MODE } state;

	(void)gethostname(hostname, MAXHOSTNAMELEN);
	idx = dir_idx = file_idx = info_idx = regexp = many = hidden = 0;
	metap = 0;
	state = DIR_MODE;
	info.menu = NULL;
	p = NULL;

	signal(SIGWINCH, resize);
	(void)memset(wtfbuf, ' ', 1023);
	
	(void)setlocale(LC_ALL, "");

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

	dir.win = newwin(LINES - INFO_LEN - 1, COLS / 2, 1, 0);
	file.win = newwin(LINES - INFO_LEN - 1, COLS / 2, 1, COLS / 2 + 1);
	info.win = newwin(INFO_LEN - 1, COLS, LINES - INFO_LEN + 1, 9);
	edit.win = newwin(1, COLS, LINES - 1, 0);

	keypad(dir.win, TRUE);
	keypad(file.win, TRUE);
	keypad(info.win, TRUE);
	keypad(edit.win, TRUE);

	intrflush(stdscr, FALSE);
	intrflush(dir.win, FALSE);
	intrflush(file.win, FALSE);
 	intrflush(info.win, FALSE);
 	intrflush(edit.win, FALSE);

	meta(stdscr, TRUE);
	meta(dir.win, TRUE);
	meta(file.win, TRUE);
	meta(info.win, TRUE);
	meta(edit.win, TRUE);

	if (getcwd(curdir, PATH_MAX) == NULL)
		err(1, "getcwd");

	set_menu_format(NULL, LINES - INFO_LEN - 1, 0);

	dir.menu = new_menu(path_make_items(curdir, hidden));
	file.menu = new_menu(NULL);
	info.menu = new_menu(NULL);

	set_menu_win(dir.menu, dir.win);
	set_menu_sub(dir.menu, dir.win);
	set_menu_win(file.menu, file.win);
	set_menu_sub(file.menu, file.win);
	set_menu_win(info.menu, info.win);
	set_menu_sub(info.menu, info.win);

	set_menu_mark(dir.menu, " ");
	set_menu_mark(file.menu, " ");
	set_menu_mark(info.menu, " ");

	menu_opts_off(file.menu, O_ONEVALUE);

	edit_field[0] = new_field(1, COLS, 0, 0, 0, 0);
	edit_field[1] = NULL;
	field_opts_off(edit_field[0], O_AUTOSKIP | O_STATIC);
	edit.form = new_form(edit_field);
	set_form_win(edit.form, edit.win);
	set_form_sub(edit.form, edit.win);
	
	set_menu_items(file.menu, list_make_items());
	post_menu(file.menu);
 	post_menu(dir.menu);

	nth_item(dir.menu, dir_idx);
	nth_item(file.menu, file_idx);

	if (state == INFO_MODE) {
		set_menu_items(info.menu, 
			       info_make_items(ENTRY(file.menu), 0));
		post_menu(info.menu);		
		nth_item(info.menu, info_idx);
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
			dir_idx = item_index(current_item
					     ((const MENU *)dir.menu));
			file_idx = item_index(current_item
					      ((const MENU *)file.menu));
			info_idx = item_index(current_item
					      ((const MENU *)info.menu));

			resizep = 0;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			resize_term(w.ws_row, w.ws_col);
			
			erase();
			werase(file.win);
			werase(dir.win);
			werase(info.win);
			
			delwin(dir.win);
			delwin(file.win);
			delwin(info.win);
			delwin(edit.win);
			endwin();
			
			goto resize;
		}
		if (state == DIR_MODE)
		switch (c) {
  		case ' ':	/* add all mp3/flac/ogg files */
			if ((tmp = strlen(curdir)) >= PATH_MAX)
				goto longer;
			curdir[tmp] = '/';
			curdir[tmp + 1] = '\0';

			item = current_item((const MENU *)dir.menu);

			if (strlcat(curdir, item_name(item), PATH_MAX)
			    >= PATH_MAX)
				goto longer;
			
			if (populate_active(curdir, AFLG_REC) == 1)
				return 1;

			curdir[tmp] = '\0';

			unpost_menu(file.menu);			
			items = menu_items(file.menu);
			set_menu_items(file.menu, list_make_items());

			if (items != NULL) {
				free_items(items);
				free(items);
			}

			post_menu(file.menu);
 
			wrefresh(info.win);
			wrefresh(file.win);
			refresh();
			break;
		case 13:	/* LF: step into directory */
			item = current_item((const MENU *)dir.menu);
			
			if (strcmp(item_name(item), "..") == 0) {
				if ((s = strrchr(curdir, '/')) == curdir)
					curdir[1] = '\0';
				else
					*s = '\0';
			} else if (strcmp(item_name(item), ".") != 0) {
				if ((tmp = strlen(curdir)) >= PATH_MAX)
					goto longer;
				curdir[tmp] = '/';
				curdir[tmp + 1] = '\0';
				
				if (strlcat(curdir, item_name(item), PATH_MAX)
				    >= PATH_MAX)
					goto longer;
			}

			unpost_menu(dir.menu);
			items = menu_items(dir.menu);
			set_menu_items(dir.menu, 
				       path_make_items(curdir, hidden));
			free_item_strings(items);
			free_items(items);
			free(items);
			post_menu(dir.menu);
			break;
		longer:
			stag_warnx("PATH_MAX of %d violated", PATH_MAX);
			curdir[tmp] = '\0';
			break;

		case 'h':	/* toggle hidden directories */
			unpost_menu(dir.menu);
			items = menu_items(dir.menu);
			set_menu_items(dir.menu, 
				       path_make_items(curdir, 
						       hidden = !hidden));
			free_item_strings(items);
			free_items(items);
			free(items);
			post_menu(dir.menu);
			break;
		case KEY_DOWN:
		case 'n':
			menu_driver(dir.menu, REQ_DOWN_ITEM);
			break;
		case KEY_NPAGE:
		case 14:	/* C-n */
			menu_driver(dir.menu, REQ_SCR_DPAGE);
			break;
		case KEY_RIGHT:	/* switch focus to active file list */
		case 'o':
			if (item_count(file.menu) > 0)
				state = FILE_MODE;
			continue;
		case KEY_UP:
		case 'p':
			menu_driver(dir.menu, REQ_UP_ITEM);
			break;
		case KEY_PPAGE:
		case 16:	/* C-p */
			menu_driver(dir.menu, REQ_SCR_UPAGE);
			break;
		}

		if (state == FILE_MODE)
		switch (c) {
		case ' ':	/* toggle mark */
			ENTRY(file.menu)->mark = !ENTRY(file.menu)->mark;
			menu_driver(file.menu, REQ_TOGGLE_ITEM);
			break;
		case 13:	/* LF: single edit */
			set_menu_items(info.menu, 
				       info_make_items(ENTRY(file.menu), 0));
			post_menu(info.menu);

			many = 0;
			state = INFO_MODE;
			continue;
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
			set_field_buffer(edit_field[0], 0, 
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

			s = str_cleanup(field_buffer(edit_field[0], 0));

			set_field_buffer(edit_field[0], 0, "");

			if (regexp) {
				regexp = 0;
				state = FILE_MODE;
				goto rjmp; /* ick */
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

	delwin(dir.win);
	delwin(file.win);
	delwin(info.win);
	delwin(edit.win);
	endwin();	

	return 0;
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
