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

installwww: all
	install -m 0444 $(STATICS) $(HTDOCS)
	install -m 0444 $(PAGES) $(DATADIR)
	install -m 0755 admin $(CGIBIN)/admin.cgi
	install -m 0755 admin $(CGIBIN)
	install -m 0755 lab $(CGIBIN)/lab.cgi
	install -m 0755 lab $(CGIBIN)

install: installwww gamelab.db
	install -m 0666 gamelab.db $(DATADIR)

gamelab.db: gamelab.sql
	rm -f $@
	sqlite3 $@ < gamelab.sql

clean:
	rm -f admin admin.o gamelab.db lab lab.o $(OBJS) config.h config.log
	rm -rf *.dSYM
