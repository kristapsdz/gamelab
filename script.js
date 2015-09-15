"use strict";

function onNavUp()
{
	var nav = document.getElementById('nav');
	var btn = document.getElementById('navButton');

	if ( ! ('' == nav.style.maxHeight || nav.style.maxHeight == '0px')) {
		nav.style.maxHeight = '0px';
		btn.style.textShadow = 'none';
	}
}

function onNav(event) 
{
	var nav = document.getElementById('nav');
	var btn = document.getElementById('navButton');

	if ('' == nav.style.maxHeight || nav.style.maxHeight == '0px') {
		nav.style.maxHeight = '100em';
		btn.style.textShadow = '1px 1px #fff';
	} else {
		nav.style.maxHeight = '0px';
		btn.style.textShadow = 'none';
	}

	if (event.stopPropagation)
		event.stopPropagation();
	else
		event.cancelBubble = true;

	return false;
}

function timerCountdown(donefunc, e, value)
{
	var now;

	now = Math.floor(new Date().getTime() / 1000);
	if (now >= value) {
		if (null != donefunc)
			donefunc();
		return;
	}
	doClearNode(e);
	formatCountdown(value - now, e);
	setTimeout(timerCountdown, 1000, donefunc, e, value);
}

function doHideNode(e)
{

	if (null != e && ! e.classList.contains('noshow'))
		e.classList.add('noshow');

	return(e);
}


function doHide(name)
{

	return(doHideNode(document.getElementById(name)));
}

function doUnhideNode(e)
{

	if (null != e)
		e.classList.remove('noshow');

	return(e);
}

function doUnhide(name) 
{

	return(doUnhideNode(document.getElementById(name)));
}

function doClearNode(e) 
{
	if (null == e)
		return(null);

	while (e.firstChild)
		e.removeChild(e.firstChild);

	return(e);
}

function doValue(name, value)
{
	var e;

	if (null != (e = document.getElementById(name)))
		e.value = value;
}

function doClear(name) 
{

	return(doClearNode(document.getElementById(name)));
}

function doClearReplaceMarkup(name, str) 
{
	var e;

	if (null != (e = doClearNode(document.getElementById(name))))
		e.innerHTML = str;

	return(e);
}

function doClearReplace(name, str) 
{
	var e;

	if (null != (e = doClearNode(document.getElementById(name))))
		e.appendChild(document.createTextNode(str));

	return(e);
}

/*
 * Give a number of seconds "v", format the time as a human-readable
 * string (e.g., 1d4h3m).
 * Make sure that v is non-negative.
 */
function formatCountdown(v, e)
{
	var p, span, showseconds;

	/* Clamp time. */
	if (v < 0)
		v = 0;
	showseconds = v < 60;

	span = document.createElement('span');
	if (v >= 24 * 60 * 60) {
		p = Math.floor(v / (24 * 60 * 60));
		span.appendChild(document.createTextNode(p));
		span.appendChild(document.createTextNode('d'));
		v -= p * (24 * 60 * 60);
	} 

	e.appendChild(span);
	span = document.createElement('span');
	if (v >= 60 * 60) {
		p = Math.floor(v / (60 * 60));
		/*if (p < 10)
			span.appendChild(document.createTextNode('0'));*/
		span.appendChild(document.createTextNode(p));
		span.appendChild(document.createTextNode('h'));
		v -= p * (60 * 60);
	} 

	e.appendChild(span);
	span = document.createElement('span');
	if (v >= 60) {
		p = Math.floor(v / 60);
		/*if (p < 10)
			span.appendChild(document.createTextNode('0'));*/
		span.appendChild(document.createTextNode(p));
		span.appendChild(document.createTextNode('m'));
		v -= p * (60);
	} 

	e.appendChild(span);

	if (showseconds) {
		span = document.createElement('span');
		span.setAttribute('class', 'seconds');
		p = Math.floor(v);
		/*if (p < 10)
			span.appendChild(document.createTextNode('0'));*/
		span.appendChild(document.createTextNode(p));
		span.appendChild(document.createTextNode('s'));
		e.appendChild(span);
	}
}

function sendQuery(url, setup, success, error) 
{
	var xmlhttp = new XMLHttpRequest();

	if (null != setup)
		setup();

	xmlhttp.onreadystatechange=function() {
		if (xmlhttp.readyState == 4 && xmlhttp.status == 200 && null != success)
			success(xmlhttp.responseText);
		else if (xmlhttp.readyState == 4 && null != error)
			error(xmlhttp.status);
	} 

	xmlhttp.open('GET', url, true);
	xmlhttp.send(null);
}

function sendFormTo(oFormElement, action, setup, error, success) 
{
	var xmlhttp = new XMLHttpRequest();
	var formdata = new FormData(oFormElement);

	if (null != setup)
		setup();

	xmlhttp.onreadystatechange=function() {
		if (xmlhttp.readyState==4 && xmlhttp.status==200 && null != success)
			success(xmlhttp.responseText);
		else if (xmlhttp.readyState == 4 && null != error)
			error(xmlhttp.status);
	} 

	xmlhttp.open(oFormElement.method, action, true);
	xmlhttp.send(formdata);
	return(false);
}

function sendForm(oFormElement, setup, error, success) 
{

	return(sendFormTo(oFormElement, 
		oFormElement.action, setup, error, success));
}
