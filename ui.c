/* ui.c */
/* 
 * This file is public domain as declared by Sturm Mabie.
 */

#include "stag.h"

#define SAFE(s) (strlen(s) != 0 ? (s) : " ")
#define INT_SAFE(s) ((*s) != '0' ? (s) : " ")

/* 
 * This is fucking retarded but is useful.
 */
char wtfbuf[1024];

void item_sort(ITEM *list[], int b, int e)
{
        ITEM *tmp, *piv;
        int l, r;

        if (e <= b + 1)
		return;

	piv = list[b];
	l = b + 1;
	r = e;

	while (l < r) {
		if (strcmp(item_name(list[l]),  item_name(piv)) <= 0)
			l++;
		else {
			tmp = list[l];
			list[l] = list[--r];
			list[r] = tmp;
		}
	}
	tmp = list[--l];
	list[l] = list[b];
	list[b] = tmp;

	item_sort(list, b, l);
	item_sort(list, r, e);
}

/* 
 * Given a path, allocates space for and fills items with all of the path's
 * subdirectories. If hidden is non-zero show hidden files.
 */
ITEM **
path_make_items(const char *path, int hidden)
{
	struct stat sb;
	DIR *dirp;
	struct dirent *dp;
	ITEM **ret;
	static char buf[PATH_MAX];
	size_t size;
	int first;
	int d;
	size_t s;

	ret = NULL;
	size = d = 2;
	first = 1;

	if ((dirp = opendir(path)) == NULL) {
		stag_warn("opendir: %s", path);
		return NULL;
	}

again:
	while (errno = 0, (dp = readdir(dirp)) != NULL) {
		if ((s = strlcpy(buf, path, PATH_MAX)) >= PATH_MAX)
			goto longer;
		if (path[(s = strlen(path)) - 1] != '/') {
			buf[s] = '/';
			buf[s + 1] = '\0';
		}
		if (strlcat(buf, dp->d_name, PATH_MAX) >= PATH_MAX)
			goto longer;
		if (stat(buf, &sb) == -1) {
			if (lstat(buf, &sb) == -1) {
				stag_warn("stat: %s", buf);
				goto er;
			} 
			continue;
		}
		if ((S_ISDIR(sb.st_mode) || S_ISLNK(sb.st_mode)) &&
		    (hidden || *dp->d_name != '.')) {
			if (strcmp(dp->d_name, ".") == 0 
			    || strcmp(dp->d_name, "..") == 0)
				continue;
			if (first)
				size++;
			else {
				if ((ret[d] = new_item(clean_xstrdup(dp->d_name), 
                                                       wtfbuf)) == NULL)
					err(1, "new_item: %s", dp->d_name);
				set_item_userptr(ret[d++], NULL); 
			}
		}

		buf[buf[s] == '/' ? s + 1 : s] = '\0';
	}
	if (errno) {
		stag_warn("readdir");
		goto er;
	}
	if (first) {
		first = 0;
		if ((ret = calloc(size + 1, sizeof(ITEM *))) == NULL)
			err(1, "calloc");
		ret[size] = NULL;
		rewinddir(dirp);
		goto again;
	}
	(void)closedir(dirp);
	if ((ret[0] = new_item(".", wtfbuf)) == NULL)
		err(1, "new_item: .");
	if ((ret[1] = new_item("..", wtfbuf)) == NULL)
		err(1, "new_item: ..");
	item_sort(ret + 2, 0, size - 2);
	return ret;
longer:
	stag_warnx("PATH_MAX of %d violated", PATH_MAX);
er:
	(void)closedir(dirp);
	free_items(ret);
	return NULL;
}

/* 
 * Make a a item list from the active list. O(2n). What's a constant among
 * friends?
 */
ITEM **
list_make_items()
{
	ITEM **ret;
	struct entry *p;
	size_t size;
	int d;

	d = size = 0;

	if (LIST_EMPTY(&active))
		return NULL;

	LIST_FOREACH(p, &active, entries)
		size++; 

	if ((ret = calloc(size + 1, sizeof(ITEM *))) == NULL)
		err(1, "calloc");
	ret[size] = NULL;	
       
	LIST_FOREACH(p, &active, entries) {
		if ((ret[d] = new_item(p->name, wtfbuf)) == NULL)
			err(1, "new_item: %s", p->name);
		set_item_userptr(ret[d++], p); 
	}

	return ret;
}

unsigned int all_equal_int(unsigned int (*fp)(const TagLib_Tag *))
{
	struct entry *p;
	unsigned int ret;
	int first;

	ret = 0;
	first = 1;
	LIST_FOREACH(p, &active, entries) {
		if (p->mark) {
			if (first) {
				ret = (*fp)(p->tagp);
				first = 0;
				continue;
			}
			if (ret != (*fp)(p->tagp))
				return 0;
		}
	}
	return ret;
}

char *all_equal_str(char * (*fp)(const TagLib_Tag *))
{
	struct entry *p;
	char *ret;
	int first;

	ret = NULL;
	first = 1;
	LIST_FOREACH(p, &active, entries) {
		if (p->mark) {
			if (first) {
				ret = (*fp)(p->tagp);
				first = 0;
				continue;
			}
			if (strcmp(ret, (*fp)(p->tagp)) != 0)
				return " ";
		}
	}
	return ret;
}

/* 
 * if many is non-zero, populate based on multi-valued selection
 */
ITEM **
info_make_items(const struct entry *p, int many)
{
#define MANY(f) (many ? all_equal_str(f) : f(p->tagp))
#define INT_MANY(f) (many ? all_equal_int(f) : f(p->tagp))
	static char trackbuf[3];
	static char yearbuf[5];
	static ITEM *ret[8];

	(void)snprintf(trackbuf, 3, "%d", INT_MANY(taglib_tag_track));
	(void)snprintf(yearbuf, 5, "%d", INT_MANY(taglib_tag_year));

        free_item_strings(ret);

        ret[0] = new_item(clean_xstrdup(INT_SAFE(trackbuf)), wtfbuf);
        ret[1] = new_item(clean_xstrdup(SAFE(MANY(taglib_tag_title))), wtfbuf);
        ret[2] = new_item(clean_xstrdup(SAFE(MANY(taglib_tag_artist))), wtfbuf);
        ret[3] = new_item(clean_xstrdup(SAFE(MANY(taglib_tag_album))), wtfbuf);
        ret[4] = new_item(clean_xstrdup(SAFE(MANY(taglib_tag_genre))), wtfbuf);
        ret[5] = new_item(clean_xstrdup(INT_SAFE(yearbuf)), wtfbuf);
        ret[6] = new_item(clean_xstrdup(SAFE(MANY(taglib_tag_comment))), wtfbuf);
	ret[7] = NULL;

	return ret;
}

void
draw_info(const struct entry *p, WINDOW *win)
{
#define S() (p->mod ? '*' : ' ')
	static char yearbuf[5];
	static char trackbuf[3];

	(void)snprintf(yearbuf, 5, "%d", taglib_tag_year(p->tagp));
	(void)snprintf(trackbuf, 3, "%d", taglib_tag_track(p->tagp));

	mvwprintw(win, 0, 0, "%c%s\n", S(), INT_SAFE(trackbuf));
	mvwprintw(win, 1, 0, "%c%s\n", S(), SAFE(taglib_tag_title(p->tagp)));
	mvwprintw(win, 2, 0, "%c%s\n", S(), SAFE(taglib_tag_artist(p->tagp)));
	mvwprintw(win, 3, 0, "%c%s\n", S(), SAFE(taglib_tag_album(p->tagp)));
	mvwprintw(win, 4, 0, "%c%s\n", S(), SAFE(taglib_tag_genre(p->tagp)));
	mvwprintw(win, 5, 0, "%c%s\n", S(), INT_SAFE(yearbuf));
	mvwprintw(win, 6, 0, "%c%s\n", S(), SAFE(taglib_tag_comment(p->tagp)));
}

void
nth_item(MENU *menu, int n)
{
	if (n > item_count(menu)) {
		menu_driver(menu, REQ_LAST_ITEM);
		return;
	}
	menu_driver(menu, REQ_FIRST_ITEM);
	while (n-- > 0)
		menu_driver(menu, REQ_DOWN_ITEM);
}

int
any_marked(void)
{
        struct entry *p;

        LIST_FOREACH(p, &active, entries) {
                if (p->mark)
                        return 1;
        }
        return 0;
}

void
free_items(ITEM **items)
{
	ITEM **p;

	for (p = items; *p != NULL; p++)
		free_item(*p);
}

void
free_item_strings(ITEM **items)
{
	ITEM **p;

	for (p = items + 2; *p != NULL; p++)
		free((char *)item_name(*p));
}

char *
clean_xstrdup(char *s)
{
        char *ret;

        if ((ret = strdup(s)) == NULL)
                err(1, "strdup: ");

        return ret;
}

void
stag_warn(const char *fmt, ...)
{
	va_list ap;
	
	va_start(ap, fmt);
	mvprintw(0, 0, "stag: ");
	if (fmt != NULL) {
		vw_printw(stdscr, fmt, ap);
		printw(": ");
	}
	printw("%s\n", strerror(errno));
	va_end(ap);
	refresh();
}

void
stag_warnx(const char *fmt, ...)
{
	va_list ap;
	
	va_start(ap, fmt);
	mvprintw(0, 0, "stag: ");
	if (fmt != NULL)
		vw_printw(stdscr, fmt, ap);
	va_end(ap);
	refresh();
}
