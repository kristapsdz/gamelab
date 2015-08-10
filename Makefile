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

# Linux testing.
# LIBS		 = -lbsd -lm

# OpenBSD production.
#PREFIX		 = /var/www
#HTDOCS		 = $(PREFIX)/htdocs/gamelab
#HTURI		 = /gamelab
#CGIBIN		 = $(PREFIX)/cgi-bin/gamelab
#LABURI		 = /cgi-bin/gamelab/lab
#ADMINURI	 = /cgi-bin/gamelab/admin
#DATADIR	 	 = $(PREFIX)/data/gamelab
#RDATADIR	 = /data/gamelab
#LIBS		 = -lintl -liconv -lm
#STATIC		 = -static

# You really don't want to change anything below this line.

VERSION	 = 1.0.10
VMONTH	 = August
VYEAR	 = 2015
CFLAGS 	+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -I/usr/local/include
CFLAGS	+= -DDATADIR=\"$(RDATADIR)\" -DHTURI=\"$(HTURI)\"
PAGES 	 = addplayer.eml \
	   backupfail.eml \
	   backupsuccess.eml \
	   test.eml
BUILTPS	 = adminhome-new.html \
	   adminhome-started.html
SCREENS	 = screen-admin1.png \
	   screen-admin2.png \
	   screen-admin3.png \
	   screen-admin4.png \
	   screen-admin5.png \
	   screen-admin6.png \
	   screen-admin7.png
STATICS	 = adminhome.css \
	   adminlogin.css \
	   playerhome.css \
	   playerlobby.css \
	   playerlogin.css \
	   style.css \
	   script.js
OBJS	 = db.o \
	   json.o \
	   mail.o \
	   mpq.o
SRCS	 = Makefile \
	   admin.c \
	   adminhome.in.js \
	   adminhome-new.in.html \
	   adminhome-started.in.html \
	   adminlogin.in.html \
	   db.c \
	   extern.h \
	   gamelab.sql \
	   json.c \
	   lab.c \
	   mail.c \
	   mpq.c \
	   playerautoadd.in.html \
	   playerautoadd.in.js \
	   playerhome.in.html \
	   playerhome.in.js \
	   playerlobby.in.html \
	   playerlogin.in.html \
	   privacy.in.html
BUILT	 = adminhome.js \
	   adminlogin.html \
	   playerautoadd.html \
	   playerautoadd.js \
	   playerlobby.html \
	   playerhome.html \
	   playerhome.js \
	   playerlogin.html \
	   privacy.html
VERSIONS = version_1_0_1.xml \
	   version_1_0_2.xml \
	   version_1_0_3.xml \
	   version_1_0_4.xml \
	   version_1_0_5.xml \
	   version_1_0_6.xml \
	   version_1_0_7.xml \
	   version_1_0_8.xml \
	   version_1_0_9.xml \
	   version_1_0_10.xml \
	   version_1_0_11.xml

all: admin lab gamers $(BUILT) $(BUILTPS)

jsmin: jsmin.c
	$(CC) $(CFLAGS) -o $@ jsmin.c

gamers: gamers.c
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ gamers.c `curl-config --libs` -ljson-c -lm

admin: admin.o $(OBJS)
	$(CC) $(STATIC) -L/usr/local/lib -o $@ admin.o $(OBJS) -lsqlite3 -lkcgi -lkcgijson -lz -lgmp `curl-config --libs` $(LIBS)

lab: lab.o $(OBJS)
	$(CC) $(STATIC) -L/usr/local/lib -o $@ lab.o $(OBJS) -lsqlite3 -lkcgi -lkcgijson -lz -lgmp `curl-config --libs` $(LIBS)

admin.o lab.o $(OBJS): extern.h

www: index.html manual.html gamelab.tgz gamelab.tgz.sha512 gamelab.bib 

gamelab.bib: gamelab.in.bib
	sed -e "s!@VERSION@!$(VERSION)!g" \
	    -e "s!@VMONTH@!$(VMONTH)!g" \
	    -e "s!@VYEAR@!$(VYEAR)!g" gamelab.in.bib >$@

update: all
	install -m 0444 $(STATICS) $(BUILT) flotr2.min.js logo.png logo-dark.png $(HTDOCS)
	install -m 0444 $(PAGES) $(BUILTPS) $(DATADIR)
	install -m 0755 admin $(CGIBIN)/admin.cgi
	install -m 0755 admin $(CGIBIN)
	install -m 0755 lab $(CGIBIN)/lab.cgi
	install -m 0755 lab $(CGIBIN)/lab.fcgi
	install -m 0755 lab $(CGIBIN)

install: update gamelab.db
	install -m 0666 gamelab.db $(DATADIR)

gamelab.tgz:
	mkdir -p .dist/gamelab-$(VERSION)
	cp $(SRCS) $(PAGES) $(STATICS) .dist/gamelab-$(VERSION)
	(cd .dist && tar zcf ../$@ gamelab-$(VERSION))
	rm -rf .dist

gamelab.tgz.sha512: gamelab.tgz
	openssl dgst -sha512 gamelab.tgz >$@

installwww: www
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/snapshots
	install -m 0444 index.html gamelab.bib manual.html index.css manual.css logo.png bchs-logo.png $(PREFIX)
	install -m 0444 $(SCREENS) $(PREFIX)
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots/gamelab-$(VERSION).tgz
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots/gamelab-$(VERSION).tgz.sha512

gamelab.db: gamelab.sql
	rm -f $@
	sqlite3 $@ < gamelab.sql

manual.html: manual.xml
	sed "s!@VERSION@!$(VERSION)!g" manual.xml >$@

index.html: index.xml $(VERSIONS)
	sblg -t index.xml -o- $(VERSIONS) | \
		sed "s!@VERSION@!$(VERSION)!g" >$@

adminhome.js playerautoadd.js playerhome.js: jsmin

.in.js.js:
	rm -f $@
	sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
		-e "s!@LABURI@!$(LABURI)!g" \
		-e "s!@HTURI@!$(HTURI)!g" $< | ./jsmin >$@
	chmod 444 $@

.in.html.html:
	rm -f $@
	sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
		-e "s!@LABURI@!$(LABURI)!g" \
		-e "s!@VERSION@!$(VERSION)!g" \
		-e "s!@HTURI@!$(HTURI)!g" $< >$@
	chmod 444 $@

clean:
	rm -f admin admin.o gamelab.db lab lab.o $(OBJS) jsmin gamers
	rm -f $(BUILT) $(BUILTPS) 
	rm -f index.html manual.html gamelab.tgz gamelab.tgz.sha512 gamelab.bib
	rm -rf *.dSYM
