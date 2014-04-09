/**
 * $Id: fretboard.h,v 1.7 2006/01/22 07:27:19 nate Exp $
 *
 * A GTK+ widget that implements a guitar's fretboard
 *
 * Authors:
 *   Nate Lally  <nate.lally@gmail.com>
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software Foundation.
 * Please see the file COPYING for details.
 */

#ifndef __FRETBOARD_H__
#define __FRETBOARD_H__

#include <gtk/gtk.h>
#include "scales.h"

G_BEGIN_DECLS

#define FRETBOARD_TYPE		(fretboard_get_type ())
#define FRETBOARD(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), FRETBOARD_TYPE, Fretboard))
#define FRETBOARD_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), FRETBOARD, FretboardClass))
#define FRETBOARD_IS_(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRETBOARD_TYPE))
#define FRETBOARD_IS_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), FRETBOARD_TYPE))
#define FRETBOARD_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), FRETBOARD_TYPE, FretboardClass))

static GObjectClass *parent_class = NULL;

typedef struct _Fretboard		Fretboard;
typedef struct _Note				Note;
typedef struct _FretboardClass	FretboardClass;

#define RULE						17.817
#define PITCH_R					1.0595
#define MAX_FRETS 			24
#define MAX_STRINGS 		12

#define NOTE_OFF					0x00
#define NOTE_ON						0x01
#define NOTE_HIGHLIGHT		0x02
#define NOTE_REVERSE			0x04
#define NOTE_TEXT_ONLY 		0x08

struct _Note
{
	int x;
	int y;
	int r;
	int state;
	int state_saved;
	char *note; //pointer to an item in chromatic scale array
	int octave;
};

struct _Fretboard
{
	GtkDrawingArea parent;
	int num_strings;
	int num_frets;
	double note_radius;	
	Note *note;	
};

struct _FretboardClass
{
	GtkDrawingAreaClass parent_class;
};

static const struct _Tuning
{
	char *note;
	int octave;
} tuning_standard[] = {
	{"E", 3 },
	{"B", 3 },
	{"G", 2 },
	{"D", 2 },
	{"A", 2 },
	{"E", 1 },
	{NULL, 0}
};

typedef struct _Tuning Tuning;

static Tuning tuning_open_d[] = {
	{"D", 3 },
	{"A", 3 },
	{"F#/Gb", 2 },
	{"D", 2 },
	{"A", 2 },
	{"D", 1 },
	{NULL, 0}
};

static Tuning tuning_drop_d[] = {
	{"E", 3 },
	{"B", 3 },
	{"G", 2 },
	{"D", 2 },
	{"A", 2 },
	{"D", 1 },
	{NULL, 0}
};

static Tuning tuning_open_e[] = {
	{"E", 3 },
	{"B", 3 },
	{"G#/Ab", 2 },
	{"E", 2 },
	{"B", 2 },
	{"E", 1 },
	{NULL, 0}
};

static Tuning tuning_open_g[] = {
	{"D", 3 },
	{"B", 3 },
	{"G", 2 },
	{"D", 2 },
	{"G", 2 },
	{"D", 1 },
	{NULL, 0}
};

static Tuning tuning_open_c[] = {
	{"E", 3 },
	{"C", 3 },
	{"G", 2 },
	{"C", 2 },
	{"G", 2 },
	{"C", 1 },
	{NULL, 0}
};

static Tuning tuning_dadgad[] = {
	{"D", 3 },
	{"A", 3 },
	{"G", 2 },
	{"D", 2 },
	{"A", 2 },
	{"D", 1 },
	{NULL, 0}
};

typedef struct _tuning_register {
	char *name;
	Tuning *tuning;
} tuning_register;

static tuning_register tunings[] = {
	{"Standard", tuning_standard},
	{"Drop D", tuning_drop_d},
	{"Open D", tuning_open_d},
	{"Open E", tuning_open_e},
	{"Open G", tuning_open_g},
	{"Open C", tuning_open_c},
	{"D75525", tuning_dadgad},
	{NULL, NULL}
};

static Tuning *
find_tuning(char *name)
{
	int i;
	if (name == NULL) return NULL;
	for (i = 0; tunings[i].name != NULL; i++) {
		if (g_ascii_strcasecmp(name, tunings[i].name) == 0) {
			return tunings[i].tuning;
		}	
	}
}

GtkWidget *fretboard_new (void);
void set_note_all(Fretboard *fretboard, char *note, int state);
void set_note(Fretboard *fretboard, char *note, int octave, int state);
void save_note_state(Fretboard *fretboard, char *note, int octave);
void restore_note_state(Fretboard *fretboard, char *note, int octave);

G_END_DECLS

#endif
