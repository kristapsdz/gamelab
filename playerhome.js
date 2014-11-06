
function loadExprFinished()
{
	var e;

	if (null != (e = doClearNode(document.getElementById('expr'))))
		e.appendChild(document.createTextNode('Wait finished: reloading.'));

	window.location.reload(true);
}

function loadExprSuccess(resp)
{
	var results, e, span, v, head;

	try  { 
		results = JSON.parse(resp);
	} catch (error) {
		loadExprFailure();
		return;
	}

	if ((v = parseInt(results.tilstart)) > 0) {
		if (null == (e = doClearNode(document.getElementById('expr'))))
			return;
		head = 'Time Until Next Game';
		formatCountdown(head, v, e);
		setTimeout(timerCountdown, 1000, head, loadExprFinished, e, v, new Date().getTime());
	} else {
		if (null != (e = doClearNode(document.getElementById('expr'))))
			e.appendChild(document.createTextNode('Ok!'));
	}
}

function loadExprFailure()
{

	if (null != (e = doClearNode(document.getElementById('expr'))))
		e.appendChild(document.createTextNode('A technical error occured.'));
}

function loadExpr() 
{
	var e, gif, xmlhttp;

	if (null == (e = doClearNode(document.getElementById('expr'))))
		return;

	gif = document.createElement('img');
	gif.setAttribute('src', '@@htdocs@@/ajax-loader.gif');
	gif.setAttribute('alt', 'Loading...');
	gif.setAttribute('class', 'loader');
	e.appendChild(gif);
	e.appendChild(document.createTextNode('Loading experiment...'));

	xmlhttp = new XMLHttpRequest();
	xmlhttp.onreadystatechange=function() {
		if (xmlhttp.readyState==4 && xmlhttp.status==200) {
			loadExprSuccess(xmlhttp.responseText);
		} else if (xmlhttp.readyState == 4) {
			loadExprFailure();
		}
	} 
	xmlhttp.open('GET', '@@cgibin@@/doloadexpr.json', true);
	xmlhttp.send(null);
}
