"use strict";

function shuffle(o)
{

	for (var j, x, i = o.length; i; j = Math.floor(Math.random() * i), x = o[--i], o[i] = o[j], o[j] = x);
        return o;
};

function appendMatrix(e, matrix)
{
	var table, row, cell, i, j, poff;

	table = document.createElement('div');
	table.setAttribute('class', 'payoffs');
	table.setAttribute('style', 
		'width: ' + (matrix.length * 10) + 'em;');

	for (i = 0; i < matrix.length; i++) {
		row = document.createElement('div');
		table.appendChild(row);
		cell = document.createElement('div');
		cell.setAttribute('class', 'payoffhead');
		cell.setAttribute('style', 'width: ' + 
			(100.0 / (matrix[0].length + 1)) + '%;');
		cell.appendChild(document.createTextNode
			((i + 1) + '.'));
		row.appendChild(cell);
		for (j = 0; j < matrix[i].length; j++) {
			cell = document.createElement('div');
			cell.setAttribute('style', 'width: ' + 
				(100.0 / (matrix[i].length + 1)) + '%;');
			row.appendChild(cell);
			poff = document.createElement('div');
			poff.appendChild
				(document.createTextNode
				 (matrix[i][j][0]));
			cell.appendChild(poff);
			poff = document.createElement('div');
			poff.appendChild
				(document.createTextNode
				 (matrix[i][j][1]));
			cell.appendChild(poff);
		}
	}
	e.appendChild(table);
}

function matrixCreate(game)
{
	var matrix, i, j;

	matrix = new Array(game.payoffs.length);
	for (i = 0; i < game.payoffs.length; i++) {
		matrix[i] = new Array(game.payoffs[0].length);
		matrix[i].index = i;
		for (j = 0; j < game.payoffs[0].length; j++) {
			matrix[i][j] = new Array(2);
			matrix[i][j][0] = game.payoffs[i][j][0];
			matrix[i][j][1] = game.payoffs[i][j][1];
		}
	}

	return(matrix);
}

function matrixCreateTranspose(game)
{
	var matrix, i, j;

	matrix = new Array(game.payoffs[0].length);
	for (i = 0; i < game.payoffs[0].length; i++) {
		matrix[i] = new Array(game.payoffs.length);
		matrix[i].index = i;
		for (j = 0; j < game.payoffs.length; j++) {
			matrix[i][j] = new Array(2);
			matrix[i][j][0] = game.payoffs[j][i][1];
			matrix[i][j][1] = game.payoffs[j][i][0];
		}
	}

	return(matrix);
}

function loadExprSuccess(resp)
{
	var results, e, v, head, game, i, j, matrix, div, input, expr, ii;

	try  { 
		results = JSON.parse(resp);
	} catch (error) {
		loadExprFailure();
		return;
	}

	console.log(resp);
	doHide('exprLoading');
	expr = results.expr;

	if ((v = parseInt(expr.tilstart)) > 0) {
		/*
		 * If we haven't yet started, then simply set our timer
		 * and exit: we have nothing to show.
		 */
		e = doClearNode(doUnhide('exprCountdown'));
		head = 'Time Until Experiment';
		formatCountdown(head, v, e);
		setTimeout(timerCountdown, 1000, 
			head, loadExpr, e, v, 
			new Date().getTime());
	} else if (0 == v) {
		/*
		 * Start by setting the countdown til the next
		 * game-play.
		 */
		e = doClearNode(doUnhide('exprCountdown'));
		v = parseInt(expr.tilnext);
		head = 'Time Left in Round ' + 
			(parseInt(expr.round) + 1) + 
			'/' + expr.rounds;
		formatCountdown(head, v, e);
		setTimeout(timerCountdown, 1000, head, 
			loadExpr, e, v, 
			new Date().getTime());

		/*
		 * If we have no games (i.e., we've already played
		 * everything there is to play), then simply 
		 * Else we format our matrix and payoff list.
		 */
		if (0 == results.games.length) {
			doUnhide('exprDone');
			return;
		}

		doUnhide('exprPlay');
		game = results.games[Math.floor
			(Math.random() * results.games.length)];
		if (0 == results.role)
			matrix = matrixCreate(game);
		else
			matrix = matrixCreateTranspose(game);

		shuffle(matrix);
		appendMatrix(doClear('exprMatrix'), matrix);

		document.getElementById('exprPlayGid').value = game.id;
		document.getElementById('exprPlayRound').value = expr.round;

		e = doClear('exprPlayList');
		for (i = 0; i < matrix.length; i++) {
			div = document.createElement('div');
			div.setAttribute('class', 'input');
			ii = document.createElement('i');
			ii.setAttribute('class', 'fa fa-fw fa-square');
			input = document.createElement('input');
			input.setAttribute('type', 'text');
			input.setAttribute('required', 'required');
			input.setAttribute('placeholder', 'Strategy ' + (i + 1));
			input.setAttribute('id', 'index' + matrix[i].index);
			input.setAttribute('name', 'index' + matrix[i].index);
			div.appendChild(ii);
			div.appendChild(input);
			e.appendChild(div);
		}
		div = document.createElement('div');
		div.setAttribute('class', 'input input-submit');
		ii = document.createElement('i');
		ii.setAttribute('class', 'fa fa-fw fa-check');
		input = document.createElement('input');
		input.setAttribute('type', 'submit');
		input.setAttribute('id', 'playGameSubmit');
		input.setAttribute('value', 'Submit Play');
		div.appendChild(ii);
		div.appendChild(input);
		e.appendChild(div);
	} else {
		/*
		 * Case where the game has finished.
		 */
		doUnhide('exprPlay');
		doClearReplace('exprPlay', 'Experiment finished.');
	}
}

function loadExprSetup()
{
	var e;

	doHide('exprPlay');
	doHide('exprCountdown');
	doHide('exprDone');
	doUnhide('exprLoading');
}

function loadExpr() 
{

	sendQuery('@@cgibin@@/doloadexpr.json', 
		loadExprSetup, loadExprSuccess, null);
}

function doPlayGameSetup()
{

	doHide('playGameErrorJson');
	doHide('playGameErrorForm');
	document.getElementById('playGameSubmit').value = 'Submitting...';
}

function doPlayGameError(err)
{

	document.getElementById('playGameSubmit').value = 'Submit';
	switch (err) {
	case 400:
		doUnhide('playGameErrorForm');
		break;
	case 409:
		doHide('exprPlay');
		doUnhide('exprDone');
		break;
	default:
		doUnhide('playGameErrorJson');
		break;
	}
}

function doPlayGameSuccess(resp)
{
	var i, e;

	e = document.getElementById('playGameSubmit');
	e.setAttribute('value', 'Submitted!');

	for (i = 0; ; i++) {
		e = document.getElementById('index' + i);
		if (null == e)
			break;
		e.setAttribute('readonly', 'readonly');
	}
}

function playGame(form)
{

	return(sendForm(form, doPlayGameSetup,
		doPlayGameError, doPlayGameSuccess));
}
