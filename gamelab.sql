CREATE TABLE player (
	email TEXT NOT NULL,
	state INTEGER NOT NULL DEFAULT(0),
	enabled INTEGER NOT NULL DEFAULT(1),
	role INTEGER NOT NULL,
	hash TEXT,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (email)
);

CREATE TABLE gameplay (
	round INTEGER NOT NULL,
	choices INTEGER NOT NULL DEFAULT(0),
	playerid INTEGER REFERENCES player(id) NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round, playerid)
);

CREATE TABLE choice (
	round INTEGER NOT NULL,
	strategy INTEGER NOT NULL,
	randr DOUBLE NOT NULL,
	playerid INTEGER REFERENCES player(id) NOT NULL,
	gameid INTEGER REFERENCES game(id) NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round, playerid, gameid)
);

CREATE TABLE play (
	fraction TEXT NOT NULL,
	round INTEGER NOT NULL,
	strategy INTEGER NOT NULL,
	playerid INTEGER REFERENCES player(id) NOT NULL,
	gameid INTEGER REFERENCES game(id) NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round,playerid,gameid,strategy)
);

CREATE TABLE experiment (
	state INTEGER NOT NULL,
	start INTEGER DEFAULT(0),
	rounds INTEGER DEFAULT(0),
	minutes INTEGER DEFAULT(0),
	loginuri TEXT DEFAULT(''),
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
	cookie INTEGER NOT NULL,
	playerid INTEGER REFERENCES player(id) DEFAULT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

CREATE TABLE admin (
	email TEXT NOT NULL,
	hash TEXT NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

CREATE TABLE smtp (
	user TEXT,
	email TEXT,
	pass TEXT,
	server TEXT,
	isset INTEGER NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

INSERT INTO admin (email, hash) VALUES ('foo@example.com', 'xyzzy');
INSERT INTO experiment (state) VALUES (0);
INSERT INTO smtp (isset) VALUES (0);
