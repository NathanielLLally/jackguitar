/**
 * $Id: fftgraph.c,v 1.4 2006/01/23 14:20:42 nate Exp $
 *
 * A GTK+ widget that graphs fft output
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
#include "fftgraph.h"
#include "jack_client.h"

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

G_DEFINE_TYPE (FFTGraph, fftgraph, GTK_TYPE_DRAWING_AREA);
static gboolean fftgraph_expose (GtkWidget *fftgraph, GdkEventExpose *event);

extern volatile freq_data note_input;

static void
finalize (GObject *object)
{

	FFTGraph *graph = FFTGRAPH(object);

	fft_parent_class->finalize(object);
}

static void
fftgraph_class_init (FFTGraphClass *class)
{
	GtkWidgetClass *widget_class;
	GObjectClass *gobject_class;

	widget_class = GTK_WIDGET_CLASS (class);
	gobject_class = G_OBJECT_CLASS (class);

	widget_class->expose_event = fftgraph_expose;

	fft_parent_class = g_type_class_peek(GTK_TYPE_DRAWING_AREA);
	gobject_class->finalize = finalize;

	fft_parent_class = g_type_class_peek(GTK_TYPE_DRAWING_AREA);
}

static void
fftgraph_init (FFTGraph *graph)
{
}

void
set_graph_data(FFTGraph *graph, graph_data *fftGraphData)
{
	graph->data = fftGraphData;
 	gtk_widget_queue_draw(graph);	
}

static void
draw (GtkWidget *widget, cairo_t *cr)
{
	int i, j;
	FFTGraph *graph = FFTGRAPH(widget);
	double height = widget->allocation.height;
	double width = widget->allocation.width;
	double avg, run, scale = 1, dpts, dBn;
	double dbmax, dbmin, dbrange, fmin = 0, fmax = 1400,
		hmin = 20, vmin = 20;
	double hmax = width - hmin, vmax = height - vmin;
	char *lbl = NULL;


	if (graph->data == NULL || graph->data->db == NULL) return;
	dbmax = graph->data->dbMax;
	dbmin = graph->data->dbMin;
	dbrange = dbmax - dbmin;

	graph->data->scale_freq = hmax / fmax;

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_set_line_width (cr, 1);
	cairo_move_to(cr, hmin, vmax);

	run = fmax / (hmax);

	scale = vmax / ((dbmax - -100.));

	for (i = hmin; i < hmax; i++) {
		dpts = avg = 0;
		for (j = (int)(i*run); j < (int)((i+1)*run); j++) {
			dBn = (graph->data->db[j] + 170.);
			avg += dBn;
			if (dBn > 0.) dpts++;
		}
		avg /= dpts + 1;

		cairo_line_to(cr, i,
									(vmax) - ((avg) * scale));

		/*
		printf("avg %2.5f dbmax %+3.3f dbmin %+3.3f range %+3.3f run %3.3f scale %3.5f\n",
					 avg, dbmax, dbmin, dbrange, run, scale);
		*/

	}
	cairo_stroke(cr);

	/* graph borders */
	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_move_to(cr, hmin, vmax );
	cairo_line_to(cr, hmax, vmax);
	cairo_move_to(cr, hmin, 0 );
	cairo_line_to(cr, hmin, vmax);
	cairo_stroke(cr);

	/* line at selected/detected frequency */
	cairo_set_source_rgb(cr, 0, 1, 0);
	cairo_move_to(cr, note_input.freq * graph->data->scale_freq, 0);
	cairo_line_to(cr, note_input.freq * graph->data->scale_freq, height);
	cairo_stroke(cr);

	/* graph labels */
	cairo_select_font_face(cr, "", CAIRO_FONT_SLANT_NORMAL, 
												 CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 10);
	cairo_set_source_rgb(cr, 0, 0, 0);

	/* dB */
	for (i = vmin, j = dbmax; i < vmax;
			 i+= (vmax-vmin)/10, j-=dbrange/10.) {
		cairo_move_to(cr, 0, i);
		lbl = g_strdup_printf("%+i", j);
		cairo_show_text(cr, lbl);
		free(lbl);
	}
	cairo_stroke(cr);

	/* freq */
	for (i = hmin, j = 0; i < hmax;
			 i+= (hmax-hmin)/10, j += (fmax - fmin)/10) {
		cairo_move_to(cr, i - 10, vmax + (vmin/2));
		lbl = g_strdup_printf("%u", j);
		cairo_show_text(cr, lbl);
		free(lbl);
	}
	cairo_stroke(cr);

}

static gboolean
fftgraph_expose (GtkWidget *graph, GdkEventExpose *event)
{
	cairo_t *cr;

	/* get a cairo_t */
	cr = gdk_cairo_create(graph->window);

	cairo_rectangle(cr,
									event->area.x, event->area.y,
									event->area.width, event->area.height);
	cairo_clip(cr);
	
	draw(graph, cr);

	cairo_destroy (cr);

	return FALSE;
}

GtkWidget *
fftgraph_new (void)
{
	GtkWidget *widget = g_object_new (FFTGRAPH_TYPE, NULL);
	FFTGraph *graph = FFTGRAPH(widget);

	gtk_widget_set_events(widget,
												//												GDK_LEAVE_NOTIFY				|
												GDK_EXPOSURE_MASK				|
												GDK_POINTER_MOTION_MASK	|
												GDK_BUTTON1_MOTION_MASK	|
												GDK_BUTTON2_MOTION_MASK	|
												GDK_BUTTON3_MOTION_MASK	|
												GDK_BUTTON_PRESS_MASK		|
												GDK_BUTTON_RELEASE_MASK);

	gtk_widget_set_size_request (widget, 1024, 400);

	g_signal_connect (G_OBJECT (widget), "expose-event",
	                  G_CALLBACK(fftgraph_expose), graph);

	/*
	g_signal_connect (G_OBJECT (widget), "motion_notify_event",
	                  G_CALLBACK(event_motion), graph);

	g_signal_connect (G_OBJECT (widget), "button_press_event",
	                  G_CALLBACK(event_press), graph);

	g_signal_connect (G_OBJECT (widget), "button_release_event",
	                  G_CALLBACK(event_release), graph);
	*/

	return widget;
}
