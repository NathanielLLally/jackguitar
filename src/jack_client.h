/*
 * $Id: jack_client.h,v 1.8 2006/01/23 14:20:42 nate Exp $
 *
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software Foundation.
 * Please see the file COPYING for details.
 *
 */

#ifndef __JACK_CLIENT_H
#define __JACK_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sndfile.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <glib.h>
#include <math.h>

#include <complex.h>
#ifdef USE_FFTW
#  include <fftw3.h>
#endif
#ifdef USE_DJBFFT
#  include <fftr4.h>
#endif
#include <string.h>

/* Constants */
#define M_PI 3.14159265358979323846
#define MAX_FFT_LENGTH 96000

/* pow(2.0,1.0/12.0) == 100 cents == 1 half-tone */
#define D_NOTE          1.059463094359
/* log(pow(2.0,1.0/12.0)) */
#define LOG_D_NOTE      0.057762265047
/* pow(2.0,1.0/24.0) == 50 cents */
#define D_NOTE_SQRT     1.029302236643
/* log(2) */ 
#define LOG_2           0.693147180559

#define NOISE_GATE 0.01
#define DB_MIN -58.

#define DEFAULT_RB_SIZE 16384		/* ringbuffer size in frames */

typedef struct {
  double freq;
  double db;
} Peak;
#define MAX_PEAKS 8

typedef struct _thread_info {
    pthread_t thread_id;
    jack_nframes_t duration;
    jack_nframes_t rb_size;
    jack_client_t *client;
    unsigned int channels;
    int bitdepth;
    char *path;
    volatile int can_capture;
    volatile int can_process;
    volatile int status;
} jack_thread_info_t;

typedef struct _freq_data {
	char *note;
	float octave;
	float nfreq;
	float cents;
	float freq;
	float db;
} freq_data;

void setup_ports (int sources, char *source_names[], jack_thread_info_t *info);

void *detect_note(void *arg);
int jack_process(jack_nframes_t nframes, void *arg);
void jack_shutdown (void *arg);
void wait_detect(jack_thread_info_t *info); 
void jack_client_init(long);
void jack_client_cleanup();
#endif
