# Makefile

prefix = /usr/local
bindir = $(prefix)/bin

distdir = dist

#CROSS = i686-mingw32-

CC = $(CROSS)gcc
LD = $(CROSS)gcc

CFLAGS = -Os -Wall

STRIP     = $(CROSS)strip
STRIP_OPT = -p

SRC := cascat.c casdec.c casenc.c castopcm.c pcmtocas.c cassette.c util.c
HDR := cascat.h casdec.h casenc.h castopcm.h pcmtocas.h cassette.h util.h
OBJ := cascat.o casdec.o casenc.o castopcm.o pcmtocas.o cassette.o util.o
BIN := cascat$(EXEEXT) casdec$(EXEEXT) casenc$(EXEEXT) castopcm$(EXEEXT) pcmtocas$(EXEEXT)


all: $(BIN)

clean:
	rm -f $(BIN) $(OBJ)

install: all
	mkdir -p $(DESTDIR)$(bindir)
	for f in $(BIN) ; do \
		install -m 755 "$$f" "$(DESTDIR)$(bindir)/$$f" ; \
	done

dist: all
	mkdir -p $(distdir)
	for f in $(BIN) ; do \
		install -m 755 "$$f" "$(distdir)/$$f" ; \
		$(STRIP) $(STRIP_OPT) "$(distdir)/$$f" ; \
	done

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

cascat$(EXEEXT): cascat.o cassette.o util.o Makefile
	$(LD) $(LDFLAGS) -o $@ cascat.o cassette.o util.o

casdec$(EXEEXT): casdec.o cassette.o util.o Makefile
	$(LD) $(LDFLAGS) -o $@ casdec.o cassette.o util.o

casenc$(EXEEXT): casenc.o cassette.o util.o Makefile
	$(LD) $(LDFLAGS) -o $@ casenc.o cassette.o util.o

castopcm$(EXEEXT): castopcm.o util.o Makefile
	$(LD) $(LDFLAGS) -o $@ castopcm.o util.o -lm

pcmtocas$(EXEEXT): pcmtocas.o util.o Makefile
	$(LD) $(LDFLAGS) -o $@ pcmtocas.o util.o -lm

cascat.o:	cascat.c cascat.h cassette.h util.h
casdec.o:	casdec.c casdec.h cassette.h util.h
casenc.o:	casenc.c casenc.h cassette.h util.h
castopcm.o:	castopcm.c castopcm.h util.h
pcmtocas.o:	pcmtocas.c pcmtocas.h util.h

cassette.o:	cassette.c cassette.h

util.o:		util.c util.h
