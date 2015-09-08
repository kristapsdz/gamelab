"use strict";

/*
 * The results.
 * This is only valid after we've run loadExprSuccess(), otherwise it's
 * null.
 * It's loaded with the games that we haven't played yet.
 */
var res;

/* 
 * FIXME: store these as names, then if we reload the game, show the
 * given bimatrix and graph.
 */
var shownBimatrix;
var shownLineGraph;
var shownBarGraph;

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
	"#FF8000"
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
}

function prowOut(source, id)
{
	var e;

	if (null != (e = document.getElementById('index' + id))) {
		if (e.hasAttribute('disabled'))
			return;
		source.classList.remove('hover');
		e.parentNode.classList.remove('ihover');
	}
}

function prowOver(source, id)
{
	var e;

	if (null != (e = document.getElementById('index' + id))) {
		if (e.hasAttribute('disabled'))
			return;
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

/*
 * Append the game matrix, which consists only of the strategy (and
 * payoff) available to given players.
 * This is like appendBimatrix.
 * If this is "active", then we allow the rows to be clicked upon,
 * raising the prowClick() function (onclick), prowOver (onmouseover),
 * and prowOut (onmouseout) events.
 */
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
			row.setAttribute('id', 'payoffRow' + i);
			row.setAttribute('onclick', 
				'prowClick(this, ' + rorder[i] + ')');
			row.setAttribute('onmouseover', 
				'prowOver(this, ' + rorder[i] + ')');
			row.setAttribute('onmouseout', 
				'prowOut(this, ' + rorder[i] + ')');
			row.style.cursor = 'pointer';
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

/*
 * Convert a game, which is in top-left to bottom-right order with row
 * player payoff first, into a matrix.
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
	var game, matrix, e, div, ii, i, j, input, c, 
	    oc, lot, par, list, listitem;

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
	doClearReplace('playRoundNum', (res.expr.round - res.player.joined) + 1);
	doClearReplace('playRoundNum2', (res.expr.round - res.player.joined) + 1);
	doClearReplace('playRoundMax', res.expr.prounds);
	doClearReplace('playRoundMax2', res.expr.prounds);

	game = res.games[res.gameorders[resindex]];
	if (null == game) {
		resindex++;
		loadGame();
		return;
	}

	/* Transpose the matrix, if necessary. */
	matrix = 0 == res.player.role ? 
		bimatrixCreate(game.payoffs) : 
		bimatrixCreateTranspose(game.payoffs);


	c = res.player.rseed % colours.length;
	oc = (0 == c % 2) ? c + 1 : c - 1;

	appendBimatrix(doClear('exprMatrix'), 1, matrix, c, oc, 
		res.roworders[res.gameorders[resindex]],
		res.colorders[res.gameorders[resindex]]);

	if (null != game.roundup) {
		doUnhide('exprHistory');
		doClear('historyLineGraphsSmall');
		doClear('historyBarGraphsSmall');
		loadGameGraphs(res.gameorders[resindex], 
			'historyLineGraphsSmall', 'historyBarGraphsSmall');
		doUnhide('historyLineGraphsSmall' + res.gameorders[resindex]);
		doUnhide('historyBarGraphsSmall' + res.gameorders[resindex]);
		if (0 != game.roundup.skip && res.player.joined > res.expr.round) 
			doUnhide('skipExplain');
		else
			doHide('skipExplain');
		lot = res.lotteries[res.expr.round - 1].plays[res.gameorders[resindex]];
		par = doClear('exprHistoryPlays');
		if (null != lot) {
			doClearReplace('exprHistoryLottery', lot.poff.toFixed(2));
			par.appendChild(document.createTextNode
				(', strategy mix: '));
			list = document.createElement('ul');
			list.setAttribute('class', 'stratplays');
			par.appendChild(list);
			for (i = 0; i < lot.strats.length; i++) {
				listitem = document.createElement('li');
				listitem.appendChild(document.createTextNode
					(String.fromCharCode(97 + i) + ' \u2013 '));
				listitem.appendChild(document.createTextNode
					(lot.strats[res.roworders[res.gameorders[resindex]][i]]));
				list.appendChild(listitem);
			}
		} else 
			doClearReplace('exprHistoryLottery', '0');
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

function showGraph(name, e)
{

	document.getElementById('graphLineButton').classList.remove('clicked');
	document.getElementById('graphBarButton').classList.remove('clicked');
	doHide('historyBarGraphs');
	doHide('historyLineGraphs');
	doUnhide(name);
	e.classList.add('clicked');
}

function showHistory()
{
	var e, game;

	e = document.getElementById('historySelectGame');
	game = e.options[e.selectedIndex].value;
	doClearReplace('historyGame', (e.selectedIndex + 1));

	if (null != shownBimatrix)
		doHideNode(shownBimatrix);
	shownBimatrix = doUnhide('bimatrix' + game);

	if (null != shownLineGraph)
		doHideNode(shownLineGraph);
	shownLineGraph = doUnhide('historyLineGraphs' + game);

	if (null != shownBarGraph)
		doHideNode(shownBarGraph);
	shownBarGraph = doUnhide('historyBarGraphs' + game);
}

function loadGameGraphs(gameidx, lineName, barName)
{
	var	e, c, i, j, k, l, m, data, datas, lot, 
		avg, len, matrix, hmatrix, sum, sub, gameidx, 
		stratidx;

	e = document.getElementById(lineName);
	sub = document.createElement('div');
	sub.setAttribute('id', lineName + gameidx);
	e.appendChild(sub);
	doHideNode(sub);

	matrix = 0 == res.player.role ?
		bimatrixCreate(res.history[gameidx].payoffs) :
		bimatrixCreateTranspose(res.history[gameidx].payoffs);
	hmatrix = null;
	if (null != res.expr.history)
		hmatrix = 0 == res.player.role ?
			bimatrixCreate(res.expr.history[gameidx].payoffs) :
			bimatrixCreateTranspose(res.expr.history[gameidx].payoffs);

	/* 
	 * Hypothetical payoff. 
	 * Begin this (if appropriate) with the historical play.
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	len = 0 == res.player.role ? 
		res.history[gameidx].roundups[0].navgp1.length : 
		res.history[gameidx].roundups[0].navgp2.length;
	datas = [];
	for (j = 0; j < len; j++) {
		data = [];
		/* Start with zero! */
		data.push([0, 0.0]);
		sum = 0.0;
		k = 0;
		if (null != hmatrix)
			for ( ; k < res.expr.history[gameidx].roundups.length; k++) {
				avg = 0 == res.player.role ?
					res.expr.history[gameidx].roundups[k].navgp2 :
					res.expr.history[gameidx].roundups[k].navgp1;
				for (sum = 0.0, m = 0; m < hmatrix[0].length; m++)
					sum += avg[res.colorders[gameidx][m]] *
						hmatrix[res.roworders[gameidx][j]]
						       [res.colorders[gameidx][m]][0];
				data.push([k + 1, sum]);
			}
		l = k;
		for (k = 0; k < res.history[gameidx].roundups.length; k++) {
			avg = 0 == res.player.role ?
				res.history[gameidx].roundups[k].navgp2 :
				res.history[gameidx].roundups[k].navgp1;
			for (sum = 0.0, m = 0; m < matrix[0].length; m++)
				sum += avg[res.colorders[gameidx][m]] *
					matrix[res.roworders[gameidx][j]]
					      [res.colorders[gameidx][m]][0];
			data.push([(k + l) + 1, sum]);
		}
		datas[j] = {
			data: data,
			label: 'Strategy ' + String.fromCharCode(97 + j)
		};
	}
	Flotr.draw(c, datas, 
		{ grid: { horizontalLines: 1 },
		  xaxis: { ticks: [[ 0, 'oldest' ], [(l + k), 'newest']] },
		  shadowSize: 0,
		  subtitle: 'Hypothetical payoff',
		  yaxis: { min: 0.0 },
		  lines: { show: true },
		  points: { show: true }});

	/* Real payoff. */
	c = document.createElement('div');
	sub.appendChild(c);
	data = [];
	/* Start with zero! */
	data.push([0, 0.0]);
	j = 0;
	if (null != hmatrix)
		for ( ; j < res.expr.history[gameidx].roundups.length; j++) 
			data.push([j + 1, 0.0]);
	k = j;
	for (j = 0; j < res.history[gameidx].roundups.length; j++) {
		lot = res.lotteries[j].plays[gameidx];
		data.push([(j + k) + 1, null == lot ? 0.0 : lot.poff]);
	}
	Flotr.draw(c, 
		[{ data: data }],
		{ xaxis: { ticks: [[ 0, 'oldest' ], [(j + k), 'newest']] },
		  subtitle: 'Real payoff',
		  shadowSize: 0,
		  lines: { show: true },
		  points: { show: true },
		  yaxis: { min: 0.0 }});

	/* Accumulated payoff. */
	c = document.createElement('div');
	sub.appendChild(c);
	data = [];
	/* Start with zero! */
	data.push([0, 0.0]);
	j = 0;
	if (null != hmatrix)
		for (j = 0; j < res.expr.history[gameidx].roundups.length; j++)
			data.push([j + 1, 0]);
	k = j;
	for (l = 0.0, j = 0; j < res.history[gameidx].roundups.length; j++) {
		lot = res.lotteries[j].plays[gameidx];
		if (null != lot)
			l += lot.poff;
		data.push([(j + k) + 1, l]);
	}
	Flotr.draw(c, 
		[{ data: data }],
		{ xaxis: { ticks: [[ 0, 'oldest' ], [(j + k), 'newest']] },
		  subtitle: 'Accumulated payoff',
		  shadowSize: 0,
		  lines: { show: true },
		  points: { show: true },
		  yaxis: { min: 0.0 }});

	e = document.getElementById(barName);
	sub = document.createElement('div');
	sub.setAttribute('id', barName + gameidx);
	e.appendChild(sub);
	doHideNode(sub);

	/* 
	 * Player's strategy. 
	 * We begin with the historical data; however, since
	 * this player didn't play those, we simply set the
	 * strategies all to zero.
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	datas = [];
	len = 0 == res.player.role ? 
		res.history[gameidx].roundups[0].navgp1.length : 
		res.history[gameidx].roundups[0].navgp2.length;
	for (k = l = j = 0; j < len; j++) {
		stratidx = res.roworders[gameidx][j];
		data = [];
		k = 0;
		if (null != res.expr.history) {
			for ( ; k < res.expr.history[gameidx].roundups.length; k++)
				data.push([k, 0]);
		}
		l = k;
		for (k = 0; k < res.history[gameidx].roundups.length; k++) {
			lot = res.lotteries[k].plays[gameidx];
			if (null == lot) {
				data.push([(l + k), 0]);
				continue;
			}
			data.push([(l + k), lot.stratsd[stratidx]]);
		}
		datas[j] = {
			data: data, 
			label: 'Strategy ' + String.fromCharCode(97 + j)
		};
	}
	Flotr.draw(c, datas, { 
		bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
		grid: { horizontalLines: 1 },
		xaxis: { ticks: [[ 0, 'oldest' ], [(l + k) - 1, 'newest']] },
		subtitle: 'Your strategy',
		yaxis: { max: 1.0, min: 0.0 }
	});

	/* 
	 * Player population's average strategy. 
	 * Account for both current and historical (first) data.
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	datas = [];
	len = 0 == res.player.role ? 
		res.history[gameidx].roundups[0].navgp1.length : 
		res.history[gameidx].roundups[0].navgp2.length;
	for (l = k = j = 0; j < len; j++) {
		stratidx = res.roworders[gameidx][j];
		data = [];
		k = 0;
		if (null != res.expr.history) {
			for ( ; k < res.expr.history[gameidx].roundups.length; k++) {
				avg = 0 == res.player.role ? 
					res.expr.history[gameidx].roundups[k].navgp1 : 
					res.expr.history[gameidx].roundups[k].navgp2;
				data.push([k, avg[stratidx]]);
			}
		}
		l = k;
		for (k = 0; k < res.history[gameidx].roundups.length; k++) {
			avg = 0 == res.player.role ? 
				res.history[gameidx].roundups[k].navgp1 : 
				res.history[gameidx].roundups[k].navgp2;
			data.push([(l + k), avg[stratidx]]);
		}
		datas[j] = {
			data: data, 
			label: 'Strategy ' + String.fromCharCode(97 + j)
		};
	}
	Flotr.draw(c, datas, { 
		bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
		grid: { horizontalLines: 1 },
		subtitle: 'Row player average strategy',
		xaxis: { ticks: [[ 0, 'oldest' ], [(l + k) - 1, 'newest']] },
		yaxis: { max: 1.0, min: 0.0 }
	});

	/* 
	 * Lastly, opponent population's average strategy. 
	 * Here we also account for previous information.
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	datas = [];
	len = 0 == res.player.role ? 
		res.history[gameidx].roundups[0].navgp2.length : 
		res.history[gameidx].roundups[0].navgp1.length;
	for (k = l = j = 0; j < len; j++) {
		stratidx = res.colorders[gameidx][j];
		data = [];
		k = 0;
		if (null != res.expr.history) {
			for ( ; k < res.expr.history[gameidx].roundups.length; k++) {
				avg = 0 == res.player.role ? 
					res.expr.history[gameidx].roundups[k].navgp2 : 
					res.expr.history[gameidx].roundups[k].navgp1;
				data.push([k, avg[stratidx]]);
			}
		}
		l = k;
		for (k = 0; k < res.history[gameidx].roundups.length; k++) {
			avg = 0 == res.player.role ? 
				res.history[gameidx].roundups[k].navgp2 : 
				res.history[gameidx].roundups[k].navgp1;
			data.push([(l + k), avg[stratidx]]);
		}
		datas[j] = {
			data: data, 
			label: 'Strategy ' + String.fromCharCode(65 + j)
		};
	}
	Flotr.draw(c, datas, { 
		bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
		grid: { horizontalLines: 1 },
		xaxis: { ticks: [[ 0, 'oldest' ], [(l + k) - 1, 'newest']] },
		subtitle: 'Column player average strategy',
		yaxis: { max: 1.0, min: 0.0 }
	});
}

function loadGraphs()
{
	var i;

	if (null == res)
		return;
	else if (res.expr.round <= 0)
		return;

	doClear('historyLineGraphs');
	doHideNode(doClear('historyBarGraphs'));

	for (i = 0; i < res.history.length; i++)
		loadGameGraphs(res.gameorders[i], 'historyLineGraphs', 'historyBarGraphs');
}

function loadHistory(res)
{
	var gamee, e, i, j, k, child, bmatrix, c, oc, tbl, game;

	loadGraphs();

	c = res.player.rseed % colours.length;
	oc = (0 == c % 2) ? c + 1 : c - 1;

	doClearReplace('historyLottery', res.aggrlottery.toFixed(2));

	k = 0;
	if (null != (e = document.getElementById('historySelectGame')))
		k = e.selectedIndex;
	if (null != (e = doClear('historySelectGame'))) {
		for (i = 0; i < res.gamesz; i++) {
			child = document.createElement('option');
			child.appendChild
				(document.createTextNode((i + 1)));
			child.value = res.gameorders[i];
			e.appendChild(child);
			if (k == i)
				child.setAttribute('selected', 'selected');
		}
	} 

	gamee = doClear('bimatrixRoundups');

	for (i = 0; i < res.history.length; i++) {
		game = res.history[res.gameorders[i]];

		tbl = document.createElement('div');
		gamee.appendChild(tbl);
		tbl.setAttribute('id', 'bimatrix' + res.gameorders[i]);
		bmatrix = 0 == res.player.role ? 
			bimatrixCreate(game.payoffs) : 
			bimatrixCreateTranspose(game.payoffs);
		appendBimatrix(tbl, 0,
			bmatrix, c, oc, 
			res.roworders[res.gameorders[i]],
			res.colorders[res.gameorders[i]]);
		doHideNode(tbl);
	}

	showHistory();
}

function loadExprSuccess(resp)
{
	var i, j, e, expr, c, oc, v, elems, next;

	resindex = 0;
	res = null;
	doHide('loading');
	doUnhide('loaded');

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
		res.player.instr ? 'checked' : '';
	if (0 == res.player.instr && '' == window.location.hash)
		window.location.hash = '#play';

	doHide('exprLoading');
	doHide('historyLoading');
	doHide('instructionsLoading');
	doUnhide('exprLoaded');
	doUnhide('historyLoaded');
	doUnhide('instructionsLoaded');
	doClearReplaceMarkup('instructionsLoaded', expr.instr);

	/* 
	 * If the player is captive, don't let her log out.
	 */
	if (res.player.autoadd) {
		doHide('logoutButton');
		doHide('instructionsPrompt');
	}

	c = res.player.rseed % colours.length;
	oc = (0 == c % 2) ? c + 1 : c - 1;

	elems = document.getElementsByClassName('gamelab-colour');
	for (i = 0; i < elems.length; i++)
		elems[i].style.color = colours[c];
	elems = document.getElementsByClassName('gamelab-ocolour');
	for (i = 0; i < elems.length; i++)
		elems[i].style.color = colours[oc];

	/* Shuffle our game list and row orders. */
	if (null != res.history) {
		res.gameorders = new Array(res.history.length);
		for (j = 0; j < res.gameorders.length; j++)
			res.gameorders[j] = j;
		shuffle(res.gameorders, res.player.rseed);
		res.roworders = new Array(res.history.length);
		res.colorders = new Array(res.history.length);
		for (i = 0; i < res.roworders.length; i++) {
			res.roworders[i] = new Array
				(0 == res.player.role ?  
				 res.history[i].p1 : 
				 res.history[i].p2);
			res.colorders[i] = new Array
				(0 == res.player.role ?  
				 res.history[i].p2 : 
				 res.history[i].p1);
			for (j = 0; j < res.roworders[i].length; j++)
				res.roworders[i][j] = j;
			for (j = 0; j < res.colorders[i].length; j++)
				res.colorders[i][j] = j;
			shuffle(res.roworders[i], res.player.rseed);
			shuffle(res.colorders[i], res.player.rseed);
		}
	} else {
		res.roworders = null;
		res.colorders = null;
		res.gameorders = null;
	}

	if (expr.round < res.player.joined) {
		/*
		 * If we haven't yet started, then simply set our timer
		 * and exit: we have nothing to show.
		 */
		next = expr.start - Math.floor(new Date().getTime() / 1000);
		doUnhide('historyNotStarted');
		e = doClear('exprCountdown');
		formatCountdown(next, e);
		/*
		 * If the game hasn't started, then set this to be the
		 * time that the game begins.
		 * Otherwise, set it to the next round.
		 */
		if (expr.round < 0)
			setTimeout(timerCountdown, 1000, 
				loadExpr, e, expr.start);
		else
			setTimeout(timerCountdown, 1000, 
				loadExpr, e, expr.roundbegan + 
				(expr.minutes * 60));
		doUnhide('exprNotStarted');
	} else if (expr.round < res.player.joined + expr.prounds) {
		/*
		 * Start by setting the countdown til the next
		 * game-play.
		 */
		next = (expr.roundbegan + (expr.minutes * 60)) -
			Math.floor(new Date().getTime() / 1000);
		e = doClear('exprCountdown');
		formatCountdown(next, e);
		setTimeout(timerCountdown, 1000, loadExpr, e, 
			expr.roundbegan + (expr.minutes * 60));
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
	} else if (expr.round < expr.rounds) {
		/* 
		 * Case where player has finished, but game has not.
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
		doUnhide('exprNotAllFinished');
		doClearReplace('exprNotAllFinishedScore', res.aggrlottery.toFixed(2));
		doClearReplace('exprNotAllFinishedTickets', res.aggrtickets);
		doHide('exprAllFinished');
		doClearReplace('exprCountdown', 'finished');
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
		doHide('exprNotAllFinished');
		doUnhide('exprAllFinished');
		doClearReplace('exprFinishedScore', res.aggrlottery.toFixed(2));
		doClearReplace('exprFinishedTicketsMax', expr.maxtickets);
		doClearReplace('exprFinishedFinalRank', res.player.finalrank);
		doClearReplace('exprFinishedTickets', res.player.finalscore);
		v = res.player.finalrank + res.player.finalscore;
		doClearReplace('exprFinishedFinalRankEnd', v);
		doClearReplace('exprCountdown', 'finished');
		if (expr.nolottery) {
			doHide('exprLottery');
		} else {
			doUnhide('exprLottery');
		}

		if (null == res.winner) {
			doHide('exprFinishedResults');
			doUnhide('exprFinishedWinWait');
			doHide('exprFinishedWin');
			doHide('exprFinishedLose');
			doHide('exprFinishedWinHead');
			doHide('exprFinishedWinRnums');
			setTimeout(loadExpr, 1000 * 60);
		} else if (res.winner < 0) {
			doUnhide('exprFinishedResults');
			doHide('exprFinishedWinWait');
			doHide('exprFinishedWin');
			doUnhide('exprFinishedLose');
			doUnhide('exprFinishedWinHead');
			doUnhide('exprFinishedWinRnums');
			e = doClear('exprFinishedRnums');
			for (i = 0; i < res.winrnums.length; i++) {
				if (i > 0)
					e.appendChild(document.createTextNode(', '));
				e.appendChild(document.createTextNode(res.winrnums[i]));
			}
		} else {
			doUnhide('exprFinishedResults');
			doHide('exprFinishedWinWait');
			doUnhide('exprFinishedWin');
			doHide('exprFinishedLose');
			doUnhide('exprFinishedWinHead');
			doUnhide('exprFinishedWinRnums');
			e = doClear('exprFinishedRnums');
			for (i = 0; i < res.winrnums.length; i++) {
				if (i > 0)
					e.appendChild(document.createTextNode(', '));
				e.appendChild(document.createTextNode(res.winrnums[i]));
			}
			doClearReplace('exprFinishedWinRank', (res.winner + 1));
			doClearReplace('exprFinishedWinRnum', res.winrnum);
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

function loadExprFirst() 
{

	doHide('loaded');
	loadExpr();
}

function loadExprFailure(err)
{

	switch (err) {
	case 429:
		location.href = '@HTURI@/playerlobby.html';
		break;
	default:
		location.href = '@HTURI@/playerlogin.html#loggedout';
		break;
	}
}

function loadExpr() 
{

	sendQuery('@LABURI@/doloadexpr.json', 
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
		location.href = '@HTURI@/playerlogin.html#loggedout';
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
	var i, e, div, ii, input, elems;

	e = document.getElementById('playGameSubmit');
	e.setAttribute('value', 'Submitted!');
	e.setAttribute('disabled', 'disabled');

	for (i = 0; ; i++) {
		e = document.getElementById('index' + i);
		if (null == e)
			break;
		e.setAttribute('disabled', 'disabled');
		e = document.getElementById('payoffRow' + i);
		if (null != e)
			e.style.cursor = 'auto';
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

function playGame(form)
{

	return(sendForm(form, 
		doPlayGameSetup,
		doPlayGameError, 
		doPlayGameSuccess));
}

function updateInstr(form)
{

	return(sendForm(form, 
		null, 
		null, 
		function() { window.location.reload(true); }));
}
