/*
 * Yabar - A modern and lightweight status bar for X window managers.
 *
 * Copyright (c) 2016, George Badawi
 * See LICENSE for more information.
 *
 */

#include "yabar.h"

enum {
	NET_WM_WINDOW_TYPE,
	NET_WM_WINDOW_TYPE_DOCK,
	NET_WM_DESKTOP,
	NET_WM_STRUT_PARTIAL,
	NET_WM_STRUT,
	NET_WM_STATE,
	NET_WM_STATE_STICKY,
	NET_WM_STATE_ABOVE,
};

/*
 *Setup correct properties for a bar's window.
 */
static void ya_setup_ewmh(ya_bar_t *bar) {
	// I really hope I understood this correctly from lemonbar code :|
	const char *atom_names[] = {
		"_NET_WM_WINDOW_TYPE",
		"_NET_WM_WINDOW_TYPE_DOCK",
		"_NET_WM_DESKTOP",
		"_NET_WM_STRUT_PARTIAL",
		"_NET_WM_STRUT",
		"_NET_WM_STATE",
		"_NET_WM_STATE_STICKY",
		"_NET_WM_STATE_ABOVE",
	};
	const int atoms = sizeof(atom_names)/sizeof(char *);
	xcb_intern_atom_cookie_t atom_cookie[atoms];
	xcb_atom_t atom_list[atoms];
	xcb_intern_atom_reply_t *atom_reply;
	for (int i = 0; i < atoms; i++)
		atom_cookie[i] = xcb_intern_atom(ya.c, 0, strlen(atom_names[i]), atom_names[i]);

	for (int i = 0; i < atoms; i++) {
		atom_reply = xcb_intern_atom_reply(ya.c, atom_cookie[i], NULL);
		if (!atom_reply)
			return;
		atom_list[i] = atom_reply->atom;
		free(atom_reply);
	}

	int strut[12] = {0};

	if (bar->position == YA_TOP) {
		strut[2] = bar->vgap + bar->height;
		strut[8] = bar->x;
		strut[9] = bar->x + bar->width - 1;
	}
	else if (bar->position == YA_BOTTOM) {
		strut[3] = bar->vgap + bar->height;
		strut[10] = bar->x;
		strut[11] = bar->x + bar->width - 1;
	}
	else {
	//TODO right and left bars if implemented.
	}

	xcb_change_property(ya.c, XCB_PROP_MODE_REPLACE, bar->win, atom_list[NET_WM_WINDOW_TYPE], XCB_ATOM_ATOM, 32, 1, &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
	xcb_change_property(ya.c, XCB_PROP_MODE_APPEND,  bar->win, atom_list[NET_WM_STATE], XCB_ATOM_ATOM, 32, 2, &atom_list[NET_WM_STATE_STICKY]);
	xcb_change_property(ya.c, XCB_PROP_MODE_REPLACE, bar->win, atom_list[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1, (const uint32_t []){ -1 } );
	xcb_change_property(ya.c, XCB_PROP_MODE_REPLACE, bar->win, atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32, 12, strut);
	xcb_change_property(ya.c, XCB_PROP_MODE_REPLACE, bar->win, atom_list[NET_WM_STRUT], XCB_ATOM_CARDINAL, 32, 4, strut);
	xcb_change_property(ya.c, XCB_PROP_MODE_REPLACE, bar->win, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen("yabar"), "yabar");

#ifdef YA_BSPWM
	// set bspwm windows to be above the bar
	xcb_query_tree_reply_t *qtree = xcb_query_tree_reply(ya.c, xcb_query_tree(ya.c, ya.scr->root), NULL);
	if(qtree == NULL)
		fprintf(stderr, "Failed to query window tree");
	xcb_window_t *wins = xcb_query_tree_children(qtree);

	for(int i = 0; i < xcb_query_tree_children_length(qtree); i++) {
		xcb_window_t found_win = wins[i];
		// apparently class cannot be a pointer
		xcb_icccm_get_wm_class_reply_t class;
		if(xcb_icccm_get_wm_class_reply(ya.c, xcb_icccm_get_wm_class(ya.c, found_win), &class, NULL)) {
			if(!strcmp("Bspwm", class.class_name) && !strcmp("root", class.instance_name)) {
				uint32_t stack_mask[2] = {found_win, XCB_STACK_MODE_ABOVE};
				xcb_configure_window(ya.c, bar->win, XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE, stack_mask);
			}
		}
	}

	free(qtree);
#endif
}

/*
 * Setup X variables(pixmap and graphical context) and the position of a block
 */
void ya_create_block(ya_block_t *blk) {
	ya_block_t *tmpblk;
	if (blk->bar->curblk[blk->align]) {
		blk->bar->curblk[blk->align]->next_blk = blk;
		blk->prev_blk = blk->bar->curblk[blk->align];
	}
	switch (blk->align) {
		case A_LEFT:
			blk->shift = blk->bar->occupied_width[A_LEFT];
			blk->bar->occupied_width[A_LEFT] += blk->width + blk->bar->slack;
			break;
		case A_CENTER:
			blk->bar->occupied_width[A_CENTER] += blk->width;
			tmpblk = blk->bar->curblk[A_CENTER];
			if(tmpblk) {
				for(;tmpblk->prev_blk; tmpblk = tmpblk->prev_blk);
				tmpblk->shift = (blk->bar->width - blk->bar->occupied_width[A_CENTER])/2;
				for(tmpblk = tmpblk->next_blk; tmpblk; tmpblk = tmpblk->next_blk) {
					tmpblk->shift = tmpblk->prev_blk->shift + tmpblk->prev_blk->width + blk->bar->slack;
				}
			}
			else {
				blk->shift = (blk->bar->width - blk->bar->occupied_width[A_CENTER])/2;
			}
			break;
		case A_RIGHT:
			tmpblk = blk->bar->curblk[A_RIGHT];
			blk->bar->occupied_width[A_RIGHT] += (blk->width + blk->bar->slack);
			blk->shift = blk->bar->width - blk->width;
			if(tmpblk) {
				for(; tmpblk; tmpblk = tmpblk->prev_blk) {
					tmpblk->shift -= (blk->width + blk->bar->slack);
				}

			}
			break;
	}
	blk->bar->curblk[blk->align] = blk;
	blk->pixmap = xcb_generate_id(ya.c);
	int blk_width = 0;
	//If variable width attribute is enabled, use the maximum available width for a block
	//which is the monitor width to create a pixmap.
		blk_width = blk->bar->mon->pos.width;
	xcb_create_pixmap(ya.c,
			ya.depth,
			blk->pixmap,
			blk->bar->win, blk_width, blk->bar->height);
	blk->gc = xcb_generate_id(ya.c);
	xcb_create_gc(ya.c, blk->gc, blk->pixmap, XCB_GC_FOREGROUND, (const uint32_t[]){blk->bgcolor});
	ya_inherit_bar_bgcol(blk);
}

/*
 *Create the bar's window using xcb
 */
void ya_create_bar(ya_bar_t * bar) {
	bar->win = xcb_generate_id(ya.c);
	bar->x=0, bar->y=0;
	if ((ya.gen_flag & GEN_RANDR))
		bar->x = bar->hgap + bar->mon->pos.x - bar->brsize;
	else
		bar->x = bar->hgap - bar->brsize;
	switch(bar->position){
		case YA_TOP:{
			if ((ya.gen_flag & GEN_RANDR))
				bar->y = bar->vgap + bar->mon->pos.y;
			else
				bar->y = bar->vgap;
			break;
		}
		case YA_BOTTOM: {
			if ((ya.gen_flag & GEN_RANDR))
				bar->y = bar->mon->pos.height - bar->vgap - bar->height - 2*bar->brsize;
			else
				bar->y = ya.scr->height_in_pixels - bar->vgap - bar->height - 2*bar->brsize;
			break;
		}
	}
	uint32_t w_mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
	uint32_t w_val[] = {bar->bgcolor, bar->brcolor, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_BUTTON_PRESS, ya.colormap};
	xcb_create_window(ya.c,
			ya.depth,
			bar->win,
			ya.scr->root,
			bar->x,bar->y,
			bar->width, bar->height,
			bar->brsize,
			XCB_WINDOW_CLASS_INPUT_OUTPUT,
			ya.visualtype->visual_id,
			w_mask,
			w_val);
	bar->gc = xcb_generate_id(ya.c);
	xcb_create_gc(ya.c, bar->gc, bar->win, XCB_GC_FOREGROUND, (const uint32_t[]){bar->bgcolor});
	ya_draw_bar_curwin(bar);
	ya_setup_ewmh(bar);
}

/*
 * Get the first visualtype that uses the depth ya.depth, starts as 32 and may fallback to 24
 * if no depth=32
 */
xcb_visualtype_t * ya_get_visualtype() {
	xcb_depth_iterator_t depth_iter;
	depth_iter = xcb_screen_allowed_depths_iterator (ya.scr);
	xcb_visualtype_iterator_t visual_iter;
	for (; depth_iter.rem; xcb_depth_next (&depth_iter)) {
		if(depth_iter.data->depth == ya.depth) {
			visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
			return visual_iter.data;
		}
	}
	return NULL;
}
/*
 * Parse color(background, foreground, underline and overline)
 * from text buffer and then relocate start of designated text to strbuf member.
 */
void ya_buf_color_parse(ya_block_t *blk) {
	char *cur = blk->buf;
	char *end;
	int offset = 0;
	// reset colors
	blk->bgcolor = blk->bgcolor_old;
	blk->fgcolor = blk->fgcolor_old;
	blk->ulcolor = blk->ulcolor_old;
	blk->olcolor = blk->olcolor_old;
	blk->attr &= ~BLKA_DIRTY_COL;
	if(cur[0] != '!' && cur[1] != 'Y') {
		blk->strbuf = cur;
		//pthread_mutex_lock(&blk->mutex);
		xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){blk->bgcolor});
		//pthread_mutex_unlock(&blk->mutex);
		return;
	}
	cur+=2;
	offset+=2;
	for(; *cur != 'Y'; cur++) {
		if((cur[0]=='B' && cur[1]=='G') || (cur[0]=='b' && cur[1]=='g')) {
			cur+=2;
			offset+=2;
			blk->bgcolor = strtoul(cur, &end, 16);
			blk->attr |= BLKA_DIRTY_COL;
			offset+=(end-cur);
		}
		if((cur[0]=='F' && cur[1]=='G') || (cur[0]=='f' && cur[1]=='g')) {
			cur+=2;
			offset+=2;
			blk->fgcolor = strtoul(cur, &end, 16);
			offset+=(end-cur);
		}
		else if ((cur[0]=='U') || (cur[0]=='u')) {
			cur++;
			offset++;
			blk->ulcolor = strtoul(cur, &end, 16);
			offset+=(end-cur);
		}
		else if ((cur[0]=='O') || (cur[0]=='o')) {
			cur++;
			offset++;
			blk->olcolor = strtoul(cur, &end, 16);
			offset+=(end-cur);
		}
		else if (*cur == ' '){
			offset++;
		}
	}
	if(cur[0]=='Y' && cur[1]=='!') {
		blk->strbuf =  blk->buf+offset+2;
	}
	xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){blk->bgcolor});
}

#ifdef YA_INTERNAL_EWMH
/*
 * Get current window title using EWMH
 */
inline void ya_get_cur_window_title(ya_block_t * blk) {
	xcb_ewmh_get_utf8_strings_reply_t reply;
	if(ya.curwin == XCB_NONE) {
		blk->buf[0] = '\0';
	}
	else {
		// Note that this is incomplete. Windows /should/ set
		// _NET_WM_NAME according to the ewmh docs but not all
		// do. ewmh bindings don't provide clean bindings to
		// WM_NAME.
		xcb_get_property_cookie_t wm_ck, wmvis_ck;
		wm_ck = xcb_ewmh_get_wm_name(ya.ewmh, ya.curwin);
		wmvis_ck = xcb_ewmh_get_wm_visible_name(ya.ewmh, ya.curwin);
                if(xcb_ewmh_get_wm_name_reply(ya.ewmh, wm_ck, &reply, NULL) == 1 || xcb_ewmh_get_wm_visible_name_reply(ya.ewmh, wmvis_ck, &reply, NULL) == 1) {
			int len = GET_MIN(blk->bufsize - 1, reply.strings_len);
			strncpy(blk->buf, reply.strings, len);
			blk->buf[len]='\0';
                	xcb_ewmh_get_utf8_strings_reply_wipe(&reply);
                } else {
                	blk->buf[0]='\0';
                }
	}
}
#endif //YA_INTERNAL_EWMH

#ifdef YA_ICON
//This function is obtained from Awesome WM code.
//I actually did not know how to copy pixbuf to surface.
static cairo_surface_t * ya_draw_surface_from_pixbuf(GdkPixbuf *buf) {
    int width = gdk_pixbuf_get_width(buf);
    int height = gdk_pixbuf_get_height(buf);
    int pix_stride = gdk_pixbuf_get_rowstride(buf);
    guchar *pixels = gdk_pixbuf_get_pixels(buf);
    int channels = gdk_pixbuf_get_n_channels(buf);
    cairo_surface_t *surface;
    int cairo_stride;
    unsigned char *cairo_pixels;


    cairo_format_t format = CAIRO_FORMAT_ARGB32;
    if (channels == 3)
        format = CAIRO_FORMAT_RGB24;

    surface = cairo_image_surface_create(format, width, height);
    cairo_surface_flush(surface);
    cairo_stride = cairo_image_surface_get_stride(surface);
    cairo_pixels = cairo_image_surface_get_data(surface);

    for (int y = 0; y < height; y++) {
        guchar *row = pixels;
        uint32_t *cairo = (uint32_t *) cairo_pixels;
        for (int x = 0; x < width; x++) {
            if (channels == 3) {
                uint8_t r = *row++;
                uint8_t g = *row++;
                uint8_t b = *row++;
                *cairo++ = (r << 16) | (g << 8) | b;
            }
            else {
                uint8_t r = *row++;
                uint8_t g = *row++;
                uint8_t b = *row++;
                uint8_t a = *row++;
                double alpha = a / 255.0;
                r = r * alpha;
                g = g * alpha;
                b = b * alpha;
                *cairo++ = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
        pixels += pix_stride;
        cairo_pixels += cairo_stride;
    }

    cairo_surface_mark_dirty(surface);
    return surface;
}


/*
 * Open the image/icon file and then copy its data to a temporary surface, this function is called from
 * ya_draw_pango_text()
 */
cairo_surface_t * ya_draw_graphics(ya_block_t *blk) {
	GError *gerr =NULL;
	cairo_surface_t *ret = NULL;
	GdkPixbuf *gbuf = gdk_pixbuf_new_from_file(blk->img->path, &gerr);
	if(gbuf == NULL) {
		fprintf(stderr, "Cannot allocate pixbuf for block (%s.%s)\n. %s\n", blk->bar->name, blk->name, gerr->message);
		return NULL;
	}
	ret = ya_draw_surface_from_pixbuf(gbuf);
	g_object_unref(gbuf);
	return ret;
}
#endif //YA_ICON

inline void ya_inherit_bar_bgcol(ya_block_t *blk) {
	//if (ya.curwin == ya.lstwin)
	//	return;
	//If there is no current window, set color to bgcolor_none
	//Otherwise set it to the normal bar background color
	uint32_t col = 0x0;
	if(ya.curwin == XCB_NONE)
		col = blk->bar->bgcolor_none;
	else
		col = blk->bar->bgcolor;
	if(!(blk->attr & BLKA_BGCOLOR)) {
		if((!(blk->attr & BLKA_DIRTY_COL))) {
			blk->bgcolor = col;
			blk->bgcolor_old = col;
			xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){col});
			fprintf(stderr, "INH %s\n", blk->name);
		}
		else {
			blk->bgcolor_old = col;
		}
	}
}

inline void ya_draw_bar_curwin(ya_bar_t *bar) {
	uint32_t col = 0x0;
	//If there is no current window, set color to bgcolor_none
	//Otherwise set it to the normal bar background color
	if(ya.curwin == XCB_NONE)
		col = bar->bgcolor_none;
	else
		col = bar->bgcolor;
	xcb_change_gc(ya.c, bar->gc, XCB_GC_FOREGROUND, (const uint32_t[]){col});
	xcb_poly_fill_rectangle(ya.c, bar->win,
			bar->gc, 1,
			(const xcb_rectangle_t[]) { {0,0,bar->width, bar->height} });
}

/*
 * Redraw the entire bar
 */
void ya_redraw_bar(ya_bar_t *bar) {
	ya_draw_bar_curwin(bar);

	for(int i=0; i<3;i++) {
		for(ya_block_t *blk = bar->curblk[i]; blk; blk = blk->next_blk) {
		    //if(i!=A_RIGHT) {
				ya_draw_text_var_width(blk);
		//		break;
			}
			//else
				//ya_draw_pango_text(blk);
		//}
	}

}

/*
 * Compute the maximum width for text as if there were no width
 * restriction
 */
static inline void ya_get_text_max_width(ya_block_t *blk) {
	cairo_surface_t *surface = cairo_xcb_surface_create(ya.c,
			blk->pixmap, ya.visualtype,
			blk->width, blk->bar->height);
	cairo_t *cr = cairo_create(surface);
	PangoContext *context = pango_cairo_create_context(cr);
	PangoLayout *layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, blk->bar->desc);

	//Start drawing text at strbuf member, where !Y COLORS Y! config ends
	if (!(blk->attr & BLKA_MARKUP_PANGO))
		pango_layout_set_text(layout, blk->strbuf, strlen(blk->strbuf));
	else
		pango_layout_set_markup(layout, blk->strbuf, strlen(blk->strbuf));
	//pango_layout_set_alignment(layout, blk->justify);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	pango_layout_set_auto_dir(layout, false);

	//pango_layout_set_height(layout, blk->bar->height);
	int ht;
	//Get the width that text can be drawn comfortably and put it in curwidth member.
	pango_layout_get_pixel_size(layout, &blk->curwidth, &ht);
	fprintf(stderr, "blk %s text_max_width is %d\n", blk->name, blk->width);
	cairo_surface_flush(surface);
	xcb_flush(ya.c);

	g_object_unref(layout);
	g_object_unref(context);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

/*
 * Redraw the bar for the specific area to be redrawn.
 */
static inline void ya_draw_bar_var(ya_block_t *blk) {
	int start =0, end=0;
	int scrw = blk->bar->mon->pos.width;
	switch(blk->align) {
		case A_LEFT:
			start = blk->shift;
			if(blk->bar->curblk[1]) {
				end = (scrw - blk->bar->occupied_width[1])/2;
			}
			else if (blk->bar->curblk[2]) {
				end = scrw - blk->bar->occupied_width[2];
			}
			else {
				end = scrw;
			}
			break;
		case A_CENTER:
			start = (scrw - blk->bar->occupied_width[1])/2;
			end = (scrw + blk->bar->occupied_width[1])/2;
			break;
		case A_RIGHT:
			if(blk->bar->curblk[1]) {
				start = (scrw + blk->bar->occupied_width[1])/2;
			}
			else if (blk->bar->curblk[0]) {
				start = blk->bar->occupied_width[0];
			}
			else
				start = 0;
			end = blk->shift + blk->width;
			break;
	}
	xcb_poly_fill_rectangle(ya.c, blk->bar->win,
			blk->bar->gc, 1,
			(const xcb_rectangle_t[]) { {start,0, end-start, blk->bar->height} });
	//fprintf(stderr, "WIDTH DRWN=%d\n", end-start);
}
/*
 *Calculate the max allowed width for the variable-sized block that does not minimize
 *area from the neighboring alignment.
 */
static inline void ya_set_width_resetup(ya_block_t *blk) {
	int width_init = blk->curwidth;
	int scrw = blk->bar->mon->pos.width;
	//maxw is used to deduce the maximum allowed width for the block without minimizing
	//the area for all other blocks
	int maxw = scrw;
	//othw is the total width for all blocks but blk in its alignment
	int othw = blk->bar->occupied_width[blk->align] - blk->width;
	switch(blk->align) {
		case A_LEFT:
			if(blk->bar->curblk[1]) {
				maxw = (scrw - blk->bar->occupied_width[1])/2 -othw;
				if (blk->curwidth > maxw)
					blk->curwidth = maxw;
			}
			else if (blk->bar->curblk[2]) {
				maxw = scrw - blk->bar->occupied_width[2] -othw;
			}
			break;
		case A_CENTER:
			if(blk->bar->curblk[0] || blk->bar->curblk[2]) {
				int maxsw = GET_MAX(blk->bar->occupied_width[0], blk->bar->occupied_width[2]);
				maxw = scrw -2*maxsw -othw;
			}
			break;
		case A_RIGHT:
			if(blk->bar->curblk[1]) {
				maxw = (scrw - blk->bar->occupied_width[1])/2 - othw;
				if (blk->curwidth > maxw)
					blk->curwidth = maxw;
			}
			else if (blk->bar->curblk[0]) {
				maxw = scrw - blk->bar->occupied_width[0] - othw;
			}
			break;
	}
	blk->curwidth = GET_MIN(width_init, maxw);
	blk->bar->occupied_width[blk->align] = blk->curwidth + othw;

	//Now, we adjust the shift of every other block in the designated alignment to its new
	//value depending on the new width of the new width of our variable-width block.
	int delta = blk->curwidth - blk->width;
	switch(blk->align) {
		case A_LEFT:
			for(ya_block_t *curblk = blk->next_blk;curblk; curblk = curblk->next_blk) {
				curblk->shift += delta;
			}
			break;
		case A_CENTER: {
			ya_block_t *curblk = blk->bar->curblk[1];
			curblk->shift -= delta/2;
			for(curblk = curblk->next_blk;curblk;curblk=curblk->next_blk)
				curblk->shift = curblk->prev_blk->shift+curblk->prev_blk->width;
			}
			break;
		case A_RIGHT: {
			for(ya_block_t *curblk = blk;curblk; curblk = curblk->prev_blk)
				curblk->shift -= delta;
			}
			break;
	}
	blk->width = blk->curwidth;
}

/*
 * Equivalent to ya_draw_pango_text() but also redraw all
 * other necessary blocks in the designated alignment of the current block
 */
void ya_draw_text_var_width(ya_block_t * blk) {
	//pthread_mutex_lock(&blk->bar->mutex);
	ya_get_text_max_width(blk);
	if (blk->curwidth == blk->width) {
		ya_draw_pango_text(blk);
		return;
	}
	ya_draw_bar_var(blk);
	ya_set_width_resetup(blk);
	switch(blk->align) {
		case A_LEFT:
			for(ya_block_t *curblk = blk; curblk; curblk = curblk->next_blk) {
				ya_draw_pango_text(curblk);
				//fprintf(stderr, "BLKK= %s\n", curblk->name);
			}
			break;
		case A_CENTER:
			for(ya_block_t *curblk = blk->bar->curblk[A_CENTER]; curblk; curblk = curblk->next_blk) {
				ya_draw_pango_text(curblk);
			}
			break;
		case A_RIGHT:
			for(ya_block_t *curblk = blk; curblk; curblk = curblk->prev_blk) {
				ya_draw_pango_text(curblk);
			}
			break;
	}
	//pthread_mutex_unlock(&blk->bar->mutex);
}

/*
 * This filters out the underline and background attributes so we can separate them
 */
static gboolean filter(PangoAttribute *attribute, gpointer user_data) {
	switch(attribute->klass->type) {
		case PANGO_ATTR_UNDERLINE_COLOR:
		case PANGO_ATTR_UNDERLINE:
		case PANGO_ATTR_BACKGROUND:
#if (PANGO_VERSION_MAJOR == 1 && PANGO_VERSION_MINOR >= 38)
		case PANGO_ATTR_BACKGROUND_ALPHA:
#endif
			return true;
		default:
			return false;
	}
}
/*
 * Here is how the text buffer of a block is rendered on the screen
 */
void ya_draw_pango_text(struct ya_block *blk) {
	//fprintf(stderr, "001=%s\n", blk->name);
#ifdef YA_MUTEX
	pthread_mutex_lock(&blk->bar->mutex);
#endif
	if((blk->bar->attr & BARA_REDRAW)) {
		fprintf(stderr, "E: %s\n", blk->name);
		ya_inherit_bar_bgcol(blk);
	}
	//First override the block area with its background color in order to not draw the new text above the old one.
	xcb_poly_fill_rectangle(ya.c, blk->pixmap, blk->gc, 1, (const xcb_rectangle_t[]) { {0,0,blk->width, blk->bar->height} });
	cairo_surface_t *surface = cairo_xcb_surface_create(ya.c, blk->pixmap, ya.visualtype, blk->width, blk->bar->height);
	cairo_t *cr = cairo_create(surface);
#ifdef YA_ICON
	if((blk->attr & BLKA_ICON)) {
		cairo_surface_t *iconsrf = ya_draw_graphics(blk);
		if (iconsrf) {
			//Copy a newly created temporary surface that contains the scaled image to our surface and then
			//destroy that temporary surface and return to our normal cairo scale.
			cairo_scale(cr, blk->img->scale_w, blk->img->scale_h);
			cairo_set_source_surface(cr, iconsrf,
					(double)(blk->img->x)/(blk->img->scale_w),
					(double)(blk->img->y)/(blk->img->scale_h));
			cairo_paint(cr);
			cairo_surface_destroy(iconsrf);
			cairo_scale(cr, 1.0/(blk->img->scale_w), 1.0/(blk->img->scale_h));
		}
	}
#endif //YA_ICON
	PangoContext *context = pango_cairo_create_context(cr);
	PangoLayout *layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, blk->bar->desc);

	//fprintf(stderr, "010=%s\n", blk->name);
	//Start drawing text at strbuf member, where !Y COLORS Y! config ends.
	//strbuf member is initialized to buf member by default. So it will work
	//well with internal blocks.
	if (!(blk->attr & BLKA_MARKUP_PANGO))
		pango_layout_set_text(layout, blk->strbuf, strlen(blk->strbuf));
	else
		pango_layout_set_markup(layout, blk->strbuf, strlen(blk->strbuf));
	//fprintf(stderr, "020=%s\n", blk->name);
	pango_layout_set_alignment(layout, blk->justify);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	pango_layout_set_auto_dir(layout, false);

	pango_layout_set_height(layout, blk->bar->height);

	int wd, ht;
	pango_layout_get_pixel_size(layout, &wd, &ht);

	pango_layout_set_width(layout, blk->width * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	int offset = (blk->bar->height - ht)/2 + blk->bar->oosize;
	cairo_move_to(cr, 0, offset);

	//Draw overline if defined
	if(blk->attr & BLKA_OVERLINE) {
		cairo_set_source_rgba(cr,
			GET_RED(blk->olcolor),
			GET_GREEN(blk->olcolor),
			GET_BLUE(blk->olcolor),
			GET_ALPHA(blk->olcolor));
		cairo_rectangle(cr, 0, 0, blk->width, blk->bar->olsize);
		cairo_fill(cr);
	}
	//Draw underline if defined
	if(blk->attr & BLKA_UNDERLINE) {
		cairo_set_source_rgba(cr,
			GET_RED(blk->ulcolor),
			GET_GREEN(blk->ulcolor),
			GET_BLUE(blk->ulcolor),
			GET_ALPHA(blk->ulcolor));
		cairo_rectangle(cr, 0, blk->bar->height - blk->bar->ulsize, blk->width, blk->bar->ulsize);
		cairo_fill(cr);
	}

	//retrieve list of attributes; we will filter it to look for underline and background attrs
	//so we can draw it ourselves
	PangoAttrList *orig = pango_layout_get_attributes(layout), *list;
	//filtering results in a new list with only the filtered attributes (see filter function)
	if(orig != NULL && (list = pango_attr_list_filter(orig, filter, NULL)) != NULL) {
		PangoAttrIterator *it = pango_attr_list_get_iterator(list);
		do {
			PangoAttribute *attr;
			PangoRectangle rect1, rect2;
			gint start, end;
			//get the starting and ending byte position for the attribute, and their positional data
			//note that PangoRectangle units are in Pango units, so we have to divide by PANGO_SCALE
			pango_attr_iterator_range(it, &start, &end);
			pango_layout_index_to_pos(layout, start, &rect1);
			pango_layout_index_to_pos(layout, end, &rect2);

			if((attr = pango_attr_iterator_get(it, PANGO_ATTR_BACKGROUND)) != NULL) {
				PangoColor color = ((PangoAttrColor *)attr)->color;
				int alpha = G_MAXUINT16;
				//bgalpha is only available on Pango >= 1.38, use if specified
#if (PANGO_VERSION_MAJOR == 1 && PANGO_VERSION_MINOR >= 38)
				if((attr = pango_attr_iterator_get(it, PANGO_ATTR_BACKGROUND_ALPHA)) != NULL)
					alpha = ((PangoAttrInt *)attr)->value;
#endif
				cairo_set_source_rgba(cr,
					(double)color.red/G_MAXUINT16,
					(double)color.green/G_MAXUINT16,
					(double)color.blue/G_MAXUINT16,
					(double)alpha/G_MAXUINT16);
				//create the background rectangle using the starting and ending positions
				cairo_rectangle(cr, rect1.x/PANGO_SCALE, 0,
						rect2.x/PANGO_SCALE - rect1.x/PANGO_SCALE, blk->bar->height);
				cairo_fill(cr);
			}
			//make sure that the underline attribute value isn't 0 (there actually is underline)
			if((attr = pango_attr_iterator_get(it, PANGO_ATTR_UNDERLINE)) != NULL &&
					((PangoAttrInt *)attr)->value) {
				//if underline color is specified, set the source to be that color
				if((attr = pango_attr_iterator_get(it, PANGO_ATTR_UNDERLINE_COLOR)) != NULL)
					cairo_set_source_rgb(cr,
						(double)((PangoAttrColor *)attr)->color.red/G_MAXUINT16,
						(double)((PangoAttrColor *)attr)->color.green/G_MAXUINT16,
						(double)((PangoAttrColor *)attr)->color.blue/G_MAXUINT16);
				//if not, use foreground color
				else
					cairo_set_source_rgba(cr,
							GET_RED(blk->fgcolor),
							GET_GREEN(blk->fgcolor),
							GET_BLUE(blk->fgcolor),
							GET_ALPHA(blk->fgcolor));
				//create the underline rectangle using the starting and ending positions
				cairo_rectangle(cr, rect1.x/PANGO_SCALE, blk->bar->height - blk->bar->ulsize,
						rect2.x/PANGO_SCALE - rect1.x/PANGO_SCALE, blk->bar->ulsize);
				cairo_fill(cr);
			}
		} while(pango_attr_iterator_next(it));
		pango_attr_iterator_destroy(it);
		pango_attr_list_unref(list);
	}

	cairo_set_source_rgba(cr,
			GET_RED(blk->fgcolor),
			GET_GREEN(blk->fgcolor),
			GET_BLUE(blk->fgcolor),
			GET_ALPHA(blk->fgcolor));

	cairo_move_to(cr, 0, offset);
	pango_cairo_show_layout(cr, layout);

	cairo_surface_flush(surface);
	xcb_copy_area(ya.c, blk->pixmap, blk->bar->win, blk->gc, 0,0,blk->shift, 0, blk->width, blk->bar->height);
	xcb_flush(ya.c);
#ifdef YA_MUTEX
	pthread_mutex_unlock(&blk->bar->mutex);
#endif

	g_object_unref(layout);
	g_object_unref(context);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

