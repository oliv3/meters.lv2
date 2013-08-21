/* gtk scale widget
 *
 * Copyright (C) 2013 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _GTK_EXT_SCALE_H_
#define _GTK_EXT_SCALE_H_

#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <string.h>

/* default values used by gtkext_scale_new()
 * for calling gtkext_scale_new_with_size()
 */
#define GSC_LENGTH 250
#define GSC_GIRTH 18

typedef struct {
	GtkWidget* w;
	GtkWidget* c;

	float min;
	float max;
	float acc;
	float cur;

	float drag_x, drag_y, drag_c;
	gboolean sensitive;
	gboolean prelight;

	gboolean (*cb) (GtkWidget* w, gpointer handle);
	gpointer handle;

	cairo_pattern_t* dpat;
	cairo_pattern_t* fpat;
	cairo_surface_t* bg;

	float w_width, w_height;
	gboolean horiz;

	char **mark_txt;
	float *mark_val;
	int    mark_cnt;
	gboolean mark_expose;
	PangoFontDescription *mark_font;
	float c_txt[4];
	float mark_space;

	pthread_mutex_t _mutex;

} GtkExtScale;


static int gtkext_scale_round_length(GtkExtScale * d, float val) {
	if (d->horiz) {
		return rint((d->w_width - 8) *  (val - d->min) / (d->max - d->min));
	} else {
		return rint((d->w_height - 8) * (1.0 - (val - d->min) / (d->max - d->min)));
	}
}

static void gtkext_scale_set_rect(GtkExtScale * d, float val, GdkRectangle *rect) {
	val = gtkext_scale_round_length(d, val);
	if (d->horiz) {
		rect->x = 1 + val;
		rect->width = 8;
		rect->y = d->mark_space + 3;
		rect->height = d->w_height - 6 - d->mark_space;
	} else {
		rect->x = 5;
		rect->width = d->w_width - 5 - d->mark_space;
		rect->y = 1 + val;
		rect->height = 9;
	}
}

static void gtkext_scale_update_value(GtkExtScale * d, float val) {
	if (val < d->min) val = d->min;
	if (val > d->max) val = d->max;
	if (val != d->cur) {
		float oldval = d->cur;
		d->cur = val;
		if (d->cb) d->cb(d->w, d->handle);
		if (d->w->window &&
				gtkext_scale_round_length(d, oldval) != gtkext_scale_round_length(d, val)
				) {
#if 0
			GdkRectangle rect;
			gtkext_scale_set_rect(d, val, &rect);
			GdkRegion* region0 =  gdk_region_rectangle (&rect);
			gtkext_scale_set_rect(d, oldval, &rect);
			GdkRegion* region1 =  gdk_region_rectangle (&rect);
			gdk_region_union(region0, region1);
			gdk_window_invalidate_region (d->w->window, region0, TRUE);
			gdk_region_destroy(region1);
			gdk_region_destroy(region0);
#else
			val = gtkext_scale_round_length(d, val);
			oldval = gtkext_scale_round_length(d, oldval);
			GdkRectangle rect;
			if (oldval > val) {

				if (d->horiz) {
					rect.x = 1 + val;
					rect.width = 9 + oldval - val;
					rect.y = d->mark_space + 3;
					rect.height = d->w_height - 6 - d->mark_space;
				} else {
					rect.x = 5;
					rect.width = d->w_width - 5 - d->mark_space;
					rect.y = 1 + val;
					rect.height = 9 + oldval - val;
				}
			} else {
				if (d->horiz) {
					rect.x = 1 + oldval;
					rect.width = 9 + val - oldval;
					rect.y = d->mark_space + 3;
					rect.height = d->w_height - 6 - d->mark_space;
				} else {
					rect.x = 5;
					rect.width = d->w_width - 5 - d->mark_space;
					rect.y = 1 + oldval;
					rect.height = 9 + val - oldval;
				}
			}
			GdkRegion* region =  gdk_region_rectangle (&rect);
			gdk_window_invalidate_region (d->w->window, region, TRUE);
			gdk_region_destroy(region);
#endif
		}
	}
}

static gboolean gtkext_scale_mousedown(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	GtkExtScale * d = (GtkExtScale *)handle;
	if (!d->sensitive) { return FALSE; }
	d->drag_x = event->x;
	d->drag_y = event->y;
	d->drag_c = d->cur;
	gtk_widget_queue_draw(d->w);
	return TRUE;
}

static gboolean gtkext_scale_mouseup(GtkWidget *w, GdkEventButton *event, gpointer handle) {
	GtkExtScale * d = (GtkExtScale *)handle;
	if (!d->sensitive) { return FALSE; }
	d->drag_x = d->drag_y = -1;
	gtk_widget_queue_draw(d->w);
	return TRUE;
}

static gboolean gtkext_scale_mousemove(GtkWidget *w, GdkEventMotion *event, gpointer handle) {
	GtkExtScale * d = (GtkExtScale *)handle;
	if (d->drag_x < 0 || d->drag_y < 0) return FALSE;

	if (!d->sensitive) {
		d->drag_x = d->drag_y = -1;
		gtk_widget_queue_draw(d->w);
		return FALSE;
	}
	float len;
	float diff;
	if (d->horiz) {
		len = d->w_width - 8;
		diff = (event->x - d->drag_x) / len;
	} else {
		len = d->w_height - 8;
		diff = (d->drag_y - event->y) / len;
	}
#if 0
	diff = ((event->x - d->drag_x) - (event->y - d->drag_y)) / len;
#endif
	diff = rint(diff * (d->max - d->min) / d->acc ) * d->acc;
	float val = d->drag_c + diff;

	/* snap to mark */
	const int snc = gtkext_scale_round_length(d, val);
	// lock ?!
	for (int i = 0; i < d->mark_cnt; ++i) {
		int sn = gtkext_scale_round_length(d, d->mark_val[i]);
		if (abs(sn-snc) < 3) {
			val = d->mark_val[i];
			break;
		}
	}

	gtkext_scale_update_value(d, val);
	return TRUE;
}

static gboolean gtkext_scale_enter_notify(GtkWidget *w, GdkEvent *event, gpointer handle) {
	GtkExtScale * d = (GtkExtScale *)handle;
	if (!d->prelight) {
		d->prelight = TRUE;
		gtk_widget_queue_draw(d->w);
	}
	return FALSE;
}

static gboolean gtkext_scale_leave_notify(GtkWidget *w, GdkEvent *event, gpointer handle) {
	GtkExtScale * d = (GtkExtScale *)handle;
	if (d->prelight) {
		d->prelight = FALSE;
		gtk_widget_queue_draw(d->w);
	}
	return FALSE;
}

static void gtkext_scale_size_request(GtkWidget *w, GtkRequisition *r, gpointer handle) {
	GtkExtScale * d = (GtkExtScale *)handle;
	if (d->horiz) {
		r->height = GSC_GIRTH + (d->mark_cnt > 0 ? d->mark_space : 0);
		r->width = 250;
	} else {
		r->width = GSC_GIRTH + (d->mark_cnt > 0 ? d->mark_space : 0);
		r->height = 250;
	}
}

static void gtkext_scale_size_allocate(GtkWidget *w, GdkRectangle *r, gpointer handle) {
	GtkExtScale * d = (GtkExtScale *)handle;
	if (d->horiz) {
		d->w_width = r->width;
		d->w_height = GSC_GIRTH + (d->mark_cnt > 0 ? d->mark_space : 0);
		if (d->w_height > r->height) {
			d->w_height =r->height;
		}
	} else {
		d->w_height = r->height;
		d->w_width = GSC_GIRTH + (d->mark_cnt > 0 ? d->mark_space : 0);
		if (d->w_width > r->width) {
			d->w_width = r->width;
		}
	}
	gtk_widget_set_size_request(d->w, d->w_width, d->w_height);
	if (d->mark_cnt > 0) { d->mark_expose = TRUE; }
}

static gboolean gtkext_scale_scroll(GtkWidget *w, GdkEventScroll *ev, gpointer handle) {
	GtkExtScale * d = (GtkExtScale *)handle;
	if (!d->sensitive) { return FALSE; }

	if (!(d->drag_x < 0 || d->drag_y < 0)) {
		d->drag_x = d->drag_y = -1;
	}

	float val = d->cur;
	switch (ev->direction) {
		case GDK_SCROLL_RIGHT:
		case GDK_SCROLL_UP:
			val += d->acc;
			break;
		case GDK_SCROLL_LEFT:
		case GDK_SCROLL_DOWN:
			val -= d->acc;
			break;
		default:
			break;
	}
	gtkext_scale_update_value(d, val);
	return TRUE;
}

static void create_scale_pattern(GtkExtScale * d) {
	if (d->horiz) {
		d->dpat = cairo_pattern_create_linear (0.0, 0.0, 0.0, GSC_GIRTH);
	} else {
		d->dpat = cairo_pattern_create_linear (0.0, 0.0, GSC_GIRTH, 0);
	}
	cairo_pattern_add_color_stop_rgb (d->dpat, 0.0, .3, .3, .33);
	cairo_pattern_add_color_stop_rgb (d->dpat, 0.4, .5, .5, .55);
	cairo_pattern_add_color_stop_rgb (d->dpat, 1.0, .2, .2, .22);

	if (d->horiz) {
		d->fpat = cairo_pattern_create_linear (0.0, 0.0, 0.0, GSC_GIRTH);
	} else {
		d->fpat = cairo_pattern_create_linear (0.0, 0.0, GSC_GIRTH, 0);
	}
	cairo_pattern_add_color_stop_rgb (d->fpat, 0.0, .0, .0, .0);
	cairo_pattern_add_color_stop_rgb (d->fpat, 0.4,  1,  1,  1);
	cairo_pattern_add_color_stop_rgb (d->fpat, 1.0, .1, .1, .1);
}

#define SXX_W(minus) (d->w_width  + minus - ((d->bg && !d->horiz) ? d->mark_space : 0))
#define SXX_H(minus) (d->w_height + minus - ((d->bg &&  d->horiz) ? d->mark_space : 0))
#define SXX_T(plus)  (plus + ((d->bg && d->horiz) ? d->mark_space : 0))

static void gtkext_scale_render_metrics(GtkExtScale* d) {
	if (d->bg) {
		cairo_surface_destroy(d->bg);
	}
	d->bg = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, d->w_width, d->w_height);
	cairo_t *cr = cairo_create (d->bg);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba (cr, .0, .0, .0, 0);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba (cr, .7, .7, .7, 1.0);
	cairo_set_line_width (cr, 1.0);

	for (int i = 0; i < d->mark_cnt; ++i) {
		float v = 4.0 + gtkext_scale_round_length(d, d->mark_val[i]);
		if (d->horiz) {
			if (d->mark_txt[i]) {
				write_text_full(cr, d->mark_txt[i], d->mark_font, v, /* d->w_height - d->mark_space + 1 */ 1, -M_PI/2, 1, d->c_txt);
			}
			cairo_move_to(cr, v+.5, SXX_T(1.5));
			cairo_line_to(cr, v+.5, SXX_T(0) + SXX_H(-.5));
		} else {
			if (d->mark_txt[i]) {
				write_text_full(cr, d->mark_txt[i], d->mark_font, d->w_width -2, v, 0, 1, d->c_txt);
			}
			cairo_move_to(cr, 1.5, v+.5);
			cairo_line_to(cr, SXX_W(-.5) , v+.5);
		}
		cairo_stroke(cr);
	}
	cairo_destroy(cr);
}

static gboolean gtkext_scale_expose_event(GtkWidget *w, GdkEventExpose *ev, gpointer handle) {
	GtkExtScale * d = (GtkExtScale *)handle;
	cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(w->window));
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	GtkStyle *style = gtk_widget_get_style(w);
	GdkColor *c = &style->bg[GTK_STATE_NORMAL];
	cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
	cairo_rectangle (cr, 0, 0, d->w_width, d->w_height);
	cairo_fill(cr);

	if (d->mark_cnt > 0 && d->mark_expose) {
		pthread_mutex_lock (&d->_mutex);
		d->mark_expose = FALSE;
		gtkext_scale_render_metrics(d);
		pthread_mutex_unlock (&d->_mutex);
	}

	if (d->bg) {
		if (!d->sensitive) {
			cairo_set_operator (cr, CAIRO_OPERATOR_EXCLUSION);
		} else {
			cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		}
		cairo_set_source_surface(cr, d->bg, 0, 0);
		cairo_paint (cr);
		cairo_set_source_rgb (cr, c->red/65536.0, c->green/65536.0, c->blue/65536.0);
	}

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	if (d->sensitive) {
		cairo_matrix_t matrix;
		cairo_matrix_init_translate(&matrix, 0, -SXX_T(0));
		cairo_pattern_set_matrix (d->dpat, &matrix);
		cairo_set_source(cr, d->dpat);
	}

	rounded_rectangle(cr, 4.5, SXX_T(4.5), SXX_W(-8), SXX_H(-8), 6);
	cairo_fill_preserve (cr);
	cairo_set_line_width(cr, .75);
	cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
	cairo_stroke_preserve (cr);
	cairo_clip (cr);

	float val = gtkext_scale_round_length(d, d->cur);
#if 1
	if (d->sensitive) {
		cairo_set_source_rgba (cr, .5, .0, .0, 0.3);
	} else {
		cairo_set_source_rgba (cr, .5, .0, .0, 0.2);
	}
	if (d->horiz) {
		cairo_rectangle(cr, 3.0 + 0, SXX_T(4.5), val, SXX_T(4.5));
	} else {
		cairo_rectangle(cr, 4.5, SXX_T(3), SXX_W(-8), val);
	}
	cairo_fill(cr);
	if (d->sensitive) {
		cairo_set_source_rgba (cr, .0, .5, .0, 0.3);
	} else {
		cairo_set_source_rgba (cr, .0, .5, .0, 0.2);
	}
	if (d->horiz) {
		cairo_rectangle(cr, 3.0 + val, SXX_T(4.5), SXX_W(-8) - val, SXX_T(4.5));
	} else {
		cairo_rectangle(cr, 4.5, SXX_T(3) + val, SXX_W(-8), SXX_H(-8) - val);
	}
	cairo_fill(cr);
#endif
	if (d->sensitive) {
#if 1
		cairo_set_source(cr, d->fpat);
		cairo_matrix_t matrix;
		cairo_matrix_init_translate(&matrix, 0.0, -SXX_T(0));
		cairo_pattern_set_matrix (d->fpat, &matrix);
#else
		cairo_set_source_rgba (cr, .95, .95, .95, 1.0);
#endif
		if (d->horiz) {
			cairo_rectangle(cr, 3.0 + val, SXX_T(4.5), 3, SXX_T(4.5));
		} else {
			cairo_rectangle(cr, 4.5, SXX_T(3) + val, SXX_W(-8), 3);
		}
		cairo_fill(cr);
	} else {
		cairo_set_line_width(cr, 3.0);
		cairo_set_source_rgba (cr, .7, .7, .7, .7);
		if (d->horiz) {
			cairo_move_to(cr, 4.5 + val, SXX_T(4.5));
			cairo_line_to(cr, 4.5 + val, SXX_T(0) + SXX_H(-4.5));
		} else {
			cairo_move_to(cr, 4.5        , SXX_T(4.5) + val);
			cairo_line_to(cr, SXX_W(-4.5), SXX_T(4.5) + val);
		}
		cairo_stroke (cr);
	}

	if (d->sensitive && (d->prelight || d->drag_x > 0)) {
		cairo_reset_clip (cr);
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, .1);
		rounded_rectangle(cr, 4.5, SXX_T(4.5), SXX_W(-8), SXX_H(-8), 6);
		cairo_fill_preserve(cr);
		cairo_set_line_width(cr, .75);
		cairo_set_source_rgba (cr, .0, .0, .0, 1.0);
		cairo_stroke(cr);
	}

	cairo_destroy (cr);
	return TRUE;
}



/******************************************************************************
 * public functions
 */

static GtkExtScale * gtkext_scale_new_with_size(float min, float max, float step,
		int girth, int length, gboolean horiz) {

	assert(max > min);
	assert(step > 0);
	assert( (max - min) / step >= 1.0);

	GtkExtScale *d = (GtkExtScale *) malloc(sizeof(GtkExtScale));

	d->mark_font = get_font_from_gtk();
	get_cairo_color_from_gtk(0, d->c_txt);

	pthread_mutex_init (&d->_mutex, 0);
	d->mark_space = 0.0; // XXX longest annotation text

	d->horiz = horiz;
	if (horiz) {
		d->w_width = length; d->w_height = girth;
	} else {
		d->w_width = girth; d->w_height = length;
	}

	d->w = gtk_drawing_area_new();
	d->c = gtk_alignment_new(.5, .5, 0, 0);
	gtk_container_add(GTK_CONTAINER(d->c), d->w);

	d->mark_expose = FALSE;
	d->cb = NULL;
	d->handle = NULL;
	d->min = min;
	d->max = max;
	d->acc = step;
	d->cur = min;
	d->sensitive = TRUE;
	d->prelight = FALSE;
	d->drag_x = d->drag_y = -1;
	d->bg  = NULL;
	create_scale_pattern(d);

	d->mark_cnt = 0;
	d->mark_val = NULL;
	d->mark_txt = NULL;

	gtk_drawing_area_size(GTK_DRAWING_AREA(d->w), d->w_width, d->w_height);
	gtk_widget_set_redraw_on_allocate(d->w, TRUE);

	gtk_widget_add_events(d->w, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);

	g_signal_connect (G_OBJECT (d->w), "expose_event",         G_CALLBACK (gtkext_scale_expose_event), d);
	g_signal_connect (G_OBJECT (d->w), "button-release-event", G_CALLBACK (gtkext_scale_mouseup), d);
	g_signal_connect (G_OBJECT (d->w), "button-press-event",   G_CALLBACK (gtkext_scale_mousedown), d);
	g_signal_connect (G_OBJECT (d->w), "motion-notify-event",  G_CALLBACK (gtkext_scale_mousemove), d);
	g_signal_connect (G_OBJECT (d->w), "scroll-event",         G_CALLBACK (gtkext_scale_scroll), d);
	g_signal_connect (G_OBJECT (d->w), "enter-notify-event",   G_CALLBACK (gtkext_scale_enter_notify), d);
	g_signal_connect (G_OBJECT (d->w), "leave-notify-event",   G_CALLBACK (gtkext_scale_leave_notify), d);
	g_signal_connect (G_OBJECT (d->c), "size-request",  G_CALLBACK (gtkext_scale_size_request), d);
	g_signal_connect (G_OBJECT (d->c), "size-allocate", G_CALLBACK (gtkext_scale_size_allocate), d);

	return d;
}

static GtkExtScale * gtkext_scale_new(float min, float max, float step, gboolean horiz) {
	return gtkext_scale_new_with_size(min, max, step, GSC_GIRTH, GSC_LENGTH, horiz);
}

static void gtkext_scale_destroy(GtkExtScale *d) {
	gtk_widget_destroy(d->w);
	gtk_widget_destroy(d->c);
	cairo_pattern_destroy(d->dpat);
	cairo_pattern_destroy(d->fpat);
	pthread_mutex_destroy(&d->_mutex);
	for (int i = 0; i < d->mark_cnt; ++i) {
		free(d->mark_txt[i]);
	}
	free(d->mark_txt);
	free(d->mark_val);
	pango_font_description_free(d->mark_font);
	free(d);
}

static GtkWidget * gtkext_scale_widget(GtkExtScale *d) {
	return d->c;
}

static void gtkext_scale_set_callback(GtkExtScale *d, gboolean (*cb) (GtkWidget* w, gpointer handle), gpointer handle) {
	d->cb = cb;
	d->handle = handle;
}

static void gtkext_scale_set_value(GtkExtScale *d, float v) {
	v = d->min + rint((v-d->min) / d->acc ) * d->acc;
	gtkext_scale_update_value(d, v);
}

static void gtkext_scale_set_sensitive(GtkExtScale *d, gboolean s) {
	if (d->sensitive != s) {
		d->sensitive = s;
		gtk_widget_queue_draw(d->w);
	}
}

static float gtkext_scale_get_value(GtkExtScale *d) {
	return (d->cur);
}


static void gtkext_scale_add_mark(GtkExtScale *d, float v, const char *txt) {
	int tw = 0;
	int th = 0;
	if (txt && strlen(txt)) {
		get_text_geometry(txt, d->mark_font, &tw, &th);
	}
	pthread_mutex_lock (&d->_mutex);
	if ((tw + 3) > d->mark_space) {
		d->mark_space = tw + 3;
	}
	d->mark_val = (float *) realloc(d->mark_val, sizeof(float) * (d->mark_cnt+1));
	d->mark_txt = (char **) realloc(d->mark_txt, sizeof(char*) * (d->mark_cnt+1));
	d->mark_val[d->mark_cnt] = v;
	d->mark_txt[d->mark_cnt] = txt ? strdup(txt): NULL;
	d->mark_cnt++;
	d->mark_expose = TRUE;
	pthread_mutex_unlock (&d->_mutex);
}
#endif