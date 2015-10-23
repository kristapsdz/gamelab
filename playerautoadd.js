"use strict";

function doLogErr(err) 
{

	if (409 == err) 
		setTimeout(doRegTimeout, 10000);
	else
		doUnhide('loginSysError');
}

function doLogOk(resp) 
{
	var res;

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

function doLogSetup() 
{

	doHide('loginSysError');
}

function doRegTimeout()
{

	sendForm(document.getElementById('loginSection'),
		doLogSetup, doLogErr, doLogOk);
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

	doValue('ident', res.ident);
	doValue('password', res.password);
	doRegTimeout();
}

function doRegSetup() 
{

	doHide('registerError');
	doHide('registerSysError');
	doValue('registering', 'Registering...');
}

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
