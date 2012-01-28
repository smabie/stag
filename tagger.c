/* tagger.c */
/* 
 * This file is public domain as declared by Sturm Mabie.
 */

#include "stag.h"

/* 
 * There are alternate extensions but people don't use them I think (hope).
 */
const char *ext[] = {
	"mp3",
	"ogg",
	"flac",
	"mpc",
	"oga",
	"wv",
	"spx",
	"tta",
	"mp4",
	"asf",
};

struct h active = LIST_HEAD_INITIALIZER(active);

/* 
 * Based on the mode, determine if s is of an applicable type (currently
 * {flac,ogg,mp3}).
 */
int is_valid_extension(const char *s, unsigned int mode)
{
	int c, d;
 	static char buf[NAME_MAX];
	char *p;

	(void)strlcpy(buf, s, NAME_MAX);

	if ((mode >> 3) == 0)	/* if no bits are set we match all */
		mode = UINT_MAX;
	if ((p = strrchr(buf, '.')) == NULL)
		return 0;
	*p = '/';
	if ((p = basename(buf)) == NULL) {
		stag_warn("basename of %s", buf);
		return 0;
 	}
	for (c = 8, d = 0; c <= AFLG_ASF; c <<= 1, d++) {
		if ((mode & c) == c) {
			if (strcmp(ext[d], p) == 0)
				return 1;
		}
	}
	return 0;
}

/* 
 * Create an entry structure from a path. 
 */
struct entry *
make_entry(const char *path)
{
	struct entry *ret;
	char *tmp;

	if ((ret = malloc(sizeof(struct entry))) == NULL)
		err(1, "malloc");
	if ((tmp = dirname(path)) == NULL) {
		stag_warn("dirname: %s", path);
		goto er;
	}
	(void)strlcpy(ret->dir, tmp, PATH_MAX);
	if ((tmp = basename(path)) == NULL) {
		stag_warn("basename: %s", path);
		goto er;
	}
	(void)strlcpy(ret->name, tmp, NAME_MAX);
	if ((ret->filep = taglib_file_new(path)) == NULL) {
		stag_warnx("the type of %s cannot be determined or the file"
				" cannot be opened", ret->name);
		goto er;
	}
	if (!taglib_file_is_valid(ret->filep)) {
		stag_warnx("the file %s is not valid", ret->name);
		goto er;
	}
	if ((ret->tagp = taglib_file_tag(ret->filep)) == NULL) {
		stag_warnx("something is wrong with the file %s", ret->name);
		goto er;
	}
	ret->mod = ret->mark = 0;

	return ret;
er:
	free(ret);
	return NULL;
}

/* 
 * Free an entry created with make_entry().
 */
void
free_entry(struct entry *p) 
{
	taglib_tag_free_strings();
	taglib_file_free(p->filep);
	free(p);
}

/* 
 * Clear the active list.
 */
void
clear_active(void)
{
	struct entry *p;

	while (!LIST_EMPTY(&active)) {
		p = LIST_FIRST(&active);
		LIST_REMOVE(p, entries);
		free_entry(p);
	}
}

/* 
 * Find files to add to the active list. Maybe it should use ftw(3) or fts(3)?
 * But implementing this takes less time then reading their man pages. Much of
 * the functionality of this routine is not used and probably could be removed.
 */
int
populate_active(const char *path, unsigned int mode)
{
	struct stat sb;
	struct dirent *dp;
	struct entry *ep;
	DIR *dirp;
	static char buf[PATH_MAX];
	size_t s;

	dirp = NULL;

	if ((s = strlcpy(buf, path, PATH_MAX)) >= PATH_MAX) {
		goto longer;
	}
	if (path[(s = strlen(path)) - 1] != '/') {
		buf[s] = '/';
		buf[s + 1] = '\0';
	}
	if ((mode & AFLG_CLR) == AFLG_CLR)
		clear_active();
	if ((dirp = opendir(path)) == NULL) {
		stag_warn("opendir: %s", path);
		return 1;
	}

	errno = 0;
	while ((dp = readdir(dirp)) != NULL) {
		if (((mode & AFLG_HID) != AFLG_HID && dp->d_name[0] == '.') ||
		    strcmp(dp->d_name, ".") == 0 || 
		    strcmp(dp->d_name, "..") == 0)
			continue;
		if (strlcat(buf, dp->d_name, PATH_MAX) >= PATH_MAX)
			goto longer;
		if (stat(buf, &sb) == -1)  {
			stag_warn("stat: %s", buf);
			goto er;
		}

		if ((S_ISDIR(sb.st_mode) || S_ISLNK(sb.st_mode)) && 
		    (mode & AFLG_REC) == AFLG_REC) {
			if (populate_active(buf, mode) == 1)
				goto er;
		} else if (is_valid_extension(dp->d_name, mode)) {
			if ((ep = make_entry(buf)) == NULL) 
				stag_warnx("skipping %s...", buf); 
			else
				LIST_INSERT_HEAD(&active, ep, entries);
		}

		buf[buf[s] == '/' ? s + 1 : s] = '\0';
	}
	if (errno) {
		stag_warn("readdir");
		goto er;
	}

	(void)closedir(dirp);
	return 0;

longer:
	stag_warnx("PATH_MAX of %d violated", PATH_MAX);
er:
	(void)closedir(dirp);
	return 1;
}

void
set_title(struct entry *p, const char *s)
{
	taglib_tag_set_title(p->tagp, s);
	p->mod = 1;
}

void set_artist(struct entry *p, const char *s)
{
	taglib_tag_set_artist(p->tagp, s);
	p->mod = 1;
}

void set_album(struct entry *p, const char *s)
{
	taglib_tag_set_album(p->tagp, s);
	p->mod = 1;
}

void set_comment(struct entry *p, const char *s)
{
	taglib_tag_set_comment(p->tagp, s);
	p->mod = 1;
}

void set_genre(struct entry *p, const char *s)
{
	taglib_tag_set_genre(p->tagp, s);
	p->mod = 1;
}

void set_year(struct entry *p, unsigned int c)
{
	taglib_tag_set_year(p->tagp, c);
	p->mod = 1;
}

void set_track(struct entry *p, unsigned int c)
{
	taglib_tag_set_track(p->tagp, c);
	p->mod = 1;
}

void
write_entry(struct entry *p)
{
	(void)taglib_file_save(p->filep);
	p->mod = 0;
}

void
revert_entry(struct entry *p)
{
	static char buf[PATH_MAX];
	struct entry *np;

	if (strlcpy(buf, p->dir, PATH_MAX) >= PATH_MAX ||
	    strlcat(buf, p->name, PATH_MAX) >= PATH_MAX) {
		stag_warnx("PATH_MAX of %d violated", PATH_MAX);
		return;
	}
	if ((np = make_entry(buf)) == NULL)
		return;
	LIST_REPLACE(p , np, entries);
	free_entry(p);
}


void remove_entry(struct entry *p)
{
	LIST_REMOVE(p, entries);
	free_entry(p);
}

/* 
 * It's pretty lame that the *marked* functions all have this boilerplate.
 * Using macros or function pointers isn't  worth it.
 */

/* 
 *  Write marked files to the disk.
 */
void
write_marked(void)
{
	struct entry *p;

	LIST_FOREACH(p, &active, entries) {
		if (p->mark) {
			/* 
			 * XXX I don't really know what the return value means.
			 * It shouldn't matter.
			 */
			(void)taglib_file_save(p->filep);
			p->mod = 0;
		}
	}
}

/* 
 * Update marked entries from disk.
 */
void
revert_marked(void)
{
	struct entry *p, *np;
	static char buf[PATH_MAX];

	LIST_FOREACH(p, &active, entries) {
		if (p->mark) {
			if (strlcpy(buf, p->dir, PATH_MAX) >= PATH_MAX ||
			    strlcat(buf, p->name, PATH_MAX) >= PATH_MAX) {
				stag_warnx("PATH_MAX of %d violated", PATH_MAX);
				continue;
			}
			if ((np = make_entry(buf)) == NULL)
				continue;
			LIST_REPLACE(p , np, entries);
			free_entry(p);
			np->mark = 1;
		}
	}
}

/* 
 * Remove marked entries from the active list.
 */
void
remove_marked(void) 
{
	struct entry *p;

	LIST_FOREACH(p, &active, entries) {
		if (p->mark) {
			LIST_REMOVE(p, entries);
			free_entry(p);
		}
	}
}

/* 
 * Set title of all marked entries in the active list.
 */
void
set_marked_title(const char *s)
{
	struct entry *p;

	LIST_FOREACH(p, &active, entries) {
		if (p->mark)
			set_title(p, s);
	}
}

/* 
 * Set artist of all marked entries in the active list.
 */
void
set_marked_artist(const char *s)
{
	struct entry *p;

	LIST_FOREACH(p, &active, entries) {
		if (p->mark)
			set_artist(p, s);
	}
}

/* 
 * Set album of all marked entries in the active list.
 */
void
set_marked_album(const char *s)
{
	struct entry *p;

	LIST_FOREACH(p, &active, entries) {
		if (p->mark)
			set_album(p, s);
	}
}

/* 
 * Set comment of all marked entries in the active list.
 */
void
set_marked_comment(const char *s)
{
	struct entry *p;

	LIST_FOREACH(p, &active, entries) {
		if (p->mark)
			set_comment(p, s);
	}

}


/* 
 * Set genre of all marked entries in the active list.
 */
void
set_marked_genre(const char *s)
{
	struct entry *p;
	
	LIST_FOREACH(p, &active, entries) {
		if (p->mark)
			set_genre(p, s);
	}
}


/* 
 * Set year of all marked entries in the active list.
 */
void
set_marked_year(unsigned int c)
{
	struct entry *p;

	LIST_FOREACH(p, &active, entries) {
		if (p->mark)
			set_year(p, c);
	}
}


/* 
 * Set track numbre of all marked entries in the active list.
 */
void
set_marked_track(unsigned int c)
{
	struct entry *p;

	LIST_FOREACH(p, &active, entries) {
		if (p->mark)
			set_track(p, c);
	}
}
