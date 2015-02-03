"use strict";

/*
 * The results.
 * This is only valid after we've run loadExprSuccess(), otherwise it's
 * null.
 * It's loaded with the games that we haven't played yet.
 */
var res;

/* Yech... */
var shownHistory;
var shownLottery;
var shownBimatrix;

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

function appendMatrix(e, matrix, rorder, corder, ravg, cavg, payoffs, colour)
{
	var table, row, cell, i, j, poff, rowinner, sum, span;

	table = document.createElement('div');
	e.appendChild(table);

	table.setAttribute('class', 'history');
	table.setAttribute('style', 
		'max-width: ' + (matrix[0].length * 10) + 'em;');

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
			(String.fromCharCode(65 + i)));
		row.appendChild(cell);
	}

	cell = document.createElement('div');
	cell.setAttribute('class', 'sumaside');
	cell.setAttribute('style', 'width: ' + 
		((100.0 / (matrix[0].length + 1)) - 1) + '%;');
	span = document.createElement('span');
	span.appendChild(document.createTextNode('\u2211'));
	cell.appendChild(span);
	if (null != payoffs) {
		span = document.createElement('span');
		span.appendChild(document.createTextNode(': E\u27e6'));
		cell.appendChild(span);
		span = document.createElement('var');
		span.appendChild(document.createTextNode('X'));
		cell.appendChild(span);
		span = document.createElement('span');
		span.appendChild(document.createTextNode('\u27e7'));
		cell.appendChild(span);
	}
	row.appendChild(cell);

	for (i = 0; i < matrix.length; i++) {
		row = document.createElement('div');
		table.appendChild(row);

		cell = document.createElement('div');
		cell.setAttribute('class', 'labelaside');
		cell.appendChild(document.createTextNode
			(String.fromCharCode(97 + i)));
		row.appendChild(cell);

		sum = 0;
		for (j = 0; j < matrix[rorder[i]].length; j++) {
			cell = document.createElement('div');
			cell.setAttribute('class', 'mix');
			row.appendChild(cell);
			cell.appendChild
				(document.createTextNode
				 (matrix[rorder[i]][corder[j]].toFixed(2)));
			if (null != payoffs)
				sum += cavg[corder[j]] * 
					payoffs[rorder[i]][corder[j]][0];
		}

		cell = document.createElement('div');
		cell.setAttribute('class', 'sumaside sum');

		span = document.createElement('span');
		span.appendChild(document.createTextNode
			(ravg[rorder[i]].toFixed(2)));
		cell.appendChild(span);
		if (null != payoffs) {
			span = document.createElement('span');
			span.appendChild(document.createTextNode(': '));
			cell.appendChild(span);
			span = document.createElement('span');
			span.setAttribute('style', 'color: ' + colours[colour]);
			span.appendChild(document.createTextNode(sum.toFixed(2)));
			cell.appendChild(span);
		}
		row.appendChild(cell);
	}

	row = document.createElement('div');
	table.appendChild(row);

	cell = document.createElement('div');
	cell.setAttribute('class', 'labelaside');
	cell.appendChild(document.createTextNode('\u2211'));
	row.appendChild(cell);

	for (i = 0; i < cavg.length; i++) {
		cell = document.createElement('div');
		cell.setAttribute('class', 'sumbelow sum');
		row.appendChild(cell);
		cell.appendChild(document.createTextNode
			(cavg[corder[i]].toFixed(2)));
	}

	cell = document.createElement('div');
	cell.setAttribute('class', 'sumaside');
	row.appendChild(cell);
	return(table);
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

	if (e.hasAttribute('disabled'))
		return;

	if (e.hasAttribute('readonly')) {
		source.classList.add('active');
		e.value = '0';
		e.removeAttribute('readonly');
		e.select();
		e.parentNode.classList.add('iactive');
	} else {
		source.classList.remove('active');
		e.value = '';
		e.setAttribute('readonly', 'readonly');
		e.parentNode.classList.remove('iactive');
	}

	if (source.classList.contains('hover'))
		source.classList.remove('hover');
	if (e.parentNode.classList.contains('ihover'))
		e.parentNode.classList.remove('ihover');
}

function appendBimatrix(e, active, matrix, colour, ocolour, rorder, corder)
{
	var table, row, cell, i, j, poff, inputs;

	inputs = document.getElementById('playGame');

	table = document.createElement('div');
	table.setAttribute('class', 'payoffs');
	table.setAttribute('style', 'max-width: ' + 
		(matrix[0].length * 10) + 'em;');

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
			(String.fromCharCode(65 + i)));
		row.appendChild(cell);
	}

	for (i = 0; i < matrix.length; i++) {
		row = document.createElement('div');
		if (active) {
			row.setAttribute('onclick', 
				'prowClick(this, ' + rorder[i] + ')');
			row.setAttribute('onmouseover', 
				'prowOver(this, ' + rorder[i] + ')');
			row.setAttribute('onmouseout', 
				'prowOut(this, ' + rorder[i] + ')');
		}
		table.appendChild(row);
		cell = document.createElement('div');
		cell.setAttribute('class', 'labelaside');
		cell.appendChild(document.createTextNode
			(String.fromCharCode(97 + i)));
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
				 (matrix[rorder[i]][corder[j]][0]));
			cell.appendChild(poff);
			poff = document.createElement('div');
			poff.setAttribute('style', 
				'color: ' + colours[ocolour]);
			poff.appendChild
				(document.createTextNode
				 (matrix[rorder[i]][corder[j]][1]));
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

function disableEnter(e) {
	e = e || window.event;
	var keycode = e.which || e.keyCode;
	if (keycode == 13) {
		if (e.preventDefault) { 
			e.preventDefault();
		} else { 
			e.returnValue = false;
		}
		if (e.stopPropagation) {
			e.stopPropagation();
		} else { 
			e.cancelBubble = true;
		}
		return (false);
	}
}

/*
 * Given that we've already loaded the experiment via loadGameSuccess(),
 * take the existing game and lay out our inputs.
 */
function loadGame()
{
	var game, matrix, hmatrix, e, div, ii, i, j, input, c, oc, ravg, cavg, lot, par;

	if (resindex == res.games.length) {
		doUnhide('exprDone');
		doHide('exprPlay');
		doHide('exprFinished');
		doHide('exprNotStarted');
		return;
	} 

	doHide('exprNotStarted');
	doHide('historyNotStarted');
	doHide('exprDone');
	doHide('exprFinished');
	doUnhide('exprPlay');

	doClearReplace('playGameNum', (resindex + 1));
	doClearReplace('playGameMax', res.gamesz);
	doClearReplace('playRoundNum', res.expr.round + 1);
	doClearReplace('playRoundNum2', res.expr.round + 1);
	doClearReplace('playRoundMax', res.expr.rounds);
	doClearReplace('playRoundMax2', res.expr.rounds);

	game = res.games[res.gameorders[resindex]];
	if (null == game) {
		resindex++;
		loadGame();
		return;
	}

	/* Transpose the matrix, if necessary. */
	matrix = 0 == res.role ? 
		bimatrixCreate(game.payoffs) : 
		bimatrixCreateTranspose(game.payoffs);
	hmatrix = null;

	if (null != game.roundup) {
		/* Transpose vectors and matrix. */
		ravg = 0 == res.role ? 
			game.roundup.navgp1 : game.roundup.navgp2;
		cavg = 0 == res.role ? 
			game.roundup.navgp2 : game.roundup.navgp1;
		hmatrix = 0 == res.role ? 
			matrixCreate(game.roundup.navgs) : 
			matrixCreateTranspose(game.roundup.navgs);
	} 

	c = res.rseed % colours.length;
	oc = (0 == c % 2) ? c + 1 : c - 1;

	appendBimatrix(doClear('exprMatrix'), 1, matrix, c, oc, 
		res.roworders[res.gameorders[resindex]],
		res.colorders[res.gameorders[resindex]]);

	document.getElementById('playerColour').setAttribute('style', 'color: ' + colours[c] + ';');
	document.getElementById('playerColour2').setAttribute('style', 'color: ' + colours[c] + ';');
	document.getElementById('playerOColour2').setAttribute('style', 'color: ' + colours[oc] + ';');

	if (null != hmatrix) {
		doUnhide('exprHistory');
		if (0 != game.roundup.skip) 
			doUnhide('skipExplain');
		else
			doHide('skipExplain');
		doClearReplace('exprHistoryLottery', res.curlottery);
		appendMatrix(doClear('exprHistoryMatrix'), hmatrix, 
			res.roworders[res.gameorders[resindex]], 
			res.colorders[res.gameorders[resindex]], 
			ravg, cavg, matrix, c);
		par = doClear('exprHistoryPlays');
		lot = res.lotteries[res.expr.round - 1].plays[res.gameorders[resindex]];
		if (null != lot) {
			par.appendChild(document.createTextNode
				(', strategy mix: '));
			for (i = 0; i < lot.strats.length; i++) {
				if (i > 0)
					par.appendChild(document.createTextNode(', '));
				par.appendChild(document.createTextNode
					(String.fromCharCode(97 + i) + '-'));
				par.appendChild(document.createTextNode
					(lot.strats[res.roworders[res.gameorders[resindex]][i]]));
			}
		}
	} else {
		doHide('exprHistory');
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
		ii.appendChild(document.createTextNode
			(String.fromCharCode(97 + i)));
		input = document.createElement('input');
		input.setAttribute('type', 'text');
		input.setAttribute('readonly', 'readonly');
		input.setAttribute('id', 'index' + 
			res.roworders[res.gameorders[resindex]][i]);
		input.setAttribute('name', 'index' + 
			res.roworders[res.gameorders[resindex]][i]);
		input.setAttribute('onkeypress', 'return(disableEnter(event));');
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
	input.setAttribute('value', 'Submit play');
	div.appendChild(ii);
	div.appendChild(input);
	e.appendChild(div);
}

function showHistoryRoundPrev()
{
	var e;

	e = document.getElementById('historySelectRound');
	if (null == e)
		return;
	if (0 == e.selectedIndex)
		return;
	e.selectedIndex--;
	showHistory();
}

function showHistoryRoundNext()
{
	var e;

	e = document.getElementById('historySelectRound');
	if (null == e)
		return;
	if (e.selectedIndex == e.options.length - 1)
		return;
	e.selectedIndex++;
	showHistory();
}

function showHistoryGamePrev()
{
	var e;

	e = document.getElementById('historySelectGame');
	if (null == e)
		return;
	if (0 == e.selectedIndex)
		return;
	e.selectedIndex--;
	showHistory();
}

function showHistoryGameNext()
{
	var e;

	e = document.getElementById('historySelectGame');
	if (null == e)
		return;
	if (e.selectedIndex == e.options.length - 1)
		return;
	e.selectedIndex++;
	showHistory();
}

function showHistory()
{
	var e, round, game;

	e = document.getElementById('historySelectRound');
	if (null == e)
		return;
	round = e.options[e.selectedIndex].value;
	e = document.getElementById('historySelectGame');
	if (null == e)
		return;
	game = e.options[e.selectedIndex].value;
	if (null != shownHistory)
		doHideNode(shownHistory);
	shownHistory = doUnhide('game' + game + 'round' + round);
	if (null != shownLottery)
		doHideNode(shownLottery);
	shownLottery = doUnhide('lottery' + round);
	if (null != shownBimatrix)
		doHideNode(shownBimatrix);
	shownBimatrix = doUnhide('bimatrix' + game);
}

function loadHistory(res)
{
	var e, i, j, k, child, matrix, bmatrix, c, oc, 
	    ravg, cavg, tbl, game, par, lot;

	c = res.rseed % colours.length;
	oc = (0 == c % 2) ? c + 1 : c - 1;

	doClearReplace('historyLottery', res.aggrlottery);

	if (null != (e = doClear('historySelectGame'))) {
		for (i = 0; i < res.gamesz; i++) {
			child = document.createElement('option');
			child.appendChild
				(document.createTextNode((i + 1)));
			child.value = res.gameorders[i];
			e.appendChild(child);
			if (0 == i)
				child.setAttribute('selected', 'selected');
		}
	} 

	if (null != (e = doClear('historySelectRound'))) {
		for (i = 0; i < res.history[0].roundups.length; i++) {
			child = document.createElement('option');
			child.appendChild
				(document.createTextNode((i + 1)));
			child.value = i;
			e.appendChild(child);
		}
		if (null != child)
			child.setAttribute('selected', 'selected');
	} 

	e = doClear('historyRoundups');
	for (i = 0; i < res.history.length; i++) {
		game = res.history[res.gameorders[i]];
		for (j = 0; j < game.roundups.length; j++) {
			tbl = document.createElement('div');
			tbl.setAttribute('id', 'game' + 
				res.gameorders[i] + 'round' + j);
			doHideNode(tbl);
			e.appendChild(tbl);
			ravg = 0 == res.role ? 
				game.roundups[j].navgp1 : 
				game.roundups[j].navgp2;
			cavg = 0 == res.role ? 
				game.roundups[j].navgp2 : 
				game.roundups[j].navgp1;
			matrix = 0 == res.role ?
				matrixCreate(game.roundups[j].navgs) :
				matrixCreateTranspose
				(game.roundups[j].navgs);
			appendMatrix(tbl, matrix, 
				res.roworders[res.gameorders[i]], 
				res.colorders[res.gameorders[i]], 
				ravg, cavg, null, c);

			par = document.createElement('p');
			tbl.appendChild(par);
			lot = res.lotteries[j].plays[res.gameorders[i]];
			if (null == lot) {
				par.appendChild(document.createTextNode
					('No plays for this round and game.'));
				continue;
			}
			par.appendChild(document.createTextNode
				('Your strategy mix: '));
			for (k = 0; k < lot.strats.length; k++) {
				if (k > 0)
					par.appendChild(document.createTextNode(', '));
				par.appendChild(document.createTextNode
					(String.fromCharCode(97 + k) + '-'));
				par.appendChild(document.createTextNode
					(lot.strats[res.roworders[res.gameorders[i]][k]]));
			}

			par = document.createElement('p');
			tbl.appendChild(par);
			par.appendChild(document.createTextNode
				('Your payoff: '));
			par.appendChild(document.createTextNode(lot.poff));
		}
	}

	e = doClear('bimatrixRoundups');
	for (i = 0; i < res.history.length; i++) {
		tbl = document.createElement('div');
		e.appendChild(tbl);
		tbl.setAttribute('id', 'bimatrix' + i);
		game = res.history[res.gameorders[i]];
		bmatrix = 0 == res.role ? 
			bimatrixCreate(game.payoffs) : 
			bimatrixCreateTranspose(game.payoffs);
		appendBimatrix(tbl, 0,
			bmatrix, c, oc, 
			res.roworders[res.gameorders[i]],
			res.colorders[res.gameorders[i]]);
		doHideNode(tbl);
	}

	e = doClear('lotteryRoundups');
	for (i = 0; i < res.lotteries.length; i++) {
		tbl = document.createElement('p');
		e.appendChild(tbl);
		tbl.setAttribute('id', 'lottery' + i);
		tbl.appendChild(document.createTextNode
			('Round payoff: ' + res.lotteries[i].curlottery));
		doHideNode(tbl);
	}

	showHistory();
}

function loadExprSuccess(resp)
{
	var i, j, e, expr, c, oc;

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

	document.getElementById('instructionsPromptYes').checked = 
		res.instr ? 'checked' : '';
	if (0 == res.instr && '' == window.location.hash)
		window.location.hash = '#play';

	doHide('exprLoading');
	doHide('historyLoading');
	doHide('instructionsLoading');
	doUnhide('exprLoaded');
	doUnhide('historyLoaded');
	doUnhide('instructionsLoaded');
	doClearReplaceMarkup('instructionsLoaded', expr.instr);
	doClearReplaceMarkup('exprFinishedWin', expr.instrWin);

	c = res.rseed % colours.length;
	oc = (0 == c % 2) ? c + 1 : c - 1;

	document.getElementById('playerColour').setAttribute
		('style', 'color: ' + colours[c] + ';');
	document.getElementById('playerColour2').setAttribute
		('style', 'color: ' + colours[c] + ';');
	document.getElementById('playerOColour2').setAttribute
		('style', 'color: ' + colours[oc] + ';');

	/* Shuffle our game list and row orders. */
	if (null != res.history) {
		res.gameorders = new Array(res.history.length);
		for (j = 0; j < res.gameorders.length; j++)
			res.gameorders[j] = j;
		shuffle(res.gameorders, res.rseed);
		res.roworders = new Array(res.history.length);
		res.colorders = new Array(res.history.length);
		for (i = 0; i < res.roworders.length; i++) {
			res.roworders[i] = new Array
				(0 == res.role ?  
				 res.history[i].p1 : 
				 res.history[i].p2);
			res.colorders[i] = new Array
				(0 == res.role ?  
				 res.history[i].p2 : 
				 res.history[i].p1);
			for (j = 0; j < res.roworders[i].length; j++)
				res.roworders[i][j] = j;
			for (j = 0; j < res.colorders[i].length; j++)
				res.colorders[i][j] = j;
			shuffle(res.roworders[i], res.rseed);
			shuffle(res.colorders[i], res.rseed);
		}
	} else {
		res.roworders = null;
		res.colorders = null;
		res.gameorders = null;
	}

	if (expr.tilstart > 0) {
		/*
		 * If we haven't yet started, then simply set our timer
		 * and exit: we have nothing to show.
		 */
		doUnhide('historyNotStarted');
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
		e = doClear('exprCountdown');
		formatCountdown(expr.tilnext, e);
		setTimeout(timerCountdown, 1000, loadExpr, 
			e, expr.tilnext, new Date().getTime());
		doValue('exprPlayRound', expr.round);
		if (expr.round > 0) {
			doUnhide('historyPlay');
			doHide('historyNotYet');
			loadHistory(res);
		} else {
			doHide('historyPlay');
			doUnhide('historyNotYet');
		}
		loadGame();
	} else {
		/*
		 * Case where the game has finished.
		 */
		doHide('exprPlay');
		doHide('exprDone');
		doUnhide('exprFinished');
		if (expr.round > 0) {
			doUnhide('historyPlay');
			doHide('historyNotYet');
			loadHistory(res);
		} else {
			doHide('historyPlay');
			doUnhide('historyNotYet');
		}
		doClearReplace('exprFinishedTicketsMax', expr.maxtickets);
		doClearReplace('exprFinishedTickets', res.aggrlottery);
		doClearReplace('exprCountdown', 'finished');
		if (null == res.winner) {
			doHide('exprFinishedResults');
			doUnhide('exprFinishedWinWait');
			doHide('exprFinishedWin');
		} else if (res.winner < 0) {
			doUnhide('exprFinishedResults');
			doHide('exprFinishedWinWait');
			doHide('exprFinishedWin');
		} else {
			doUnhide('exprFinishedResults');
			doHide('exprFinishedWinWait');
			doUnhide('exprFinishedWin');
			doClearReplace('exprFinishedWinRank', (res.winner + 1));
		}
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
	doHide('playGameErrorJson');
	doHide('playGameErrorForm');
	doHide('playGameErrorState');
}

function sendLoggedOut()
{
	var url = document.URL;

	url = url.substring(0, url.lastIndexOf("/"));
	location.href = url + '/login.html#loggedout';
}

function loadExprFailure(err)
{

	sendLoggedOut();
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
	case 404:
		sendLoggedOut();
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
		e.setAttribute('disabled', 'disabled');
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
		 'Go to next game' : 'Finish round'));

	div.appendChild(ii);
	div.appendChild(input);
	e.appendChild(div);
}

function updateInstrSetup(resp)
{

}

function updateInstrSuccess(resp)
{

	window.location.reload(true);
}

function updateInstr(form)
{

	return(sendForm(form, updateInstrSetup,
		null, updateInstrSuccess));
}

function playGame(form)
{

	return(sendForm(form, doPlayGameSetup,
		doPlayGameError, doPlayGameSuccess));
}
