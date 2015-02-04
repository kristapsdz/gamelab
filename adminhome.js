"use strict";

function doClassOk(name)
{
	var e;
	if (null == (e = document.getElementById(name)))
		return;
	e.className = 'fa fa-check';
}

function doClassLoading(name)
{
	var e;
	if (null == (e = document.getElementById(name)))
		return;
	e.className = 'fa fa-spinner fa-spin';
}

function doClassFail(name)
{
	var e;
	if (null == (e = document.getElementById(name)))
		return;
	e.className = 'fa fa-warning';
}

function doSuccess(submitName, formName) 
{

	doValue(submitName, 'Submit');
	document.getElementById(formName).reset();
}

function doError(err, submitName, errName) 
{

	doValue(submitName, 'Submit');
	switch (err) {
	case 400:
		doUnhide(errName + 'Form');
		break;
	case 409:
		doUnhide(errName + 'State');
		break;
	default:
		doUnhide(errName + 'System');
		break;
	}
}

function doSetup(submitName, errName) 
{

	doValue(submitName, 'Submitting...');
	doHide(errName + 'Form');
	doHide(errName + 'System');
	doHide(errName + 'State');
}

function loadNewPlayersSuccess(resp) 
{
	var e, li, i, results, icon, link, span, count, pspan, fa;

	e = doClearNode(document.getElementById('loadNewPlayers'));
	if (null == e)
		return;

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
		span = document.createElement('span');
		span.setAttribute('class', 'error');
		icon = document.createElement('i');
		icon.setAttribute('class', 'fa fa-warning');
		span.appendChild(icon);
		span.appendChild(document.createTextNode(' '));
		span.appendChild(document.createTextNode('No players.'));
		li.appendChild(span);
		e.appendChild(li);
		doClassFail('checkPlayersLoad');
		return;
	}

	e.className = '';
	li = document.createElement('li');
	e.appendChild(li);

	for (count = i = 0; i < results.length; i++) {
		if (results[i].enabled)
			count++;

		span = document.createElement('span');
		span.setAttribute('id', 'player' + results[i].id);

		icon = document.createElement('a');
		icon.setAttribute('class', 'fa fa-remove');
		icon.setAttribute('href', '#;');
		icon.setAttribute('id', 'playerDelete' + results[i].id);
		icon.setAttribute('onclick', 'doDeletePlayer(' + results[i].id + '); return false;');
		span.appendChild(icon);
		span.appendChild(document.createTextNode(' '));
		span.appendChild(document.createTextNode(results[i].mail));
		li.appendChild(span);
	}

	if (count >= 2)
		doClassOk('checkPlayersLoad');
	else
		doClassFail('checkPlayersLoad');
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

function loadPlayersSuccess(resp) 
{
	var e, li, i, results, icon, link, span, link, sup;

	e = doClearNode(document.getElementById('loadPlayers'));
	if (null == e)
		return;

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
		span = document.createElement('span');
		span.setAttribute('class', 'error');
		icon = document.createElement('i');
		icon.setAttribute('class', 'fa fa-warning');
		span.appendChild(icon);
		span.appendChild(document.createTextNode(' '));
		span.appendChild(document.createTextNode('No players.'));
		li.appendChild(span);
		e.appendChild(li);
		return;
	}

	e.className = '';
	li = document.createElement('li');
	e.appendChild(li);

	for (i = 0; i < results.length; i++) {
		span = document.createElement('span');

		icon = document.createElement('a');
		icon.setAttribute('href', '#');
		icon.setAttribute('id', 'playerLoad' + results[i].id);
		if (0 == results[i].enabled) {
			icon.setAttribute('class', 'fa fa-toggle-off');
			icon.setAttribute('onclick', 'doEnablePlayer(' + results[i].id + '); return false;');
		} else {
			icon.setAttribute('class', 'fa fa-toggle-on');
			icon.setAttribute('onclick', 'doDisablePlayer(' + results[i].id + '); return false;');
		}
		span.appendChild(icon);
		span.appendChild(document.createTextNode(' '));

		span.setAttribute('id', 'player' + results[i].id);
		span.setAttribute('data-gamelab-status', results[i].status);
		span.setAttribute('data-gamelab-mail', results[i].mail);
		span.setAttribute('data-gamelab-enabled', results[i].enabled);
		span.setAttribute('data-gamelab-playerid', results[i].id);

		link = document.createElement('a');
		link.setAttribute('href', '#');
		link.setAttribute('onclick', 'doShowPlayer("player' + results[i].id + '"); return false;');
		link.appendChild(document.createTextNode(results[i].mail));
		span.appendChild(link);

		sup = document.createElement('i');
		if (0 == parseInt(results[i].role))
			sup.setAttribute('class', 'fa fa-bars');
		else
			sup.setAttribute('class', 'fa fa-columns');

		span.appendChild(document.createTextNode(' '));
		span.appendChild(sup);

		li.appendChild(span);
	}
}

function loadGamesSuccess(resp) 
{
	loadGamesSuccessInner(resp, 1);
}

function loadNewGamesSuccess(resp) 
{
	loadGamesSuccessInner(resp, 0);
}

function loadGamesSuccessInner(resp, code) 
{
	var i, j, k, results, li, e, div, icon;

	e = doClearNode(document.getElementById('loadGames'));
	if (null == e)
		return;

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
		icon = document.createElement('i');
		icon.setAttribute('class', 'fa fa-warning');
		div.appendChild(icon);
		div.appendChild(document.createTextNode(' '));
		div.appendChild(document.createTextNode('No games.'));
		li.appendChild(div);
		doClassFail('checkGameLoad');
		return;
	}

	doClassOk('checkGameLoad');

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

function loadGamesError(err) 
{

	loadError(err, 'loadGames');
}

function loadNewPlayersError(err) 
{

	loadError(err, 'loadNewPlayers');
}


function loadPlayersError(err) 
{

	loadError(err, 'loadPlayers');
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
	xrh.open('GET', '@@cgibin@@/' + url + '.json?pid=' + id, true);
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
			loadNewPlayers();
		}
	} 
	xrh.open('GET', '@@cgibin@@/dodeletegame.json?gid=' + id, true);
	xrh.send(null);
}

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
	xrh.open('GET', '@@cgibin@@/dodeleteplayer.json?pid=' + id, true);
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
	doHide('checkSmtpResultsNone');
	doClassLoading('checkSmtpLoad');
	doUnhide('checkSmtpResultsLoad');
}

function loadSmtpSuccess(resp)
{
	var results;

	doClassOk('checkSmtpLoad');

	try {
		results = JSON.parse(resp);
	} catch (error) {
		return;
	}

	doHide('checkSmtpResultsLoad');
	doUnhide('checkSmtpResults');
	doValue('checkSmtpResultsServer', results.server);
	doValue('checkSmtpResultsUser', results.user);
	doValue('checkSmtpResultsFrom', results.mail);
}

function loadSmtpError(err)
{

	if (400 != err)
		return;

	doClassFail('checkSmtpLoad');
	doHide('checkSmtpResultsLoad');
	doUnhide('checkSmtpResultsNone');
}

function loadSmtp() 
{

	sendQuery('@@cgibin@@/dochecksmtp.json', 
		loadSmtpSetup, loadSmtpSuccess, loadSmtpError);
}

function loadList(url, name, onsuccess, onerror) 
{
	var e, li, ii;

	li = document.createElement('li');
	ii = document.createElement('i');
	ii.setAttribute('class', 'fa fa-spinner fa-spin');
	li.appendChild(ii);
	doClear(name).appendChild(li);
	sendQuery(url, null, onsuccess, onerror);
}

function loadNewPlayers() 
{

	doClassLoading('checkPlayersLoad');
	loadList('@@cgibin@@/doloadplayers.json', 'loadNewPlayers', 
		loadNewPlayersSuccess, loadNewPlayersError);
}


function loadPlayers() 
{

	loadList('@@cgibin@@/doloadplayers.json', 'loadPlayers', 
		loadPlayersSuccess, loadPlayersError);
}

function loadNewGames() 
{

	doClassLoading('checkGameLoad');
	loadList('@@cgibin@@/doloadgames.json', 'loadGames', 
		loadNewGamesSuccess, loadGamesError);
}

function loadGames() 
{

	doClassLoading('checkGameLoad');
	loadList('@@cgibin@@/doloadgames.json', 'loadGames', 
		loadGamesSuccess, loadGamesError);
}

function loadExprSuccess(resp) 
{
	var res, e, i, chld, expr, li, div, href, span;

	try  { 
		res = JSON.parse(resp);
	} catch (error) {
		return;
	}

	expr = res.expr;

	doClearReplace('instr', expr.instr);
	doClearReplace('instrWin', expr.instrWin);

	if (expr.tilstart < 0 && expr.tilnext < 0) {
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
			}
		} else
			doUnhide('statusExprFinishedWin');
		doClearReplace('exprCountdown', 'finished');
		return;
	}

	if (expr.tilstart > 0) {
		doUnhide('statusExprWaiting');
		doHide('statusExprLoading');
		e = doClear('exprCountdown');
		formatCountdown(expr.tilstart, e);
		setTimeout(timerCountdown, 1000, loadExpr, 
			e, expr.tilstart, new Date().getTime());
	} else {
		if (0 == expr.tilstart) {
			e = doClear('exprCountdown');
			doHide('statusExprLoading');
			formatCountdown(expr.tilnext, e);
			setTimeout(timerCountdown, 1000, loadExpr, 
				e, expr.tilnext, new Date().getTime());
		}

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
		div.appendChild(document.createTextNode('Name'));
		li.appendChild(div);
		div = document.createElement('div');
		div.appendChild(document.createTextNode('Row Plays'));
		li.appendChild(div);
		div = document.createElement('div');
		div.appendChild(document.createTextNode('Column Plays'));
		li.appendChild(div);
		e.appendChild(li);

		for (i = 0; i < res.games.length; i++) {
			li = document.createElement('div');
			div = document.createElement('div');
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

function loadExprFailure(err)
{
	var url = document.URL;

	url = url.substring(0, url.lastIndexOf("/"));
	location.href = url + '/login.html#loggedout';
}

function loadExpr() 
{

	sendQuery('@@cgibin@@/dogetexpr.json', 
		loadExprSetup, loadExprSuccess, loadExprFailure);
}

function doStartExprSetup() 
{

	doSetup('startExprSubmit', 'startExprErr');
	doValue('startExprSubmit', 'Starting...');
}

function doStartExprError(err) 
{

	doError(err, 'startExprSubmit', 'startExprErr');
	doValue('startExprSubmit', 'Start');
}

function doStartExprSuccess(resp) 
{

	doValue('startExprSubmit', 'Started!  Reloading...');
	document.getElementById('startExpr').reset();
	window.location.reload(true);
}

function startExpr(form)
{

	return(sendForm(form, doStartExprSetup, 
		doStartExprError, doStartExprSuccess));
}

function doAddGameSetup() 
{

	doSetup('addGameSubmit', 'addGameErr');
}

function doAddGameError(err) 
{

	doError(err, 'addGameSubmit', 'addGameErr');
}

function doAddGameSuccess(resp) 
{

	doSuccess('addGameSubmit', 'addGame');
	loadNewGames();
}

function addGame(form)
{

	return(sendForm(form, doAddGameSetup, 
		doAddGameError, doAddGameSuccess));
}

function doAddPlayersSetup() 
{

	doSetup('addPlayersSubmit', 'addPlayersErr');
}

function doAddPlayersError(err) 
{

	doError(err, 'addPlayersSubmit', 'addPlayersErr');
}

function doAddPlayersSuccess(resp) 
{

	doSuccess('addPlayersSubmit', 'addPlayers');
	loadNewPlayers();
}

function addPlayers(form)
{

	return(sendForm(form, doAddPlayersSetup, 
		doAddPlayersError, doAddPlayersSuccess));
}

function doWinnersSetup()
{

	doHide('statusExprFinishedWinError');
	doHide('statusExprFinishedWinForm');
	doValue('winnersButton', 'Computing...');
}

function doBackupSetup()
{

	doClearReplace('backupButton', 'Backing up...');
}


function doWipeExprSetup()
{

	doHide('wipeExprResults');
	doClearReplace('wipeExprButton', 'Wiping...');
}

function doResetPasswordsSetup()
{

	doClearReplace('resetPasswordsButton', 'Resetting All Passwords...');
}

function doReTestSmtpSetup()
{

	doClearReplace('recheckSmtpButton', 'Resending...');
}

function doTestSmtpSetup()
{

	doHide('testSmtpResults');
	doClearReplace('checkSmtpButton', 'Mailing test...');
}

function doWinnersError(code)
{

	doValue('winnersButton', 'Compute Winners');
	if (400 == code)
		doUnhide('statusExprFinishedWinForm');
	else
		doUnhide('statusExprFinishedWinError');
}

function doWinnersSuccess(resp)
{

	doValue('winnersButton', 'Compute Winners');
	window.location.reload(true);
}

function doBackupSuccess(resp)
{

	doClearReplace('backupButton', 'Backup Experiment');
}

function doWipeExprSuccess(resp)
{

	doClearReplace('wipeExprButton', 'Wipe Experiment');
	doUnhide('wipeExprResults');
}

function doResetPasswordsSuccess(resp)
{

	doClearReplace('resetPasswordsButton', 'Reset All Password');
	loadPlayers();
}

function doReTestSmtpSuccess(resp)
{

	doClearReplace('recheckSmtpButton', 'Resend Error Mails');
	loadPlayers();
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

function sendWinners(form) 
{

	return(sendForm(form, doWinnersSetup, 
		doWinnersError, doWinnersSuccess));
}

function backup() 
{

	sendQuery('@@cgibin@@/dobackup.json', 
		doBackupSetup, doBackupSuccess, null);
}


function wipeExpr() 
{

	sendQuery('@@cgibin@@/dowipe.json', 
		doWipeExprSetup, doWipeExprSuccess, null);
}

function resetPasswords() 
{

	sendQuery('@@cgibin@@/doresetpasswords', 
		doResetPasswordsSetup, doResetPasswordsSuccess, null);
}

function reTestSmtp() 
{

	sendQuery('@@cgibin@@/doresendemail.json', 
		doReTestSmtpSetup, doReTestSmtpSuccess, null);
}

function testSmtp() 
{

	sendQuery('@@cgibin@@/dotestsmtp.json', 
		doTestSmtpSetup, doTestSmtpSuccess, null);
}

function doChangeInstrSetup() 
{

	doSetup('changeInstrSubmit', 'changeInstrErr');
}


function doChangeMailSetup() 
{

	doSetup('changeMailSubmit', 'changeMailErr');
}

function doChangeInstrError(err) 
{

	doError(err, 'changeInstrSubmit', 'changeInstrErr');
}

function doChangeMailError(err) 
{

	doError(err, 'changeMailSubmit', 'changeMailErr');
}

function doChangeInstrSuccess(resp) 
{

	doSuccess('changeInstrSubmit', 'changeInstr');
	location.href = '@@cgibin@@';
}


function doChangeMailSuccess(resp) 
{

	doSuccess('changeMailSubmit', 'changeMail');
	loadExpr();
}

function changeInstr(form)
{

	return(sendForm(form, doChangeInstrSetup, 
		doChangeInstrError, doChangeInstrSuccess));
}


function changeMail(form)
{

	return(sendForm(form, doChangeMailSetup, 
		doChangeMailError, doChangeMailSuccess));
}

function doChangePassSetup() 
{

	doSetup('changePassSubmit', 'changePassErr');
}

function doChangePassError(err) 
{

	doError(err, 'changePassSubmit', 'changePassErr');
}

function doChangePassSuccess(resp) 
{

	doSuccess('changePassSubmit', 'changePass');
	location.href = '@@cgibin@@';
}

function changePass(form)
{
	return(sendForm(form, doChangePassSetup, 
		doChangePassError, doChangePassSuccess));
}

function doResetPassSetup() 
{

	doValue('resetPasswordButton', 'Resetting Password...');
}

function doChangeSmtpSetup() 
{

	doSetup('changeSmtpSubmit', 'changeSmtpErr');
}

function doChangeSmtpError(err) 
{

	doError(err, 'changeSmtpSubmit', 'changeSmtpErr');
}

function doResetPassSuccess(resp) 
{

	doValue('resetPasswordButton', 'Reset Password');
	loadPlayers();
}

function doChangeSmtpSuccess(resp) 
{

	doSuccess('changeSmtpSubmit', 'changeSmtp');
	loadSmtp();
}

function changeSmtp(form)
{

	return(sendForm(form, doChangeSmtpSetup, 
		doChangeSmtpError, doChangeSmtpSuccess));
}

function resetPass(form)
{

	return(sendForm(form, doResetPassSetup, 
		null, doResetPassSuccess));
}

function
previewInstr()
{
	var e;

	if (null == (e = document.getElementById('instrText')))
		return;
	doClearReplaceMarkup('previewInstrInner', e.value);
	doUnhide('previewInstr');
}

function
checkWipeButton(e)
{

	if (e.checked)
		document.getElementById('wipeExprButton').disabled='';
	else
		document.getElementById('wipeExprButton').disabled='disabled';
}
