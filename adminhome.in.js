"use strict";

function doClassOk(name)
{
	var e;
	if (null != (e = document.getElementById(name)))
		e.className = 'fa fa-check-square-o';
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
		e.className = 'fa fa-square-o';
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

	if (null != (e = document.getElementById(submitName)))
		if (e.hasAttribute('gamelab-submit-default'))
			e.value = e.getAttribute('gamelab-submit-default');

	switch (err) {
	case 400:
		doUnhide(errName + 'Form');
		break;
	case 403:
		location.href = '@HTURI@/adminlogin.html#loggedout';
		break;
	case 409:
		doUnhide(errName + 'State');
		break;
	default:
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

	if (null != (e = document.getElementById(submitName))) 
		if (e.hasAttribute('gamelab-submit-pending')) {
			e.setAttribute('gamelab-submit-default', e.value);
			e.value = e.getAttribute('gamelab-submit-pending');
		}

	doHide(errName + 'Form');
	doHide(errName + 'System');
	doHide(errName + 'State');
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

	if (null != (icon = document.getElementById('autoaddToggle'))) {
		icon.classList.remove('fa-toggle-off');
		icon.classList.remove('fa-toggle-on');
		if (results.autoadd) {
			icon.classList.add('fa-toggle-on');
			doValue('autoadd', 1);
		} else {
			icon.classList.add('fa-toggle-off');
			doValue('autoadd', 0);
		}
	}

	players = results.players;

	if (0 == players.length) {
		li = document.createElement('li');
		e.appendChild(li);
		li.setAttribute('class', 'error');
		li.appendChild(document.createTextNode('No players.'));
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
		icon.setAttribute('class', 'fa fa-remove');
		icon.setAttribute('href', '#;');
		icon.setAttribute('id', 'playerDelete' + player.id);
		icon.setAttribute('onclick', 'doDeletePlayer(' + player.id + '); return false;');
		span.appendChild(icon);
		span.appendChild(document.createTextNode(' '));
		span.appendChild(document.createTextNode(player.mail));
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

function doShowPlayer(name)
{
	var e, status;

	if (null == (e = document.getElementById(name)))
		return;
	if ( ! e.hasAttribute('data-gamelab-status'))
		return;
	if ( ! e.hasAttribute('data-gamelab-mail'))
		return;
	if ( ! e.hasAttribute('data-gamelab-enabled'))
		return;
	if ( ! e.hasAttribute('data-gamelab-playerid'))
		return;

	doValue('playerInfoId', e.getAttribute('data-gamelab-playerid'));
	doClearReplace('playerInfoEmail', e.getAttribute('data-gamelab-mail'));

	switch (parseInt(e.getAttribute('data-gamelab-status'))) {
	case (0):
		doClearReplace('playerInfoStatus', 'not e-mailed');
		break;
	case (1):
		doClearReplace('playerInfoStatus', 'e-mailed');
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
	var e, li, i, results, players, icon, link, span, link, sup, player;

	e = doClearNode(document.getElementById('loadPlayers'));

	try  { 
		results = JSON.parse(resp);
	} catch (error) {
		return;
	}

	players = results.players;

	/* We have no players... */
	if (0 == players.length) {
		li = document.createElement('li');
		li.setAttribute('class', 'error');
		li.appendChild(document.createTextNode('No players.'));
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

		/*
		 * Append the toggle-able link for enabling or disable
		 * the current player.
		 */
		icon = document.createElement('a');
		icon.setAttribute('href', '#');
		icon.setAttribute('id', 'playerLoad' + player.id);
		if (0 == player.enabled) {
			icon.className = 'fa fa-fw fa-toggle-off';
			icon.setAttribute('onclick', 'doEnablePlayer(' + 
				player.id + '); return false;');
		} else {
			icon.className = 'fa fa-fw fa-toggle-on';
			icon.setAttribute('onclick', 'doDisablePlayer(' + 
				player.id + '); return false;');
		}
		span.appendChild(icon);
		span.appendChild(document.createTextNode(' '));

		/* Append whether the player is a row or column role. */
		sup = document.createElement('i');
		if (0 == parseInt(player.role))
			sup.className = 'fa fa-fw fa-bars';
		else
			sup.className = 'fa fa-fw fa-columns';
		span.appendChild(document.createTextNode(' '));
		span.appendChild(sup);

		/* Append the link to show more information. */
		link = document.createElement('a');
		link.setAttribute('href', '#');
		link.setAttribute('onclick', 'doShowPlayer("player' + 
			player.id + '"); return false;');
		link.appendChild(document.createTextNode(player.mail));
		span.appendChild(link);
		if (player.autoadd) {
			icon = document.createElement('i');
			icon.className = 'fa fa-thumb-tack';
			span.appendChild(document.createTextNode(' '));
			span.appendChild(icon);
		}
	}
}

function loadGamesSuccessInner(resp, code) 
{
	var i, j, k, results, li, e, div, icon;

	e = doClearNode(document.getElementById('loadGames'));

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
		div = document.createElement('span');
		div.setAttribute('id', 'game' + results[i].id);

		if (0 == code) {
			icon = document.createElement('a');
			icon.setAttribute('href', '#');
			icon.setAttribute('class', 'fa fa-remove');
			icon.setAttribute('id', 'gameDelete' + results[i].id);
			icon.setAttribute('onclick', 'doDeleteGame(' + results[i].id + '); return false;');
			div.appendChild(icon);
			div.appendChild(document.createTextNode(' '));
		}

		div.appendChild(document.createTextNode(results[i].name));
		div.appendChild(document.createTextNode(': {'));
		for (j = 0; j < results[i].payoffs.length; j++) {
			if (j > 0)
				div.appendChild(document.createTextNode(', '));
			div.appendChild(document.createTextNode('{'));
			for (k = 0; k < results[i].payoffs[j].length; k++) {
				if (k > 0)
					div.appendChild(document.createTextNode(', '));
				div.appendChild(document.createTextNode('('));
				div.appendChild(document.createTextNode(results[i].payoffs[j][k][0]));
				div.appendChild(document.createTextNode(', '));
				div.appendChild(document.createTextNode(results[i].payoffs[j][k][1]));
				div.appendChild(document.createTextNode(')'));
			}
			div.appendChild(document.createTextNode('}'));
		}
		div.appendChild(document.createTextNode('}'));
		li.appendChild(div);
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
			loadPlayers()
	} 
	xrh.open('GET', '@ADMINURI@/' + url + '.json?pid=' + id, true);
	xrh.send(null);
}

function doDeleteGame(id)
{
	var xrh, e;

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
	} 
	xrh.open('GET', '@ADMINURI@/dodeletegame.json?gid=' + id, true);
	xrh.send(null);
}

/*
 * This is called in the administratiev "new" phase to remove a player
 * from the list of potential players.
 */
function doDeletePlayer(id)
{
	var xrh, e;

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
	} 
	xrh.open('GET', '@ADMINURI@/dodeleteplayer.json?pid=' + id, true);
	xrh.send(null);
}

function doEnablePlayer(id) 
{
	var e;

	doDisableEnablePlayer(id, 'doenableplayer');
	if (null != (e = document.getElementById('player' + id)))
		e.setAttribute('data-gamelab-enabled', '1');

}

function doDisablePlayer(id) 
{
	var e;

	doDisableEnablePlayer(id, 'dodisableplayer');
	if (null != (e = document.getElementById('player' + id)))
		e.setAttribute('data-gamelab-enabled', '0');
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
		function(resp) { loadGamesSuccessInner(resp, 0); },
		function(err) { loadError(err, 'loadGames'); });
}

function loadGames() 
{

	doClassLoading('checkGameLoad2');
	loadList('@ADMINURI@/doloadgames.json', 'loadGames', 
		function(resp) { loadGamesSuccessInner(resp, 1); },
		function(err) { loadError(err, 'loadGames'); });
}

function loadNewExprSuccess(resp) 
{
	var res, e, expr;

	console.log('resp = ' + resp);

	try  { 
		res = JSON.parse(resp);
	} catch (error) {
		return;
	}

	expr = res.expr;
	if (0 != expr.start) 
		exprStartAbsoluteTime(expr.start);
	if (0 != expr.rounds)
		document.getElementById('rounds').value = expr.rounds;
	if (0 != expr.minutes)
		document.getElementById('minutes').value = expr.minutes;
	if ('' != expr.instr)
		doClearReplace('instrText', expr.instr);
}

function loadExprSuccess(resp) 
{
	var res, e, i, chld, expr, li, div, href, span, next;

	try  { 
		res = JSON.parse(resp);
	} catch (error) {
		return;
	}

	expr = res.expr;

	doClearReplace('instr', expr.instr);

	if (expr.round >= expr.rounds) {
		doHide('statusExprProg');
		doHide('statusExprLoading');
		doUnhide('statusExprFinished');
		if (expr.postwin) {
			doHide('statusExprFinishedWin');
			e = doClear('statusExprFinishedWinners');
			for (i = 0; i < res.winners.length; i++) {
				chld = document.createElement('li');
				e.appendChild(chld);
				div = document.createElement('div');
				chld.appendChild(div);
				href = document.createElement('a');
				href.setAttribute('href', 'mailto:' + res.winners[i].email);
				href.appendChild(document.createTextNode(res.winners[i].email));
				div.appendChild(href);
				div.appendChild(document.createTextNode(': '));
				div.appendChild(document.createTextNode((res.winners[i].rank + 1)));
				div.appendChild(document.createTextNode
					(' (range ' + res.winners[i].winrank + '\u2013' + 
					 (res.winners[i].winrank + res.winners[i].winscore) + 
					 ', ticket ' +
					 res.winners[i].winnum + ')'));
			}
		} else
			doUnhide('statusExprFinishedWin');
		doClearReplace('exprCountdown', 'finished');
	}  else if (expr.round < 0) {
		next = expr.start - Math.floor(new Date().getTime() / 1000);
		doUnhide('statusExprWaiting');
		doHide('statusExprLoading');
		e = doClear('exprCountdown');
		formatCountdown(next, e);
		setTimeout(timerCountdown, 1000, loadExpr, e, expr.start);
	} else {
		next = (expr.roundbegan + (expr.minutes * 60)) -
			Math.floor(new Date().getTime() / 1000);
		e = doClear('exprCountdown');
		doHide('statusExprLoading');
		formatCountdown(next, e);
		setTimeout(timerCountdown, 1000, loadExpr, e, 
			expr.roundbegan + (expr.minutes * 60));

		doUnhide('statusExprProg');
		doClearReplace('statusExprPBar', 
			Math.round(expr.progress * 100.0) + '%');
		doClearReplace('statusExprPRound', (expr.round + 1));
		doClearReplace('statusExprPMax', expr.rounds);
		doValue('statusExprPBar', expr.progress);

		doClearReplace('statusExprFrow', res.frow);
		doClearReplace('statusExprFcol', res.fcol);

		e = doClear('statusExprPGames');
		li = document.createElement('div');
		div = document.createElement('div');
		li.appendChild(div);
		div = document.createElement('div');
		div.className = 'table-top-head';
		div.appendChild(document.createTextNode('Row Plays'));
		li.appendChild(div);
		div = document.createElement('div');
		div.className = 'table-top-head';
		div.appendChild(document.createTextNode('Column Plays'));
		li.appendChild(div);
		e.appendChild(li);

		for (i = 0; i < res.games.length; i++) {
			li = document.createElement('div');
			div = document.createElement('div');
			div.className = 'table-left-head';
			div.appendChild(document.createTextNode(res.games[i].name));
			li.appendChild(div);
			div = document.createElement('div');
			div.appendChild(document.createTextNode(res.games[i].prow));
			li.appendChild(div);
			div = document.createElement('div');
			div.appendChild(document.createTextNode(res.games[i].pcol));
			li.appendChild(div);
			e.appendChild(li);
		}
	}
}

function loadExprSetup()
{

	doUnhide('statusExprLoading');
	doHide('statusExprWaiting');
	doHide('statusExprFinished');
	doHide('statusExprProg');
}

function loadNewExpr() 
{

	sendQuery('@ADMINURI@/dogetnewexpr.json', 
		null, loadNewExprSuccess, null);
}

function loadExpr() 
{

	sendQuery('@ADMINURI@/dogetexpr.json', 
		loadExprSetup, 
		loadExprSuccess, 
		function() { location.href = '@HTURI@/adminlogin.html#loggedout'; });
}

function doStartExprSuccess(resp) 
{

	doValue('startExprSubmit', 'Started!  Reloading...');
	document.getElementById('startExpr').reset();
	window.location.reload(true);
}

function startExprTo(form, to)
{

	return(sendFormTo(form, to,
		function() { doSetup('startExprSave', 'startExprErr'); },
		function(err) { doError(err, 'startExprSave', 'startExprErr'); },
		function() { window.location.reload(true); }));
}

function startExpr(form)
{

	return(sendForm(form, 
		function() { doSetup('startExprSubmit', 'startExprErr'); doValue('startExprSubmit', 'Starting...'); },
		function(err) { doError(err, 'startExprSubmit', 'startExprErr'); doValue('startExprSubmit', 'Start'); },
		doStartExprSuccess));
}

function addGame(form)
{

	return(sendForm(form, 
		function() { doSetup('addGameSubmit', 'addGameErr'); },
		function(err) { doError(err, 'addGameSubmit', 'addGameErr'); },
		function() { doSuccess('addGameSubmit', 'addGame'); loadNewGames(); }));
}

function addPlayers(form)
{

	return(sendForm(form, 
		function() { doSetup('addPlayersSubmit', 'addPlayersErr'); },
		function(err) { doError(err, 'addPlayersSubmit', 'addPlayersErr'); },
		function () { doSuccess('addPlayersSubmit', 'addPlayers'); loadNewPlayers(); }));
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

function advanceRound() 
{

	sendQuery('@ADMINURI@/doadvance.json', 
		null,
		null,
		function() { window.location.reload(true); });
}

function wipeExpr() 
{

	sendQuery('@ADMINURI@/dowipe.json', 
		function() { doHide('wipeExprResults'); doClearReplace('wipeExprButton', 'Wiping...'); },
		function() { doClearReplace('wipeExprButton', 'Wipe Experiment'); doUnhide('wipeExprResults'); },
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

function doTestSmtpSuccess(resp)
{
	var results, mail;

	try  { 
		results = JSON.parse(resp);
		mail = results.mail;
	} catch (error) {
		mail = 'unknown';
	}

	doClearReplace('checkSmtpButton', 'Send Test');
	doClearReplace('testSmtpResultsMail', mail);
	doUnhide('testSmtpResults');
}

function testSmtp() 
{

	sendQuery('@ADMINURI@/dotestsmtp.json', 
		function() { doHide('testSmtpResults'); doClearReplace('checkSmtpButton', 'Mailing test...'); },
		doTestSmtpSuccess, 
		null);
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
		function() { doSuccess('changeMailSubmit', 'changeMail'); loadExpr(); }));
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

	if (e.checked)
		document.getElementById('wipeExprButton').disabled='';
	else
		document.getElementById('wipeExprButton').disabled='disabled';
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
 * Set the experiment start time to be the given time (in SECONDS).
 */
function exprStartAbsoluteTime(val)
{
	var t, d, now;

	now = new Date();
	t = document.getElementById('time');
	d = document.getElementById('date');

	now.setTime(val * 1000);

	d.value = (now.getUTCFullYear() + '-' + 
		   ('0' + (now.getUTCMonth()+1)).slice(-2) + '-' + 
		   ('0' + now.getUTCDate()).slice(-2));
	t.value = (('0' + now.getUTCHours()).slice(-2) + ':' +
		   ('0' + now.getUTCMinutes()).slice(-2));
}
