
function loadExprFinished()
{
	var e;

	if (null != (e = doClearNode(document.getElementById('expr'))))
		e.appendChild(document.createTextNode('Wait finished: reloading.'));

	window.location.reload(true);
}

function loadExprSuccess(resp)
{
	var results, e, v, head, game, i, j, table, row, cell, matrix, div;

	try  { 
		results = JSON.parse(resp);
	} catch (error) {
		loadExprFailure();
		return;
	}

	console.log(resp);

	if ((v = parseInt(results.tilstart)) > 0) {
		if (null == (e = doClearNode(document.getElementById('expr'))))
			return;
		head = 'Time Until Next Game';
		formatCountdown(head, v, e);
		setTimeout(timerCountdown, 1000, head, loadExprFinished, e, v, new Date().getTime());
	} else if (v < 0) {
		if (null == (e = doClearNode(document.getElementById('expr'))))
			return;
		e.appendChild(document.createTextNode('Experiment finished.'));
	} else {
		if (null == (e = doClearNode(document.getElementById('expr'))))
			return;
		div = document.createElement('div');
		div.appendChild(document.createTextNode('Round: ' + results.round));
		e.appendChild(div);

		game = results.games[Math.floor(Math.random() * results.games.length)];

		if (0 == results.role) {
			matrix = new Array(game.payoffs.length);
			for (i = 0; i < game.payoffs.length; i++) {
				matrix[i] = new Array(game.payoffs[0].length);
				for (j = 0; j < game.payoffs[0].length; j++)
					matrix[i][j] = game.payoffs[i][j][0];
			}
		} else {
			matrix = new Array(game.payoffs[0].length);
			for (i = 0; i < game.payoffs[0].length; i++) {
				matrix[i] = new Array(game.payoffs.length);
				for (j = 0; j < game.payoffs.length; j++)
					matrix[i][j] = game.payoffs[j][i][1];
			}
		}

		table = document.createElement('div');
		table.setAttribute('class', 'payoffs');
		table.setAttribute('style', 'width: ' + (matrix.length * 5) + 'em;');
		for (i = 0; i < matrix.length; i++) {
			row = document.createElement('div');
			table.appendChild(row);
			for (j = 0; j < matrix[i].length; j++) {
				cell = document.createElement('div');
				cell.setAttribute('style', 'width: ' + (100.0 * 1 / matrix[i].length) + '%;');
				console.log('cell width = ' + cell.getAttribute('style'));
				row.appendChild(cell);
				cell.appendChild(document.createTextNode(matrix[i][j]));
			}
		}
		e.appendChild(table);
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
