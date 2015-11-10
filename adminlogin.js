"use strict";

function doError(err) 
{

	if ('' != window.location.hash)
		window.location.hash = '';
	document.getElementById('loggingIn').value = 'Login';
	doUnhide(400 == err ? 'loginError' : 'ajaxError');
}

function doSuccess() 
{

	if ('' != window.location.hash)
		window.location.hash = '';
	document.getElementById('loggingIn').value = 'Success!  Redirecting...';
	location.href = '@ADMINURI@/home.html';
}

function doSetup() 
{

	doHide('loginError');
	doHide('ajaxError');
	document.getElementById('loggingIn').value = 'Logging in...';
}
