/**
 * $Id: fftgraph.h,v 1.3 2006/01/23 10:16:03 nate Exp $
 *
 * A GTK+/cairo widget that draws a graph of fft data
 *
 * Authors:
 *   Nate Lally  <nate.lally@gmail.com>
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software Foundation.
 * Please see the file COPYING for details.
 */

#ifndef __FFTGRAPH_H__
#define __FFTGRAPH_H__

#include <gtk/gtk.h>

#include "jack_client.h"

G_BEGIN_DECLS

#define FFTGRAPH_TYPE		(fftgraph_get_type ())
#define FFTGRAPH(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), FFTGRAPH_TYPE, FFTGraph))
#define FFTGRAPH_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), FFTGRAPH, FFTGraphClass))
#define FFTGRAPH_IS_(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), FFTGRAPH_TYPE))
#define FFTGRAPH_IS_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), FFTGRAPH_TYPE))
#define FFTGRAPH_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), FFTGRAPH_TYPE, FFTGraphClass))

static GObjectClass *fft_parent_class = NULL;

typedef struct _FFTGraph		FFTGraph;
typedef struct _FFTGraphClass	FFTGraphClass;

#define GRAPH_MAX_FREQ 2000
typedef struct {
	double *db;
	double scale_freq;
	double dbMax;
	double dbMin;
} graph_data;

struct _FFTGraph
{
	GtkDrawingArea parent;
	graph_data *data;
};

struct _FFTGraphClass
{
	GtkDrawingAreaClass fft_parent_class;
};

GtkWidget *fftgraph_new (void);

G_END_DECLS

#endif
