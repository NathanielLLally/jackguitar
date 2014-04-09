/**
 * $Id: main.c,v 1.12 2006/01/23 14:20:42 nate Exp $
 *
 * Test fretboard widgetin a GtkWindow
 *
 * Authors:
 *   Nate Lally <nate.lally@gmail.com>
 *
 * This is free software, placed under the terms of the
 * GNU General Public License, as published by the Free Software Foundation.
 * Please see the file COPYING for details.
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "config.h"
#include "fretboard.h"
#include "jack_client.h"
#include "fftgraph.h"

#define DBG_LVL 0
#define DEBUG(...) { if (DBG_LVL) {		\
			fprintf(stderr, __VA_ARGS__);	\
			fflush(stderr); } }

extern volatile jack_thread_info_t thread_info;
extern volatile freq_data note_input;
extern volatile graph_data fftGraphData;
extern Tuning *tuning;
extern int rate;
extern int latency;

static GtkWidget *cmbNote;
static GtkWidget *cmbTuning;
static GtkWidget *cmbSet;
static GtkWidget *cmbKey;
static GtkWidget *cmbScale;
GtkWidget *lblFreq[15];
GtkWidget *tblFreq;

static void
jackListPorts ()
{
  int i;
  const char **ports = jack_get_ports(thread_info.client, NULL, NULL, 0);

  for (i=0; ports[i]; i++) {
    jack_port_t *port = jack_port_by_name(thread_info.client, ports[i]);

    if (port && jack_port_flags(port) & JackPortIsOutput)
      printf("%s\n", ports[i]);
  }
}

/***********************************************************************/

static void
set_unset_event(GtkWidget *widget,
              gpointer   data)
{
	const gchar *entry_text;
	char **notes = NULL;
	int mode = NOTE_ON, i = 0;

	Fretboard *fretboard = FRETBOARD(data);

	if (g_ascii_strcasecmp
			("GtkButton", gtk_widget_get_name(widget)) == 0) {
		if (strstr(gtk_button_get_label(GTK_BUTTON(widget)), "unset")) {
			mode = NOTE_OFF;
		} else if (strstr(gtk_button_get_label(GTK_BUTTON(widget)),
										 "highlight")) {
			mode = NOTE_HIGHLIGHT;
		} else if (strstr(gtk_button_get_label(GTK_BUTTON(widget)),
										 "reverse")) {
			mode = NOTE_REVERSE;
		}

		if (strstr(gtk_button_get_label(GTK_BUTTON(widget)), "note")) {
			entry_text = gtk_entry_get_text
				(GTK_ENTRY(GTK_COMBO(cmbNote)->entry));
		} else if (strstr(gtk_button_get_label(GTK_BUTTON(widget)),
										 "scale")) {
			entry_text = gtk_entry_get_text
				(GTK_ENTRY(GTK_COMBO(cmbSet)->entry));
			notes = find_scale((char *)entry_text);
		}
	} else	if (g_ascii_strcasecmp
			("GtkEntry", gtk_widget_get_name(widget)) == 0) {

		entry_text = gtk_entry_get_text
			(GTK_ENTRY(GTK_COMBO(cmbTuning)->entry));

		tuning = find_tuning(entry_text);
	}

	if (notes) {
		for (i = 0; notes[i] != NULL; i++) {
			set_note_all(fretboard, notes[i], mode);
		}	
	} else {
		set_note_all(fretboard, (char *)entry_text, mode);
	}
}

static void
scale_set_unset_event(GtkWidget *widget,
								gpointer   data)
{
	char **notes = NULL;
	const char *key = NULL, *scale = NULL;
	int i = 0;

	Fretboard *fretboard = FRETBOARD(data);

	key = gtk_entry_get_text
		(GTK_ENTRY(GTK_COMBO(cmbKey)->entry));
	scale = gtk_entry_get_text
		(GTK_ENTRY(GTK_COMBO(cmbScale)->entry));

	notes = build_scale(key, scale);

	DEBUG("key [%s] scale [%s]: \n", key, scale);
	set_notes(fretboard, notes, NOTE_ON);			
	set_note_all(fretboard, key, NOTE_REVERSE);

	if (notes) free(notes);
}

gint
redraw_callback(gpointer data)
{
	static char *n;
	static int o;
	Fretboard *fb = (Fretboard *)data;
	char *tmp;

	if (n == NULL) n = "";
	if (note_input.note == NULL) note_input.note = "";
	
	if ((strcmp(note_input.note, n) != 0) ||
			o != note_input.octave) {
		restore_note_state(fb, n, o);

		n = note_input.note;
		o = note_input.octave;

		save_note_state(fb, n, o);
		set_note(fb, n, o, NOTE_HIGHLIGHT);
	}

	gtk_label_set_text(GTK_LABEL(lblFreq[5]), n);

	tmp = g_strdup_printf("%1.f", note_input.octave);
	gtk_label_set_text(GTK_LABEL(lblFreq[6]), tmp);  free(tmp);

	tmp = g_strdup_printf("%8.3fHz", note_input.nfreq);
	gtk_label_set_text(GTK_LABEL(lblFreq[7]), tmp);  free(tmp);

	tmp = g_strdup_printf("%8.3fHz", note_input.freq);
	gtk_label_set_text(GTK_LABEL(lblFreq[12]), tmp);  free(tmp);

	tmp = g_strdup_printf("%+3.f", note_input.cents);
	gtk_label_set_text(GTK_LABEL(lblFreq[13]), tmp);  free(tmp);
										 
	tmp = g_strdup_printf("%+3.3fdB", note_input.db);
	gtk_label_set_text(GTK_LABEL(lblFreq[14]), tmp);  free(tmp);

 	gtk_widget_queue_draw(tblFreq);
	return TRUE;
}

gint
update_graph_callback(gpointer data)
{
	FFTGraph *graph = FFTGRAPH(data);

	set_graph_data(graph, &fftGraphData);

	return TRUE;
}

void
main_quit(void)
{
	wait_detect(&thread_info);
	jack_client_cleanup();

	gtk_main_quit();
}

static GtkItemFactoryEntry menu_items[] = {
  { "/_File",         NULL,         NULL,           0, "<Branch>" },
  { "/File/_New",     "<control>N", NULL,    0, "<StockItem>", GTK_STOCK_NEW },
  { "/File/_Open",    "<control>O", NULL,    0, "<StockItem>", GTK_STOCK_OPEN },
  { "/File/_Save",    "<control>S", NULL,    0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/Save _As", NULL,         NULL,           0, "<Item>" },
  { "/File/sep1",     NULL,         NULL,           0, "<Separator>" },
  { "/File/_Quit",    "<CTRL>Q", main_quit, 0, "<StockItem>", GTK_STOCK_QUIT },
  { "/_Options",      NULL,         NULL,           0, "<Branch>" },
  { "/Options/tear",  NULL,         NULL,           0, "<Tearoff>" },
  { "/Options/Check", NULL,         NULL,   1, "<CheckItem>" },
  { "/Options/sep",   NULL,         NULL,           0, "<Separator>" },
  { "/Options/Rad1",  NULL,         NULL, 1, "<RadioItem>" },
  { "/Options/Rad2",  NULL,         NULL, 2, "/Options/Rad1" },
  { "/Options/Rad3",  NULL,         NULL, 3, "/Options/Rad1" },
  { "/_Help",         NULL,         NULL,           0, "<LastBranch>" },
  { "/_Help/About",   NULL,         NULL,           0, "<Item>" },
};

static gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

/* Returns a menubar widget made from the above menu */
static GtkWidget *get_menubar_menu( GtkWidget  *window )
{
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;

  /* Make an accelerator group (shortcut keys) */
  accel_group = gtk_accel_group_new ();

  /* Make an ItemFactory (that makes a menubar) */
  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>",
                                       accel_group);

  /* This function generates the menu items. Pass the item factory,
     the number of items in the array, the array itself, and any
     callback data for the the menu items. */
  gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);

  /* Attach the new accelerator group to the window. */
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

  /* Finally, return the actual menu bar created by the item factory. */
  return gtk_item_factory_get_widget (item_factory, "<main>");
}

int
main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *hbxTuning;
	GtkWidget *hbxGraph;
	GtkWidget *btnNoteSet;
	GtkWidget *btnNoteUnset;
	GtkWidget *btnNoteHighlight;
	GtkWidget *btnNoteReverse;
	GtkWidget *frmFret;
	GtkWidget *frmTuner;
	GtkWidget *fretboard;
	GtkWidget *fftGraph;
	GtkWidget *lblNote;
	GtkWidget *lblSet;
	GtkWidget *lblTuning;
	GtkWidget *lblKey;
	GtkWidget *lblScale;
	GtkWidget *btnSetSet;
	GtkWidget *btnSetUnset;
	GtkWidget *btnScaleSet;
	GtkWidget *menubar;
	GList *lNotes = NULL;
	GList *lTuning = NULL;
	GList *lSet = NULL;
	GList *lKey = NULL;
	GList *lScale = NULL;
	int i, c, j;
	char *captureDevice = NULL;
	char **sources, **ports;
	long rb_size =  DEFAULT_RB_SIZE;
	
	signal(SIGPIPE, SIG_IGN);

	jack_client_init(rb_size);

	while ((c = getopt(argc, argv, "lR:")) != -1) {
		switch (c)
			{
			case 'l':
				jackListPorts();
				exit(1);
				break;
			case 'R':
				rb_size = atol(optarg);
				break;
			default:
				printf("%s [capture port]\n\n", argv[0]);
				break;
		}
	}

	if (optind < argc) {
		captureDevice = argv[optind++];
	}

	gtk_init (&argc, &argv);
/***********************************************************************/

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "jackGuitar");
	gtk_container_set_border_width (GTK_CONTAINER(window), 2);

	menubar = get_menubar_menu(window);

	//gtk_window_set_default_size(window, 1280, 300); 

	frmFret = gtk_frame_new(NULL);
	//gtk_frame_set_label (GTK_frmFret (frmFret), "Fretboard");
	gtk_container_set_border_width (GTK_CONTAINER(frmFret), 0);
	
	fretboard = fretboard_new();
	gtk_container_add(GTK_CONTAINER(frmFret), fretboard);

#ifdef USE_GRAPH
	hbxGraph = gtk_hbox_new(FALSE, 0);
	fftGraph = fftgraph_new();
	gtk_box_pack_start(GTK_BOX(hbxGraph), fftGraph, FALSE, FALSE, 0);
#endif

/*****  tuning row  **********************************************************/	

	lblTuning = gtk_label_new("Tuning: ");
	lblKey = gtk_label_new("Key: ");
	lblScale = gtk_label_new("Scale: ");

	btnScaleSet = gtk_button_new_with_label(" set ");

	cmbTuning = gtk_combo_new();
	gtk_widget_set_size_request (cmbTuning, 120, 20);

	for (i = 0; tunings[i].name != NULL; i++) {
		lTuning = g_list_append(lTuning, tunings[i].name);
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(cmbTuning), lTuning);
	g_list_free(lTuning);


	cmbKey = gtk_combo_new();
	gtk_entry_set_max_length(GTK_ENTRY(GTK_COMBO(cmbKey)->entry), 5);

	gtk_widget_set_size_request (cmbKey, 80, 20);

	lKey = g_list_append(lKey, "");
	for (i = 0; scale_chromatic[i] != NULL; i++) {
		lKey = g_list_append(lKey, scale_chromatic[i]);
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(cmbKey), lKey);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(cmbKey)->entry), "");
	g_list_free(lKey);

	cmbScale = gtk_combo_new();
	gtk_widget_set_size_request (cmbScale, 160, 20);

	for (i = 0; patterns[i].name != NULL; i++) {
		lScale = g_list_append(lScale, patterns[i].name);
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(cmbScale), lScale);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(cmbScale)->entry), "");
	g_list_free(lScale);


	g_signal_connect(GTK_ENTRY(GTK_COMBO(cmbTuning)->entry), "changed",
			G_CALLBACK(set_unset_event), fretboard);

	g_signal_connect(btnScaleSet, "clicked",
			G_CALLBACK(scale_set_unset_event), fretboard);
	g_signal_connect(GTK_ENTRY(GTK_COMBO(cmbKey)->entry), "changed",
			G_CALLBACK(scale_set_unset_event), fretboard);
	g_signal_connect(GTK_ENTRY(GTK_COMBO(cmbScale)->entry), "changed",
			G_CALLBACK(scale_set_unset_event), fretboard);

	hbxTuning = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbxTuning), lblTuning, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbxTuning), cmbTuning, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbxTuning), lblKey, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbxTuning), cmbKey, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbxTuning), lblScale, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbxTuning), cmbScale, FALSE, FALSE, 0);
 	//gtk_box_pack_start(GTK_BOX(hbxTuning), btnScaleSet, FALSE, FALSE, 0);


/*****  Frequency Display **********************************************/

	tblFreq = gtk_table_new( 3, 5, TRUE);

	frmTuner = gtk_frame_new(NULL);
	gtk_container_set_border_width(GTK_CONTAINER(frmTuner), 0);

	//table header
	lblFreq[0] = gtk_label_new("Note");
	lblFreq[1] = gtk_label_new("Octave");
	lblFreq[2] = gtk_label_new("Frequency");
	lblFreq[3] = gtk_label_new("Cents");
	lblFreq[4] = gtk_label_new("Level");

	//note row
	lblFreq[5] = gtk_label_new("A    ");
	lblFreq[6] = gtk_label_new("4");
	lblFreq[7] = gtk_label_new("440.000");
	lblFreq[8] = gtk_label_new("");
	lblFreq[9] = gtk_label_new("");

	//input row
	lblFreq[10] = gtk_label_new("");
	lblFreq[11] = gtk_label_new("");
	lblFreq[12] = gtk_label_new("000.000");
	lblFreq[13] = gtk_label_new("+00");
	lblFreq[14] = gtk_label_new("-200");

	for (i=0; i<5; i++)
		gtk_label_set_pattern (GTK_LABEL(lblFreq[i]), "__________");
	
	for (i = 0; i < 5; i++)
		gtk_table_attach_defaults
			(GTK_TABLE(tblFreq), lblFreq[i], i, i+1, 0, 1);

	for (i = 0; i < 5; i++)
		gtk_table_attach_defaults
			(GTK_TABLE(tblFreq), lblFreq[i+5], i, i+1, 1, 2);

	for (i = 0; i < 5; i++)
		gtk_table_attach_defaults
			(GTK_TABLE(tblFreq), lblFreq[i+10], i, i+1, 2, 3);

	gtk_box_pack_start(GTK_BOX(hbxTuning), tblFreq, TRUE, TRUE, 3);

/******  set/unset row  *******************************************************/

	cmbNote = gtk_combo_new();
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(cmbNote)->entry), "A");
	gtk_entry_set_max_length(GTK_ENTRY(GTK_COMBO(cmbNote)->entry), 5);

	gtk_widget_set_size_request (cmbNote, 80, 20);
	for (i = 0; scale_chromatic[i] != NULL; i++) {
		lNotes = g_list_append(lNotes, scale_chromatic[i]);
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(cmbNote), lNotes);
	g_list_free(lNotes);
	
	btnNoteSet = gtk_button_new_with_label("set note");
	g_signal_connect (btnNoteSet, "clicked",
			G_CALLBACK(set_unset_event), fretboard);

	btnNoteUnset = gtk_button_new_with_label("unset note");
	g_signal_connect (btnNoteUnset, "clicked",
			G_CALLBACK(set_unset_event), fretboard);

	btnNoteHighlight = gtk_button_new_with_label("highlight note");
	g_signal_connect (btnNoteHighlight, "clicked",
			G_CALLBACK(set_unset_event), fretboard);

	btnNoteReverse = gtk_button_new_with_label("reverse note");
	g_signal_connect (btnNoteReverse, "clicked",
			G_CALLBACK(set_unset_event), fretboard);

	lblNote = gtk_label_new("Note: ");

	btnSetSet = gtk_button_new_with_label("set scale");
	g_signal_connect (btnSetSet, "clicked",
			G_CALLBACK(set_unset_event), fretboard);

	btnSetUnset = gtk_button_new_with_label("unset scale");
	g_signal_connect (btnSetUnset, "clicked",
			G_CALLBACK(set_unset_event), fretboard);

	lblSet = gtk_label_new("Set: ");

	cmbSet = gtk_combo_new();
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(cmbSet)->entry), "fours1");
	gtk_entry_set_max_length(GTK_ENTRY(GTK_COMBO(cmbSet)->entry), 35);

	gtk_widget_set_size_request (cmbNote, 80, 20);
	for (i = 0; scale_register[i].name != NULL; i++) {
		lSet = g_list_append(lSet, scale_register[i].name);
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(cmbSet), lSet);

	hbox = gtk_hbox_new(FALSE, 2);

	gtk_box_pack_start(GTK_BOX(hbox), btnNoteSet, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), btnNoteUnset, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), btnNoteHighlight, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), btnNoteReverse, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), lblNote, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cmbNote, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), btnSetSet, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), btnSetUnset, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), lblSet, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), cmbSet, FALSE, FALSE, 0);

/***********************************************************************/

	//	gtk_container_add(GTK_CONTAINER(frmTuner), hbxFreq);
	//packing

	vbox = gtk_vbox_new(FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), menubar, TRUE, TRUE, 0);

#ifdef USE_GRAPH
	gtk_box_pack_start(GTK_BOX(vbox), hbxGraph, FALSE, FALSE, 3);
#endif

	gtk_box_pack_start(GTK_BOX(vbox), hbxTuning, FALSE, FALSE, 3);
	if (DBG_LVL)
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), frmFret, TRUE, TRUE, 3);
	gtk_container_add(GTK_CONTAINER (window), vbox);

	g_signal_connect (window, "destroy",G_CALLBACK(main_quit), NULL);

	g_signal_connect (window, "delete_event",
			G_CALLBACK(main_quit), NULL);

	gtk_widget_show_all (window);

//	jackRun();
//	g_timeout_add(1, jackIter, NULL);

	sources = alloca(1024);
	ports = jack_get_ports(thread_info.client, NULL, NULL, 0);

	j = 0;
	if (captureDevice) sources[j++] = captureDevice;

  for (i=0; ports[i]; i++, j++) {
		sources[j] = ports[i];
	}

	setup_ports(thread_info.channels, sources, &thread_info);
	thread_info.can_capture = 1;

/* setup a redraw timer for the frequency display */
	g_timeout_add (16, redraw_callback, (void *)fretboard);

#ifdef USE_GRAPH
	g_timeout_add (16, update_graph_callback, (void *)fftGraph);
#endif

	gtk_main ();

}
