CFLAGS 	+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -I/usr/local/include
PAGES 	 = adminhome.html \
	   adminlogin.html \
	   style.css
STATICS	 = mail.png lock.png script.js ajax-loader.gif calendar.png

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

all: admin

admin: admin.o db.o
	$(CC) $(STATIC) -L/usr/local/lib -o $@ admin.o db.o -lsqlite3 -lkcgi -lz -lgmp

admin.o db.o: extern.h

installwww: all
	install -m 0444 $(STATICS) $(HTDOCS)
	install -m 0444 $(PAGES) $(DATADIR)
	install -m 0755 admin $(CGIBIN)/admin.cgi

install: installwww gamelab.db
	install -m 0666 gamelab.db $(DATADIR)

gamelab.db: gamelab.sql
	rm -f $@
	sqlite3 $@ < gamelab.sql

clean:
	rm -f admin admin.o gamelab.db
