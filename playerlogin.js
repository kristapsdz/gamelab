"use strict";

function doError(err) 
{
	document.getElementById('loggingIn').value = 'Login';
	doUnhide(400 == err || 409 == err ? 'loginError' : 'ajaxError');
}

function doSuccess() 
{
	document.getElementById('loggingIn').value = 'Success!  Redirecting...';
	location.href = '@HTURI@/playerhome.html';
}

function doSetup() 
{
	doHide('loginError');
	doHide('ajaxError');
	document.getElementById('loggingIn').value = 'Logging in...';
}
