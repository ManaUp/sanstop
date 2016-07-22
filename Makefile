CC=gcc
CFLAGS=-std=c11 -Wall -Werror -O3 -I/usr/include/freetype2

release: sanstop

sanstop: sanstop.o utf8.o
	$(CC) $(CFLAGS) sanstop.o utf8.o -o sanstop -static -lfreetype -lpng -lz -lbz2

sanstop.o: sanstop.c sanstop.h
	$(CC) $(CFLAGS) -c sanstop.c

utf8.o: utf8.c utf8.h
	$(CC) $(CFLAGS) -c utf8.c

yukkuri:
	@echo "ゆっくりしていってね！"
