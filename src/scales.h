/***
 * $Id: scales.h,v 1.7 2006/01/22 07:27:19 nate Exp $
 *
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software Foundation.
 * Please see the file COPYING for details.
 *
 */
#ifndef __SCALES_H__
#define __SCALES_H__

#define SCALE_MAX	32
#define SCALE_NUM	32

#define W 2
#define H 1

typedef struct _scale {
	char *name;
	char **notes;
} scale;

/* many things depend on the existance of a static char ** named scale_chromatic */
extern char *scale_chromatic[];
static char *scale_fifth_1[] = {"E", "A", "D", "G", NULL };
static char *scale_fifth_2[] = { "C", "F", "A#/Bb", "D#/Eb", NULL };
static char *scale_fifth_3[] = { "G#/Ab", "C#/Db", "F#/Gb", "B", NULL };

static scale scale_register[SCALE_NUM] = {
	{"chromatic", scale_chromatic},
	{"fifth1", scale_fifth_1},
	{"fifth2", scale_fifth_2},
	{"fifth3", scale_fifth_3},
	{NULL, NULL}
};


#define SCALE_NOTES	12
typedef struct {
	char *name;
	int step[SCALE_NOTES]; /* 2 = whole step, 1 = half_step */
}  scale_pattern;

/* 0 terminated */
static scale_pattern patterns[SCALE_NUM] = {
	{ "", 0},
	{ "chromatic", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
	{ "major", 2, 2, 1, 2, 2, 2, 1, 0},
	{ "natural minor", 2, 1, 2, 2, 1, 2, 2, 0},
	{ "melodic minor", 2, 1, 2, 2, 2, 2, 1, 0},
	{ "melodic minor2", 2, 2, 1, 2, 2, 1, 2, 0},
	{ "harmonic minor", 2, 1, 2, 2, 1, 3, 1, 0},
	{ "pentatonic major", 2, 2, 3, 2, 0},
	{ "pentatonic minor", 3, 1, 2, 3, 0},
	{ "blues", 3, 2, 1, 1, 2, 0},
	{ "diminished half-whole", 1, 2, 1, 2, 1, 2, 1, 0},
	{ "diminished whole-half", 2, 1, 2, 1, 2, 1, 2, 0},
	{ "bebop", 2, 2, 1, 2, 2, 1, 1, 0},
	{ "whole tone", 2, 2, 2, 2, 2, 0},
	{ NULL, 0}
};

char **find_scale(char *);
char **build_scale(const char *key, const char *name);

#endif
