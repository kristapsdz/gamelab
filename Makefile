CFLAGS 	+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -I/usr/local/include
PAGES 	 = adminhome.html \
	   adminlogin.html
PREFIX	 = /Users/kristaps/Sites
STATICS	 = mail.png lock.png style.css script.js

all: admin

admin: admin.o db.o
	$(CC) -L/usr/local/lib -o $@ admin.o db.o -lsqlite3 -lkcgi -lz

admin.o db.o: extern.h

installwww: all
	install -m 0444 $(PAGES) $(STATICS) $(PREFIX)
	install -m 0755 admin $(PREFIX)/admin.cgi

clean:
	rm -f admin admin.o
