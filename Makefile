.SUFFIXES: .min.js .js .html .xml

# Mechanical Turk testing.
MTURKURI	 = https://workersandbox.mturk.com/mturk/externalSubmit
#MTURKURI	 = https://www.mturk.com/mturk/externalSubmit

# Mac OSX testing.
# This is useful when running in a userdir.
#ADMINURI	 = /~kristaps/admin.cgi
#CGIBIN		 = $(PREFIX)
#CFLAGS		+= -Wno-deprecated-declarations
#DATADIR		 = $(PREFIX)
#DSYMUTIL	 = sudo dsymutil
#FONTURI		 = /~kristaps/font-awesome-4.4.0/css/font-awesome.min.css
#HTDOCS		 = $(PREFIX)
#HTURI		 = /~kristaps
#LABURI		 = /~kristaps/lab.cgi
#LIBS		 = 
#PREFIX		 = /Users/kristaps/Sites
#RDATADIR	 = $(PREFIX)
#STATIC		 = 

# Linux testing.
# LIBS		 = -lbsd -lm

# OpenBSD production.
PREFIX		?= /var/www/gamelab
URIPREFIX	?= /gamelab
RELPREFIX	?= /gamelab
ADMINURI	 = $(URIPREFIX)/cgi-bin/admin
CGIBIN		 = $(PREFIX)/cgi-bin
CFLAGS		+= -DLOGTIME=1
DATADIR	 	 = $(PREFIX)/data
FONTURI		 = //maxcdn.bootstrapcdn.com/font-awesome/4.4.0/css/font-awesome.min.css
HTDOCS		 = $(PREFIX)/htdocs
HTURI		 = $(URIPREFIX)
LABURI		 = $(URIPREFIX)/cgi-bin/lab
LIBS		 = -lintl -liconv -lm
RDATADIR	 = $(RELPREFIX)/data
STATIC		 = -static

# You really don't want to change anything below this line.

VERSION	 = 1.0.21
VMONTH	 = October
VYEAR	 = 2015
CFLAGS 	+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -I/usr/local/include
CFLAGS	+= -DDATADIR=\"$(RDATADIR)\" -DHTURI=\"$(HTURI)\"
INSTRS 	 = instructions-lottery.xml \
	   instructions-nolottery.xml \
	   instructions-mturk.xml
MAILS	 = mail-addplayer.eml \
	   mail-backupfail.eml \
	   mail-backupsuccess.eml \
	   mail-roundadvance.eml \
	   mail-roundfirst.eml \
	   mail-test.eml
BUILTPS	 = adminhome-new.html \
	   adminhome-started.html
STATICS	 = adminhome.css \
	   adminlogin.css \
	   mturkpreview1.png \
	   mturkpreview2.png \
	   playerhome.css \
	   playerlobby.css \
	   playerlogin.css \
	   style.css
OBJS	 = db.o \
	   json.o \
	   log.o \
	   mail.o \
	   mpq.o
SRCS	 = Makefile \
	   admin.c \
	   adminhome.js \
	   adminhome-new.xml \
	   adminhome-started.xml \
	   adminlogin.xml \
	   db.c \
	   extern.h \
	   gamelab.sql \
	   json.c \
	   lab.c \
	   mail.c \
	   mpq.c \
	   mturkpreview.xml \
	   playerautoadd.xml \
	   playerautoadd.js \
	   playerhome.xml \
	   playerhome.js \
	   playerlobby.xml \
	   playerlobby.js \
	   playermturk.xml \
	   playermturk.js \
	   playerlogin.xml \
	   privacy.xml \
	   script.js
BUILT	 = adminhome.min.js \
	   adminlogin.html \
	   mturkpreview.html \
	   playerautoadd.html \
	   playerautoadd.min.js \
	   playerlobby.html \
	   playerlobby.min.js \
	   playermturk.html \
	   playermturk.min.js \
	   playerhome.html \
	   playerhome.min.js \
	   playerlogin.html \
	   privacy.html \
	   script.min.js
IMAGES   = eskil.jpg \
	   jorgen.jpg \
	   kristaps.jpg
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
	   version_1_0_11.xml \
	   version_1_0_12.xml \
	   version_1_0_13.xml \
	   version_1_0_14.xml \
	   version_1_0_15.xml \
	   version_1_0_16.xml \
	   version_1_0_17.xml \
	   version_1_0_18.xml \
	   version_1_0_19.xml \
	   version_1_0_20.xml \
	   version_1_0_21.xml \
	   version_1_0_22.xml

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

updatecgi: all
	mkdir -p $(HTDOCS)
	mkdir -p $(DATADIR)
	mkdir -p $(CGIBIN)
	install -m 0444 $(STATICS) $(BUILT) flotr2.min.js logo.png logo-dark.png $(HTDOCS)
	for f in $(INSTRS) ; do install -m 0444 $$f $(HTDOCS)/`basename $$f`.txt ; done
	install -m 0444 $(INSTRS) $(MAILS) $(BUILTPS) $(DATADIR)
	install -m 0755 admin $(CGIBIN)/admin.cgi
	install -m 0755 admin $(CGIBIN)
	install -m 0755 lab $(CGIBIN)/lab.cgi
	install -m 0755 lab $(CGIBIN)/lab.fcgi
	install -m 0755 lab $(CGIBIN)
	[ -z "$(DSYMUTIL)" ] || $(DSYMUTIL) $(CGIBIN)/lab
	[ -z "$(DSYMUTIL)" ] || $(DSYMUTIL) $(CGIBIN)/admin

installcgi: updatecgi gamelab.db
	mkdir -p $(DATADIR)
	rm -f $(DATADIR)/gamelab.db
	rm -f $(DATADIR)/gamelab.db-wal
	rm -f $(DATADIR)/gamelab.db-shm
	install -m 0666 gamelab.db $(DATADIR)
	chmod 0777 $(DATADIR)

gamelab.tgz:
	mkdir -p .dist/gamelab-$(VERSION)
	cp $(SRCS) $(INSTRS) $(MAILS) $(STATICS) .dist/gamelab-$(VERSION)
	(cd .dist && tar zcf ../$@ gamelab-$(VERSION))
	rm -rf .dist

gamelab.tgz.sha512: gamelab.tgz
	openssl dgst -sha512 gamelab.tgz >$@

installwww: www
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/snapshots
	install -m 0444 $(IMAGES) index.html gamelab.bib manual.html index.css manual.css logo.png $(PREFIX)
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots/gamelab-$(VERSION).tgz
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots/gamelab-$(VERSION).tgz.sha512

gamelab.db: gamelab.sql
	rm -f $@
	sqlite3 $@ < gamelab.sql

index.html: index.xml $(VERSIONS)
	sblg -t index.xml -o- $(VERSIONS) | \
		sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
			-e "s!@LABURI@!$(LABURI)!g" \
			-e "s!@FONTURI@!$(FONTURI)!g" \
			-e "s!@VERSION@!$(VERSION)!g" \
			-e "s!@MTURKURI@!$(MTURKURI)!g" \
			-e "s!@HTURI@!$(HTURI)!g" >$@

adminhome.js playerautoadd.js playerlobby.js playerhome.js playermturk.js script.js: jsmin

.js.min.js:
	rm -f $@
	sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
		-e "s!@LABURI@!$(LABURI)!g" \
		-e "s!@MTURKURI@!$(MTURKURI)!g" \
		-e "s!@HTURI@!$(HTURI)!g" $< | ./jsmin > $@
	chmod 444 $@

.xml.html:
	sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
		-e "s!@LABURI@!$(LABURI)!g" \
		-e "s!@FONTURI@!$(FONTURI)!g" \
		-e "s!@VERSION@!$(VERSION)!g" \
		-e "s!@MTURKURI@!$(MTURKURI)!g" \
		-e "s!@HTURI@!$(HTURI)!g" $< >$@

clean:
	rm -f admin admin.o gamelab.db lab lab.o $(OBJS) jsmin gamers
	rm -f $(BUILT) $(BUILTPS) 
	rm -f index.html manual.html gamelab.tgz gamelab.tgz.sha512 gamelab.bib
	rm -rf *.dSYM
