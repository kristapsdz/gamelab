"use strict";

function timerCountdown(head, donefunc, e, value, start)
{
	var elapsed;

	elapsed = new Date().getTime() - start;
	value -= elapsed / 1000;
	if (value < 0) {
		donefunc();
		return;
	}

	doClearNode(e);
	formatCountdown(head, value, e);
	setTimeout(timerCountdown, 1000, head, donefunc, e, value, new Date().getTime());
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

function doClearReplace(name, str) 
{
	var e;

	if (null != (e = doClearNode(document.getElementById(name))))
		e.appendChild(document.createTextNode(str));
}

function formatCountdown(head, v, e)
{
	var p, div, table, row, cell;

	if (null != head) {
		div = document.createElement('div');
		div.setAttribute('class', 'countdownHead');
		div.appendChild(document.createTextNode(head));
		e.appendChild(div);
	}

	table = document.createElement('div');
	table.setAttribute('class', 'countdown');
	e.appendChild(table);

	row = document.createElement('div');
	table.appendChild(row);

	cell = document.createElement('div');
	if (v > 24 * 60 * 60) {
		p = Math.floor(v / (24 * 60 * 60));
		cell.appendChild(document.createTextNode(p));
		v -= p * (24 * 60 * 60);
	} else
		cell.appendChild(document.createTextNode("00"));

	row.appendChild(cell);
	cell = document.createElement('div');
	if (v > 60 * 60) {
		p = Math.floor(v / (60 * 60));
		if (p < 10)
			cell.appendChild(document.createTextNode("0"));
		cell.appendChild(document.createTextNode(p));
		v -= p * (60 * 60);
	} else
		cell.appendChild(document.createTextNode("00"));

	row.appendChild(cell);
	cell = document.createElement('div');
	if (v > 60) {
		p = Math.floor(v / 60);
		if (p < 10)
			cell.appendChild(document.createTextNode("0"));
		cell.appendChild(document.createTextNode(p));
		v -= p * (60);
	} else
		cell.appendChild(document.createTextNode("00"));

	row.appendChild(cell);
	cell = document.createElement('div');
	p = Math.round(v);
	if (p < 10)
		cell.appendChild(document.createTextNode("0"));
	cell.appendChild(document.createTextNode(p));
	row.appendChild(cell);

	row = document.createElement('div');
	table.appendChild(row);
	cell = document.createElement('div');
	cell.appendChild(document.createTextNode('days'));
	row.appendChild(cell);
	cell = document.createElement('div');
	cell.appendChild(document.createTextNode('hours'));
	row.appendChild(cell);
	cell = document.createElement('div');
	cell.appendChild(document.createTextNode('minutes'));
	row.appendChild(cell);
	cell = document.createElement('div');
	cell.appendChild(document.createTextNode('seconds'));
	row.appendChild(cell);
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
