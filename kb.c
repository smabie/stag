/* kb.c */
/* 
 * This file is public domain as declared by Sturm Mabie
 */

#include "stag.h"

void
kb_add()
{
        ITEM *item, **items;
        size_t len;

        if ((len = strlen(cwd)) >= PATH_MAX)
                goto err;

        cwd[len] = '/';
        cwd[len + 1] = '\0';

        item = current_item((const MENU *)dir.menu);

        if (strlcat(cwd, item_name(item), PATH_MAX) >= PATH_MAX)
                goto err;

        if (populate_active(cwd, AFLG_REC) == 1)
                exit(1);

        cwd[len] = '\0';

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
        
        return;

err:
        stag_warnx("PATH_MAX of %d violated", PATH_MAX);
        cwd[len] = '\0';
}

void
kb_enter(int hidden)
{
        ITEM *item, **items;
        size_t len;
        char *s;

        item = current_item((const MENU *)dir.menu);
			
        if (strcmp(item_name(item), "..") == 0) {
                if ((s = strrchr(cwd, '/')) == cwd)
                        cwd[1] = '\0';
                else
                        *s = '\0';
        } else if (strcmp(item_name(item), ".") != 0) {
                if ((len = strlen(cwd)) >= PATH_MAX)
                        goto err;
                cwd[len] = '/';
                cwd[len + 1] = '\0';
				
                if (strlcat(cwd, item_name(item), PATH_MAX)
                    >= PATH_MAX)
                        goto err;
        }

        unpost_menu(dir.menu);
        items = menu_items(dir.menu);
        set_menu_items(dir.menu, 
                       path_make_items(cwd, hidden));
        free_item_strings(items);
        free_items(items);
        free(items);
        post_menu(dir.menu);

        return;
err:
        stag_warnx("PATH_MAX of %d violated", PATH_MAX);
        cwd[len] = '\0';
}

void
kb_hide(int *hidden)
{
        ITEM **items;

        unpost_menu(dir.menu);
        items = menu_items(dir.menu);
        set_menu_items(dir.menu, 
                       path_make_items(cwd, *hidden = !*hidden));
        free_item_strings(items);
        free_items(items);
        free(items);
        post_menu(dir.menu);
}

void
kb_file_mode()
{
        if (item_count(file.menu) > 0)
                state = FILE_MODE;
}

void
kb_toggle()
{
        ENTRY(file.menu)->mark = !ENTRY(file.menu)->mark;
        menu_driver(file.menu, REQ_TOGGLE_ITEM);
}

void
kb_edit(int *many)
{
        set_menu_items(info.menu, 
                       info_make_items(ENTRY(file.menu), 0));
        post_menu(info.menu);

        *many = 0;
        state = INFO_MODE;
}

void
kb_regex1(int *regexp)
{
        *regexp = 1;
        post_form(edit.form);
        wrefresh(edit.win);
        state = EDIT_MODE;
}

void
kb_regex2(regex_t *reg, char *s)
{	
        int c;

        if (regcomp(reg, s, REG_EXTENDED | REG_ICASE | REG_NOSUB) != 0)
                return;
        menu_driver(file.menu, REQ_FIRST_ITEM);

        for (c = 0; c < item_count(file.menu); c++, 
                     menu_driver(file.menu, REQ_DOWN_ITEM)) {
                if (regexec(reg, make_regex_str(ENTRY(file.menu)->tagp), 
                            0, NULL, 0) == 0) {
                        ENTRY(file.menu)->mark = !ENTRY(file.menu)->mark;
                        menu_driver(file.menu, REQ_TOGGLE_ITEM);
                }
        }
        menu_driver(file.menu, REQ_FIRST_ITEM);        
}

void
kb_toggle_all()
{
        int c;
        int idx;

        idx = item_index(current_item((const MENU *)file.menu));
        menu_driver(file.menu, REQ_FIRST_ITEM);
        for (c = 0; c < item_count(file.menu); c++, 
                     menu_driver(file.menu, REQ_DOWN_ITEM)) {
                ENTRY(file.menu)->mark = !ENTRY(file.menu)->mark;
                menu_driver(file.menu, REQ_TOGGLE_ITEM);
        }
        nth_item(file.menu, idx);
}

void
kb_clear()
{
        ITEM **items;

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
}

void
kb_remove()
{
        ITEM **items;
        char *s;
        int idx, c;

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
        for (c = 0; c < item_count(file.menu) && 
                     s != ENTRY(file.menu)->name; c++) {
                menu_driver(file.menu, REQ_DOWN_ITEM);
        }
        /* The selected item was one of the items deleted. */
        if (c == item_count(file.menu))
                nth_item(file.menu, idx);
        wrefresh(file.win);
}

void
kb_multi_edit(int *many)
{
        struct entry *p;
        int flag;
        
        flag = 1;
        LIST_FOREACH(p, &active, entries) {
                if (p->mark)
                        flag = 0;
        }
        if (flag)
                return;
        set_menu_items(info.menu, info_make_items(ENTRY(file.menu), 1));
        post_menu(info.menu);
        *many = 1;
        state = INFO_MODE;
}

void
kb_move(int c)
{
        menu_driver(file.menu, c);
        draw_info(ENTRY(file.menu), info.win);
        wrefresh(info.win);
}

void
kb_reload()
{
        revert_marked();
        draw_info(ENTRY(file.menu), info.win);
        wrefresh(info.win);
}

void
kb_write()
{
        write_entry(ENTRY(file.menu));
        draw_info(ENTRY(file.menu), info.win);
        wrefresh(info.win);
}

void
kb_unmark()
{
        int idx, c;

        idx = item_index(current_item((const MENU *)file.menu));

        menu_driver(file.menu, REQ_FIRST_ITEM);
        for (c = 0; c < item_count(file.menu); c++, 
                     menu_driver(file.menu, REQ_DOWN_ITEM)) {
                if (ENTRY(file.menu)->mark) {
                        menu_driver(file.menu, REQ_TOGGLE_ITEM);
                        ENTRY(file.menu)->mark = 0;
                }
        }
        nth_item(file.menu, idx);
}

void
kb_write_marked()
{
        write_marked();
        draw_info(ENTRY(file.menu), info.win);
        wrefresh(info.win);
}


void
kb_edit_field()
{
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
}

void
kb_other()
{
	unpost_menu(info.menu);
	free_items(menu_items(info.menu));
	draw_info(ENTRY(file.menu), info.win);

	wrefresh(info.win);
	state = FILE_MODE;
}

void
kb_left()
{
	unpost_menu(info.menu);
	free_items(menu_items(info.menu));
	draw_info(ENTRY(file.menu), info.win);

	wrefresh(info.win);
	state = DIR_MODE;
}
