"use strict";

function doHide(name)
{
	var e;
	if (null != (e = document.getElementById(name)))
		e.style.display = 'none';
	return(e);
}

function doUnhide(name) 
{
	var e;
	if (null != (e = document.getElementById(name)))
		e.style.display = 'inherit';
	return(e);
}

function doClearNode(e) 
{
	if (null == e)
		return(null);

	while (e.firstChild)
		e.removeChild(e.firstChild);

	return(e);
}

function doClearReplace(name, str) 
{
	var e;

	if (null != (e = doClearNode(document.getElementById(name))))
		e.appendChild(document.createTextNode(str));
}

function formatCountdown(v, e)
{
	var p;

	if (v > 24 * 60 * 60) {
		p = Math.floor(v / (24 * 60 * 60));
		e.appendChild(document.createTextNode(p + ' days, '));
		v -= p * (24 * 60 * 60);
	} 
	if (v > 60 * 60) {
		p = Math.floor(v / (60 * 60));
		e.appendChild(document.createTextNode(p + ' hours, '));
		v -= p * (60 * 60);
	}
	if (v > 60) {
		p = Math.floor(v / 60);
		e.appendChild(document.createTextNode(p + ' minutes, '));
		v -= p * (60);
	}

	e.appendChild(document.createTextNode(Math.round(v) + ' seconds.'));
}

function sendForm(oFormElement, setup, error, success) 
{
	var xmlhttp = new XMLHttpRequest();
	var formdata = new FormData(oFormElement);

	setup();

	xmlhttp.onreadystatechange=function() {
		if (xmlhttp.readyState==4 && xmlhttp.status==200)
			success(xmlhttp.responseText);
		else if (xmlhttp.readyState == 4)
			error(xmlhttp.status);
	} 

	xmlhttp.open(oFormElement.method, oFormElement.action, true);
	xmlhttp.send(formdata);
	return(false);
}
