"use strict";

function doSuccess(submitName, formName) {
	document.getElementById(submitName).value = 'Submit';
	document.getElementById(formName).reset();
}

function doError(err, submitName, errName) {
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

function doSetup(submitName, errName) {
	document.getElementById(submitName).value = 'Submitting...';
	doHide(errName + 'Form');
	doHide(errName + 'System');
	doHide(errName + 'State');
}

function doAddPlayersSuccess(resp) {
	doSuccess('addPlayersSubmit', 'addPlayers');
	loadPlayers();
}

function doChangeMailSuccess(resp) {
	doSuccess('changeMailSubmit', 'changeMail');
	location.href = '@@cgibin@@';
}

function doChangePassSuccess(resp) {
	doSuccess('changePassSubmit', 'changePass');
	location.href = '@@cgibin@@';
}

function doChangeSmtpSuccess(resp) {
	doSuccess('changeSmtpSubmit', 'changeSmtp');
	checkSmtp();
}

function doAddGameSuccess(resp) {
	doSuccess('addGameSubmit', 'addGame');
	loadGames();
}

function doStartExprSuccess(resp) {
	document.getElementById('startExprSubmit').value = 'Started!  Reloading...';
	document.getElementById('startExpr').reset();
	window.location.reload(true);
}

function loadPlayersSuccess(resp) {
	var e, li, i, results, icon, link, span, count;

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
		li.appendChild(document.createTextNode('No players.'));
		e.appendChild(li);
		doClearReplace('checkPlayers', 'No.');
		return;
	}

	e.className = '';
	li = document.createElement('li');
	e.appendChild(li);

	for (count = i = 0; i < results.length; i++) {
		if (i > 0)
			li.appendChild(document.createTextNode(', '));
		span = document.createElement('span');
		span.setAttribute('id', 'player' + results[i].id);
		if (0 == results[i].enabled)
			span.setAttribute('class', 'disabled');
		else {
			span.setAttribute('class', 'enabled');
			count++;
		}
		span.appendChild(document.createTextNode(results[i].mail));
		li.appendChild(span);

		icon = document.createElement('img');
		icon.setAttribute('src', '@@htdocs@@/disable.png');
		icon.setAttribute('id', 'playerDisable' + results[i].id);
		icon.setAttribute('onclick', 'doDisablePlayer(' + results[i].id + '); return false;');
		icon.setAttribute('alt', 'Disable');
		icon.setAttribute('class', 'disable');
		span.appendChild(icon);

		icon = document.createElement('img');
		icon.setAttribute('src', '@@htdocs@@/enable.png');
		icon.setAttribute('id', 'playerEnable' + results[i].id);
		icon.setAttribute('onclick', 'doEnablePlayer(' + results[i].id + '); return false;');
		icon.setAttribute('alt', 'Enable');
		icon.setAttribute('class', 'enable');
		span.appendChild(icon);
	}

	doClearReplace('checkPlayers', count >= 2 ? 'Yes!' : 'No.');
}

function loadGamesSuccess(resp) {
	var i, j, k, results, li, e;

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
		li.appendChild(document.createTextNode('No games.'));
		e.appendChild(li);
		doClearReplace('checkGames', 'No.');
		return;
	}

	doClearReplace('checkGames', 'Yes!');
	e.className = '';

	for (i = 0; i < results.length; i++) {
		li = document.createElement('li');
		li.appendChild(document.createTextNode(results[i].name));
		li.appendChild(document.createTextNode(': {'));
		for (j = 0; j < results[i].payoffs.length; j++) {
			if (j > 0)
				li.appendChild(document.createTextNode(', '));
			li.appendChild(document.createTextNode('{'));
			for (k = 0; k < results[i].payoffs[j].length; k++) {
				if (k > 0)
					li.appendChild(document.createTextNode(', '));
				li.appendChild(document.createTextNode(results[i].payoffs[j][k]));
			}
			li.appendChild(document.createTextNode('}'));
		}
		li.appendChild(document.createTextNode('}'));
		e.appendChild(li);
	}
}

function doAddPlayersError(err) {
	doError(err, 'addPlayersSubmit', 'addPlayersErr');
}

function doChangeMailError(err) {
	doError(err, 'changeMailSubmit', 'changeMailErr');
}

function doChangePassError(err) {
	doError(err, 'changePassSubmit', 'changePassErr');
}

function doChangeSmtpError(err) {
	doError(err, 'changeSmtpSubmit', 'changeSmtpErr');
}

function doAddGameError(err) {
	doError(err, 'addGameSubmit', 'addGameErr');
}

function doStartExprError(err) {
	doError(err, 'startExprSubmit', 'startExprErr');
	document.getElementById('startExprSubmit').value = 'Start';
}

function loadError(err, name) {
	var li, e;

	e = doClearNode(document.getElementById(name));
	if (null == e)
		return;
	li = document.createElement('li');
	li.appendChild(document.createTextNode('An error occured.'));
	e.appendChild(li);
}

function loadGamesError(err) {
	loadError(err, 'loadGames');
}

function loadPlayersError(err) {
	loadError(err, 'loadPlayers');
}

function doAddPlayersSetup() {
	doSetup('addPlayersSubmit', 'addPlayersErr');
}

function doChangeMailSetup() {
	doSetup('changeMailSubmit', 'changeMailErr');
}

function doChangePassSetup() {
	doSetup('changePassSubmit', 'changePassErr');
}

function doChangeSmtpSetup() {
	doSetup('changeSmtpSubmit', 'changeSmtpErr');
}

function doAddGameSetup() {
	doSetup('addGameSubmit', 'addGameErr');
}

function doStartExprSetup() {
	doSetup('startExprSubmit', 'startExprErr');
	document.getElementById('startExprSubmit').value = 'Starting...';
}

function doEnablePlayer(id) {
	var xmlhttp, e;

	if (null == (e = document.getElementById('player' + id)))
		return;
	e.className = 'enabling';

	if (null == (e = document.getElementById('playerEnable' + id)))
		return;
	e.src = '@@htdocs@@/ajax-loader.gif';
	e.className = 'enabling';
	e.alt = 'Enabling';

	xmlhttp = new XMLHttpRequest();
	xmlhttp.onreadystatechange=function() {
		if (xmlhttp.readyState==4 && xmlhttp.status==200)
			loadPlayers();
	} 
	xmlhttp.open('GET', '@@cgibin@@/doenableplayer.json?pid=' + id, true);
	xmlhttp.send(null);
}

function doDisablePlayer(id) {
	var xmlhttp, e;

	if (null == (e = document.getElementById('player' + id)))
		return;
	e.className = 'disabling';

	if (null == (e = document.getElementById('playerDisable' + id)))
		return;
	e.src = '@@htdocs@@/ajax-loader.gif';
	e.className = 'disabling';
	e.alt = 'Disabling';

	xmlhttp = new XMLHttpRequest();
	xmlhttp.onreadystatechange=function() {
		if (xmlhttp.readyState==4 && xmlhttp.status==200)
			loadPlayers();
	} 
	xmlhttp.open('GET', '@@cgibin@@/dodisableplayer.json?pid=' + id, true);
	xmlhttp.send(null);
}

function checkSmtp() {
	var li, e, gif, xmlhttp;

	doClearReplace('checkSmtp', 'Checking...');

	xmlhttp = new XMLHttpRequest();
	xmlhttp.onreadystatechange=function() {
		if (xmlhttp.readyState==4 && xmlhttp.status==200)
			doClearReplace('checkSmtp', 'Yes!');
		else if (xmlhttp.readyState==4 && xmlhttp.status==400)
			doClearReplace('checkSmtp', 'No.');
	} 
	xmlhttp.open('GET', '@@cgibin@@/dochecksmtp.json', true);
	xmlhttp.send(null);
}

function loadList(url, name, onsuccess, onerror) {
	var li, e, gif, xmlhttp;

	if (null == (e = doClearNode(document.getElementById(name))))
		return;

	e.className = 'default';
	li = document.createElement('li');

	gif = document.createElement('img');
	gif.setAttribute('src', '@@htdocs@@/ajax-loader.gif');
	gif.setAttribute('alt', 'Loading...');
	gif.setAttribute('class', 'loader');
	li.appendChild(gif);
	li.appendChild(document.createTextNode('Loading...'));
	e.appendChild(li);

	xmlhttp = new XMLHttpRequest();
	xmlhttp.onreadystatechange=function() {
		if (xmlhttp.readyState == 4 && xmlhttp.status==200) {
			onsuccess(xmlhttp.responseText);
		} else if (xmlhttp.readyState == 4) {
			onerror(xmlhttp.status);
		}
	} 
	xmlhttp.open('GET', url, true);
	xmlhttp.send(null);
}

function loadPlayers() {
	doClearReplace('checkPlayers', 'Checking...');
	loadList('@@cgibin@@/doloadplayers.json', 
		'loadPlayers', loadPlayersSuccess, loadPlayersError);
}

function loadGames() {
	doClearReplace('checkGames', 'Checking...');
	loadList('@@cgibin@@/doloadgames.json', 
		'loadGames', loadGamesSuccess, loadGamesError);
}

function loadExprSuccess(resp) {
	var results, v, e, chld;

	try  { 
		results = JSON.parse(resp);
	} catch (error) {
		return;
	}

	if ((v = parseInt(results.tilstart)) > 0) {
		if (null == (e = doClearNode(doUnhide('statusExprWaiting'))))
			return;
		doHide('statusExprLoading');
		chld = document.createElement('div');
		chld.appendChild(document.createTextNode('Time to start: '));
		formatCountdown(v, chld);
		e.appendChild(chld);
	} else {
		if (null == (e = doClearNode(doUnhide('statusExprProgress'))))
			return;
		doHide('statusExprLoading');
		chld = document.createElement('progress');
		chld.setAttribute('max', '1.0');
		chld.setAttribute('value', results.progress);
		chld.appendChild(document.createTextNode((results.progress * 100.0) + '%'));
		e.appendChild(chld);
	}
}

function loadExpr() {
	var xhr;

	doUnhide('statusExprLoading');
	doHide('statusExprWaiting');
	doHide('statusExprProgress');

	xhr = new XMLHttpRequest();
	xhr.onreadystatechange = function() {
		if (xhr.readyState == 4 && xhr.status == 200)
			loadExprSuccess(xhr.responseText);
	} 
	xhr.open('GET', '@@cgibin@@/dogetexpr.json', true);
	xhr.send(null);
}
