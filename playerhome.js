"use strict";

/*
 * The results.
 * This is only valid after we've run loadExprSuccess(), otherwise it's
 * null.
 * It's loaded with the games that we haven't played yet.
 */
var res = null;

/* 
 * FIXME: store these as names, then if we reload the game, show the
 * given bimatrix and graph.
 */
var shownBimatrix = null;
var shownLineGraph = null;
var shownBarGraph = null;

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

/*
 * Silly way to create a seedable pseudo-random number.
 * We don't need anything more complicated than this: it's only used to
 * permute the game matrices on the client.
 */
function random(object) 
{
	var x = Math.sin(object.seed++) * 10000;
	return x - Math.floor(x);
}

/*
 * Should we show history?
 * We only do this if we're after the first round OR we have pre-game
 * history, and we let this be overriden by the nohistory clause.
 */
function checkShowHistory(res)
{

	if (res.expr.round > 0 || null !== res.expr.history)
		return ( ! res.expr.nohistory);
	return(0);
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

function rational2float(fraction) 
{
	var result, frac;

	if (fraction.search('/') >= 0) {
		frac = fraction.split('/');
		if (0 == parseInt(frac[1], 10))
			result = 0;
		else
			result = parseInt(frac[0], 10) / parseInt(frac[1], 10);
	} else
		result = fraction

	return (result);
}

function prowOut(source)
{
	var e, id;

	id = source.getAttribute('data-row');

	if (null !== (e = document.getElementById('index' + id))) {
		if (e.hasAttribute('disabled'))
			return;
		source.classList.remove('hover');
		e.parentNode.classList.remove('ihover');
	}
}

function prowOver(source)
{
	var e, id;

	id = source.getAttribute('data-row');

	if (null !== (e = document.getElementById('index' + id))) {
		if (e.hasAttribute('disabled'))
			return;
		source.classList.add('hover');
		e.parentNode.classList.add('ihover');
	}
}

function prowClick(source)
{
	var e, id;

	id = source.getAttribute('data-row');

	if (null === (e = document.getElementById('index' + id)))
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
			row.setAttribute('data-row', rorder[i]);
			row.onclick = function() {
				return function() { prowClick(this); };
			}();
			row.onmouseover = function() {
				return function() { prowOver(this); };
			}();
			row.onmouseout = function() {
				return function() { prowOut(this); };
			}();
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

function loadGame()
{
	var game, matrix, e, div, ii, i, j, input, c, oc;

	/*
	 * In this case, we've played through all of the games that we
	 * have available to play: just wait for the next round.
	 */
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

	/*
	 * It might be that we play our games out of order for some
	 * reason--I don't know, but it can happen.
	 * If so, simply skip over this game index.
	 */
	game = res.games[res.gameorders[resindex]];
	if (null === game) {
		resindex++;
		loadGame();
		return;
	}

	e = document.getElementById('nextround');
	if (null !== e && 0 === resindex) {
		doClearReplace('nextroundround', 
			res.expr.absoluteround ?  
			(res.expr.round + 1) :
			((res.expr.round - res.player.joined) + 1));
		doUnhideNode(e);
		if ('Notification' in window &&
		    window.Notification && Notification.permission === "granted") {
			var options = {
				body: 'Gamelab round has advanced to ' +
					(res.expr.absoluteround ?  
					 (res.expr.round + 1) :
					 ((res.expr.round - res.player.joined) + 1))
			};
			var n;
			try {
				n = new Notification('Gamelab Update', options);
			} catch (e) {
				n = null;
			}
			if (null !== n) {
				n.onclick = function() { doHide('nextround'); };
				setTimeout(n.close.bind(n), 5000);
			}
		}
	}

	/* Transpose the matrix, if necessary. */
	matrix = 0 === res.player.role ? 
		bimatrixCreate(game.payoffs) : 
		bimatrixCreateTranspose(game.payoffs);

	c = res.player.rseed % colours.length;
	oc = (0 === c % 2) ? c + 1 : c - 1;

	appendBimatrix(doClear('exprMatrix'), 1, matrix, c, oc, 
		res.roworders[res.gameorders[resindex]],
		res.colorders[res.gameorders[resindex]]);

	/*
	 * We don't have a history if we're still on the first round,
	 * otherwise load the graphs for our history.
	 */
	if (checkShowHistory(res)) {
		doUnhide('exprHistory');
		doClear('historyLineGraphsSmall');
		doClear('historyBarGraphsSmall');
		loadGameGraphs(res.gameorders[resindex], 
			'historyLineGraphsSmall', 
			'historyBarGraphsSmall', 1);
		doUnhide('historyLineGraphsSmall' + 
			res.gameorders[resindex]);
		doUnhide('historyBarGraphsSmall' + 
			res.gameorders[resindex]);
		if (res.expr.round > res.player.joined &&
		    0 !== game.roundup.skip)
			doUnhide('skipExplain');
		else
			doHide('skipExplain');
	} else
		doHide('exprHistory');

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
		input.onkeypress = function() {
			return function(event){return(disableEnter(event));};
		}();
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
	if (null === e)
		return;
	if (0 === e.selectedIndex)
		return;
	e.selectedIndex--;
	showHistory();
}

function showHistoryGameNext()
{
	var e;

	e = document.getElementById('historySelectGame');
	if (null === e)
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
	if (null === e)
		return;
	else if (e.selectedIndex < 0)
		return;

	game = e.options[e.selectedIndex].value;
	doClearReplace('historyGame', (e.selectedIndex + 1));

	if (null !== shownBimatrix)
		doHideNode(shownBimatrix);
	shownBimatrix = doUnhide('bimatrix' + game);

	if (null !== shownLineGraph)
		doHideNode(shownLineGraph);
	shownLineGraph = doUnhide('historyLineGraphs' + game);

	if (null !== shownBarGraph)
		doHideNode(shownBarGraph);
	shownBarGraph = doUnhide('historyBarGraphs' + game);
}

function loadGameGraphs(gameidx, lineName, barName, small)
{
	var	e, c, i, j, k, l, m, data, datas, lot, 
		avg, len, len1, len2, matrix, hmatrix, sum, sub, 
		stratidx, oldest, newest, min;

	oldest = small ? 'old' : 'oldest';
	newest = small ? 'new' : 'newest';

	e = document.getElementById(lineName);
	sub = document.createElement('div');
	sub.setAttribute('id', lineName + gameidx);
	e.appendChild(sub);
	doHideNode(sub);

	matrix = null;
	if (res.expr.round > 0) {
		/*
		 * We have a real history: use that for our length
		 * calculations (number of strategies).
		 */
		matrix = 0 === res.player.role ?
			bimatrixCreate(res.history[gameidx].payoffs) :
			bimatrixCreateTranspose(res.history[gameidx].payoffs);
		len1 = res.history[gameidx].roundups[0].navgp1.length;
		len2 = res.history[gameidx].roundups[0].navgp2.length;
	}

	hmatrix = null;
	if (null !== res.expr.history) {
		hmatrix = 0 === res.player.role ?
			bimatrixCreate(res.expr.history[gameidx].payoffs) :
			bimatrixCreateTranspose(res.expr.history[gameidx].payoffs);
		/* 
		 * If we don't have a real history (expr.history when on
		 * the zeroth round) use ourselves for the strategy
		 * count.
		 */
		if (null === matrix) {
			len1 = res.expr.history[gameidx].roundups[0].navgp1.length;
			len2 = res.expr.history[gameidx].roundups[0].navgp2.length;
		}
	}

	/* 
	 * Hypothetical payoff. 
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	len = 0 === res.player.role ? len1 : len2;
	datas = [];
	min = 0.0;
	for (j = 0; j < len; j++) {
		data = [];
		/* Start with zero! */
		data.push([0, 0.0]);
		sum = 0.0;
		k = 0;
		if (null !== hmatrix)
			for ( ; k < res.expr.history[gameidx].roundups.length; k++) {
				avg = 0 === res.player.role ?
					res.expr.history[gameidx].roundups[k].navgp2 :
					res.expr.history[gameidx].roundups[k].navgp1;
				for (sum = 0.0, m = 0; m < hmatrix[0].length; m++)
					sum += avg[res.colorders[gameidx][m]] * rational2float
						(hmatrix[res.roworders[gameidx][j]][res.colorders[gameidx][m]][0]);
				data.push([k + 1, sum]);
				if (sum < min)
					min = sum;
			}
		l = k;
		if (null !== matrix)
			for (k = 0; k < res.history[gameidx].roundups.length; k++) {
				avg = 0 === res.player.role ?
					res.history[gameidx].roundups[k].navgp2 :
					res.history[gameidx].roundups[k].navgp1;
				for (sum = 0.0, m = 0; m < matrix[0].length; m++)
					sum += avg[res.colorders[gameidx][m]] * rational2float
						(matrix[res.roworders[gameidx][j]][res.colorders[gameidx][m]][0]);
				data.push([(k + l) + 1, sum]);
				if (sum < min)
					min = sum;
			}
		datas[j] = {
			data: data,
			label: (small ? '' : 'Row ') + String.fromCharCode(97 + j)
		};
	}
	Flotr.draw(c, datas, 
		{ grid: { horizontalLines: 1 },
		  xaxis: { ticks: [[ 0, oldest ], [(l + k), newest]] },
		  shadowSize: 0,
		  subtitle: (small ? 'Row PO' : 'Previous row payoffs'),
		  yaxis: { min: min, tickDecimals: 1 },
		  lines: { show: true },
		  points: { show: true }});

	/* 
	 * Real payoff. 
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	data = [];
	/* Start with zero! */
	data.push([0, 0.0]);
	j = 0;
	min = 0.0;
	if (null !== hmatrix)
		for ( ; j < res.expr.history[gameidx].roundups.length; j++) 
			data.push([j + 1, 0.0]);
	k = j;
	if (null !== matrix)
		for (j = 0; j < res.history[gameidx].roundups.length; j++) {
			lot = res.lotteries[j].plays[gameidx];
			data.push([(j + k) + 1, null === lot ? 0.0 : lot.poff]);
			if (null !== lot && lot.poff < min)
				min = lot.poff;
		}
	Flotr.draw(c, 
		[{ data: data }],
		{ xaxis: { ticks: [[ 0, oldest ], [(j + k), newest]] },
		  subtitle: (small ? 'Your PO' : 'Your payoff'),
		  shadowSize: 0,
		  lines: { show: true },
		  points: { show: true },
		  yaxis: { min: min, tickDecimals: 1 }});

	/* 
	 * Accumulated payoff. 
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	data = [];
	/* Start with zero! */
	data.push([0, 0.0]);
	min = 0.0;
	j = 0;
	if (null !== hmatrix)
		for (j = 0; j < res.expr.history[gameidx].roundups.length; j++)
			data.push([j + 1, 0]);
	k = j;
	if (null !== matrix)
		for (l = 0.0, j = 0; j < res.history[gameidx].roundups.length; j++) {
			lot = res.lotteries[j].plays[gameidx];
			if (null !== lot)
				l += lot.poff;
			data.push([(j + k) + 1, l]);
			if (l < min)
				min = l;
		}
	Flotr.draw(c, 
		[{ data: data }],
		{ xaxis: { ticks: [[ 0, oldest ], [(j + k), newest]] },
		  subtitle: (small ? 'Tot. PO' : 'Your total payoff'),
		  shadowSize: 0,
		  lines: { show: true },
		  points: { show: true },
		  yaxis: { min: min, tickDecimals: 1 }});

	e = document.getElementById(barName);
	sub = document.createElement('div');
	sub.setAttribute('id', barName + gameidx);
	e.appendChild(sub);
	doHideNode(sub);

	/* 
	 * Player's strategy. 
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	datas = [];
	len = 0 === res.player.role ? len1 : len2;
	for (k = l = j = 0; j < len; j++) {
		stratidx = res.roworders[gameidx][j];
		data = [];
		k = 0;
		if (null !== hmatrix)
			for ( ; k < res.expr.history[gameidx].roundups.length; k++)
				data.push([k, 0]);
		l = k;
		if (null !== matrix)
			for (k = 0; k < res.history[gameidx].roundups.length; k++) {
				lot = res.lotteries[k].plays[gameidx];
				if (null === lot) {
					data.push([(l + k), 0]);
					continue;
				}
				data.push([(l + k), lot.stratsd[stratidx]]);
			}
		datas[j] = {
			data: data, 
			label: (small ? '' : 'Row ') + String.fromCharCode(97 + j)
		};
	}
	Flotr.draw(c, datas, { 
		bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
		grid: { horizontalLines: 1 },
		xaxis: { ticks: [[ 0, (0 === (l + k) - 1 ? '' : oldest) ], 
			         [(l + k) - 1, (0 === (l + k) - 1 ? (newest + ' ' + oldest) : newest)]] },
		subtitle: (small ? 'You' : 'Your frequencies'),
		yaxis: { max: 1.0, min: 0.0, tickDecimals: 1 }
	});

	/* 
	 * Player population's average strategy. 
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	datas = [];
	len = 0 === res.player.role ? len1 : len2;
	for (l = k = j = 0; j < len; j++) {
		stratidx = res.roworders[gameidx][j];
		data = [];
		k = 0;
		if (null !== hmatrix)
			for ( ; k < res.expr.history[gameidx].roundups.length; k++) {
				avg = 0 === res.player.role ? 
					res.expr.history[gameidx].roundups[k].navgp1 : 
					res.expr.history[gameidx].roundups[k].navgp2;
				data.push([k, avg[stratidx]]);
			}
		l = k;
		if (null !== matrix)
			for (k = 0; k < res.history[gameidx].roundups.length; k++) {
				avg = 0 === res.player.role ? 
					res.history[gameidx].roundups[k].navgp1 : 
					res.history[gameidx].roundups[k].navgp2;
				data.push([(l + k), avg[stratidx]]);
			}
		datas[j] = {
			data: data, 
			label: (small ? '' : 'Row ') + String.fromCharCode(97 + j)
		};
	}
	Flotr.draw(c, datas, { 
		bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
		grid: { horizontalLines: 1 },
		subtitle: (small ? 'Row' : 'Previous row'),
		xaxis: { ticks: [[ 0, (0 === (l + k) - 1 ? '' : oldest) ], 
			         [(l + k) - 1, (0 === (l + k) - 1 ? (newest + ' ' + oldest) : newest)]] },
		yaxis: { max: 1.0, min: 0.0, tickDecimals: 1 }
	});

	/* 
	 * Lastly, opponent population's average strategy. 
	 */
	c = document.createElement('div');
	sub.appendChild(c);
	datas = [];
	len = 0 === res.player.role ? len2 : len1;
	for (k = l = j = 0; j < len; j++) {
		stratidx = res.colorders[gameidx][j];
		data = [];
		k = 0;
		if (null !== hmatrix)
			for ( ; k < res.expr.history[gameidx].roundups.length; k++) {
				avg = 0 === res.player.role ? 
					res.expr.history[gameidx].roundups[k].navgp2 : 
					res.expr.history[gameidx].roundups[k].navgp1;
				data.push([k, avg[stratidx]]);
			}
		l = k;
		if (null !== matrix)
			for (k = 0; k < res.history[gameidx].roundups.length; k++) {
				avg = 0 === res.player.role ? 
					res.history[gameidx].roundups[k].navgp2 : 
					res.history[gameidx].roundups[k].navgp1;
				data.push([(l + k), avg[stratidx]]);
			}
		datas[j] = {
			data: data, 
			label: (small ? '' : 'Column ') + String.fromCharCode(65 + j)
		};
	}
	Flotr.draw(c, datas, { 
		bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
		grid: { horizontalLines: 1 },
		xaxis: { ticks: [[ 0, (0 === (l + k) - 1 ? '' : oldest) ], 
			         [(l + k) - 1, (0 === (l + k) - 1 ? (newest + ' ' + oldest) : newest)]] },
		subtitle: (small ? 'Column' : 'Previous column'),
		yaxis: { max: 1.0, min: 0.0, tickDecimals: 1 }
	});
}

function loadGraphs()
{
	var i;

	if (null === res || ! checkShowHistory(res))
		return;

	doClear('historyLineGraphs');
	doHideNode(doClear('historyBarGraphs'));

	for (i = 0; i < res.history.length; i++)
		loadGameGraphs(res.gameorders[i], 
			'historyLineGraphs', 'historyBarGraphs', 0);
}

function loadHistory(res)
{
	var gamee, e, i, j, k, child, bmatrix, c, oc, tbl, game;

	if (res.expr.nohistory)
		return;

	loadGraphs();

	c = res.player.rseed % colours.length;
	oc = (0 === c % 2) ? c + 1 : c - 1;

	k = 0;
	if (null !== (e = document.getElementById('historySelectGame')))
		k = e.selectedIndex;

	if (null !== (e = doClear('historySelectGame'))) {
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
		bmatrix = 0 === res.player.role ? 
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
	var i, j, e, c, oc, v, elems, next;

	if (null === (res = parseJson(resp)))
		return;

	resindex = 0;

	doHide('loading');
	doUnhide('loaded');

	document.getElementById('instructionsPromptYes').checked = 
		res.player.instr ? 'checked' : '';
	if (0 === res.player.instr && '' == window.location.hash)
		window.location.hash = '#play';

	doHide('exprLoading');
	doHide('historyLoading');
	doHide('instructionsLoading');
	doUnhide('exprLoaded');
	doUnhide('historyLoaded');
	doUnhide('instructionsLoaded');
	doClearReplaceMarkup('instructionsLoaded', res.expr.instr);

	/* 
	 * If the player is captive, don't let her log out.
	 * Also don't let her change whether to show the instructions
	 * when she logs in because she can't log in anyway.
	 */
	if (res.player.autoadd) {
		doHide('logoutButton');
		doHide('instructionsPrompt');
	}

	c = res.player.rseed % colours.length;
	oc = (0 === c % 2) ? c + 1 : c - 1;
	elems = document.getElementsByClassName('gamelab-colour');
	for (i = 0; i < elems.length; i++)
		elems[i].style.color = colours[c];
	elems = document.getElementsByClassName('gamelab-ocolour');
	for (i = 0; i < elems.length; i++)
		elems[i].style.color = colours[oc];

	/* 
	 * Shuffle our game list and row orders. 
	 * To do this, we use our player's random seed as input to a
	 * very simple deterministic PRNG.
	 * This by no means needs to be strong or have any particularly
	 * random properties.
	 * Don't randomise if the experiment stipulates so.
	 */
	if (null !== res.history) {
		res.gameorders = new Array(res.history.length);
		for (j = 0; j < res.gameorders.length; j++)
			res.gameorders[j] = j;
		if ( ! res.expr.noshuffle)
			shuffle(res.gameorders, res.player.rseed);
		res.roworders = new Array(res.history.length);
		res.colorders = new Array(res.history.length);
		for (i = 0; i < res.roworders.length; i++) {
			res.roworders[i] = new Array
				(0 === res.player.role ?  
				 res.history[i].p1 : 
				 res.history[i].p2);
			res.colorders[i] = new Array
				(0 === res.player.role ?  
				 res.history[i].p2 : 
				 res.history[i].p1);
			for (j = 0; j < res.roworders[i].length; j++)
				res.roworders[i][j] = j;
			for (j = 0; j < res.colorders[i].length; j++)
				res.colorders[i][j] = j;
			if ( ! res.expr.noshuffle) {
				shuffle(res.roworders[i], res.player.rseed);
				shuffle(res.colorders[i], res.player.rseed);
			}
		}
	} else {
		res.roworders = null;
		res.colorders = null;
		res.gameorders = null;
	}

	if (res.expr.sandbox) {
		doHide('mturkformreal');
		doUnhide('mturkformsand');
	} else {
		doUnhide('mturkformreal');
		doHide('mturkformsand');
	}

	if (null !== res.player.assignmentid) {
		if (res.expr.nohistory) {
			doHide('exprHistoryExplain');
			doHide('historyButton');
		} else {
			doUnhide('exprHistoryExplain');
			doUnhide('historyButton');
		}
	}

	doClearReplace('nextRound', 'Next round');

	/*
	 * Make lots of common substitutions.
	 */
	doValueClass('gamelab-assignmentid', 
		null !== res.player.assignmentid ?
		res.player.assignmentid : '');
	doClearReplaceClass('gamelab-aggrlottery', 
		null !== res.aggrlottery ?
		res.aggrlottery.toFixed(2) : '0');
	doClearReplaceClass('gamelab-aggrtickets', 
		null !== res.aggrtickets ?
		res.aggrtickets : '0');
	doClearReplaceClass('gamelab-mturkbonus', 
		null !== res.aggrtickets ?
		(res.aggrtickets * res.expr.conversion) : '0');
	doClearReplaceClass('gamelab-maxtickets', res.expr.maxtickets);
	doClearReplaceClass('gamelab-rounds',
		res.expr.absoluteround ? res.expr.rounds :
		(res.player.joined + res.expr.prounds > res.expr.rounds ?
			 (res.expr.prounds - 
			  ((res.player.joined + res.expr.prounds) - 
			   res.expr.rounds)) : res.expr.prounds));
	doClearReplaceClass('gamelab-round',
		res.expr.absoluteround ? 
		(res.expr.round + 1) :
		((res.expr.round - res.player.joined) + 1));
	doClearReplaceClass('gamelab-finaltickets',
		res.player.finalscore);
	doClearReplaceClass('gamelab-finalrank',
		res.player.finalrank);
	doClearReplaceClass('gamelab-finalrankend',
		(res.player.finalrank + res.player.finalscore));
	v = '';
	if (null !== res.win) 
		for (i = 0; i < res.win.winrnums.length; i++)
			v += (i ? ', ' : '') + res.win.winrnums[i];
	doClearReplaceClass('gamelab-winrnums', v);
	doClearReplaceClass('gamelab-winrnum', 
		null !== res.win ? res.win.winrnum : '0');
	doClearReplaceClass('gamelab-winrank', 
		null !== res.win ? (res.win.winner + 1) : '0');
	doClearReplaceClass('gamelab-conversion',
		res.expr.conversion);

	if (res.expr.round < 0) {
		/*
		 * Branch indicating that we haven't yet started: simply
		 * set our timer and exit: we have nothing to show.
		 */
		doUnhide('historyNotStarted');
		doUnhide('exprNotStarted');
		next = res.expr.start - Math.floor(new Date().getTime() / 1000);
		e = doClear('exprCountdown');
		formatCountdown(next, e);
		setTimeout(timerCountdown, 1000, loadExpr, e, res.expr.start);
	} else if (res.expr.round < res.player.joined) {
		/*
		 * We won't be here if we haven't joined, so this branch
		 * covers that we've joined but for a future round.
		 */
		doUnhide('historyNotStarted');
		doUnhide('exprNotStarted');
		if (res.expr.roundmin > 0 && Math.floor(new Date().getTime() / 1000) - 
				res.expr.roundbegan <= res.expr.roundmin * 60) {
			doClearReplace('nextRound', 'Grace time');
			next = (res.expr.roundbegan + (res.expr.roundmin * 60)) -
				Math.floor(new Date().getTime() / 1000);
			e = doClear('exprCountdown');
			formatCountdown(next, e);
			setTimeout(timerCountdown, 1000, checkRound, e, 
				(res.expr.roundbegan + (res.expr.roundmin * 60)));
		} else {
			next = (res.expr.roundbegan + (res.expr.minutes * 60)) -
				Math.floor(new Date().getTime() / 1000);
			e = doClear('exprCountdown');
			formatCountdown(next, e);
			setTimeout(timerCountdown, 1000, null, e, 
				(res.expr.roundbegan + (res.expr.minutes * 60)));
			setTimeout(checkRoundEnd, 
				((res.expr.minutes < 10 || res.expr.roundpct > 0) ? 
				5000 : 60000));
		}
	} else if (res.expr.round < res.expr.rounds && 
		   res.expr.round < res.player.joined + res.expr.prounds) {
		/*
		 * This is the main branch where we're actually playing.
		 * Start by setting the countdown til the next
		 * game-play.
		 */
		if (res.expr.roundmin > 0 && Math.floor(new Date().getTime() / 1000) - 
				res.expr.roundbegan <= res.expr.roundmin * 60) {
			doClearReplace('nextRound', 'Grace time');
			next = (res.expr.roundbegan + (res.expr.roundmin * 60)) -
				Math.floor(new Date().getTime() / 1000);
			e = doClear('exprCountdown');
			formatCountdown(next, e);
			setTimeout(timerCountdown, 1000, checkRound, e, 
				res.expr.roundbegan + (res.expr.roundmin * 60));
		} else {
			next = (res.expr.roundbegan + (res.expr.minutes * 60)) -
				Math.floor(new Date().getTime() / 1000);
			e = doClear('exprCountdown');
			formatCountdown(next, e);
			setTimeout(timerCountdown, 1000, null, e, 
				(res.expr.roundbegan + (res.expr.minutes * 60)));
			setTimeout(checkRoundEnd, 
				((res.expr.minutes < 10 || res.expr.roundpct > 0) ? 
				5000 : 60000));
		}
		doValue('exprPlayRound', res.expr.round);
		if (checkShowHistory(res)) {
			doUnhide('historyPlay');
			doHide('historyNotYet');
			loadHistory(res);
		} else {
			doHide('historyPlay');
			doUnhide('historyNotYet');
		}
		loadGame();
	} else if (res.expr.round < res.expr.rounds) {
		/* 
		 * Branch where player has finished, but experiment has
		 * not finished.
		 */
		doHide('exprPlay');
		doHide('exprDone');
		doUnhide('exprFinished');
		if (null !== res.player.assignmentid && ! res.player.mturkdone) {
			doUnhide('exprFinishedMturk');
			doUnhide('exprFinishedMturkProfit');
		} 
		if (checkShowHistory(res)) {
			doUnhide('historyPlay');
			doHide('historyNotYet');
			loadHistory(res);
		} else {
			doHide('historyPlay');
			doUnhide('historyNotYet');
		}
		doUnhide('exprNotAllFinished');
		if (null !== res.expr.lottery && res.expr.lottery.length) {
			doUnhide('exprNotAllFinishedLottery');
			doHide('exprNotAllFinishedNoLottery');
		} else {
			doHide('exprNotAllFinishedLottery');
			doUnhide('exprNotAllFinishedNoLottery');
		}
		doHide('exprAllFinished');
		doClearReplace('exprCountdown', 'finished');
	} else {
		/*
		 * Lastly, branch where the experiment has finished.
		 */
		doHide('exprPlay');
		doHide('exprDone');
		doUnhide('exprFinished');
		if (checkShowHistory(res)) {
			doUnhide('historyPlay');
			doHide('historyNotYet');
			loadHistory(res);
		} else {
			doHide('historyPlay');
			doUnhide('historyNotYet');
		}
		doHide('exprNotAllFinished');
		doUnhide('exprAllFinished');
		if (null !== res.player.assignmentid && ! res.player.mturkdone) {
			doUnhide('exprFinishedMturk');
			doUnhide('exprFinishedMturkProfit');
		} else if (null !== res.player.hitid)
			doClearReplace('hitid', res.player.hitid);

		doClearReplace('exprCountdown', 'finished');
		if (null !== res.expr.lottery && 
		    '' !== res.player.hitid &&
		    res.expr.lottery.length > 0) {
			doUnhide('exprLottery');
			doHide('exprNoLottery');
		} else {
			doHide('exprLottery');
			doUnhide('exprNoLottery');
		}

		if (null === res.win) {
			doUnhide('exprFinishedWinWait');
			doHide('exprFinishedLotteryDone');
			setTimeout(loadExpr, 1000 * 60);
		} else if (res.win.winner < 0) {
			doHide('exprFinishedWinWait');
			doUnhide('exprFinishedLotteryDone');
			doHide('exprFinishedWin');
			doUnhide('exprFinishedLose');
		} else {
			doHide('exprFinishedWinWait');
			doUnhide('exprFinishedLotteryDone');
			doUnhide('exprFinishedWin');
			doHide('exprFinishedLose');
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
	if ('Notification' in window &&
	    window.Notification && 
	    Notification.permission !== "granted") {
		Notification.requestPermission(function (status) {
			if (Notification.permission !== status) {
				Notification.permission = status;
			}
		});
	}
	loadExpr();
}

function loadExprFailure(err)
{

	switch (err) {
	case 429:
		location.href = getURL('@HTURI@/playerlobby.html');
		break;
	default:
		location.href = '@HTURI@/playerlogin.html#loggedout';
		break;
	}
}

function checkRoundEndSuccess(resp)
{
	var r, e, next;

	if (null === (r = parseJson(resp)))
		return;

	if (r.round > res.expr.round) {
		window.location.reload();
		return;
	}
	setTimeout(checkRoundEnd, 
		(res.expr.minutes < 10 || res.expr.roundpct > 0) ? 
		5000 : 60000);
}

function checkRoundSuccess(resp)
{
	var r, e, next;

	if (null === (r = parseJson(resp)))
		return;

	if (r.round > res.expr.round) {
		window.location.reload();
		return;
	}

	doClearReplace('nextRound', 'Next round');
	next = (res.expr.roundbegan + (res.expr.minutes * 60)) -
		Math.floor(new Date().getTime() / 1000);
	e = doClear('exprCountdown');
	formatCountdown(next, e);
	setTimeout(timerCountdown, 1000, 
		function(){window.location.reload();}, e, 
		res.expr.roundbegan + (res.expr.minutes * 60));
	setTimeout(checkRoundEnd, 
		(res.expr.minutes < 10 || res.expr.roundpct > 0) ? 
		5000 : 60000);
}

function checkRound()
{

	sendQuery(getURL('@LABURI@/docheckround.json'),
		null, checkRoundSuccess, null);
}

function checkRoundEnd()
{

	sendQuery(getURL('@LABURI@/docheckround.json'),
		null, checkRoundEndSuccess, null);
}

function loadExpr() 
{

	sendQuery(getURL('@LABURI@/doloadexpr.json'),
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

function submitMturkSetup(resp)
{

	doHide('mturkbtn');
	doUnhide('mturkpbtn');
	doHide('mturkfbtn');
}

function submitMturkSuccess(resp)
{

	doHide('mturkbtn');
	doHide('mturkpbtn');
	doUnhide('mturkfbtn');
}

function doPlayGameSuccess(resp)
{
	var i, e, div, ii, input, elems;

	e = document.getElementById('playGameSubmit');
	e.setAttribute('value', 'Submitted!');
	e.setAttribute('disabled', 'disabled');

	for (i = 0; ; i++) {
		e = document.getElementById('index' + i);
		if (null === e)
			break;
		e.setAttribute('disabled', 'disabled');
		e = document.getElementById('payoffRow' + i);
		if (null !== e)
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
	input.onclick = loadGame;
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
	var data = new FormData(form);

	augmentForm(data);

	return(sendFormData(form, data,
		doPlayGameSetup, doPlayGameError, doPlayGameSuccess));
}

function submitMturk()
{

	sendQuery(getURL('@LABURI@/mturkfinish.json'),
		submitMturkSetup, 
		submitMturkSuccess, 
		submitMturkSuccess);
}

function updateInstr(form)
{
	var data = new FormData(form);

	augmentForm(data);

	return(sendFormData(form, data, null, 
		null, function() { window.location.reload(true); }));
}
