/*	$Id$ */
/*
 * Copyright (c) 2014--2015 Kristaps Dzonsons <kristaps@kcons.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * All of these tables and columns are documented in manual.xml.
 */

CREATE TABLE winner (
	playerid INTEGER REFERENCES player(id) NOT NULL,
	winner BOOLEAN NOT NULL,
	winrank INTEGER NOT NULL,
	rnum INTEGER NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	unique (playerid)
);

CREATE TABLE player (
	email TEXT NOT NULL,
	state INTEGER NOT NULL DEFAULT(0),
	enabled INTEGER NOT NULL DEFAULT(1),
	rseed INTEGER NOT NULL,
	role INTEGER NOT NULL DEFAULT(0),
	rank INTEGER NOT NULL DEFAULT(0),
	autoadd INTEGER NOT NULL DEFAULT(0),
	instr INTEGER NOT NULL DEFAULT(1),
	joined INTEGER NOT NULL DEFAULT(-1),
	finalrank INTEGER NOT NULL DEFAULT(0),
	finalscore INTEGER NOT NULL DEFAULT(0),
	version INTEGER NOT NULL DEFAULT(0),
	hash TEXT,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (email)
);

CREATE TABLE lottery (
	round INTEGER NOT NULL,
	playerid INTEGER REFERENCES player(id) NOT NULL,
	aggrpayoff TEXT NOT NULL,
	curpayoff TEXT NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round, playerid)
);

CREATE TABLE gameplay (
	round INTEGER NOT NULL,
	choices INTEGER NOT NULL DEFAULT(0),
	playerid INTEGER REFERENCES player(id) NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round, playerid)
);

CREATE TABLE payoff (
	round INTEGER NOT NULL,
	playerid INTEGER REFERENCES player(id) NOT NULL,
	gameid INTEGER REFERENCES game(id) NOT NULL,
	payoff TEXT NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round, playerid, gameid)
);

CREATE TABLE choice (
	round INTEGER NOT NULL,
	strats TEXT NOT NULL,
	stratsz INTEGER NOT NULL,
	playerid INTEGER REFERENCES player(id) NOT NULL,
	gameid INTEGER REFERENCES game(id) NOT NULL,
	sessid INTEGER REFERENCES sess(id) NOT NULL,
	created INTEGER NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round, playerid, gameid)
);

CREATE TABLE experiment (
	state INTEGER NOT NULL DEFAULT(0),
	start INTEGER DEFAULT(0),
	rounds INTEGER DEFAULT(0),
	playermax INTEGER DEFAULT(0),
	prounds INTEGER DEFAULT(0),
	round INTEGER DEFAULT(-1),
	roundbegan INTEGER DEFAULT(0),
	roundpct REAL DEFAULT(0),
	roundmin INTEGER DEFAULT(0),
	minutes INTEGER DEFAULT(0),
	autoadd INTEGER NOT NULL DEFAULT(0),
	loginuri TEXT DEFAULT(''),
	instr TEXT DEFAULT(''),
	total INTEGER DEFAULT(0),
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

CREATE TABLE game (
	p1 INTEGER NOT NULL,
	p2 INTEGER NOT NULL,
	payoffs TEXT NOT NULL,
	name TEXT NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

CREATE TABLE sess (
	created INTEGER NOT NULL,
	cookie INTEGER NOT NULL,
	playerid INTEGER REFERENCES player(id) DEFAULT NULL,
	useragent TEXT NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

CREATE TABLE admin (
	email TEXT NOT NULL,
	hash TEXT NOT NULL,
	isset INTEGER NOT NULL DEFAULT(0),
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

CREATE TABLE past (
	round INTEGER NOT NULL,
	gameid INTEGER REFERENCES game(id) NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	averagesp1 TEXT NOT NULL,
	averagesp2 TEXT NOT NULL,
	currentsp1 TEXT NOT NULL,
	currentsp2 TEXT NOT NULL,
	skip INTEGER NOT NULL,
	roundcount INTEGER NOT NULL,
	unique (round, gameid)
);

CREATE TABLE smtp (
	user TEXT NOT NULL DEFAULT(''),
	email TEXT NOT NULL DEFAULT(''),
	pass TEXT NOT NULL DEFAULT(''),
	server TEXT NOT NULL DEFAULT(''),
	isset INTEGER NOT NULL DEFAULT(0),
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

INSERT INTO admin (email, hash) VALUES ('foo@example.com', 'xyzzy');
INSERT INTO experiment DEFAULT VALUES;
INSERT INTO smtp DEFAULT VALUES;
