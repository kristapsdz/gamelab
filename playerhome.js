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
 * Append the roundup matrix by way of the payoff matrix, which has one
 * attribute per row that points to the roundup matrix row.
 * This lets us show the shuffled matrix's corresponding rows.
 */
function appendMatrix(e, matrix, rowavgs, colavgs)
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
			(100.0 / (matrix[0].hmatrix.length + 2)) + '%;');
		cell.appendChild(document.createTextNode
			((i + 1) + '.'));
		row.appendChild(cell);
		for (j = 0; j < matrix[i].hmatrix.length; j++) {
			cell = document.createElement('div');
			cell.setAttribute('class', 'payoff');
			cell.setAttribute('style', 'width: ' + 
				(100.0 / (matrix[i].hmatrix.length + 2)) + '%;');
			row.appendChild(cell);
			cell.appendChild
				(document.createTextNode
				 (matrix[i].hmatrix[j]));
		}

		cell = document.createElement('div');
		cell.setAttribute('class', 'payoffright');
		cell.setAttribute('style', 'width: ' + 
			(100.0 / (matrix[i].hmatrix.length + 2)) + '%;');
		cell.appendChild(document.createTextNode(rowavgs[i]));
		row.appendChild(cell);
	}

	row = document.createElement('div');
	table.appendChild(row);
	cell = document.createElement('div');
	cell.setAttribute('class', 'payoffbottom');
	cell.setAttribute('style', 'width: ' + 
		(100.0 / (matrix[0].hmatrix.length + 2)) + '%;');
	row.appendChild(cell);
	for (i = 0; i < matrix[0].hmatrix.length; i++) {
		cell = document.createElement('div');
		cell.setAttribute('class', 'payoffbottom');
		cell.setAttribute('style', 'width: ' + 
			(100.0 / (matrix[0].hmatrix.length + 2)) + '%;');
		row.appendChild(cell);
		cell.appendChild(document.createTextNode(colavgs[i]));
	}

	cell = document.createElement('div');
	cell.setAttribute('class', 'payoffbottom');
	cell.setAttribute('style', 'width: ' + 
		(100.0 / (matrix[0].hmatrix.length + 2)) + '%;');
	row.appendChild(cell);

	e.appendChild(table);
}

/*
 * Append an HTML matrix "matrix" to the element "e".
 * This formats everything properly.
 */
function appendBimatrix(e, matrix, colour, ocolour)
{
	var table, row, cell, i, j, poff;
	var colours = [
		"magenta",
		"#ff00cc",
		"red",
		"orange",
		"yellow",
		"#99ff00",
		"green",
		"#00ff99",
		"cyan",
		"azure",
		"blue",
		"violet"
	];

	cell = document.createElement('div');
	cell.setAttribute('class', 'payoffheader');
	cell.appendChild(document.createTextNode('Payoff Matrix'));
	e.appendChild(cell);

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
			poff.setAttribute('style', 'color: ' + colours[colour]);
			poff.appendChild
				(document.createTextNode
				 (matrix[i][j][0]));
			cell.appendChild(poff);
			poff = document.createElement('div');
			poff.setAttribute('style', 'color: ' + colours[ocolour]);
			poff.appendChild
				(document.createTextNode
				 (matrix[i][j][1]));
			cell.appendChild(poff);
		}
	}
	e.appendChild(table);
}

function matrixCreate(vector)
{
	var matrix, i, j;

	matrix = new Array(vector.length);
	for (i = 0; i < vector.length; i++) {
		matrix[i] = new Array(vector[0].length);
		matrix[i].index = i;
		for (j = 0; j < vector[0].length; j++)
			matrix[i][j] = vector[i][j];
	}

	return(matrix);
}

/*
 * Convert a game, which is in top-left to bottom-right order with row
 * player payoff first, into a matrix we'll use in appendMatrix() and
 * other places.
 * We add some extra bookkeeping to the matrix--beyond that, it's more
 * or less what we have in the game matrix.
 */
function bimatrixCreate(vector)
{
	var matrix, i, j;

	matrix = new Array(vector.length);
	for (i = 0; i < vector.length; i++) {
		matrix[i] = new Array(vector[0].length);
		matrix[i].index = i;
		for (j = 0; j < vector[0].length; j++) {
			matrix[i][j] = new Array(2);
			matrix[i][j][0] = vector[i][j][0];
			matrix[i][j][1] = vector[i][j][1];
		}
	}

	return(matrix);
}

function matrixCreateTranspose(vector)
{
	var matrix, i, j;

	matrix = new Array(vector[0].length);
	for (i = 0; i < vector[0].length; i++) {
		matrix[i] = new Array(vector.length);
		matrix[i].index = i;
		for (j = 0; j < vector.length; j++)
			matrix[i][j] = vector[j][i];
	}

	return(matrix);
}

/*
 * This is like bimatrixCreate(), but it transposes the matrix so that the
 * column player can appear to play as a row player.
 */
function bimatrixCreateTranspose(vector)
{
	var matrix, i, j;

	matrix = new Array(vector[0].length);
	for (i = 0; i < vector[0].length; i++) {
		matrix[i] = new Array(vector.length);
		matrix[i].index = i;
		for (j = 0; j < vector.length; j++) {
			matrix[i][j] = new Array(2);
			matrix[i][j][0] = vector[j][i][1];
			matrix[i][j][1] = vector[j][i][0];
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
	var game, matrix, hmatrix, e, div, ii, i, input;

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
	/*
	 * Transpose the matrix so that we always appear to be the row
	 * player (for consistency).
	 */
	matrix = 0 == res.role ? 
		bimatrixCreate(game.payoffs) : 
		bimatrixCreateTranspose(game.payoffs);

	/*
	 * If we have roundup information, first transpose it in the
	 * same way as above.
	 * Then assign each matrix row to a row in the roundup; this
	 * will allow us to shuffle both in tandem.
	 */
	if (null != game.roundup) {
		hmatrix = 0 == res.role ? 
			matrixCreate(game.roundup.avgs) : 
			matrixCreateTranspose(game.roundup.avgs);
		for (i = 0; i < matrix.length; i++)
			matrix[i].hmatrix = hmatrix[i];
	} else
		hmatrix = null;

	/* Shuffle the presentation of rows. */
	shuffle(matrix);
	appendBimatrix(doClear('exprMatrix'), 
		matrix, res.colour, res.ocolour);

	/* Show the shuffled roundup matrix, if it exists. */
	if (null != hmatrix) {
		e = doClear('exprHistory');
		div = document.createElement('div');
		div.setAttribute('class', 'lottery');
		div.appendChild
			(document.createTextNode
			 ('Lottery tickets: ' + res.payoff));
		e.appendChild(div);
		appendMatrix(e, matrix,
			0 == res.role ? game.roundup.avgp1 : 
				game.roundup.avgp2,
			0 == res.role ? game.roundup.avgp2 : 
				game.roundup.avgp1);
	} else
		doClear('exprHistory');

	/*
	 * Assign an input field per strategy.
	 * These will correspond to the input fields that are in our
	 * matrix by textual representation.
	 * Include submission button and everything.
	 */
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
		input.setAttribute('placeholder', 
			'Strategy ' + (i + 1));
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

	console.log('Expr response: ' + resp);

	try  { 
		res = JSON.parse(resp);
	} catch (error) {
		doUnhide('exprLoading');
		doHide('exprDone');
		doHide('exprCountdown');
		doHide('exprError');
		doHide('exprPlay');
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
