AM_CPPFLAGS = \
        -DPACKAGE_DATA_DIR=\""$(datadir)"\" \
        -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
        @PACKAGE_CFLAGS@

AM_CFLAGS = 
jackguitar_CFLAGS = -ggdb -ffast-math -I/usr/local/djbfft/include
jackguitar_LDADD = @PACKAGE_LIBS@ /usr/local/djbfft/lib/djbfft.a

bin_PROGRAMS=jackguitar
jackguitar_SOURCES=\
	fretboard.c \
	fretboard.h \
	scales.c \
	scales.h \
	jack_client.c \
	jack_client.h \
	fftgraph.c \
	fftgraph.h \
	main.c

