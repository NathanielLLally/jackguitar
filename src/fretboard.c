/**
 * $Id: fretboard.c,v 1.10 2006/01/23 06:15:36 nate Exp $
 *
 * A GTK+ widget that implements a guitar's fretboard
 *
 * Authors:
 *   Nate Lally  <nate.lally@gmail.com>
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software Foundation.
 * Please see the file COPYING for details.
 *
 */

#include <gtk/gtk.h>
#include <math.h>

#include "config.h"
#include "fretboard.h"

static int font_main = 20, font_single = 20, font_double = 16;
static double gnut_to_fret[MAX_FRETS];
static double gfret_size[MAX_FRETS];

Tuning *tuning = tuning_standard;

enum {
	FRETBOARD_SIGNAL_SET_NOTE,
	FRETBOARD_SIGNAL_SET_ALL,
	LAST_SIGNAL
};

static gint fretboard_signals[LAST_SIGNAL] = { 0 };

/* prototypes for event functions registered with the object */

static gboolean event_press (GtkWidget      *widget,
														 GdkEventButton *bev,
														 gpointer        user_data);

static gboolean event_release (GtkWidget      *widget,
															 GdkEventButton *bev,
															 gpointer        user_data);

static gboolean event_motion (GtkWidget      *widget,
															GdkEventMotion *mev,
															gpointer        user_data);

static gboolean leave_notify(GtkWidget					*widget,
														 GdkEventCrossing	*ev,
														 gpointer					 user_data);


G_DEFINE_TYPE (Fretboard, fretboard, GTK_TYPE_DRAWING_AREA);
static gboolean fretboard_expose (GtkWidget *fretboard, GdkEventExpose *event);

static void
finalize (GObject *object)
{

	Fretboard *fretboard = FRETBOARD(object);

	if (fretboard->note)
		{
			free (fretboard->note);
		}

	parent_class->finalize(object);
}

static void
fretboard_class_init (FretboardClass *class)
{
	GtkWidgetClass *widget_class;
	GObjectClass *gobject_class;

	widget_class = GTK_WIDGET_CLASS (class);
	gobject_class = G_OBJECT_CLASS (class);

	widget_class->expose_event = fretboard_expose;

	parent_class = g_type_class_peek(GTK_TYPE_DRAWING_AREA);
	gobject_class->finalize = finalize;

	parent_class = g_type_class_peek(GTK_TYPE_DRAWING_AREA);

	/*
		fretboard_signals[FRETBOARD_SIGNAL_SET_ALL] =
		g_signal_new ("fretboard_set_all",
		G_TYPE_FROM_CLASS (gobject_class),
		G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, NULL);
	*/
}

static void
fretboard_init (Fretboard *fretboard)
{
	int s = 0, f = 0;

	//make these properties
	fretboard->num_strings = 6;
	fretboard->num_frets = 16;
	fretboard->note_radius = font_single;
	bzero(&gnut_to_fret[0], MAX_FRETS * sizeof(double));
	bzero(&gfret_size[0], MAX_FRETS * sizeof(double));

	if (fretboard->note) {
		free(fretboard->note);
	}

	fretboard->note = (Note *)malloc
		(fretboard->num_strings * (fretboard->num_frets + 1) * sizeof(Note));

	for (s = 0; s < fretboard->num_strings; s++) {
		for (f = 0; f <= fretboard->num_frets; f++) {
			Note *n = &fretboard->note[s*(fretboard->num_frets +1) + f];
			n->state = NOTE_OFF;
			n->x = 0;
			n->y = 0;
			n->r = fretboard->note_radius;
			n->note = NULL;
		}
	}

}

/*
	get_note
	parameters: string & fret position
	returns struct _Tuning representing the given note
*/
Tuning
get_note(int string, int fret) {
	int i = 0, off = 0;
	Tuning ref = tuning[string];
	Tuning note;
	int scale_length = 0;
	while (scale_chromatic[scale_length] != NULL) {
		scale_length++;
	}

	for (i = 0; i < scale_length; i++) {
		if (strcmp(scale_chromatic[i], ref.note) == 0) {
			off = i;
			i = (i + fret) % scale_length;
			note.note = scale_chromatic[i];
			note.octave = ref.octave + (off + fret) / 12;
			return note;
		}
	}
}

//bounds box collision detection
static Note *
query_pos(Fretboard *fretboard, int x, int y)
{
	int s, f;
	for (s = 0; s < fretboard->num_strings; s++) {
		for (f = 0; f <= fretboard->num_frets; f++) {
			Note *n = &fretboard->note[s*(fretboard->num_frets+1)+f];
			if ((x < (n->x + n->r)) && (x > (n->x - n->r)) &&
					(y < (n->y + n->r)) && (y > (n->y - n->r))) {
				return n;	
			}
		}
	}
	return NULL;	
}

void
set_note_all(Fretboard *fretboard, char *note, int state)
{
	int s, f;
	for (s = 0; s < fretboard->num_strings; s++) {
		for (f = 0; f <= fretboard->num_frets; f++) {
			Note *n = &fretboard->note[s*(fretboard->num_frets+1)+f];
			if (n && n->note && note) {
				if ((g_ascii_strcasecmp(n->note, note) == 0) ||
						(g_ascii_strcasecmp("all", note) == 0)) { 
					n->state = state;
				}
			}
		}
	}

	/* request a redraw, since we've changed the state of our widget */
	gtk_widget_queue_draw (GTK_WIDGET(fretboard));
}


/* this one is more efficient when having a list of notes
	 also clearing notes not in list
 */
void
set_notes(Fretboard *fretboard, char **notes, int state)
{
	int s, f, i;
	for (s = 0; s < fretboard->num_strings; s++) {
		for (f = 0; f <= fretboard->num_frets; f++) {
			Note *n = &fretboard->note[s*(fretboard->num_frets+1)+f];
			if (n && n->note) {
				n->state = NOTE_OFF;
				for (i = 0; notes != NULL && notes[i] != NULL; i++) {
					if (g_ascii_strcasecmp(n->note, notes[i]) == 0) {
						n->state = state;

		//partial re-draw 
		gtk_widget_queue_draw_area (GTK_WIDGET(fretboard), 		      
 																n->x - (n->r + 5),
																n->y - (n->r + 5),
								      	      	(n->r + 5) * 2,
								      	      	(n->r + 5) * 2);

					}
				}
			}
		}
	}
}

void
set_note(Fretboard *fretboard, char *note, int octave, int state)
{
	int s, f;
	for (s = 0; s < fretboard->num_strings; s++) {
		for (f = 0; f <= fretboard->num_frets; f++) {
			Note *n = &fretboard->note[s*(fretboard->num_frets+1)+f];
			if (n && n->note && note) {
				if ((g_ascii_strcasecmp(n->note, note) == 0)
						&& octave == n->octave) { 
					n->state = state;

		//partial re-draw 
		gtk_widget_queue_draw_area (GTK_WIDGET(fretboard), 		      
 																n->x - (n->r + 5),
																n->y - (n->r + 5),
								      	      	(n->r + 5) * 2,
								      	      	(n->r + 5) * 2);

				}
			}
		}
	}
}

void
save_note_state(Fretboard *fretboard, char *note, int octave)
{
	int s, f;
	for (s = 0; s < fretboard->num_strings; s++) {
		for (f = 0; f <= fretboard->num_frets; f++) {
			Note *n = &fretboard->note[s*(fretboard->num_frets+1)+f];
			if (n && n->note && note) {
				if ((g_ascii_strcasecmp(n->note, note) == 0)
						&& octave == n->octave) { 
					n->state_saved = n->state;
				}
			}
		}
	}
}

void
restore_note_state(Fretboard *fretboard, char *note, int octave)
{
	int s, f;
	for (s = 0; s < fretboard->num_strings; s++) {
		for (f = 0; f <= fretboard->num_frets; f++) {
			Note *n = &fretboard->note[s*(fretboard->num_frets+1)+f];
			if (n && n->note && note) {
				if ((g_ascii_strcasecmp(n->note, note) == 0)
						&& octave == n->octave) { 
					n->state = n->state_saved;

		//partial re-draw 
		gtk_widget_queue_draw_area (GTK_WIDGET(fretboard), 		      
 																n->x - (n->r + 5),
																n->y - (n->r + 5),
								      	      	(n->r + 5) * 2,
								      	      	(n->r + 5) * 2);

				}
			}
		}
	}
}


static void
draw_note(Note *note, cairo_t *cr)
{
	if (note->state == 0) return;

	if (!(note->state & NOTE_OFF) && !(note->state & NOTE_TEXT_ONLY)) {
		//draw circle
		cairo_move_to(cr, note->x + note->r, note->y);
		cairo_arc(cr, note->x, note->y, note->r, 0, 2 * M_PI);
		if (note->state & NOTE_REVERSE) {
			cairo_set_source_rgb (cr, 0, 0, 0);
		} else {
			cairo_set_source_rgb (cr, 1, 1, 1);
		}
		cairo_fill_preserve (cr);

		if (note->state & NOTE_HIGHLIGHT) {
			cairo_set_source_rgb (cr, 0, 0, 1);
		} else if (note->state & NOTE_REVERSE) {
			cairo_set_source_rgb (cr, 1, 1, 1);
		} else if (note->state & NOTE_ON) {
			cairo_set_source_rgb (cr, 0, 0, 0);
		}

		cairo_stroke(cr);
	}

	//print note text
	if (note->state & NOTE_HIGHLIGHT) {
		cairo_set_source_rgb (cr, 1, 0, 0);
	} else	if (note->state & NOTE_REVERSE) {
		cairo_set_source_rgb (cr, 1, 1, 1);
	} else if (note->state & NOTE_TEXT_ONLY) {
		cairo_set_source_rgb (cr, 0, 0, 0);
	} else if (note->state & NOTE_ON) {
		cairo_set_source_rgb (cr, 0, 0, 0);
	}
	cairo_select_font_face(cr, "", CAIRO_FONT_SLANT_NORMAL, 
												 CAIRO_FONT_WEIGHT_NORMAL);

	//note is single character
	if (note->note && strlen(note->note) == 1) {
		cairo_set_font_size(cr, font_single);
		cairo_move_to(cr,
									note->x - font_single / 2.75,
									note->y + font_single / 2.75);
		cairo_show_text(cr, note->note);

#if 0
		char *octave = g_strdup_printf("%u", note->octave);
		cairo_move_to(cr,
									note->x - font_single / 2.75 + font_single,
									note->y + font_single / 2.75);
		cairo_show_text(cr, octave);
		free(octave);
#endif

		//note has sharp & flat
	} else if (note->note) {
		char **sharp_flat;	
		sharp_flat = g_strsplit(note->note, "/", 2);
		//sharp
		cairo_set_font_size(cr, font_double);
		cairo_move_to(cr,
									note->x - font_double / 1.3,
									note->y - 2);
		cairo_show_text(cr, sharp_flat[0]);
		//flat
		cairo_move_to(cr,
									note->x - font_double / 1.3,
									note->y + font_double / 1.3);
		cairo_show_text(cr, sharp_flat[1]);
		g_strfreev(sharp_flat);
	}
}

static void
draw (GtkWidget *widget, cairo_t *cr)
{
	Fretboard *fretboard = FRETBOARD(widget);

	double height, width, x, y;
	int i, s, f;
	double string_spacing, font_main = 18;
	double vinset = font_main; //fretboard->allocation.height / 17;
	double hinset = font_main;//fretboard->allocation.width / 57;
	//vars for fret spacing calculation
	double view_width = 0, nut_to_fret = 0, scale = 0, len_string = 0;
	double edge = 5; //from edge strings to edge of nut in pixels
	GList *n = NULL;

	gchar *fret = NULL;
	gchar *string = NULL;

	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_stroke (cr);

	//give 3/4ths the size of inset to text on left and top
	height = widget->allocation.height - (vinset * 4);
	width = widget->allocation.width - (hinset * 4);

	//	x = widget->allocation.x + hinset * 2.5;
	//	y = widget->allocation.y + vinset * 2.5;
	x = hinset * 2.5;
	y = vinset * 2.5;

	string_spacing = (height - (edge * 2)) / (fretboard->num_strings - 1);

	//font_main = string_spacing * 0.7;

	//draw nut
	cairo_save (cr); /* stack-pen-size */
	cairo_set_line_width (cr, 4 *
												cairo_get_line_width (cr));
	cairo_move_to (cr,
								 x + 3, //(cairo_get_line_width (cr) / 2),
								 y);
		
	cairo_line_to (cr,
								 x + 3, //(cairo_get_line_width (cr) / 2),
								 y + height);

	cairo_stroke(cr); 

	//draw saddle 
	cairo_move_to (cr,
								 x + width - 3, // - (cairo_get_line_width (cr) / 2),
								 y);
		
	cairo_line_to (cr,
								 x + width - 3, // - (cairo_get_line_width (cr) / 2),
								 y + height);

	cairo_stroke(cr); 
	cairo_restore (cr); /* stack-pen-size */

	//draw frets
	view_width = width;
	len_string = (pow(PITCH_R, fretboard->num_frets) * view_width)  /
		(pow(PITCH_R, fretboard->num_frets) - 1) ;

	/*
	while (len_string - (len_string / (pow(PITCH_R, fretboard->num_frets)))
				 < view_width) {
		len_string+=5;
	}
	*/
	//fret_size = (width - x) / fretboard->num_frets; 

	for (i = 0; i < fretboard->num_frets; i++) {
		cairo_save (cr); /* stack-pen-size */
		cairo_set_line_width (cr, 2 *
													cairo_get_line_width (cr));

		gnut_to_fret[i+1] = 
			len_string - (len_string / pow(PITCH_R, i + 1));

		gfret_size[i] = gnut_to_fret[i+1] - nut_to_fret;

		nut_to_fret = gnut_to_fret[i+1];

		//scale -= fret_size;
		cairo_move_to (cr,
									 x + nut_to_fret,
									 y);
		
		cairo_line_to (cr,
									 x + nut_to_fret,
									 y + height);
		cairo_stroke(cr); 

		//print the fret number up top
		cairo_select_font_face(cr, "", CAIRO_FONT_SLANT_NORMAL, 
													 CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, font_main);
		cairo_set_source_rgb(cr, 0, 0, 0);
		fret = g_strdup_printf("%i", i + 1);
		cairo_move_to(cr,
									x + nut_to_fret - ((font_main / 2) * strlen(fret)),
									y - (font_main / 2));
		cairo_show_text(cr, fret);
		g_free(fret);

		double cx, cy, dxs = ((width - x) * 3) / (RULE * fretboard->num_frets),
			dys = string_spacing / 2 * 0.6;
		cairo_set_line_width(cr, 1);

		//draw diamond inlay
		cairo_set_source_rgb (cr, 0.3, 0.3, 0.3);
		switch(i+1) {
		case 5:
		case 9:
		case 15:
		case 17:
		case 19:
		case 21:
			cx = x + nut_to_fret - (gfret_size[i] / 2);
			cy = y + string_spacing * 2.5 + edge;
			cairo_move_to(cr, cx, cy - dys);
			cairo_line_to(cr, cx + dxs, cy);
			cairo_line_to(cr, cx, cy + dys);
			cairo_line_to(cr, cx - dxs, cy);
			cairo_line_to(cr, cx, cy - dys);
			cairo_fill_preserve (cr);
			cairo_stroke(cr);
			break;
		case 7:
		case 12:
			cx = x + nut_to_fret - (gfret_size[i] / 2);
			cy = y + string_spacing * 1.5 + edge;
			cairo_move_to(cr, cx, cy - dys);
			cairo_line_to(cr, cx + dxs, cy);
			cairo_line_to(cr, cx, cy + dys);
			cairo_line_to(cr, cx - dxs, cy);
			cairo_line_to(cr, cx, cy - dys);
			cairo_fill_preserve (cr);
			cairo_stroke(cr);
			cy = y + string_spacing * 3.5 + edge;
			cairo_move_to(cr, cx, cy - dys);
			cairo_line_to(cr, cx + dxs, cy);
			cairo_line_to(cr, cx, cy + dys);
			cairo_line_to(cr, cx - dxs, cy);
			cairo_line_to(cr, cx, cy - dys);
			cairo_fill_preserve (cr);
			cairo_stroke(cr);
			break;
		default:
			break;
		}

		cairo_restore (cr); /* stack-pen-size */
	}

	//draw strings
	for (i = 0; i < fretboard->num_strings; i++) {
		cairo_set_source_rgb (cr, 0.6, 0.6, 0.6);

		cairo_move_to (cr,
									 x,
									 y + string_spacing * i + edge);
		
		cairo_line_to (cr,
									 x + width,
									 y + string_spacing * i + edge);
		cairo_stroke(cr);
	}


	//draw note markers
	for (s = 0; s < fretboard->num_strings; s++) {

		for (f = 0; f <= fretboard->num_frets; f++) {
			Note *n = &fretboard->note[s*(fretboard->num_frets+1) + f];
			Tuning t = get_note(s, f);

			n->r = fretboard->note_radius;
			n->x = x + gnut_to_fret[f] - (n->r);
			n->y = y + string_spacing * s + edge;
			n->note = t.note;
			n->octave = t.octave;
			draw_note(n, cr);
		}
	}

	//draw string tuning
	for (s = 0; s < fretboard->num_strings; s++) {
		Note *n = &fretboard->note[s*(fretboard->num_frets+1)];
		n->state |= NOTE_TEXT_ONLY;
		draw_note(n, cr);
		n->state &= ~NOTE_TEXT_ONLY;
	}
}

static gboolean
fretboard_expose (GtkWidget *fretboard, GdkEventExpose *event)
{
	cairo_t *cr;

	/* get a cairo_t */
	cr = gdk_cairo_create(fretboard->window);

	cairo_rectangle(cr,
									event->area.x, event->area.y,
									event->area.width, event->area.height);
	cairo_clip(cr);
	
	draw(fretboard, cr);

	cairo_destroy (cr);

	return FALSE;
}

GtkWidget *
fretboard_new (void)
{
	GtkWidget *widget = g_object_new (FRETBOARD_TYPE, NULL);
	Fretboard *fretboard = FRETBOARD(widget);

	gtk_widget_set_events(widget,
												//												GDK_LEAVE_NOTIFY				|
												GDK_EXPOSURE_MASK				|
												GDK_POINTER_MOTION_MASK	|
												GDK_BUTTON1_MOTION_MASK	|
												GDK_BUTTON2_MOTION_MASK	|
												GDK_BUTTON3_MOTION_MASK	|
												GDK_BUTTON_PRESS_MASK		|
												GDK_BUTTON_RELEASE_MASK);

	gtk_widget_set_size_request (widget, 1024, 300);

	g_signal_connect (G_OBJECT (widget), "expose-event",
	                  G_CALLBACK(fretboard_expose), fretboard);

	g_signal_connect (G_OBJECT (widget), "motion_notify_event",
	                  G_CALLBACK(event_motion), fretboard);

	g_signal_connect (G_OBJECT (widget), "button_press_event",
	                  G_CALLBACK(event_press), fretboard);

	g_signal_connect (G_OBJECT (widget), "button_release_event",
	                  G_CALLBACK(event_release), fretboard);

	/*
		g_signal_connect(G_OBJECT (widget), "fretboard_set_all",
		G_CALLBACK(note_set_all), fretboard);

		g_signal_connect (G_OBJECT (widget), "leave_notify_event",
		G_CALLBACK(leave_notify), fretboard);
	*/

	return widget;
}


/* the actually user inter event functions */
static Note *last_note;

static gboolean
event_press (GtkWidget      *widget,
	           GdkEventButton *bev,
	           gpointer        user_data)
{
	Fretboard *fretboard = FRETBOARD(user_data);
	Note *n = NULL;

	switch (bev->button)
	  {
		case 1:
			n = query_pos(fretboard, bev->x, bev->y);
			if (n) {
				if (n == last_note) {
					n->state |= NOTE_REVERSE;
					gtk_widget_queue_draw_area (GTK_WIDGET(fretboard), 		      
																			n->x - (n->r + 5),
																			n->y - (n->r + 5),
																			(n->r + 5) * 2,
																			(n->r + 5) * 2);
				}
			}
			/* request a redraw, since we've changed the state of our widget */
			//        gtk_widget_queue_draw (GTK_WIDGET(fretboard));
			break;
	  }
	return FALSE;
}

static gboolean
event_release (GtkWidget * widget,
	             GdkEventButton * bev, gpointer user_data) {
	Fretboard *fretboard = FRETBOARD(user_data);
	Note *n = NULL;
	switch (bev->button)
	  {
		case 1:
			n = query_pos(fretboard, bev->x, bev->y);
			if (n) {
				if (n == last_note) {
					if (n->state & NOTE_ON) {
						n->state = NOTE_OFF;
					} else {
						n->state = NOTE_ON;
					}
					gtk_widget_queue_draw_area (GTK_WIDGET(fretboard), 		      
																			n->x - (n->r + 5),
																			n->y - (n->r + 5),
																			(n->r + 5) * 2,
																			(n->r + 5) * 2);
					last_note = NULL;
				}
			}
			/* request a redraw, since we've changed the state of our widget */
			/*        gtk_widget_queue_draw (GTK_WIDGET(fretboard));
								gtk_widget_queue_draw_area (GTK_WIDGET(fretboard), 		      
								bev->x - (fretboard->note_radius * 3),
								bev->y - (fretboard->note_radius * 3),
								(fretboard->note_radius * 6),
								(fretboard->note_radius * 6));
			*/
			break;
	  }
	return FALSE;
}

static gboolean
event_motion (GtkWidget      *widget,
	            GdkEventMotion *mev,
	            gpointer        user_data)
{
	Fretboard *fretboard = FRETBOARD(user_data);
	Note *n = query_pos(fretboard, mev->x, mev->y);
	if (n) {
		if (last_note && last_note != n) {
			last_note->state &= ~NOTE_HIGHLIGHT;
			/*
			if (!(last_note->state & NOTE_ON))
				last_note->state = NOTE_OFF;
			*/
			gtk_widget_queue_draw_area (GTK_WIDGET(fretboard), 		      
																	last_note->x - (last_note->r + 5),
																	last_note->y - (last_note->r + 5),
																	(last_note->r + 5) * 2,
																	(last_note->r + 5) * 2);
		}
		last_note = n;
		n->state |= NOTE_HIGHLIGHT;

		//partial re-draw 
		gtk_widget_queue_draw_area (GTK_WIDGET(fretboard), 		      
 																n->x - (n->r + 5),
																n->y - (n->r + 5),
								      	      	(n->r + 5) * 2,
								      	      	(n->r + 5) * 2);
	} else {
		if (last_note) {
			last_note->state &= ~NOTE_HIGHLIGHT;
			/*
			if (!(last_note->state & NOTE_ON)) {
				last_note->state = NOTE_OFF;
			}
			*/
			gtk_widget_queue_draw_area (GTK_WIDGET(fretboard), 		      
																	last_note->x - (last_note->r + 5),
																	last_note->y - (last_note->r + 5),
																	(last_note->r + 5) * 2,
																	(last_note->r + 5) * 2);
		}
	}

	return FALSE;
}


static gboolean
leave_notify(GtkWidget				*widget,
						 GdkEventCrossing	*ev,
						 gpointer					 user_data) {

	if (last_note) {
		if (!(last_note->state & NOTE_ON))
			last_note->state = NOTE_OFF;
	}
	return FALSE;
}


