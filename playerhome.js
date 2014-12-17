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

var colours = [
	"#CC0000",
	"#009900",
	"#0066CC",
	"#FF8000",
];

function random(object) {
	var x = Math.sin(object.seed++) * 10000;
	return x - Math.floor(x);
}

/*
 * Use a simple in-place fisher-yates shuffle on an array.
 * (The array can be anything--we use it both for arrays and matrices,
 * which are just arrays of arrays of course.)
 */
function shuffle(o, seed)
{
	var object = new Object();

	object.seed = seed;

	for (var j, x, i = o.length; i; 
		j = Math.floor(random(object) * i), 
		x = o[--i], o[i] = o[j], o[j] = x);
        return o;
};

/*
 * Append the roundup matrix by way of the payoff matrix, which has one
 * attribute per row that points to the roundup matrix row.
 * This lets us show the shuffled matrix's corresponding rows.
 */
function appendMatrix(e, matrix)
{
	var table, row, cell, i, j, poff, rowinner;

	table = document.createElement('div');
	e.appendChild(table);

	table.setAttribute('class', 'history');
	table.setAttribute('style', 
		'width: ' + (matrix[0].length * 10) + 'em;');

	row = document.createElement('div');
	table.appendChild(row);

	cell = document.createElement('div');
	cell.setAttribute('class', 'labelaside');
	cell.setAttribute('width', '1%;');
	row.appendChild(cell);

	for (i = 0; i < matrix[0].length; i++) {
		cell = document.createElement('div');
		cell.setAttribute('class', 'labelatop');
		cell.setAttribute('style', 'width: ' + 
			((100.0 / (matrix[0].length + 1)) - 1) + '%;');
		cell.appendChild(document.createTextNode
			(String.fromCharCode(97 + i) + '.'));
		row.appendChild(cell);
	}

	cell = document.createElement('div');
	cell.setAttribute('class', 'sumaside');
	cell.setAttribute('style', 'width: ' + 
		((100.0 / (matrix[0].length + 1)) - 1) + '%;');
	cell.appendChild(document.createTextNode('\u2211'));
	row.appendChild(cell);

	for (i = 0; i < matrix.length; i++) {
		row = document.createElement('div');
		table.appendChild(row);

		cell = document.createElement('div');
		cell.setAttribute('class', 'labelaside');
		row.appendChild(cell);

		for (j = 0; j < matrix[i].length; j++) {
			cell = document.createElement('div');
			cell.setAttribute('class', 'mix');
			row.appendChild(cell);
			cell.appendChild
				(document.createTextNode
				 (matrix[i].hmatrix[j]));
		}

		cell = document.createElement('div');
		cell.setAttribute('class', 'sumaside sum');
		cell.appendChild(document.createTextNode(matrix[i].ravg));
		row.appendChild(cell);
	}

	row = document.createElement('div');
	table.appendChild(row);

	cell = document.createElement('div');
	cell.setAttribute('class', 'labelaside');
	cell.appendChild(document.createTextNode('\u2211'));
	row.appendChild(cell);

	for (i = 0; i < matrix[0].hmatrix.length; i++) {
		cell = document.createElement('div');
		cell.setAttribute('class', 'sumbelow sum');
		row.appendChild(cell);
		cell.appendChild(document.createTextNode(matrix[0][i].cavg));
	}

	cell = document.createElement('div');
	cell.setAttribute('class', 'sumaside');
	row.appendChild(cell);
}

function prowOut(source, id)
{
	var e;

	if (null != (e = document.getElementById('index' + id))) {
		source.classList.remove('hover');
		e.parentNode.classList.remove('ihover');
	}
}

function prowOver(source, id)
{
	var e;

	if (null != (e = document.getElementById('index' + id))) {
		source.classList.add('hover');
		e.parentNode.classList.add('ihover');
	}
}

function prowClick(source, id)
{
	var e;

	if (null == (e = document.getElementById('index' + id)))
		return;

	if (e.hasAttribute('readonly')) {
		source.classList.add('active');
		e.value = '0';
		e.removeAttribute('readonly');
	} else {
		source.classList.remove('active');
		e.value = '';
		e.setAttribute('readonly', 'readonly');
	}

}

/*
 * Append an HTML matrix "matrix" to the element "e".
 * This formats everything properly.
 */
function appendBimatrix(e, matrix, colour, ocolour)
{
	var table, row, cell, i, j, poff, inputs;

	inputs = document.getElementById('playGame');
	inputs.setAttribute('style', 
		'width: ' + (matrix[0].length * 10) + 'em;');

	table = document.createElement('div');
	table.setAttribute('class', 'payoffs');
	table.setAttribute('style', 
		'width: ' + (matrix[0].length * 10) + 'em;');

	row = document.createElement('div');
	table.appendChild(row);

	cell = document.createElement('div');
	cell.setAttribute('class', 'labelaside');
	row.appendChild(cell);

	for (i = 0; i < matrix[0].length; i++) {
		cell = document.createElement('div');
		cell.setAttribute('class', 'labelatop');
		cell.setAttribute('style', 'width: ' + 
			(100.0 / (matrix[0].length)) + '%;');
		cell.appendChild(document.createTextNode
			(String.fromCharCode(97 + i) + '.'));
		row.appendChild(cell);
	}


	for (i = 0; i < matrix.length; i++) {
		row = document.createElement('div');
		row.setAttribute('onclick', 
			'prowClick(this, ' + matrix[i].index + ')');
		row.setAttribute('onmouseover', 
			'prowOver(this, ' + matrix[i].index + ')');
		row.setAttribute('onmouseout', 
			'prowOut(this, ' + matrix[i].index + ')');
		table.appendChild(row);
		cell = document.createElement('div');
		cell.setAttribute('class', 'labelaside');
		cell.appendChild(document.createTextNode
			((i + 1) + '.'));
		row.appendChild(cell);
		for (j = 0; j < matrix[i].length; j++) {
			cell = document.createElement('div');
			cell.setAttribute('class', 'pair');
			row.appendChild(cell);
			poff = document.createElement('div');
			poff.setAttribute('style', 
				'color: ' + colours[colour]);
			poff.appendChild
				(document.createTextNode
				 (matrix[i][j][0]));
			cell.appendChild(poff);
			poff = document.createElement('div');
			poff.setAttribute('style', 
				'color: ' + colours[ocolour]);
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
	var game, matrix, hmatrix, e, div, ii, i, j, input, c, oc, ravg, cavg;

	if (resindex == res.games.length) {
		doUnhide('exprDone');
		doHide('exprPlay');
		doHide('exprNotStarted');
		return;
	} 

	doHide('exprNotStarted');
	doHide('exprDone');
	doUnhide('exprPlay');

	doClearReplace('playGameNum', 
		((res.gamesz - res.games.length) + 
		 resindex + 1));
	doClearReplace('playGameMax', res.gamesz);
	doClearReplace('playRoundNum', res.expr.round + 1);
	doClearReplace('playRoundMax', res.expr.rounds);

	game = res.games[resindex];

	/* Transpose the matrix, if necessary. */
	matrix = 0 == res.role ? 
		bimatrixCreate(game.payoffs) : 
		bimatrixCreateTranspose(game.payoffs);
	hmatrix = null;

	if (null != game.roundup) {
		/* Transpose vectors and matrix. */
		ravg = 0 == res.role ? 
			game.roundup.avgp1 : game.roundup.avgp2;
		cavg = 0 == res.role ? 
			game.roundup.avgp2 : game.roundup.avgp1;
		hmatrix = 0 == res.role ? 
			matrixCreate(game.roundup.avgs) : 
			matrixCreateTranspose(game.roundup.avgs);
		/* 
		 * Attach values to the matrix so that they persist
		 * after we shuffle the matrix rows.
		 */
		for (i = 0; i < matrix.length; i++) {
			matrix[i].hmatrix = hmatrix[i];
			matrix[i].ravg = ravg[i];
			for (j = 0; j < matrix[i].length; j++)
				matrix[i][j].cavg = cavg[j];
		}
	} 

	/* Assign colours. */
	c = res.rseed % colours.length;
	oc = (0 == c % 2) ? c + 1 : c - 1;

	/* Shuffle the presentation of rows. */
	shuffle(matrix, res.rseed);
	appendBimatrix(doClear('exprMatrix'), matrix, c, oc);

	/* Show the shuffled roundup matrix, if it exists. */
	e = doClear('exprHistory');
	if (null != hmatrix) {
		doClearReplace('exprHistoryLottery', res.curlottery);
		appendMatrix(doClear('exprHistoryMatrix'), matrix);
	} 

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
		ii = document.createElement('span');
		ii.setAttribute('class', 'strat');
		ii.appendChild(document.createTextNode((i + 1) + '.'));
		input = document.createElement('input');
		input.setAttribute('type', 'text');
		input.setAttribute('readonly', 'readonly');
		input.setAttribute('placeholder', 'Real index: ' + matrix[i].index);
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
	var e, expr;

	resindex = 0;
	res = null;

	try  { 
		res = JSON.parse(resp);
	} catch (error) {
		doUnhide('exprLoading');
		doUnhide('historyLoading');
		doUnhide('instructionsLoading');
		doHide('exprLoaded');
		doHide('historyLoaded');
		doHide('instructionsLoaded');
		return;
	}

	expr = res.expr;

	doHide('exprLoading');
	doHide('historyLoading');
	doHide('instructionsLoading');
	doUnhide('exprLoaded');
	doUnhide('historyLoaded');
	doUnhide('instructionsLoaded');
	doClearReplaceMarkup('instructionsLoaded', expr.instructions);

	if (expr.tilstart > 0) {
		/*
		 * If we haven't yet started, then simply set our timer
		 * and exit: we have nothing to show.
		 */
		doUnhide('exprCountdownTilStart');
		doHide('exprCountdownTilNext');
		e = doClear('exprCountdown');
		formatCountdown(expr.tilstart, e);
		setTimeout(timerCountdown, 1000, loadExpr, 
			e, expr.tilstart, new Date().getTime());
		doUnhide('exprNotStarted');
	} else if (0 == expr.tilstart) {
		/*
		 * Start by setting the countdown til the next
		 * game-play.
		 */
		doHide('exprCountdownTilStart');
		doUnhide('exprCountdownTilNext');

		e = doClear('exprCountdown');
		formatCountdown(expr.tilnext, e);
		setTimeout(timerCountdown, 1000, loadExpr, 
			e, expr.tilnext, new Date().getTime());
		doValue('exprPlayRound', expr.round);
		shuffle(res.games, res.rseed);
		loadGame();
	} else {
		/*
		 * Case where the game has finished.
		 */
		doUnhide('exprPlay');
		doHide('exprCountdownTilStart');
		doHide('exprCountdownTilNext');
		doClearReplace('exprCountdown', 'Finished');
		doClearReplace('exprPlay', 'Experiment finished.');
	}
}

function loadExprSetup()
{
	var e;

	doHide('exprLoaded');
	doUnhide('exprLoading');
	doHide('historyLoaded');
	doUnhide('historyLoading');
	doHide('instructionsLoaded');
	doUnhide('instructionsLoading');
	doHide('exprCountdownTilStart');
	doHide('exprCountdownTilNext');
}

function loadExprFailure(err)
{
	var url = document.URL;

	url = url.substring(0, url.lastIndexOf("/"));
	location.href = url + '/login.html#loggedout';
}

function loadExpr() 
{

	sendQuery('@@cgibin@@/doloadexpr.json', 
		loadExprSetup, loadExprSuccess, loadExprFailure);
}

function doPlayGameSetup()
{

	doHide('playGameErrorJson');
	doHide('playGameErrorForm');
	doHide('playGameErrorState');
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
		doUnhide('playGameErrorState');
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
