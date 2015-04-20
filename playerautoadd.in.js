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
	doUnhide(400 == err ? 'loginError' : 'loginSysError');
}

function doLogOk() 
{

	doValue('loggingin', 'Success! Redirecting...');
	location.href = '@HTURI@/playerhome.html';
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
