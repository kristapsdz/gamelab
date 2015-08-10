"use strict";

function doRegErr(err) 
{

	doValue('registering', 'Register');
	switch (err) {
	case (400):
		doUnhide('registerError');
		break;
	case (403):
		doUnhide('registerAgainError');
		break;
	case (404):
		doUnhide('registerNoAutoError');
		break;
	case (409):
		doUnhide('registerStartedError');
		break;
	default:
		doUnhide('registerSysError');
		break;
	}
}

function doLogErr(err) 
{

	doValue('loggingin', 'Login');
	doUnhide(409 == err ? 'loginError' : 'loginSysError');
}

/*
 * Handle success from the dologin.json script.
 * This will return a small JSON body that indicates whether we should
 * redirect to the main page for play or into the lobby.
 * In the case of errors (uh-oh), just try the main page.
 */
function doLogOk(resp) 
{
	var res;

	doValue('loggingin', 'Success! Redirecting...');

	try {
		res = JSON.parse(resp);
	} catch (error) {
		location.href = '@HTURI@/playerhome.html';
		return;
	}
	location.href = res.needjoin ?
		'@HTURI@/playerlobby.html' :
		'@HTURI@/playerhome.html';
}

function doRegOk(resp) 
{
	var res;

	doHide('registerSection');
	doUnhide('loginSection');

	try {
		res = JSON.parse(resp);
	} catch (error) {
		doUnhide('loginSysError');
		return;
	}

	doValue('email', res.email);
	doValue('password', res.password);
}

function doLogSetup() 
{

	doHide('loginError');
	doHide('loginSysError');
	doValue('registering', 'Logging in...');
}

function doRegSetup() 
{

	doHide('registerError');
	doHide('registerSysError');
	doValue('registering', 'Registering...');
}
