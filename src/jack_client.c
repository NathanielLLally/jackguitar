/*
 * $Id: jack_client.c,v 1.11 2006/01/23 14:20:42 nate Exp $
 *
 * jack interface code
 *   written to support multiple capture ports, however the
 *   performance hit is not worth using stero mics imho
 *   
 *
 * fft code and displayFrequency shamelessly ripped from:
 *
 * tuneit.c -- Detect fundamental frequency of a sound
 * Copyright (C) 2004, 2005  Mario Lang <mlang@delysid.org>
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software Foundation.
 * Please see the file COPYING for details.
 *
 */

#include <gtk/gtk.h>

#include "config.h"
#include "jack_client.h"
#include "fftgraph.h"

unsigned int rate = 96000;
int latency = 10;

static const char *englishNotes[12] =
	{"A","A#/Bb","B","C","C#/Db","D","D#/Eb","E","F","F#/Gb", "G", "G#/Ab"};
static const char **notes = englishNotes;

static double freqs[12];
static double lfreqs[12];

#ifdef USE_FFTW
/* fft globals */
static float *fftSampleBuffer;
static float *fftSample;
static float *fftLastPhase;
static int fftSize;
static int fftFrameCount = 0;
static float *fftIn;
static fftwf_complex *fftOut;
static fftwf_plan fftPlan;
#endif

/* JACK data */
unsigned int nports;
jack_port_t **ports;
jack_default_audio_sample_t **in;
jack_nframes_t nframes;
const size_t sample_size = sizeof(jack_default_audio_sample_t);

/* Thread Synchronozation */
jack_thread_info_t thread_info;
pthread_mutex_t detect_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  data_ready = PTHREAD_COND_INITIALIZER;
jack_ringbuffer_t *rb;
long overruns = 0;

freq_data note_input;
graph_data fftGraphData;

static void
displayFrequency (Peak *peak)
{
  double ldf, mldf;
  double lfreq, nfreq;
  int i, note = 0;
	char *lbl_text = NULL;
	double freq = peak->freq;

	if (peak->db > DB_MIN) {

  if (freq < 1E-15) freq = 1E-15;
  lfreq = log(freq);

  while (lfreq < lfreqs[0]-LOG_D_NOTE/2.) lfreq += LOG_2;
  while (lfreq >= lfreqs[0]+LOG_2-LOG_D_NOTE/2.) lfreq -= LOG_2;
  mldf = LOG_D_NOTE;
  for (i=0; i<12; i++) {
    ldf = fabs(lfreq-lfreqs[i]);
    if (ldf < mldf) {
      mldf = ldf;
      note = i;
    }
  }

  nfreq = freqs[note];
  while (nfreq/freq > D_NOTE_SQRT) nfreq /= 2.0;
  while (freq/nfreq > D_NOTE_SQRT) nfreq *= 2.0;
#if 1
	note_input.note = notes[note];
	note_input.octave = floorf((log(nfreq) - log(440.))/LOG_2+4.);
	note_input.nfreq = nfreq;
	note_input.cents = 1200*(log(freq/nfreq)/LOG_2);
 	note_input.freq = freq;
	note_input.db = peak->db;

#else
	/*
  lbl_text = g_strdup_printf
		("%-5s % 8.3fHz % +3.f	% 8.3fHz %8.3f",
	*/
  printf("Note %-2s (%8.3fHz): %+3.f cents (%8.3fHz) db(%8.3f)     \r",
				 notes[note], nfreq, 1200*(log(freq/nfreq)/LOG_2), freq, peak->db);
	fflush(stdout);
#endif
	}
}

void
setup_ports (int sources, char *source_names[], jack_thread_info_t *info)
{
	unsigned int i;
	size_t in_size;

	/* Allocate data structures that depend on the number of ports. */
	nports = sources;
	ports = (jack_port_t **) malloc (sizeof (jack_port_t *) * nports);
	in_size =  nports * sizeof (jack_default_audio_sample_t *);
	in = (jack_default_audio_sample_t **) malloc (in_size);
	rb = jack_ringbuffer_create(nports * sample_size * info->rb_size);

	//	printf("sample size [%u]\n", sample_size);

	/* When JACK is running realtime, jack_activate() will have
	 * called mlockall() to lock our pages into memory.  But, we
	 * still need to touch any newly allocated pages before
	 * process() starts using them.  Otherwise, a page fault could
	 * create a delay that would force JACK to shut us down. */
	memset(in, 0, in_size);
	memset(rb->buf, 0, rb->size);

	for (i = 0; i < nports; i++) {
		char name[64];

		sprintf (name, "input%d", i+1);

		if ((ports[i] = jack_port_register (info->client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0)) == 0) {
			fprintf (stderr, "cannot register input port \"%s\"!\n", name);
			jack_client_close (info->client);
			exit (1);
		}
	}

	for (i = 0; i < nports; i++) {
		if (jack_connect (info->client, source_names[i], jack_port_name (ports[i]))) {
			fprintf (stderr, "cannot connect input port %s to %s\n", jack_port_name (ports[i]), source_names[i]);
			jack_client_close (info->client);
			exit (1);
		} 
	}

	info->can_process = 1;		/* process() can start, now */
}


static void
fftInit (int size)
{
  int i;

#ifdef USE_FFTW
  fftSize = rate/size;
  fftIn = (float *)fftwf_malloc(sizeof(float) * 2 * (fftSize/2+1));
  fftOut = (fftwf_complex *)fftIn;
  fftPlan = fftwf_plan_dft_r2c_1d(fftSize, fftIn, fftOut, FFTW_MEASURE);

  fftSampleBuffer = (float *)malloc(fftSize * sizeof(float));
  fftSample = NULL;
  fftLastPhase = (float *)malloc((fftSize/2+1) * sizeof(float));
  memset(fftSampleBuffer, 0, fftSize*sizeof(float));
  memset(fftLastPhase, 0, (fftSize/2+1)*sizeof(float));
#endif
#ifdef USE_DJBFFT

#endif
#ifdef USE_GRAPH
	fftGraphData.db = (double *)malloc(sizeof(double) * GRAPH_MAX_FREQ);
#endif
}

static void
fftMeasure (int nframes, int overlap, float *indata)
{
#ifdef USE_FFTW
  int i, stepSize = fftSize/overlap;
  double freqPerBin = rate/(double)fftSize,
    phaseDifference = 2.*M_PI*(double)stepSize/(double)fftSize;

  if (!fftSample) fftSample = fftSampleBuffer + (fftSize-stepSize);

	//	bzero(fftGraphData.db, GRAPH_MAX_FREQ);

  for (i=0; i<nframes; i++) {
    *fftSample++ = indata[i];

    if (fftSample-fftSampleBuffer >= fftSize) {
      int k;
      Peak peaks[MAX_PEAKS];

      for (k=0; k<MAX_PEAKS; k++) {
				peaks[k].db = -200.;
				peaks[k].freq = 0.;
      }

      fftSample = fftSampleBuffer + (fftSize-stepSize);

      for (k=0; k<fftSize; k++) {
        double window = -.5*cos(2.*M_PI*(double)k/(double)fftSize)+.5;
        fftIn[k] = fftSampleBuffer[k] * window;
      }
      fftwf_execute(fftPlan);

			for (k=0; k<=fftSize/2; k++) {
				long qpd;
				float
			  real = creal(fftOut[k]),
			  imag = cimag(fftOut[k]),
			  magnitude = 20.*log10(2.*sqrt(real*real + imag*imag)/fftSize),
			  phase = atan2(imag, real),
					  tmp, freq;

        /* compute phase difference */
        tmp = phase - fftLastPhase[k];
        fftLastPhase[k] = phase;

        /* subtract expected phase difference */
        tmp -= (double)k*phaseDifference;

        /* map delta phase into +/- Pi interval */
        qpd = tmp / M_PI;
        if (qpd >= 0) qpd += qpd&1;
        else qpd -= qpd&1;
        tmp -= M_PI*(double)qpd;

        /* get deviation from bin frequency from the +/- Pi interval */
        tmp = overlap*tmp/(2.*M_PI);

        /* compute the k-th partials' true frequency */
        freq = (double)k*freqPerBin + tmp*freqPerBin;

#ifdef USE_GRAPH
				int fi = (int)round(freq);
				if (fi < GRAPH_MAX_FREQ) {
					fftGraphData.db[fi] = magnitude;
					if (magnitude > fftGraphData.dbMax)
						fftGraphData.dbMax = magnitude;
					if (magnitude < fftGraphData.dbMin)
						fftGraphData.dbMin = magnitude;
				}
				/*
				printf("%+8.3f % .5f %i\n", freq, fftGraphData.scale_freq,
							 (int)round(freq * fftGraphData.scale_freq));
				*/
#endif

				if (freq > 0.0 && magnitude > peaks[0].db) {
				  memmove(peaks+1, peaks, sizeof(Peak)*(MAX_PEAKS-1));
				  peaks[0].freq = freq;
				  peaks[0].db = magnitude;
				}
      }
      fftFrameCount++;
      if (fftFrameCount > 0 && fftFrameCount % overlap == 0) {
				int l, maxharm = 0;
				k = 0;
				for (l=1; l<MAX_PEAKS && peaks[l].freq > 0.0; l++) {
				  int harmonic;

				  for (harmonic=5; harmonic>1; harmonic--) {
				    if (peaks[0].freq / peaks[l].freq < harmonic+.02 &&
							peaks[0].freq / peaks[l].freq > harmonic-.02) {
				      if (harmonic > maxharm &&
								  peaks[0].db < peaks[l].db/2) {
								maxharm = harmonic;
								k = l;
				      }
				    }
				  }
					//displayFrequency(&peaks[l], lblFreq[l]);
				}

				displayFrequency(&peaks[k]);
      }

      memmove(fftSampleBuffer, fftSampleBuffer+stepSize, 
							(fftSize-stepSize)*sizeof(float));
    }
  }
#endif
#ifdef USE_DJBFFT
	float sample = *indata;
	fftr4_4(&sample);
	printf("f % 8.3f\n", sample);
#endif
}

static void
fftFree ()
{
#ifdef USE_FFTW
  fftwf_destroy_plan(fftPlan);
  fftwf_free(fftIn);
  free(fftSampleBuffer);
#endif
#ifdef USE_DJBFFT
#endif
#ifdef USE_GRAPH
	free(fftGraphData.db);
#endif
}


void *
detect_note(void *arg)
{
	jack_thread_info_t *info = (jack_thread_info_t *)arg;
	jack_nframes_t samples_per_frame = info->channels;
	size_t bytes_per_frame = samples_per_frame * sample_size, bytes;
	gboolean done = FALSE;
  float sample, *frame;
	int i;

	frame = (float *)alloca(bytes_per_frame);

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock(&detect_lock);
	info->status = 0;

	while (!done) {	
		while (info->can_capture &&
		      jack_ringbuffer_read_space(rb) >= bytes_per_frame) {

			jack_ringbuffer_read(rb, frame, bytes_per_frame);

			fftMeasure(1, bytes_per_frame, frame);
		}
		//wait for data
		pthread_cond_wait (&data_ready, &detect_lock);
	}

  pthread_mutex_unlock(&detect_lock);
}

int
jack_process(jack_nframes_t nframes, void *arg)
{
	int chn;
	size_t i;
	jack_thread_info_t *info = (jack_thread_info_t *) arg;

	/* Do nothing until we're ready to begin. */
	if ((!info->can_process) || (!info->can_capture))
		return 0;

	for (chn = 0; chn < nports; chn++)
		in[chn] = (jack_default_audio_sample_t*)
			jack_port_get_buffer(ports[chn], nframes);

	//interleave data
	for (i = 0; i < nframes; i++) {
		for (chn = 0; chn < nports; chn++) {
			if (jack_ringbuffer_write(rb, (char *)(in[chn]+i), sample_size)
				 < sample_size) {
				overruns++;
				if (overruns % 10000 == 0) {
					fprintf(stderr, "overruns [%u]\n", overruns);
				}
			}
		}
	}

	//notify detect_note that data is ready for reading
	if (pthread_mutex_trylock(&detect_lock) == 0) {
		pthread_cond_signal(&data_ready);
		pthread_mutex_unlock(&detect_lock);
	}

	return 0;
}


void
jack_shutdown (void *arg)
{
	fprintf (stderr, "JACK shutdown\n");
	// exit (0);
	abort();
}

void
wait_detect(jack_thread_info_t *info) {
	//pthread_join(info->thread_id, NULL);

	if (overruns > 0) {
		fprintf (stderr,
			 "show_note failed with %ld overruns.\n", overruns);
		fprintf (stderr, " try a bigger buffer than -B %u.\n",
						 info->rb_size);
		info->status = EPIPE;
	}
}

void
jack_client_init(long rb_size)
{
	jack_client_t *client;
	char *client_name;
	double aFreq = 440.0;
	pthread_attr_t attr;

	fftInit(latency);

  /* Initialize tuning */
	int i;
	freqs[0]=aFreq;
	lfreqs[0]=log(freqs[0]);
	for (i=1; i<12; i++) {
		freqs[i] = freqs[i-1] * D_NOTE;
		lfreqs[i] = lfreqs[i-1] + LOG_D_NOTE;
	}

	client_name = g_strdup_printf("show_note_%u", getpid());
	if ((client = jack_client_new(client_name)) == 0) {
		fprintf (stderr, "jack server not running?\n");
		exit (1);
	}

	/* try and run detect_note thread in realtime */
	pthread_attr_init(&attr);

	/*
	pthread_attr_setscope(&attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	*/

	memset (&thread_info, 0, sizeof (thread_info));
	thread_info.rb_size = rb_size;
	thread_info.client = client;
	thread_info.channels = 1;
	thread_info.can_process = 0;

	if (pthread_create (&thread_info.thread_id, &attr, detect_note, &thread_info)) {
		fprintf(stderr, "Error creating realtime thread!\n");
		abort();
	}
		
	jack_set_process_callback(client, jack_process, &thread_info);
	jack_on_shutdown(client, jack_shutdown, &thread_info);

	rate = jack_get_sample_rate(client);
	if (jack_activate(client)) {
		fprintf(stderr, "cannot activate client");
	}
}

void
jack_client_cleanup()
{
	jack_client_close(thread_info.client);

	/* seams when we get here, the following are already out of scope
	if (rb) jack_ringbuffer_free(rb);
	fftFree();
	*/
}

