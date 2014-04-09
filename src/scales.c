/*
 * $Id: scales.c,v 1.2 2006/01/20 03:45:54 nate Exp $
 *
 * utility functions for dealing with scale structures
 * home of scale_chromatic
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software Foundation.
 * Please see the file COPYING for details.
 */

#include <stdlib.h>

#include "config.h"
#include "scales.h"

char *scale_chromatic[] = { "A", "A#/Bb", "B", "C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", NULL }; 

char **find_scale(char *name) {
	int i;
	if (name == NULL) return NULL;
	for (i = 0; scale_register[i].name != NULL; i++) {
		if (g_ascii_strcasecmp(name, scale_register[i].name) == 0) {
			return scale_register[i].notes;
		}	
	}
}

char **build_scale(const char *key, const char *name) {
	int i = 0, j = 0, refi = -1, ni = 0;
	char **notes = NULL;

	if (key == NULL || name == NULL) return NULL;  //just dont croak in here

	for (; scale_chromatic[i] != NULL; i++) {
		if (strcmp(scale_chromatic[i], key) == 0) {
			refi = i;
		}
	}
	if (refi == -1)  return NULL; //incorrect key

	notes = malloc((SCALE_NOTES + 1) * sizeof(char *));
	notes[0] = scale_chromatic[refi];

	for (i = 0; patterns[i].name != NULL; i++) {
		if (strcmp(patterns[i].name, name) == 0) {
			for (; patterns[i].step[j] != 0; j++) {
				refi += patterns[i].step[j];
				notes[j+1] = scale_chromatic[refi % 12];
			}
		}
	}
	notes[j + 1] = NULL;
	return notes;
}
