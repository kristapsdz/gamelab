"use strict";

/*
 * The results.
 * This is only valid after we've run loadExprSuccess(), otherwise it's
 * null.
 * It's loaded with the games that we haven't played yet.
 */
var res;

/*
 * When we play games, we go through the [shuffled] array in "res".
 * This is the current position.
 * We'll have no more when resindex == res.gamesz.
 */
var resindex;

/*
 * Use a simple in-place fisher-yates shuffle on an array.
 * (The array can be anything--we use it both for arrays and matrices,
 * which are just arrays of arrays of course.)
 */
function shuffle(o)
{

	for (var j, x, i = o.length; i; 
		j = Math.floor(Math.random() * i), 
		x = o[--i], o[i] = o[j], o[j] = x);
        return o;
};

/*
 * Append an HTML matrix "matrix" to the element "e".
 * This formats everything properly.
 */
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

/*
 * Convert a game, which is in top-left to bottom-right order with row
 * player payoff first, into a matrix we'll use in appendMatrix() and
 * other places.
 * We add some extra bookkeeping to the matrix--beyond that, it's more
 * or less what we have in the game matrix.
 */
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

/*
 * This is like matrixCreate(), but it transposes the matrix so that the
 * column player can appear to play as a row player.
 */
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

/*
 * Given that we've already loaded the experiment via loadGameSuccess(),
 * take the existing game and lay out our inputs.
 */
function loadGame()
{
	var game, matrix, e, div, ii, i, input;

	if (resindex == res.games.length) {
		doUnhide('exprDone');
		doHide('exprPlay');
		doHide('exprError');
		return;
	} 

	doUnhide('exprPlay');

	doClearReplace('playGameNum', 
		((res.gamesz - res.games.length) + 
		 resindex + 1));
	doClearReplace('playGameMax', res.gamesz);

	game = res.games[resindex];
	matrix = 0 == res.role ? 
		matrixCreate(game) : matrixCreateTranspose(game);
	shuffle(matrix);
	appendMatrix(doClear('exprMatrix'), matrix);

	doValue('exprPlayGid', game.id);

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
}

function loadExprSuccess(resp)
{
	var e, v, head, i, j, expr;

	resindex = 0;

	try  { 
		res = JSON.parse(resp);
	} catch (error) {
		loadExprFailure();
		return;
	}

	doHide('exprLoading');
	expr = res.expr;

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

		doValue('exprPlayRound', expr.round);
		shuffle(res.games);
		loadGame();
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
	doHide('exprError');
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
	doValue('playGameSubmit', 'Submitting...');
}

function doPlayGameError(err)
{

	doValue('playGameSubmit', 'Submit');
	switch (err) {
	case 400:
		doUnhide('playGameErrorForm');
		break;
	case 409:
		doHide('exprPlay');
		doHide('exprDone');
		doUnhide('exprError');
		break;
	default:
		doUnhide('playGameErrorJson');
		break;
	}
}

function doPlayGameSuccess(resp)
{
	var i, e, div, ii, input;

	e = document.getElementById('playGameSubmit');
	e.setAttribute('value', 'Submitted!');
	e.setAttribute('disabled', 'disabled');

	for (i = 0; ; i++) {
		e = document.getElementById('index' + i);
		if (null == e)
			break;
		e.setAttribute('readonly', 'readonly');
	}

	resindex++;

	e = document.getElementById('exprPlayList');

	div = document.createElement('div');
	div.setAttribute('class', 'input input-submit');
	ii = document.createElement('i');
	ii.setAttribute('class', 'fa fa-fw fa-refresh');
	input = document.createElement('button');
	input.setAttribute('type', 'button');
	input.setAttribute('onclick', 'loadGame();');
	input.setAttribute('id', 'playGameRefresh');

	input.appendChild(document.createTextNode
		(resindex < res.games.length ?
		 'Load Next Game' : 'Wait for Next Game'));

	div.appendChild(ii);
	div.appendChild(input);
	e.appendChild(div);
}

function playGame(form)
{

	return(sendForm(form, doPlayGameSetup,
		doPlayGameError, doPlayGameSuccess));
}
