.SUFFIXES: .min.js .js .html .xml

# The (GNU) Makefile.
# This begins with a bunch of examples for different installations.
# Choose as you wish.
# Some of these you can override during Makefile.

ifeq ($(shell uname), Darwin)
# Mac OSX example: userdir and apache2.
# I use the userdir ("~/kristaps") just for demonstration.
PREFIX		?= /Users/kristaps/Sites
URIPREFIX	?= /~kristaps
RELPREFIX	?= # Not used.
ADMINURI	 = $(URIPREFIX)/admin.cgi
HTURI		 = $(URIPREFIX)
LABURI		 = $(URIPREFIX)/lab.cgi
FONTURI	 	 = $(URIPREFIX)/font-awesome-4.5.0/css/font-awesome.min.css
CGIBIN		 = $(PREFIX)
DATADIR		 = $(PREFIX)
HTDOCS		 = $(PREFIX)
RDATADIR	 = $(PREFIX)
CFLAGS		+= -Wno-deprecated-declarations
#DSYMUTIL	 = sudo dsymutil
DSYMUTIL	 = # Not used.
LIBS		 = 
STATIC		 = 
LOGFILE	 	 = $(PREFIX)/gamelab.log
else ifeq ($(shell uname), OpenBSD)
# OpenBSD example: chroot in nginx.
# This accepts RELPREFIX as the prefix within the chroot.
# This is also used in production.
PREFIX		?= /var/www
URIPREFIX	?= 
RELPREFIX	?=
ADMINURI	 = $(URIPREFIX)/cgi-bin/admin
HTURI		 = $(URIPREFIX)
LABURI		 = $(URIPREFIX)/cgi-bin/lab
CGIBIN		 = $(PREFIX)/cgi-bin
DATADIR	 	 = $(PREFIX)/data
HTDOCS		 = $(PREFIX)/htdocs
RDATADIR	 = $(RELPREFIX)/data
FONTURI		 = //maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css
DSYMUTIL	 = # Not used.
CFLAGS		+= -DLOGTIME=1
LIBS		 = -lintl -liconv -lm
STATIC		 = -static -nopie
LOGFILE	 	 = /logs/gametheorylab.org-system.log
# OpenBSD 5.8 needs -nopie or the binaries segfault.
# I think this is due to GMP.
else 
# Generic example (no chroot).
PREFIX		?= /var/www
URIPREFIX	?= 
RELPREFIX	?= # Not used.
ADMINURI	 = $(URIPREFIX)/cgi-bin/admin
HTURI		 = $(URIPREFIX)
LABURI		 = $(URIPREFIX)/cgi-bin/lab
CGIBIN		 = $(PREFIX)/cgi-bin
DATADIR	 	 = $(PREFIX)/data
HTDOCS		 = $(PREFIX)/htdocs
RDATADIR	 = $(PREFIX)/data
FONTURI		 = //maxcdn.bootstrapcdn.com/font-awesome/4.5.0/css/font-awesome.min.css
DSYMUTIL	 = # Not used.
CFLAGS		+= 
LIBS		 = -lbsd -lm # For Linux...
STATIC		 =
LOGFILE	 	 = /logs/gamelab.log
endif

#####################################################################
# You really don't want to change anything below this line.
#####################################################################

VERSION	 = 1.1.5
VMONTH	 = March
VYEAR	 = 2016
CFLAGS 	+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings -I/usr/local/include
CFLAGS	+= -DDATADIR=\"$(RDATADIR)\" -DHTURI=\"$(HTURI)\" -DLABURI=\"$(LABURI)\"
CFLAGS	+= -DLOGFILE=\"$(LOGFILE)\"
INSTRS 	 = instructions-lottery.xml \
	   instructions-nolottery.xml \
	   instructions-mturk.xml
MAILS	 = mail-addplayer.eml \
	   mail-backupfail.eml \
	   mail-backupsuccess.eml \
	   mail-roundadvance.eml \
	   mail-roundfirst.eml \
	   mail-test.eml
STATICS	 = adminhome.css \
	   adminlogin.css \
	   mturkpreview1.png \
	   mturkpreview2.png \
	   playerhome.css \
	   playerlobby.css \
	   playerlogin.css \
	   style.css
OBJS	 = base64.o \
	   db.o \
	   hmac.o \
	   json.o \
	   log.o \
	   mail.o \
	   mpq.o \
	   mturk.o \
	   sha1.o \
	   util.o
SRCS	 = GNUmakefile \
	   admin.c \
	   adminhome.js \
	   adminhome.xml \
	   adminlogin.js \
	   adminlogin.xml \
	   base64.c \
	   db.c \
	   extern.h \
	   gamelab.sql \
	   gamers.c \
	   hmac.c \
	   jsmin.c \
	   json.c \
	   lab.c \
	   log.c \
	   mail.c \
	   mpq.c \
	   mturk.c \
	   mturkpreview.xml \
	   playerautoadd.xml \
	   playerautoadd.js \
	   playerhome.xml \
	   playerhome.js \
	   playerlobby.xml \
	   playerlobby.js \
	   playerlogin.xml \
	   playerlogin.js \
	   privacy.xml \
	   script.js \
	   sha1.c \
	   util.c
HTMLS	 = adminhome.html \
	   adminlogin.html \
	   mturkpreview.html \
	   playerautoadd.html \
	   playerlobby.html \
	   playerhome.html \
	   playerlogin.html \
	   privacy.html
JSMINS   = adminhome.min.js \
	   adminlogin.min.js \
	   humanize-duration.min.js \
	   playerautoadd.min.js \
	   playerhome.min.js \
	   playerlobby.min.js \
	   playerlogin.min.js \
	   script.min.js
IMAGES   = eskil.jpg \
	   jorgen.jpg \
	   kristaps.jpg
CSSS	 = index.css \
	   manual.css
BUILTMGS = schema.png
BUILTMLS = history.html \
	   index.html \
	   manual.html \
	   quickstart.html \
	   schema.html

all: admin lab gamers $(HTMLS) $(JSMINS)

jsmin: jsmin.c
	$(CC) $(CFLAGS) -o $@ jsmin.c

gamers: gamers.c
	$(CC) $(CFLAGS) `curl-config --cflags` -o $@ gamers.c `curl-config --libs` -ljson-c -lm

admin: admin.o $(OBJS)
	$(CC) $(STATIC) -L/usr/local/lib -o $@ admin.o $(OBJS) -lsqlite3 -lkcgi -lkcgijson -lz -ljson-c -lgmp -lexpat `curl-config --libs` $(LIBS)

lab: lab.o $(OBJS)
	$(CC) $(STATIC) -L/usr/local/lib -o $@ lab.o $(OBJS) -lsqlite3 -lkcgi -lkcgijson -lz -lgmp -lexpat `curl-config --libs` $(LIBS)

admin.o lab.o $(OBJS): extern.h

www: $(BUILTMLS) $(BUILTMGS) gamelab.tgz gamelab.tgz.sha512 gamelab.bib 

gamelab.bib: gamelab.in.bib
	sed -e "s!@VERSION@!$(VERSION)!g" \
	    -e "s!@VMONTH@!$(VMONTH)!g" \
	    -e "s!@VYEAR@!$(VYEAR)!g" gamelab.in.bib >$@

updatecgi: all
	mkdir -p $(HTDOCS)
	mkdir -p $(DATADIR)
	mkdir -p $(CGIBIN)
	install -m 0444 $(STATICS) $(HTMLS) $(JSMINS) flotr2.min.js logo.png logo-dark.png $(HTDOCS)
	for f in $(INSTRS) ; do install -m 0444 $$f $(HTDOCS)/`basename $$f`.txt ; done
	install -m 0444 $(INSTRS) $(MAILS) $(DATADIR)
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
	install -m 0444 $(IMAGES) $(BUILTMLS) $(BUILTMGS) gamelab.bib $(CSSS) logo.png $(PREFIX)
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots
	install -m 0444 gamelab.tgz $(PREFIX)/snapshots/gamelab-$(VERSION).tgz
	install -m 0444 gamelab.tgz.sha512 $(PREFIX)/snapshots/gamelab-$(VERSION).tgz.sha512

gamelab.db: gamelab.sql
	rm -f $@
	sqlite3 $@ < gamelab.sql

history.html: history.xml versions.xml
	sblg -t history.xml -o- versions.xml | \
		sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
			-e "s!@LABURI@!$(LABURI)!g" \
			-e "s!@FONTURI@!$(FONTURI)!g" \
			-e "s!@VERSION@!$(VERSION)!g" \
			-e "s!@HTURI@!$(HTURI)!g" >$@

index.html: index.xml versions.xml
	sblg -t index.xml -o- versions.xml | \
		sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
			-e "s!@LABURI@!$(LABURI)!g" \
			-e "s!@FONTURI@!$(FONTURI)!g" \
			-e "s!@VERSION@!$(VERSION)!g" \
			-e "s!@HTURI@!$(HTURI)!g" >$@

$(JSMINS): jsmin

.js.min.js:
	rm -f $@
	sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
		-e "s!@LABURI@!$(LABURI)!g" \
		-e "s!@HTURI@!$(HTURI)!g" $< > $@
	chmod 444 $@

schema.html: gamelab.sql
	sqliteconvert gamelab.sql >$@

schema.png: gamelab.sql
	sqliteconvert -i gamelab.sql >$@

manual.html: manual.xml gamelab.sql
	( sed -n '1,/@DATABASE_SCHEMA@/p' manual.xml ; \
	  sqlite2html gamelab.sql ; \
	  sed -n '/@DATABASE_SCHEMA@/,$$p' manual.xml ; ) | \
	  sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
		-e "s!@LABURI@!$(LABURI)!g" \
		-e "s!@FONTURI@!$(FONTURI)!g" \
		-e "s!@VERSION@!$(VERSION)!g" \
		-e "s!@HTURI@!$(HTURI)!g" >$@

.xml.html:
	sed -e "s!@ADMINURI@!$(ADMINURI)!g" \
		-e "s!@LABURI@!$(LABURI)!g" \
		-e "s!@FONTURI@!$(FONTURI)!g" \
		-e "s!@VERSION@!$(VERSION)!g" \
		-e "s!@HTURI@!$(HTURI)!g" $< >$@

clean:
	rm -f admin admin.o gamelab.db lab lab.o $(OBJS) jsmin gamers
	rm -f $(HTMLS) $(JSMINS) $(BUILTMLS)
	rm -f gamelab.tgz gamelab.tgz.sha512 gamelab.bib
	rm -rf *.dSYM
