CREATE TABLE player (
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
	playerid INTEGER REFERENCES player(id),
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

CREATE TABLE admin (
	email TEXT NOT NULL,
	hash TEXT NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL
);

INSERT INTO admin (email, hash) VALUES ('foo@example.com', 'xyzzy');
