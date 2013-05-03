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
