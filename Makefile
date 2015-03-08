# Mac OSX testing.
PREFIX	 = /Users/kristaps/Sites
HTDOCS	 = $(PREFIX)
HTURI	 = /~kristaps
CGIBIN	 = $(PREFIX)
DATADIR	 = $(PREFIX)
RDATADIR = $(PREFIX)
LIBS	 = 
STATIC	 = 
# OpenBSD production.
#PREFIX	 = /var/www
#HTDOCS	 = $(PREFIX)/htdocs/gamelab
#HTURI	 = /gamelab
#CGIBIN	 = $(PREFIX)/cgi-bin/gamelab
#DATADIR	 = $(PREFIX)/data/gamelab
#RDATADIR = /data/gamelab
#LIBS	 = -lintl -liconv
#STATIC	 = -static

VERSION	 = 1.0.0
CFLAGS 	+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -I/usr/local/include
CFLAGS	+= -DDATADIR=\"$(RDATADIR)\" -DHTDOCS=\"$(HTURI)\"
PAGES 	 = addplayer.eml \
	   adminhome-new.html \
	   adminhome-started.html \
	   adminhome.js \
	   adminlogin.html \
	   backupfail.eml \
	   backupsuccess.eml \
	   playerhome.html \
	   playerhome.js \
	   playerlogin.html \
	   privacy.html \
	   test.eml
STATICS	 = style.css \
	   script.js
OBJS	 = compat-strtonum.o \
	   compat-strlcat.o \
	   compat-strlcpy.o \
	   db.o \
	   json.o \
	   mail.o \
	   mpq.o

all: admin lab

admin: admin.o $(OBJS)
	$(CC) $(STATIC) -L/usr/local/lib -o $@ admin.o $(OBJS) -lsqlite3 -lkcgi -lkcgijson -lz -lgmp `curl-config --libs` $(LIBS)

lab: lab.o $(OBJS)
	$(CC) $(STATIC) -L/usr/local/lib -o $@ lab.o $(OBJS) -lsqlite3 -lkcgi -lkcgijson -lz -lgmp `curl-config --libs` $(LIBS)

admin.o lab.o $(OBJS): extern.h config.h

config.h: config.h.pre config.h.post configure $(TESTS)
	rm -f config.log
	CC="$(CC)" CFLAGS="$(CFLAGS)" ./configure

www: index.html manual.html

update: all
	install -m 0444 $(STATICS) $(HTDOCS)
	install -m 0444 $(PAGES) $(DATADIR)
	install -m 0755 admin $(CGIBIN)/admin.cgi
	install -m 0755 admin $(CGIBIN)
	install -m 0755 lab $(CGIBIN)/lab.cgi
	install -m 0755 lab $(CGIBIN)

install: update gamelab.db
	install -m 0666 gamelab.db $(DATADIR)

gamelab.db: gamelab.sql
	rm -f $@
	sqlite3 $@ < gamelab.sql

manual.html: manual.xml
	sed "s!@VERSION@!$(VERSION)!g" manual.xml >$@

index.html: index.xml
	sed "s!@VERSION@!$(VERSION)!g" index.xml >$@

clean:
	rm -f admin admin.o gamelab.db lab lab.o $(OBJS) config.h config.log index.html manual.html
	rm -rf *.dSYM
