'use strict';

var currentRound = null; /* see checkRound() */

function logout()
{

	console.log('You have been logged out!');
	location.href = '@HTURI@/adminlogin.html#loggedout';
}

function doClassOk(name)
{
	var e;
	if (null != (e = document.getElementById(name)))
		e.className = 'fa fa-fw fa-check-square-o';
}

function doClassLoading(name)
{
	var e;
	if (null != (e = document.getElementById(name)))
		e.className = 'fa fa-spinner';
}

function doClassFail(name)
{
	var e;
	if (null != (e = document.getElementById(name)))
		e.className = 'fa fa-fw fa-square-o';
}

function doSuccess(submitName, formName) 
{

	doValue(submitName, 'Submit');
	document.getElementById(formName).reset();
}

/*
 * This is called by most sendForm() functions when we tear down the
 * sending process and post an error.
 * It resets the submission value to the default and unhides a
 * corresponding error message.
 */
function doError(err, submitName, errName) 
{
	var e;

	if (null != submitName && 
		 null != (e = document.getElementById(submitName)))
		if (e.hasAttribute('gamelab-submit-default'))
			e.value = e.getAttribute('gamelab-submit-default');

	switch (err) {
	case 400:
		if (null != errName)
			doUnhide(errName + 'Form');
		break;
	case 403:
		logout();
		break;
	case 409:
		if (null != errName)
			doUnhide(errName + 'State');
		break;
	default:
		if (null != errName)
			doUnhide(errName + 'System');
		break;
	}
}

/*
 * This is called by most sendForm() functions as the setup argument.
 * It's given the ID of the submit button (submitName) and the common
 * prefix shared by errors (errName).
 * If found, it sets the submit button's value to the
 * "gamelab-submit-pending" attribute, saving the existing value in
 * "gamelab-submit-default", and hides the errors.
 */
function doSetup(submitName, errName) 
{
	var e;

	if (null != submitName &&
		 null != (e = document.getElementById(submitName))) 
		if (e.hasAttribute('gamelab-submit-pending')) {
			e.setAttribute('gamelab-submit-default', e.value);
			e.value = e.getAttribute('gamelab-submit-pending');
		}

	if (null != errName) {
		doHide(errName + 'Form');
		doHide(errName + 'System');
		doHide(errName + 'State');
	}
}

function checkToggle(name, tname, enable)
{
	var icon;

	if (null == (icon = document.getElementById(name)))
		return;
	icon.classList.remove('fa-toggle-off');
	icon.classList.remove('fa-toggle-on');
	if (enable) {
		icon.classList.add('fa-toggle-on');
		doValue(tname, 1);
	} else {
		icon.classList.add('fa-toggle-off');
		doValue(tname, 0);
	}
}

/*
 * This is called by loadNewPlayers() when the AJAX request has
 * completed with the list response.
 * We must now fill in the list of names.
 */
function loadNewPlayersSuccess(resp) 
{
	var e, li, i, results, players, icon, link, span, count, pspan, fa, player;

	e = doClearNode(document.getElementById('loadNewPlayers'));

	try  { 
		results = JSON.parse(resp);
	} catch (error) {
		return;
	}

	checkToggle('autoaddToggle', 'autoadd', results.autoadd);
	checkToggle('autoadd2Toggle', 'autoadd2', results.autoadd);
	checkToggle('autoaddTogglePreserve', 'autoaddpreserve', results.autoaddpreserve);
	doValue('autoadd4', results.autoadd);
	doValue('autoadd3', results.autoadd);
	doValue('autoaddpreserve2', results.autoaddpreserve);

	if (results.autoadd) {
		doUnhide('captiveGame2');
		doUnhide('captiveGame');
	} else {
		doHide('captiveGame');
		doHide('captiveGame2');
	}

	players = results.players;

	if (0 == players.length) {
		li = document.createElement('li');
		e.appendChild(li);
		li.setAttribute('class', 'error');
		li.appendChild(document.createTextNode('No participants.'));
		doClassFail('checkPlayersLoad2');
		return;
	}

	for (count = i = 0; i < players.length; i++) {
		player = players[i].player;
		if (player.enabled)
			count++;
		span = document.createElement('li');
		span.setAttribute('id', 'player' + player.id);
		icon = document.createElement('a');
		icon.setAttribute('data-playerid', player.id);
		icon.setAttribute('class', 'fa fa-remove');
		icon.setAttribute('href', '#;');
		icon.setAttribute('id', 'playerDelete' + player.id);
		icon.onclick = function() {
			return function(){doDeletePlayer(this); return(false);};
		}();
		span.appendChild(icon);
		icon = document.createElement('span');
		icon.appendChild(document.createTextNode(player.mail));
		span.appendChild(icon);
		if (player.autoadd) {
			icon = document.createElement('i');
			icon.className = 'fa fa-thumb-tack';
			span.appendChild(document.createTextNode(' '));
			span.appendChild(icon);
		}
		e.appendChild(span);
	}

	if (count >= 2) {
		doClassOk('checkPlayersLoad2');
	} else {
		doClassFail('checkPlayersLoad2');
	}
}

function doShowPlayer(source)
{
	var e, status, name;

	name = source.getAttribute('data-playername');

	if (null == (e = document.getElementById(name)))
		return;
	if ( ! e.hasAttribute('data-gamelab-status') ||
	     ! e.hasAttribute('data-gamelab-mail') ||
	     ! e.hasAttribute('data-gamelab-enabled') ||
	     ! e.hasAttribute('data-gamelab-hitid') ||
	     ! e.hasAttribute('data-gamelab-joined') ||
	     ! e.hasAttribute('data-gamelab-answer') ||
	     ! e.hasAttribute('data-gamelab-playerid'))
		return;

	doValue('playerInfoId', e.getAttribute('data-gamelab-playerid'));
	doClearReplace('playerInfoEmail', 
		e.getAttribute('data-gamelab-mail'));
	doClearReplace('playerInfoJoined', 
		(parseInt(e.getAttribute('data-gamelab-joined')) + 1));
	doClearReplace('playerInfoAnswer', 
		e.getAttribute('data-gamelab-answer'));

	switch (parseInt(e.getAttribute('data-gamelab-status'))) {
	case (0):
		doClearReplace('playerInfoStatus', 'not configured');
		break;
	case (1):
		doClearReplace('playerInfoStatus', 'not logged in');
		break;
	case (2):
		doClearReplace('playerInfoStatus', 'logged in');
		break;
	default:
		doClearReplace('playerInfoStatus', 'error e-mailing');
		break;
	}

	switch (parseInt(e.getAttribute('data-gamelab-enabled'))) {
	case (0):
		doClearReplace('playerInfoEnabled', 'disabled');
		break;
	case (1):
		doClearReplace('playerInfoEnabled', 'enabled');
		break;
	case (2):
		doClearReplace('playerInfoEnabled', 'unknown');
		break;
	}

	doUnhide('playerInfo');
}

/*
 * Used by the "running" experiment page to show the list of all players
 * and bits about their status.
 */
function loadPlayersSuccess(resp) 
{
	var e, li, i, results, players, icon, span, link, sup, player;

	e = doClearNode(document.getElementById('loadPlayers'));

	try  { 
		results = JSON.parse(resp);
	} catch (error) {
		return;
	}

	checkToggle('autoaddToggle', 'autoadd', results.autoadd);
	checkToggle('autoadd2Toggle', 'autoadd2', results.autoadd);
	doValue('autoadd3',  results.autoadd);
	doValue('autoadd4',  results.autoadd);

	if (results.autoadd) {
		doUnhide('captiveGame');
		doUnhide('captiveGame2');
	} else {
		doHide('captiveGame');
		doHide('captiveGame2');
	}

	doPlayerGraph(doClear('playerExprGraph'), results);

	players = results.players;

	/* We have no players... */
	if (0 == players.length) {
		li = document.createElement('li');
		li.setAttribute('class', 'error');
		li.appendChild(document.createTextNode('No participants.'));
		e.appendChild(li);
		return;
	}

	for (i = 0; i < players.length; i++) {
		player = players[i].player;
		/*
		 * The row element contains the information about the
		 * player for later use: status, e-mail, etc.
		 */
		span = document.createElement('li');
		e.appendChild(span);
		span.setAttribute('id', 'player' + player.id);
		span.setAttribute('data-gamelab-status', player.status);
		span.setAttribute('data-gamelab-mail', player.mail);
		span.setAttribute('data-gamelab-enabled', player.enabled);
		span.setAttribute('data-gamelab-playerid', player.id);
		span.setAttribute('data-gamelab-answer', player.answer);
		span.setAttribute('data-gamelab-hitid', player.hitid);
		span.setAttribute('data-gamelab-joined', player.joined);

		/*
		 * Append the toggle-able link for enabling or disable
		 * the current player.
		 */
		icon = document.createElement('a');
		icon.setAttribute('href', '#');
		icon.setAttribute('id', 'playerLoad' + player.id);
		icon.setAttribute('data-playerid', player.id);
		if (0 == player.enabled) {
			icon.className = 'fa fa-fw fa-toggle-off';
			icon.onclick = function() {
				return function(){doEnablePlayer(this); return(false);};
			}();
		} else {
			icon.className = 'fa fa-fw fa-toggle-on';
			icon.onclick = function() {
				return function(){doDisablePlayer(this); return(false);};
			}();
		}
		span.appendChild(icon);
		span.appendChild(document.createTextNode(' '));

		/* Append whether the player is a row or column role. */
		sup = document.createElement('i');
		if (player.joined < 0)
			sup.className = 'fa fa-fw fa-spinner';
		else if (0 == parseInt(player.role))
			sup.className = 'fa fa-fw fa-bars';
		else
			sup.className = 'fa fa-fw fa-columns';
		span.appendChild(document.createTextNode(' '));
		span.appendChild(sup);

		/* Append the link to show more information. */
		link = document.createElement('a');
		link.setAttribute('href', '#');
		link.setAttribute('data-playername', 'player' + player.id);
		link.onclick = function() {
			return function(){doShowPlayer(this); return(false);};
		}();
		link.appendChild(document.createTextNode(player.mail));
		span.appendChild(link);
		if (player.autoadd) {
			icon = document.createElement('i');
			icon.className = 'fa fa-thumb-tack';
			span.appendChild(document.createTextNode(' '));
			span.appendChild(icon);
		}
		if (null != player.hitid) {
			icon = document.createElement('i');
			icon.className = 'fa fa-amazon';
			span.appendChild(document.createTextNode(' '));
			span.appendChild(icon);
			if (player.mturkdone) {
				icon = document.createElement('i');
				icon.className = 'fa fa-check';
				span.appendChild(icon);
			}
		}
	}
}

function loadGamesSuccessInner(resp, code, name) 
{
	var i, j, k, results, li, e, div, icon, row, col, cell, tbl;

	e = doClearNode(document.getElementById(name));

	try  { 
		results = JSON.parse(resp);
	} catch (error) {
		li = document.createElement('li');
		li.appendChild(document.createTextNode('Parse error.'));
		e.appendChild(li);
		return;
	}

	if (0 == results.length) {
		li = document.createElement('li');
		e.appendChild(li);
		div = document.createElement('span');
		div.setAttribute('class', 'error');
		div.appendChild(document.createTextNode(' '));
		div.appendChild(document.createTextNode('No games.'));
		li.appendChild(div);
		doClassFail('checkGameLoad2');
		return;
	}

	doClassOk('checkGameLoad2');

	for (i = 0; i < results.length; i++) {
		li = document.createElement('li');
		li.setAttribute('id', 'game' + results[i].id);
		if (0 == code) {
			icon = document.createElement('a');
			icon.setAttribute('href', '#');
			icon.setAttribute('data-gameid', results[i].id);
			icon.setAttribute('class', 'fa fa-remove');
			icon.setAttribute('id', 'gameDelete' + results[i].id);
			icon.onclick = function() {
				return function(){doDeleteGame(this); return(false);};
			}();
			li.appendChild(icon);
			li.appendChild(document.createTextNode(' '));
		}
		div = document.createElement('div');
		li.appendChild(div);
		div.appendChild(document.createTextNode(results[i].name));
		div = document.createElement('div');
		li.appendChild(div);
		tbl = document.createElement('div');
		div.appendChild(tbl);
		tbl.className = 'table';
		for (j = 0; j < results[i].payoffs.length; j++) {
			row = document.createElement('div');
			tbl.appendChild(row);
			for (k = 0; k < results[i].payoffs[j].length; k++) {
				col = document.createElement('div');
				row.appendChild(col);
				cell = document.createElement('span');
				col.appendChild(cell);
				cell.appendChild(document.createTextNode
					(results[i].payoffs[j][k][0] + ','));
				cell = document.createElement('span');
				col.appendChild(cell);
				cell.appendChild(document.createTextNode
					(results[i].payoffs[j][k][1]));
			}
		}
		e.appendChild(li);
	}
}

function loadError(err, name) 
{
	var li, e, div;

	e = doClearNode(document.getElementById(name));
	if (null == e)
		return;
	li = document.createElement('li');
	div = document.createElement('div');
	div.appendChild(document.createTextNode('An error occured.'));
	li.appendChild(div);
	e.appendChild(li);
}

function doDisableEnablePlayer(id, url)
{
	var xrh, e;

	if (null != (e = document.getElementById('player' + id)))
		e.className = 'waiting';

	doClassLoading('playerLoad' + id);
	doHide('playerInfo');

	xrh = new XMLHttpRequest();
	xrh.onreadystatechange=function() {
		if (xrh.readyState==4 && xrh.status==200)
			loadPlayers();
	} ;
	xrh.open('GET', '@ADMINURI@/' + url + '.json?pid=' + id, true);
	xrh.send(null);
}

function doDeleteGame(source)
{
	var xrh, e, id;

	id = source.getAttribute('data-gameid');
	if (null != (e = document.getElementById('game' + id)))
		e.className = 'waiting';

	doClassLoading('gameDelete' + id);

	xrh = new XMLHttpRequest();
	xrh.onreadystatechange=function() {
		if (xrh.readyState==4 && xrh.status==200) {
			if (null != (e = document.getElementById('game' + id)))
				e.parentNode.removeChild(e);
			loadNewGames();
		}
	};
	xrh.open('GET', '@ADMINURI@/dodeletegame.json?gid=' + id, true);
	xrh.send(null);
}

/*
 * This is called in the administratiev "new" phase to remove a player
 * from the list of potential players.
 */
function doDeletePlayer(source)
{
	var xrh, e, id;

	id = source.getAttribute('data-playerid');
	if (null != (e = document.getElementById('player' + id)))
		e.className = 'waiting';

	doClassLoading('playerDelete' + id);

	xrh = new XMLHttpRequest();
	xrh.onreadystatechange=function() {
		if (xrh.readyState==4 && xrh.status==200) {
			if (null != (e = document.getElementById('player' + id)))
				e.parentNode.removeChild(e);
			loadNewPlayers();
		}
	};
	xrh.open('GET', '@ADMINURI@/dodeleteplayer.json?pid=' + id, true);
	xrh.send(null);
}

function doEnablePlayer(source) 
{
	var e, id;

	id = source.getAttribute('data-playerid');
	doDisableEnablePlayer(id, 'doenableplayer');
	if (null != (e = document.getElementById('player' + id)))
		e.setAttribute('data-gamelab-enabled', '1');

}

function doDisablePlayer(source) 
{
	var e, id;

	id = source.getAttribute('data-playerid');
	doDisableEnablePlayer(id, 'dodisableplayer');
	if (null != (e = document.getElementById('player' + id)))
		e.setAttribute('data-gamelab-enabled', '0');
}

function doStatWinners(e, res)
{
	var i, sub, subsub;

	sub = document.createElement('li');
	sub.className = 'statuslisthead';
	subsub = document.createElement('span');
	subsub.appendChild(document.createTextNode('tickets'));
	sub.appendChild(subsub);
	subsub = document.createElement('span');
	subsub.appendChild(document.createTextNode('identifier'));
	sub.appendChild(subsub);
	e.appendChild(sub);

	for (i = 0; i < res.highest.length; i++) {
		sub = document.createElement('li');
		subsub = document.createElement('span');
		subsub.appendChild(document.createTextNode(res.highest[i].score));
		sub.appendChild(subsub);
		subsub = document.createElement('span');
		subsub.appendChild(document.createTextNode(res.highest[i].player.mail));
		sub.appendChild(subsub);
		e.appendChild(sub);
	}
}

function doStatHighest(e, res)
{
	var i, sub, subsub;

	sub = document.createElement('li');
	sub.className = 'statuslisthead';
	subsub = document.createElement('span');
	subsub.appendChild(document.createTextNode('tickets'));
	sub.appendChild(subsub);
	subsub = document.createElement('span');
	subsub.appendChild(document.createTextNode('payout'));
	sub.appendChild(subsub);
	subsub = document.createElement('span');
	subsub.appendChild(document.createTextNode('payoff'));
	sub.appendChild(subsub);
	subsub = document.createElement('span');
	subsub.appendChild(document.createTextNode('identifier'));
	sub.appendChild(subsub);
	e.appendChild(sub);

	for (i = 0; i < res.highest.length; i++) {
		sub = document.createElement('li');
		subsub = document.createElement('span');
		subsub.appendChild(document.createTextNode
			(res.highest[i].score));
		sub.appendChild(subsub);
		subsub = document.createElement('span');
		subsub.appendChild(document.createTextNode
			((res.highest[i].score * res.expr.conversion) + ' USD'));
		sub.appendChild(subsub);
		subsub = document.createElement('span');
		subsub.appendChild(document.createTextNode
			(res.highest[i].payoff));
		sub.appendChild(subsub);
		subsub = document.createElement('span');
		subsub.appendChild(document.createTextNode
			(res.highest[i].player.mail));
		sub.appendChild(subsub);
		e.appendChild(sub);
	}
}

/*
 * Graph the number of current players.
 * This will graph the existing and historical player counts, then
 * project that into the future based on the projected number of rounds
 * per player after joining.
 */
function doPlayerGraph(e, res)
{
	var data, datas, i, j, p, max;

	data = [];
	for (i = 0; i < res.players.length; i++) {
		p = res.players[i].player;
		if (p.joined < 0)
			continue;
		/* 
		 * This is inefficient: just top up the number of
		 * projected rounds incrementally.
		 */
		for (j = 0; j < res.prounds; j++) {
			while (p.joined + j >= data.length)
				data.push(0);
			data[p.joined + j]++;
		}
	}

	/* Convert into point format. */
	datas = [];
	datas.push([0, 0]);
	max = 0;
	for (i = 0; i < data.length; i++)  {
		datas.push([i + 1, data[i]]);
		if (data[i] > max)
			max = data[i];
	}
	datas.push([i + 1, 0]);

	Flotr.draw(e, [ { data: datas }, {data: [[res.round + 1, 0], [res.round + 1, max]]} ], 
		{ grid: { horizontalLines: 1 },
		  shadowSize: 0,
		  subtitle: 'Projected Participants',
		  xaxis: { min: 0, tickDecimals: 0 },
		  yaxis: { min: 0.0, tickDecimals: 0, autoscaleMargin: 1, autoscale: true },
		  lines: { show: true },
		  points: { show: true }});
}

function doStatGraph(e, res)
{
	var datas, data, j;

	datas = [];
	data = [];
	data.push([0, 0]);
	if (null != res.history) {
		for (j = 0; j < res.history[0].roundups.length; j++) 
			data.push([(j + 1), res.history[0].roundups[j].plays]);
		if (res.expr.round < res.expr.rounds)
			data.push([(j + 1), res.fcol + res.frow]);
	}
	datas[0] = {
		data: data,
	};
	Flotr.draw(e, datas, 
		{ grid: { horizontalLines: 1 },
		  shadowSize: 0,
		  subtitle: 'Plays',
		  xaxis: { min: 0, tickDecimals: 0 },
		  yaxis: { min: 0.0, tickDecimals: 0, autoscaleMargin: 1, autoscale: true },
		  lines: { show: true },
		  points: { show: true }});
}

function loadSmtpSetup()
{

	doHide('checkSmtpResults');
	doClassLoading('checkSmtpLoad2');
}

function loadSmtpSuccess(resp)
{
	var results;

	doClassOk('checkSmtpLoad2');

	try {
		results = JSON.parse(resp);
	} catch (error) {
		return;
	}

	doUnhide('checkSmtpResults');
	doValue('checkSmtpResultsServer', results.server);
	doValue('checkSmtpResultsUser', results.user);
	doValue('checkSmtpResultsFrom', results.mail);
}

function loadSmtpError(err)
{

	if (400 != err)
		return;

	doClassFail('checkSmtpLoad2');
}

function loadSmtp() 
{

	sendQuery('@ADMINURI@/dochecksmtp.json', 
		loadSmtpSetup, 
		loadSmtpSuccess, 
		loadSmtpError);
}

function loadList(url, name, onsuccess, onerror) 
{
	var e, li, ii;

	li = document.createElement('li');
	ii = document.createElement('i');
	ii.setAttribute('class', 'fa fa-spinner');
	li.appendChild(ii);
	doClear(name).appendChild(li);
	sendQuery(url, null, onsuccess, onerror);
}

/*
 * Called at load time to load the list of players in the "new"
 * administrative state.
 */
function loadNewPlayers() 
{

	doClassLoading('checkPlayersLoad2');
	loadList('@ADMINURI@/doloadplayers.json', 'loadNewPlayers', 
		loadNewPlayersSuccess, 
		function(err) { loadError(err, 'loadNewPlayers'); });
}

function loadPlayers() 
{

	loadList('@ADMINURI@/doloadplayers.json', 'loadPlayers', 
		loadPlayersSuccess, 
		function(err) { loadError(err, 'loadPlayers'); });
}

/*
 * Called at load time to load the list of games in the "new"
 * administrative state.
 */
function loadNewGames() 
{

	doClassLoading('checkGameLoad2');
	loadList('@ADMINURI@/doloadgames.json', 'loadGames', 
		function(resp) { loadGamesSuccessInner(resp, 0, 'loadGames'); },
		function(err) { loadError(err, 'loadGames'); });
}

function loadGames() 
{

	doClassLoading('checkGameLoad2');
	loadList('@ADMINURI@/doloadgames.json', 'loadGames2', 
		function(resp) { loadGamesSuccessInner(resp, 1, 'loadGames2'); },
		function(err) { loadError(err, 'loadGames2'); });
}

/*
 * This callback is invoked when a new experiment (adminhome-new.html)
 * has invoked dogetnewexpr.json on the server.
 * It populates all experiment-specific parts of the page.
 */
function loadNewExprSuccess(res) 
{
	var	 i, list, sz;

	list = document.getElementsByClassName('expr-shuffle');
	for (i = 0, sz = list.length; i < sz; i++) 
		res.expr.noshuffle ?
			doHideNode(list[i]) :
			doUnhideNode(list[i]);
	list = document.getElementsByClassName('expr-noshuffle');
	for (i = 0, sz = list.length; i < sz; i++) 
		res.expr.noshuffle ?
			doUnhdeNode(list[i]) :
			doHideNode(list[i]);

	list = document.getElementsByClassName('expr-admin');
	for (i = 0, sz = list.length; i < sz; i++) 
		doClearReplaceNode(list[i], res.expr.admin);

	list = document.getElementsByClassName('expr-admin-link');
	for (i = 0, sz = list.length; i < sz; i++) 
		list[i].href = 'mailto:' + res.expr.admin;

	list = document.getElementsByClassName('expr-admin-input');
	for (i = 0, sz = list.length; i < sz; i++) 
		list[i].value = res.expr.admin;

	/*
	 * FIXME.
	 * Until we have a neater place to put this (e.g., a record of
	 * administrative logins) that require a dogetadmin.json type of
	 * request, we're putting these in here.
	 */
	if (res.adminflags & 0x01) 
		doClassOk('checkChangeEmail');
	else
		doClassFail('checkChangeEmail');

	if (res.adminflags & 0x02) 
		doClassOk('checkChangePass');
	else
		doClassFail('checkChangePass');
}

/*
 * Poll the server to see our current round.
 * This only happens if we have a round percentage, and is used to
 * auto-progress the round if it has expired.
 */
function checkRoundSuccess(resp)
{
	var res;

	/* This shouldn't happen. */
	if (null == currentRound)
		return;

	/* Parse our JSON response. */
	try {
		res = JSON.parse(resp);
	} catch (error) {
		console.log('JSON error: ' + resp);
		return;
	}

	/* 
	 * If we're before the current (from the server) round, then
	 * reload our data structures.
	 * If we're not, then continue polling.
	 */
	if (currentRound < res.round)
		reloadExpr();
	else
		setTimeout(checkRound, 10000);
}

/*
 * Send a query that will check the current round.
 */
function checkRound()
{

	sendQuery('@ADMINURI@/docheckround.json', 
		null, checkRoundSuccess, null);
}

function timerSloppyCountdown(donefunc, e, value, round)
{
	var now;

	if (null != currentRound && round != currentRound)
		return;

	now = Math.floor(new Date().getTime() / 1000);
	if (now >= value) {
		donefunc();
		return;
	}
	doClearNode(e);
	formatCountdown(value - now, e);
	setTimeout(timerSloppyCountdown, 1000, donefunc, e, value, round);
}

function reloadExprSuccess(resp) 
{
	var res;

	try  { 
		res = JSON.parse(resp);
	} catch (error) {
		return;
	}

	loadExprSuccess(res);
}

/*
 * This callback is invoked for an alread-started experiment coming from
 * adminhome-started.html after dogetexpr.json has returned success.
 */
function loadExprSuccess(res) 
{
	var e, i, j, chld, expr, li, div, href, span, next, data, datas;

	doHide('statusExprLoading');
	doUnhide('statusExprLoaded');

	loadNewExprSuccess(res);

	expr = res.expr;
	currentRound = expr.round;

	doClearReplace('instr', expr.instr);
	doStatGraph(doClear('statusExprGraph'), res);
	doClearReplace('statusExprPRound', (expr.round + 1));
	doClearReplace('statusExprPMax', expr.rounds);
	doClearReplace('statusExprPPmax', expr.prounds);
	doClearReplace('statusExprMins', humanizeDuration(expr.minutes * 60 * 1000));
	if (expr.roundpct > 0.0) {
		doClearReplace('statusExprRoundpct', expr.roundpct);
		doClearReplace('statusExprRoundmin', humanizeDuration(expr.roundmin * 60 * 1000));
	}
	if (res.lobbysize > 0) {
		doUnhide('statusExprHasLobby');
		doClearReplace('statusExprLobbysize', res.lobbysize);
	} else
		doHide('statusExprHasLobby');

	doHide('mturkbox');
	doHide('mturkerrbox');
	doHide('mturkokbox');
	if (expr.hitid) {
		doUnhide('mturkokbox');
		doClearReplace('hitid', expr.hitid);
		doHide('mturksandbox');
		doHide('mturknosandbox');
		if (expr.sandbox)
			doUnhide('mturksandbox');
		else
			doUnhide('mturknosandbox');
	} else if (expr.awsaccesskey && expr.awssecretkey) {
		if (expr.awserror) {
			doUnhide('mturkerrbox');
			doClearReplace('awserror', expr.awserror);
		} else {
			doUnhide('mturkbox');
			doClearReplace('awsaccesskey', expr.awsaccesskey);
		} 
	}

	if (expr.roundpid > 0) {
		doUnhide('statusMailer');
		doClearReplace('statusMailerPid', expr.roundpid);
		if (expr.roundpidok) {
			doHide('statusMailerPidBad');
			doUnhide('statusMailerPidOk');
		} else {
			doUnhide('statusMailerPidBad');
			doHide('statusMailerPidOk');
		}
	} else
		doHide('statusMailer');

	if (expr.round >= expr.rounds) {
		doHide('statusExprProg');
		doUnhide('statusExprFinished');
		if (expr.postwin) {
			doUnhide('statusExprFinishedWinnersBox');
			doHide('statusExprFinishedWin');
			e = doClear('statusExprFinishedWinners');
			chld = document.createElement('li');
			chld.className = 'statuslisthead';
			span = document.createElement('span');
			span.appendChild(document.createTextNode('mail'));
			chld.appendChild(span);
			span = document.createElement('span');
			span.appendChild(document.createTextNode('rank'));
			chld.appendChild(span);
			span = document.createElement('span');
			span.appendChild(document.createTextNode('ticket'));
			chld.appendChild(span);
			span = document.createElement('span');
			span.appendChild(document.createTextNode('range'));
			chld.appendChild(span);
			e.appendChild(chld);
			for (i = 0; i < res.winners.length; i++) {
				chld = document.createElement('li');
				e.appendChild(chld);
				href = document.createElement('a');
				href.setAttribute('href', 
					'mailto:' + res.winners[i].email);
				href.appendChild(document.createTextNode
					(res.winners[i].email));
				chld.appendChild(href);
				span = document.createElement('span');
				span.appendChild(document.createTextNode
					(res.winners[i].rank + 1));
				chld.appendChild(span);
				span = document.createElement('span');
				span.appendChild(document.createTextNode
					(res.winners[i].winnum));
				chld.appendChild(span);
				span = document.createElement('span');
				span.appendChild(document.createTextNode
					(res.winners[i].winrank + 
					 '\u2013' + (res.winners[i].winrank + 
					  res.winners[i].winscore)));
				chld.appendChild(span);
			}
		} else if (expr.lottery) {
			doHide('statusExprFinishedWinnersBox');
			doUnhide('statusExprFinishedWin');
		} else {
			doHide('statusExprFinishedWinnersBox');
			doHide('statusExprFinishedWin');
		}
		doClearReplace('exprCountdown', 'finished');
		if (res.highest.length > 0) {
			doUnhide('statusHighestBox2');
			doHide('statusNoHighestBox2');
			doStatHighest(doClear('statusHighest2'), res);
		} else {
			doHide('statusExprFinishedWin');
			doHide('statusHighestBox2');
			doUnhide('statusNoHighestBox2');
		}
		if (expr.awsaccesskey && expr.awssecretkey)
			doUnhide('mturkbonusesButton');
		else
			doHide('mturkbonusesButton');
	}  else if (expr.round < 0) {
		next = expr.start - Math.floor(new Date().getTime() / 1000);
		doUnhide('statusExprWaiting');
		e = doClear('exprCountdown');
		formatCountdown(next, e);
		setTimeout(timerSloppyCountdown, 1000, 
			 reloadExpr, e, expr.start, expr.round);
	} else {
		next = (expr.roundbegan + (expr.minutes * 60)) -
			Math.floor(new Date().getTime() / 1000);
		e = doClear('exprCountdown');
		formatCountdown(next, e);
		setTimeout(timerSloppyCountdown, 1000, reloadExpr, e, 
			expr.roundbegan + (expr.minutes * 60), expr.round);
		/*
		 * Send out a poll to check our round advancement.
		 */
		if (expr.roundpct > 0.0)
			setTimeout(checkRound, 10000);
		doUnhide('statusExprProg');
		doClearReplace('statusExprPBar', 
			Math.round(expr.progress * 100.0) + '%');
		doValue('statusExprPBar', expr.progress);
		doClearReplace('statusExprFrow', res.frow);
		doClearReplace('statusExprFrowMax', res.frowmax);
		doValue('statusExprFrowPct', 
			0 == res.frowmax ? 0 : res.frow / res.frowmax);
		doClearReplace('statusExprFcol', res.fcol);
		doClearReplace('statusExprFcolMax', res.fcolmax);
		doValue('statusExprFcolPct', 
			0 == res.fcolmax ? 0 : res.fcol / res.fcolmax);

		if (res.expr.round > 0 &&
		    (0 == res.frowmax || 0 == res.fcolmax))
			doUnhide('noplayers');
		else
			doHide('noplayers');

		doHide('statusHighestBox');
		doHide('statusHighestNoBox');
		if (expr.round > 0 && res.highest.length) {
			doUnhide('statusHighestBox');
			doStatHighest(doClear('statusHighest'), res);
		} else if (expr.round > 0) {
			doUnhide('statusHighestNoBox');
			doHide('statusHighestBox');
		}
	}
}

function loadExprSetup()
{

	doUnhide('startExprSection');
	doHide('statusGraphs');
	doHide('statusExprLoading');
	doHide('exprCountdownGame');
	doHide('exprControl');
	doHide('statusExprLoaded');
	doUnhide('checkGameLoad2');
	doHide('seeGameSection');
	doUnhide('addGameSection');
	doHide('seePlayerSection');
	doUnhide('addPlayerSection');
}

function reloadExprSetup()
{

	doHide('startExprSection');
	doUnhide('seePlayerSection');
	doHide('addPlayerSection');
	doUnhide('seeGameSection');
	doHide('addGameSection');
	doHide('checkGameLoad2');
	doUnhide('exprControl');
	doUnhide('statusGraphs');
	doUnhide('statusExprLoading');
	doUnhide('exprCountdownGame');
	doHide('statusExprLoaded');
	doHide('statusExprWaiting');
	doHide('statusExprFinished');
	doHide('statusExprProg');
	doClassLoading('checkChangeEmail');
	doClassLoading('checkChangePass');
}

function reloadExpr() 
{

	sendQuery('@ADMINURI@/dogetexpr.json', 
		reloadExprSetup, reloadExprSuccess, logout);
}

function doStartExprSuccess(resp) 
{

	doValue('startExprSubmit', 'Started!  Reloading...');
	document.getElementById('startExpr').reset();
	window.location.reload(true);
}

function startExpr(form)
{

	return(sendForm(form, 
		function() { doSetup('startExprSubmit', 'startExprErr'); doValue('startExprSubmit', 'Starting...'); },
		function(err) { doError(err, 'startExprSubmit', 'startExprErr'); doValue('startExprSubmit', 'Start Experiment'); },
		doStartExprSuccess));
}

function addGame(form)
{

	return(sendForm(form, 
		function() { doSetup('addGameSubmit', 'addGameErr'); },
		function(err) { doError(err, 'addGameSubmit', 'addGameErr'); },
		function() { doSuccess('addGameSubmit', 'addGame'); loadNewGames(); }));
}

function addNewPlayers2(form)
{

	return(sendForm(form, 
		function() { doSetup('addNewPlayersSubmit2', null); },
		function(err) { doError(err, 'addNewPlayersSubmit2', null); },
		function () { doSuccess('addNewPlayersSubmit2', 'addNewPlayers2'); loadNewPlayers(); }));
}

function addNewPlayers(form)
{

	return(sendForm(form, 
		function() { doSetup('addNewPlayersSubmit', null); },
		function(err) { doError(err, 'addNewPlayersSubmit', null); },
		function () { doSuccess('addNewPlayersSubmit', 'addNewPlayers'); loadNewPlayers(); }));
}

function addPlayers2(form)
{

	return(sendForm(form, 
		function() { doSetup('addPlayersSubmit2', null); },
		function(err) { doError(err, 'addPlayersSubmit2', null); },
		function () { doSuccess('addPlayersSubmit2', 'addPlayers2'); loadPlayers(); }));
}

function addPlayers(form)
{

	return(sendForm(form, 
		function() { doSetup('addPlayersSubmit', null); },
		function(err) { doError(err, 'addPlayersSubmit', null); },
		function () { doSuccess('addPlayersSubmit', 'addPlayers'); loadPlayers(); }));
}

function doWinnersSetup()
{

	doHide('statusExprFinishedWinError');
	doHide('statusExprFinishedWinForm');
	doValue('winnersButton', 'Computing...');
}

function doWinnersError(code)
{

	doValue('winnersButton', 'Compute Winners');
	if (400 == code)
		doUnhide('statusExprFinishedWinForm');
	else
		doUnhide('statusExprFinishedWinError');
}

function sendWinners(form) 
{

	return(sendForm(form, 
		doWinnersSetup, 
		doWinnersError, 
		function() { doValue('winnersButton', 'Compute Winners'); window.location.reload(true); }));
}

function backup() 
{

	sendQuery('@ADMINURI@/dobackup.json', 
		function() { doClearReplace('backupButton', 'Backing up...'); },
		function() { doClearReplace('backupButton', 'Backup Experiment'); },
		null);
}

function advanceEndRound() 
{

	sendQuery('@ADMINURI@/doadvanceend.json', null, null, 
		function() { window.location.reload(true); });
}

function advanceRound() 
{

	sendQuery('@ADMINURI@/doadvance.json', null, null, 
		function() { window.location.reload(true); });
}

function mturkBonusesSetup()
{

	doHide('mturkbonusbtn');
	doUnhide('mturkbonuspbtn');
}

function mturkBonusesFinish()
{

	doUnhide('mturkbonusbtn');
	doHide('mturkbonuspbtn');
}

function mturkBonuses() 
{

	sendQuery('@ADMINURI@/domturkbonuses.json', 
		mturkBonusesSetup, 
		mturkBonusesFinish, 
		mturkBonusesFinish);
}

function wipeCleanExpr() 
{

	sendQuery('@ADMINURI@/dowipequiet.json', 
		function() { doHide('wipeExprResults'); doClearReplace('wipeCleanExprButton', 'Wiping...'); },
		function() { 
			doClearReplace('wipeCleanExprButton', 'Hard-Wipe Experiment'); 
			doUnhide('wipeExprResults'); 
			setTimeout(function(){window.location.reload(true)}, 1000);
		},
		null);
}

function wipeExpr() 
{

	sendQuery('@ADMINURI@/dowipe.json', 
		function() { doHide('wipeExprResults'); doClearReplace('wipeExprButton', 'Wiping...'); },
		function() { 
			doClearReplace('wipeExprButton', 'Wipe Experiment'); 
			doUnhide('wipeExprResults'); 
			setTimeout(function(){window.location.reload(true)}, 1000);
		},
		null);
}

function resetPasswords() 
{

	sendQuery('@ADMINURI@/doresetpasswords', 
		function() { doClearReplace('resetPasswordsButton', 'Resetting All Passwords...'); },
		function() { doClearReplace('resetPasswordsButton', 'Reset All Password'); loadPlayers(); },
		null);
}

function reTestSmtp() 
{

	sendQuery('@ADMINURI@/doresendemail.json', 
		function() { doClearReplace('recheckSmtpButton', 'Resending...'); },
		function() { doClearReplace('recheckSmtpButton', 'Resend Error Mails'); loadPlayers(); },
		null);
}

function testSmtpSuccess(resp)
{

	doUnhide('testSmtpBtn');
	doHide('testSmtpPBtn');
	doUnhide('testSmtpResults');
}

function testSmtpError(code)
{

	doUnhide('testSmtpBtn');
	doHide('testSmtpPBtn');
	if (403 === code)
		logout();
}

function testSmtpSetup()
{

	doHide('testSmtpResults');
	doHide('testSmtpResultsFail');
	doHide('testSmtpBtn');
	doUnhide('testSmtpPBtn');
}

function testSmtp() 
{

	sendQuery('@ADMINURI@/dotestsmtp.json', 
		testSmtpSetup, testSmtpSuccess, testSmtpError);
}

function changeInstr(form)
{

	return(sendForm(form, 
		function() { doSetup('changeInstrSubmit', 'changeInstrErr'); },
		function(err) { doError(err, 'changeInstrSubmit', 'changeInstrErr'); },
		function() { doSuccess('changeInstrSubmit', 'changeInstr'); location.href = '@ADMINURI@'; }));
}

function changeMail(form)
{

	return(sendForm(form, 
		function() { doSetup('changeMailSubmit', 'changeMailErr'); },
		function(err) { doError(err, 'changeMailSubmit', 'changeMailErr'); },
		function() { doSuccess('changeMailSubmit', 'changeMail'); reloadExpr(); }));
}

function changePass(form)
{
	return(sendForm(form, 
		function() { doSetup('changePassSubmit', 'changePassErr'); },
		function(err) { doError(err, 'changePassSubmit', 'changePassErr'); },
		function() { doSuccess('changePassSubmit', 'changePass'); location.href = '@ADMINURI@'; }));
}

function changeSmtp(form)
{

	return(sendForm(form, 
		function() { doSetup('changeSmtpSubmit', 'changeSmtpErr'); },
		function(err) { doError(err, 'changeSmtpSubmit', 'changeSmtpErr'); },
		function() { doSuccess('changeSmtpSubmit', 'changeSmtp'); loadSmtp(); }));
}

function resetPass(form)
{

	return(sendForm(form, 
		function() { doValue('resetPasswordButton', 'Resetting Password...'); },
		null, 
		function() { doValue('resetPasswordButton', 'Reset Password'); loadPlayers(); }));
}

function
checkWipeButton(e)
{

	if (e.checked) {
		document.getElementById('wipeExprButton').disabled='';
		document.getElementById('wipeCleanExprButton').disabled='';
		document.getElementById('backupButton').disabled='';
		document.getElementById('advanceButton').disabled='';
		document.getElementById('advanceEndButton').disabled='';
	} else {
		document.getElementById('wipeExprButton').disabled='disabled';
		document.getElementById('wipeCleanExprButton').disabled='disabled';
		document.getElementById('backupButton').disabled='disabled';
		document.getElementById('advanceButton').disabled='disabled';
		document.getElementById('advanceEndButton').disabled='disabled';
	}
}

/*
 * Toggle functions.
 * This is used in several places to "toggle" a "fa" image.
 */
function
toggler(input, toggle)
{
	var e, i;

	e = document.getElementById(input);
	i = document.getElementById(toggle);

	i.classList.remove('fa-toggle-off');
	i.classList.remove('fa-toggle-on');

	if (0 == parseInt(e.value)) {
		i.classList.add('fa-toggle-on');
		e.value = 1;
	} else {
		i.classList.add('fa-toggle-off');
		e.value = 0;
	}
}

/*
 * Unroll a section of the administrative page.
 */
function unroll(child, parnt) {
	child.classList.remove('fa-chevron-circle-right');
	child.classList.remove('fa-chevron-circle-down');

	if (parnt.classList.contains('rolled')) {
		parnt.classList.remove('rolled');
		parnt.classList.add('unrolled');
		child.classList.add('fa-chevron-circle-down');
	} else {
		parnt.classList.remove('unrolled');
		parnt.classList.add('rolled');
		child.classList.add('fa-chevron-circle-right');
	}
}

/*
 * Set the experiment start time to be now plus the given number of
 * milliseconds (yes, MILLISECONDS).
 */
function exprStartTime(val)
{
	var t, d, now;

	now = new Date();
	t = document.getElementById('time');
	d = document.getElementById('date');

	now.setTime(now.getTime() + val);

	d.value = (now.getUTCFullYear() + '-' + 
		   ('0' + (now.getUTCMonth()+1)).slice(-2) + '-' + 
		   ('0' + now.getUTCDate()).slice(-2));
	t.value = (('0' + now.getUTCHours()).slice(-2) + ':' +
		   ('0' + now.getUTCMinutes()).slice(-2));
}

/*
 * Replace the "login URL" identified in the experiment start section
 * with the current URL having its admin bits replaced with lab bits.
 */
function seturls() 
{
	var url = document.URL;

	if (null == url)
		return;

	url = url.substring(0, url.lastIndexOf("/"));
	url = url.replace('admin', 'lab');
	doValue('loginURI', url + '/dologin.html');
}

function loadallSuccess(resp)
{
	var res, list, i;
	
	try  { 
		res = JSON.parse(resp);
	} catch (error) {
		return;
	}

	doUnhide('exprLoaded');
	doHide('exprLoading');

	list = document.getElementsByClassName('only-started');

	if (0 === res.expr.state) {
		for (i = 0; i < list.length; i++)
			doHideNode(list[i]);
		loadNewGames();
		loadNewPlayers();
		loadSmtp();
		seturls();
		loadExprSetup();
		loadNewExprSuccess(res);
	} else {
		for (i = 0; i < list.length; i++)
			doUnhideNode(list[i]);
		loadGames();
		loadPlayers();
		loadSmtp();
		reloadExprSetup();
		loadExprSuccess(res);
	}
}

function loadallSetup()
{

	doHide('exprLoaded');
	doUnhide('exprLoading');
	doClassLoading('checkChangeEmail');
	doClassLoading('checkChangePass');
}

function loadall()
{
	sendQuery('@ADMINURI@/dogetexpr.json', 
		loadallSetup, loadallSuccess, logout);
}
