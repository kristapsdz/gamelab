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
	case (409):
		doUnhide('registerStartedError');
		break;
	default:
		doUnhide('registerSysError');
		break;
	}
}

function doRegOk(resp) 
{

	location.href = '@HTURI@/playerhome.html';
}

function doRegSetup() 
{

	doHide('registerError');
	doHide('registerSysError');
	doValue('registering', 'Registering...');
}
