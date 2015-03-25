.SUFFIXES: .in.js .js .in.html .html

# Mac OSX testing.
# This is useful when running in a userdir.
PREFIX		 = /Users/kristaps/Sites
HTDOCS		 = $(PREFIX)
HTURI		 = /~kristaps
LABURI		 = /~kristaps/lab.cgi
ADMINURI	 = /~kristaps/admin.cgi
CGIBIN		 = $(PREFIX)
DATADIR		 = $(PREFIX)
RDATADIR	 = $(PREFIX)
LIBS		 = 
STATIC		 = 

# OpenBSD production.
#PREFIX		 = /var/www
#HTDOCS		 = $(PREFIX)/htdocs/gamelab
#HTURI		 = /gamelab
#CGIBIN		 = $(PREFIX)/cgi-bin/gamelab
#LABURI		 = /cgi-bin/gamelab/lab
#ADMINURI	 = /cgi-bin/gamelab/admin
#DATADIR	 = $(PREFIX)/data/gamelab
#RDATADIR	 = /data/gamelab
#LIBS		 = -lintl -liconv
#STATIC		 = -static

# You really don't want to change anything below this line.

VERSION	 = 1.0.1
VMONTH	 = March
VYEAR	 = 2015
CFLAGS 	+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -I/usr/local/include
CFLAGS	+= -DDATADIR=\"$(RDATADIR)\" -DHTURI=\"$(HTURI)\"
PAGES 	 = addplayer.eml \
	   backupfail.eml \
	   backupsuccess.eml \
	   playerhome.html \
	   test.eml
BUILTPS	 = adminhome-new.html \
	   adminhome-started.html \
	   playerhome.html
SCREENS	 = screen-admin1.png \
	   screen-admin2.png \
	   screen-admin3.png \
	   screen-admin4.png \
	   screen-admin5.png \
	   screen-admin6.png \
	   screen-admin7.png
STATICS	 = style.css \
	   script.js
OBJS	 = compat-strtonum.o \
	   compat-strlcat.o \
	   compat-strlcpy.o \
	   db.o \
	   json.o \
	   mail.o \
	   mpq.o
SRCS	 = Makefile \
	   admin.c \
	   adminhome.in.js \
	   adminhome-new.in.html \
	   adminhome-started.in.html \
	   adminlogin.in.html \
	   compat-strlcat.c \
	   compat-strlcpy.c \
	   compat-strtonum.c \
	   db.c \
	   extern.h \
	   gamelab.sql \
	   json.c \
	   lab.c \
	   mail.c \
	   mpq.c \
	   playerhome.in.js \
	   playerlogin.in.html \
	   privacy.in.html \
	   test-strlcat.c \
	   test-strlcpy.c \
	   test-strtonum.c
BUILT	 = adminhome.js \
	   adminlogin.html \
	   playerhome.js \
	   playerlogin.html \
	   privacy.html
VERSIONS = version_1_0_1.xml

all: admin lab $(BUILT) $(BUILTPS)

admin: admin.o $(OBJS)
	$(CC) $(STATIC) -L/usr/local/lib -o $@ admin.o $(OBJS) -lsqlite3 -lkcgi -lkcgijson -lz -lgmp `curl-config --libs` $(LIBS)

lab: lab.o $(OBJS)
	$(CC) $(STATIC) -L/usr/local/lib -o $@ lab.o $(OBJS) -lsqlite3 -lkcgi -lkcgijson -lz -lgmp `curl-config --libs` $(LIBS)

admin.o lab.o $(OBJS): extern.h config.h

config.h: config.h.pre config.h.post configure $(TESTS)
	rm -f config.log
	CC="$(CC)" CFLAGS="$(CFLAGS)" ./configure

www: index.html manual.html gamelab.tgz gamelab.tgz.sha512 gamelab.bib gamelab.png

gamelab.bib: gamelab.in.bib
	sed -e "s!@VERSION@!$(VERSION)!g" \
	    -e "s!@VMONTH@!$(VMONTH)!g" \
	    -e "s!@VYEAR@!$(VYEAR)!g" gamelab.in.bib >$@

update: all
	install -m 0444 $(STATICS) $(BUILT) $(HTDOCS)
	install -m 0444 $(PAGES) $(BUILTPS) $(DATADIR)
	install -m 0755 admin $(CGIBIN)/admin.cgi
	install -m 0755 admin $(CGIBIN)
	install -m 0755 lab $(CGIBIN)/lab.cgi
	install -m 0755 lab $(CGIBIN)

install: update gamelab.db
	install -m 0666 gamelab.db $(DATADIR)

gamelab.tgz:
	mkdir -p .dist/gamelab-$(VERSION)
	cp $(SRCS) $(PAGES) $(STATICS) .dist/gamelab-$(VERSION)
	cp configure config.h.pre config.h.post .dist/gamelab-$(VERSION)
	(cd .dist && tar zcf ../$@ gamelab-$(VERSION))
	rm -rf .dist

gamelab.tgz.sha512: gamelab.tgz
	openssl dgst -sha512 gamelab.tgz >$@

installwww: www
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/snapshots
	install -m 0444 index.html gamelab.bib manual.html index.css manual.css $(PREFIX)
	install -m 0444 $(SCREENS) gamelab.png $(PREFIX)
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots/gamelab-$(VERSION).tgz
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots/gamelab-$(VERSION).tgz.sha512

gamelab.db: gamelab.sql
	rm -f $@
	sqlite3 $@ < gamelab.sql

gamelab.png: gamelab.dot
	dot -Tpng -o$@ gamelab.dot

manual.html: manual.xml
	sed "s!@VERSION@!$(VERSION)!g" manual.xml >$@

index.html: index.xml
	sblg -t index.xml -o- $(VERSIONS) | \
		sed "s!@VERSION@!$(VERSION)!g" >$@

.in.js.js:
	sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
		-e "s!@LABURI@!$(LABURI)!g" \
		-e "s!@HTURI@!$(HTURI)!g" $< >$@

.in.html.html:
	sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
		-e "s!@LABURI@!$(LABURI)!g" \
		-e "s!@HTURI@!$(HTURI)!g" $< >$@

clean:
	rm -f admin admin.o gamelab.db lab lab.o $(OBJS) config.h config.log 
	rm -f $(BUILT) $(BUILTPS) 
	rm -f index.html manual.html gamelab.tgz gamelab.tgz.sha512 gamelab.bib gamelab.png
	rm -rf *.dSYM
