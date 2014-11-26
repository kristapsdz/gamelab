CFLAGS 	+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -I/usr/local/include
PAGES 	 = addplayer.eml \
	   adminhome-new.html \
	   adminhome-started.html \
	   adminhome.js \
	   adminlogin.html \
	   playerhome.html \
	   playerhome.js \
	   playerlogin.html \
	   style.css \
	   test.eml
STATICS	 = script.js

# Mac OSX testing.
PREFIX	 = /Users/kristaps/Sites
HTDOCS	 = $(PREFIX)
CGIBIN	 = $(PREFIX)
DATADIR	 = $(PREFIX)
STATIC	 = 
# OpenBSD production.
# PREFIX	 = /var/www
# HTDOCS	 = $(PREFIX)/htdocs/gamelab
# CGIBIN	 = $(PREFIX)/cgi-bin/gamelab
# DATADIR	 = $(PREFIX)/data/gamelab
# STATIC	 = -static

all: admin lab

admin: admin.o db.o mail.o json.o
	$(CC) $(STATIC) -L/usr/local/lib -o $@ admin.o db.o mail.o json.o -lsqlite3 -lkcgi -lkcgijson -lz -lgmp `curl-config --libs`

lab: lab.o db.o mail.o json.o
	$(CC) $(STATIC) -L/usr/local/lib -o $@ lab.o db.o mail.o json.o -lsqlite3 -lkcgi -lkcgijson -lz -lgmp `curl-config --libs`

admin.o lab.o db.o mail.o: extern.h

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
	rm -f admin admin.o gamelab.db lab lab.o db.o mail.o json.o
