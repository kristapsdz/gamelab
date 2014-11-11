"use strict";

function doSuccess(submitName, formName) 
{

	document.getElementById(submitName).value = 'Submit';
	document.getElementById(formName).reset();
}

function doError(err, submitName, errName) 
{

	document.getElementById(submitName).value = 'Submit';
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

	document.getElementById(submitName).value = 'Submitting...';
	doHide(errName + 'Form');
	doHide(errName + 'System');
	doHide(errName + 'State');
}

function loadNewPlayersSuccess(resp) 
{
	var e, li, i, results, icon, link, span, count;

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
		span.appendChild(document.createTextNode('No players.'));
		li.appendChild(span);
		e.appendChild(li);
		doHide('checkPlayersLoad');
		doUnhide('checkPlayersNo');
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
		span.appendChild(document.createTextNode(results[i].mail));
		li.appendChild(span);

		icon = document.createElement('img');
		icon.setAttribute('src', '@@htdocs@@/disable.png');
		icon.setAttribute('id', 'playerDelete' + results[i].id);
		icon.setAttribute('onclick', 'doDeletePlayer(' + results[i].id + '); return false;');
		icon.setAttribute('class', 'disable');
		icon.setAttribute('alt', 'Delete');
		span.appendChild(icon);

		appendLoading(span).setAttribute
			('id', 'playerWaiting' + results[i].id);

		doHide('playerWaiting' + results[i].id);
	}

	doHide('checkPlayersLoad');
	doUnhide(count >= 2 ? 'checkPlayersYes' : 'checkPlayersNo');
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
		doClearReplace('playerInfoStatus', 'unknown');
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
		span.setAttribute('id', 'player' + results[i].id);
		span.setAttribute('data-gamelab-status', results[i].status);
		span.setAttribute('data-gamelab-mail', results[i].mail);
		span.setAttribute('data-gamelab-enabled', results[i].enabled);
		li.appendChild(span);

		link = document.createElement('a');
		link.setAttribute('href', '#');
		link.setAttribute('onclick', 'doShowPlayer("player' + results[i].id + '"); return false;');
		link.appendChild(document.createTextNode(results[i].mail));
		span.appendChild(link);

		sup = document.createElement('sup');
		if (0 == parseInt(results[i].role))
			sup.appendChild(document.createTextNode('row'));
		else
			sup.appendChild(document.createTextNode('col'));
		span.appendChild(sup);

		icon = document.createElement('img');
		icon.setAttribute('src', '@@htdocs@@/disable.png');
		icon.setAttribute('id', 'playerDisable' + results[i].id);
		icon.setAttribute('onclick', 'doDisablePlayer(' + results[i].id + ');');
		icon.setAttribute('class', 'disable');
		icon.setAttribute('alt', 'Disable');
		span.appendChild(icon);

		icon = document.createElement('img');
		icon.setAttribute('src', '@@htdocs@@/enable.png');
		icon.setAttribute('id', 'playerEnable' + results[i].id);
		icon.setAttribute('onclick', 'doEnablePlayer(' + results[i].id + ');');
		icon.setAttribute('class', 'enable');
		icon.setAttribute('alt', 'Enable');
		span.appendChild(icon);

		appendLoading(span).setAttribute
			('id', 'playerWaiting' + results[i].id);

		doHide('playerWaiting' + results[i].id);
		doHide((0 == results[i].enabled ? 'playerDisable' : 'playerEnable') + results[i].id);
	}
}

function loadGamesSuccess(resp) 
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
		div.appendChild(document.createTextNode('No games.'));
		li.appendChild(div);
		doHide('checkGameLoad');
		doUnhide('checkGameNo');
		return;
	}

	doHide('checkGameLoad');
	doUnhide('checkGameYes');

	for (i = 0; i < results.length; i++) {
		li = document.createElement('li');
		div = document.createElement('span');
		div.setAttribute('id', 'game' + results[i].id);
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

		icon = document.createElement('img');
		icon.setAttribute('src', '@@htdocs@@/disable.png');
		icon.setAttribute('id', 'gameDelete' + results[i].id);
		icon.setAttribute('onclick', 'doDeleteGame(' + results[i].id + '); return false;');
		icon.setAttribute('class', 'disable');
		icon.setAttribute('alt', 'Delete');
		div.appendChild(icon);

		appendLoading(div).setAttribute
			('id', 'gameWaiting' + results[i].id);

		li.appendChild(div);
		e.appendChild(li);

		doHide('gameWaiting' + results[i].id);
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

	doHide('playerInfo');
	doHide('playerDisable' + id);
	doHide('playerEnable' + id);
	doUnhide('playerWaiting' + id);

	xrh = new XMLHttpRequest();
	xrh.onreadystatechange=function() {
		if (xrh.readyState==4 && xrh.status==200) {
			if (null != (e = document.getElementById('player' + id)))
				e.className = '';
			doHide('playerWaiting' + id);
			if (url == 'doenableplayer')
				doUnhide('playerDisable' + id);
			else
				doUnhide('playerEnable' + id);
		}
	} 
	xrh.open('GET', '@@cgibin@@/' + url + '.json?pid=' + id, true);
	xrh.send(null);
}

function doDeleteGame(id)
{
	var xrh, e;

	if (null != (e = document.getElementById('game' + id)))
		e.className = 'waiting';

	doHide('gameDelete' + id);
	doUnhide('gameWaiting' + id);

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

	doHide('playerDelete' + id);
	doUnhide('playerWaiting' + id);

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

	doHide('checkSmtpYes');
	doHide('checkSmtpNo');
	doHide('checkSmtpResults');
	doHide('checkSmtpResultsNone');
	doUnhide('checkSmtpLoad');
	doUnhide('checkSmtpResultsLoad');
}

function loadSmtpSuccess(resp)
{
	var results, e, link, button;

	doHide('checkSmtpLoad');
	doUnhide('checkSmtpYes');

	try {
		results = JSON.parse(resp);
	} catch (error) {
		return;
	}

	doHide('checkSmtpResultsLoad');
	doUnhide('checkSmtpResults');

	doClearReplace('checkSmtpResultsServer', results.server);
	doClearReplace('checkSmtpResultsUser', results.user);
	doClearReplace('checkSmtpResultsFrom', results.mail);

	/*e.appendChild(document.createTextNode('Current values: '));
	link = document.createElement('a');
	link.setAttribute('href', 'mailto:' + results.mail);
	link.appendChild(document.createTextNode(results.mail));
	e.appendChild(link);
	e.appendChild(document.createTextNode(', '));
	e.appendChild(document.createTextNode(results.user));
	e.appendChild(document.createTextNode('@'));
	e.appendChild(document.createTextNode(results.server));

	button = document.createElement('button');
	button.setAttribute('onclick', 'testSmtp();');
	button.appendChild(document.createTextNode('Test'));
	e.appendChild(button);*/
}

function loadSmtpError(err)
{

	if (400 != err)
		return;
	doHide('checkSmtpLoad');
	doUnhide('checkSmtpNo');
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
	var e, li;

	li = document.createElement('li');
	appendLoading(li);
	doClear(name).appendChild(li);
	sendQuery(url, null, onsuccess, onerror);
}

function loadNewPlayers() 
{

	doHide('checkPlayersYes');
	doHide('checkPlayersNo');
	doUnhide('checkPlayersLoad');
	loadList('@@cgibin@@/doloadplayers.json', 'loadNewPlayers', 
		loadNewPlayersSuccess, loadNewPlayersError);
}


function loadPlayers() 
{

	loadList('@@cgibin@@/doloadplayers.json', 'loadPlayers', 
		loadPlayersSuccess, loadPlayersError);
}

function loadGames() 
{

	doHide('checkGameYes');
	doHide('checkGameNo');
	doUnhide('checkGameLoad');
	loadList('@@cgibin@@/doloadgames.json', 'loadGames', 
		loadGamesSuccess, loadGamesError);
}

function loadExprSuccess(resp) 
{
	var results, v, e, chld, head, expr;

	console.log(resp);
	try  { 
		results = JSON.parse(resp);
	} catch (error) {
		return;
	}

	expr = results.expr;

	if ((v = parseInt(expr.tilstart)) > 0) {
		doUnhide('statusExprWaiting');
		e = doClearNode(doUnhide('statusExprTtl'));
		doHide('statusExprLoading');
		chld = document.createElement('div');
		head = 'Time Until Experiment';
		formatCountdown(head, v, chld);
		e.appendChild(chld);
		setTimeout(timerCountdown, 1000, 
			head, loadExpr, chld, 
			v, new Date().getTime());
	} else {
		if (0 == v) {
			v = parseInt(expr.tilnext);
			e = doClearNode(doUnhide('statusExprTtl'));
			doHide('statusExprLoading');
			chld = document.createElement('div');
			head = 'Time Until Round Expires';
			formatCountdown(head, v, chld);
			e.appendChild(chld);
			setTimeout(timerCountdown, 1000, 
				head, loadExpr, chld, 
				v, new Date().getTime());
		}
		e = doClearNode(doUnhide('statusExprProgress'));
		doHide('statusExprLoading');
		chld = document.createElement('div');
		chld.appendChild(document.createTextNode
			('Round: ' + (parseInt(expr.round) + 1) + '/' + 
			 expr.rounds + ' (' + 
			 Math.round(parseFloat(expr.progress) * 100.0) + 
			 '%)'));
		e.appendChild(chld);
		appendProgress(e, expr.progress);
	}
}

function loadExprSetup()
{

	doUnhide('statusExprLoading');
	doHide('statusExprTtl');
	doHide('statusExprWaiting');
	doHide('statusExprProgress');
}

function loadExpr() 
{

	sendQuery('@@cgibin@@/dogetexpr.json', 
		loadExprSetup, loadExprSuccess, null);
}

function doStartExprSetup() 
{

	doSetup('startExprSubmit', 'startExprErr');
	document.getElementById('startExprSubmit').value = 'Starting...';
}

function doStartExprError(err) 
{

	doError(err, 'startExprSubmit', 'startExprErr');
	document.getElementById('startExprSubmit').value = 'Start';
}

function doStartExprSuccess(resp) 
{

	document.getElementById('startExprSubmit').value = 'Started!  Reloading...';
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
	loadGames();
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

function doTestSmtpSetup()
{

	doHide('testSmtpResults');
	doClearReplace('checkSmtpButton', 'Mailing test...');
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

	sendQuery('@@cgibin@@/dotestsmtp.json', 
		doTestSmtpSetup, doTestSmtpSuccess, null);
}

function doChangeMailSetup() 
{

	doSetup('changeMailSubmit', 'changeMailErr');
}

function doChangeMailError(err) 
{

	doError(err, 'changeMailSubmit', 'changeMailErr');
}

function doChangeMailSuccess(resp) 
{

	doSuccess('changeMailSubmit', 'changeMail');
	location.href = '@@cgibin@@';
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

function doChangeSmtpSetup() 
{

	doSetup('changeSmtpSubmit', 'changeSmtpErr');
}

function doChangeSmtpError(err) 
{

	doError(err, 'changeSmtpSubmit', 'changeSmtpErr');
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
