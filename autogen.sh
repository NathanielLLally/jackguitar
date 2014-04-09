#!/bin/sh

aclocal
autoheader -Wall
automake --gnu --add-missing -Wall
autoconf -Wall
