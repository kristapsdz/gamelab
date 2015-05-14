CREATE TABLE winner (
	playerid INTEGER REFERENCES player(id) NOT NULL,
	winner BOOLEAN NOT NULL,
	winrank INTEGER NOT NULL,
	rnum INTEGER NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	unique (playerid)
);
/*
 * This consists of a unique participant in the experiment.
 */
CREATE TABLE player (
	email TEXT NOT NULL,
	state INTEGER NOT NULL DEFAULT(0),
	enabled INTEGER NOT NULL DEFAULT(1),
	rseed INTEGER NOT NULL,
	role INTEGER NOT NULL DEFAULT(0),
	rank INTEGER NOT NULL DEFAULT(0),
	autoadd INTEGER NOT NULL DEFAULT(0),
	instr INTEGER NOT NULL DEFAULT(1),
	finalrank INTEGER NOT NULL DEFAULT(0),
	finalscore INTEGER NOT NULL DEFAULT(0),
	hash TEXT,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (email)
);

/*
 * A lottery is created for an individual player when that player has
 * all payoffs for all games.
 */
CREATE TABLE lottery (
	round INTEGER NOT NULL,
	playerid INTEGER REFERENCES player(id) NOT NULL,
	aggrpayoff TEXT NOT NULL,
	curpayoff TEXT NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round, playerid)
);

/*
 * During a given round, the gameplay shows a player's status in terms
 * of number of choices (plays) made, i.e., how many games.
 */
CREATE TABLE gameplay (
	round INTEGER NOT NULL,
	choices INTEGER NOT NULL DEFAULT(0),
	playerid INTEGER REFERENCES player(id) NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round, playerid)
);

/*
 * A payoff is created when a round has been marked as complete.
 * It marks a player's play against the average play.
 */
CREATE TABLE payoff (
	round INTEGER NOT NULL,
	playerid INTEGER REFERENCES player(id) NOT NULL,
	gameid INTEGER REFERENCES game(id) NOT NULL,
	payoff TEXT NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	UNIQUE (round, playerid, gameid)
);

/*
 * A choice is the strategies a player assigns to a particular game.
 */
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
	state INTEGER NOT NULL,
	start INTEGER DEFAULT(0),
	rounds INTEGER DEFAULT(0),
	round INTEGER DEFAULT(-1),
	roundbegan INTEGER DEFAULT(0),
	roundpct REAL DEFAULT(0),
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

CREATE TABLE tickets (
	round INTEGER NOT NULL,
	gameid INTEGER REFERENCES game(id) NOT NULL,
	id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	tickets TEXT NOT NULL,
	unique (round, gameid)

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
