.SUFFIXES: .in.js .js .html .xml

# Mechanical Turk testing.
MTURKURI	 = https://workersandbox.mturk.com/mturk/externalSubmit
#MTURKURI	 = https://www.mturk.com/mturk/externalSubmit
FONTURI		 = //maxcdn.bootstrapcdn.com/font-awesome/4.4.0/css/font-awesome.min.css

# Mac OSX testing.
# This is useful when running in a userdir.
ADMINURI	 = /~kristaps/admin.cgi
CGIBIN		 = $(PREFIX)
CFLAGS		+= -Wno-deprecated-declarations
DATADIR		 = $(PREFIX)
DSYMUTIL	 = sudo dsymutil
HTDOCS		 = $(PREFIX)
HTURI		 = /~kristaps
LABURI		 = /~kristaps/lab.cgi
LIBS		 = 
PREFIX		 = /Users/kristaps/Sites
RDATADIR	 = $(PREFIX)
STATIC		 = 

# Linux testing.
# LIBS		 = -lbsd -lm

# OpenBSD production.
#ADMINURI	 = /cgi-bin/gamelab/admin
#CGIBIN		 = $(PREFIX)/cgi-bin/gamelab
#CFLAGS		+= -DLOGTIME=1
#DATADIR	 	 = $(PREFIX)/data/gamelab
#HTDOCS		 = $(PREFIX)/htdocs/gamelab
#HTURI		 = /gamelab
#LABURI		 = /cgi-bin/gamelab/lab
#LIBS		 = -lintl -liconv -lm
#PREFIX		 = /var/www
#RDATADIR	 = /data/gamelab
#STATIC		 = -static

# You really don't want to change anything below this line.

VERSION	 = 1.0.20
VMONTH	 = September
VYEAR	 = 2015
CFLAGS 	+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -I/usr/local/include
CFLAGS	+= -DDATADIR=\"$(RDATADIR)\" -DHTURI=\"$(HTURI)\"
PAGES 	 = instructions-lottery.html \
	   instructions-nolottery.html \
	   instructions-mturk.html \
	   mail-addplayer.eml \
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
	   adminhome.in.js \
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
	   playerautoadd.in.js \
	   playerhome.xml \
	   playerhome.in.js \
	   playerlobby.xml \
	   playerlobby.in.js \
	   playermturk.xml \
	   playermturk.in.js \
	   playerlogin.xml \
	   privacy.xml \
	   script.in.js
BUILT	 = adminhome.js \
	   adminlogin.html \
	   mturkpreview.html \
	   playerautoadd.html \
	   playerautoadd.js \
	   playerlobby.html \
	   playerlobby.js \
	   playermturk.html \
	   playermturk.js \
	   playerhome.html \
	   playerhome.js \
	   playerlogin.html \
	   privacy.html \
	   script.js
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
	   version_1_0_20.xml

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
	install -m 0444 $(STATICS) $(BUILT) flotr2.min.js logo.png logo-dark.png $(HTDOCS)
	install -m 0444 $(PAGES) $(BUILTPS) $(DATADIR)
	install -m 0755 admin $(CGIBIN)/admin.cgi
	install -m 0755 admin $(CGIBIN)
	install -m 0755 lab $(CGIBIN)/lab.cgi
	install -m 0755 lab $(CGIBIN)/lab.fcgi
	install -m 0755 lab $(CGIBIN)
	[ -z "$(DSYMUTIL)" ] || $(DSYMUTIL) $(CGIBIN)/lab
	[ -z "$(DSYMUTIL)" ] || $(DSYMUTIL) $(CGIBIN)/admin

installcgi: updatecgi gamelab.db
	rm -f $(DATADIR)/gamelab.db
	rm -f $(DATADIR)/gamelab.db-wal
	rm -f $(DATADIR)/gamelab.db-shm
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
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots/gamelab-$(VERSION).tgz
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots/gamelab-$(VERSION).tgz.sha512

gamelab.db: gamelab.sql
	rm -f $@
	sqlite3 $@ < gamelab.sql

index.html: index.xml $(VERSIONS)
	sblg -t index.xml -o- $(VERSIONS) | \
		sed "s!@VERSION@!$(VERSION)!g" >$@

adminhome.js playerautoadd.js playerlobby.js playerhome.js playermturk.js script.js: jsmin

.in.js.js:
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
