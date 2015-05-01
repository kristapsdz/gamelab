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
var shownGraph;

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
 * Append a payoff matrix as a "div.history" to "e", setting its maximum
 * width to be 10em per payoff column.
 * This is a complicated function because it must not only show the
 * payoffs, but also the expected utility (payoff) for the row player if
 * playing her pure strategy (per row).
 * Moroever, all rows and columns are randomised per player, so we must
 * de-scramble them.
 * Variables:
 *  - matrix: the row-ordered payoff matrix with each cell consisting of
 *    a pair of payoffs, row player being 0, column player being 1
 *    (see matrixCreate() and matrixCreateTranspose())
 *  - rorder: the displayed row order index for each given row index
 *  - corder: the displayed column order index for each given column
 *  - ravg: the averages for the given row strategies
 *  - cavg: the averages for the given column strategies
 *  - payoffs: the multiplication of ravg*cavg per cell
 *  - colour: our (row player's) colour
 */
function appendMatrix(e, matrix, rorder, corder, ravg, cavg, payoffs, colour)
{
	var table, row, cell, i, j, poff, rowinner, sum, span;

	table = document.createElement('div');
	e.appendChild(table);
	table.setAttribute('class', 'history');
	/* Let us be 10em per column. */
	table.setAttribute('style', 'max-width: ' + 
		(matrix[0].length * 10) + 'em;');

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

	/*
	 * Create the top-most row.
	 * This will have our table headers.
	 */
	cell = document.createElement('div');
	cell.setAttribute('class', 'sumaside');
	cell.setAttribute('style', 'width: ' + 
		((100.0 / (matrix[0].length + 1)) - 1) + '%;');
	cell.appendChild(document.createTextNode('\u2211'));
	cell.appendChild(document.createTextNode(' ['));
	span = document.createElement('span');
	span.setAttribute('style', 'color: ' + colours[colour]);
	span.appendChild(document.createTextNode('U'));
	cell.appendChild(span);
	cell.appendChild(document.createTextNode(']'));
	row.appendChild(cell);

	/*
	 * Now create the rows themselves.
	 * These end in a column with row averages and the expected
	 * utility if the player plays that row as a pure strategy.
	 */
	for (i = 0; i < matrix.length; i++) {
		row = document.createElement('div');
		table.appendChild(row);

		cell = document.createElement('div');
		cell.setAttribute('class', 'labelaside');
		cell.appendChild(document.createTextNode
			(String.fromCharCode(97 + i)));
		row.appendChild(cell);

		sum = 0.0;
		for (j = 0; j < matrix[rorder[i]].length; j++) {
			cell = document.createElement('div');
			cell.setAttribute('class', 'mix');
			row.appendChild(cell);
			cell.appendChild
				(document.createTextNode
				 (matrix[rorder[i]][corder[j]].toFixed(2)));
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
			cell.appendChild(document.createTextNode(' ['));
			span = document.createElement('span');
			span.setAttribute('style', 'color: ' + colours[colour]);
			span.appendChild(document.createTextNode(sum.toFixed(2)));
			cell.appendChild(span);
			cell.appendChild(document.createTextNode(']'));
		}
		row.appendChild(cell);
	}

	/*
	 * The trailing row has the column averages.
	 */
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
	var game, matrix, hmatrix, e, div, ii, i, 
	    j, input, c, oc, ravg, cavg, lot, par, list, listitem;

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
	matrix = 0 == res.player.role ? 
		bimatrixCreate(game.payoffs) : 
		bimatrixCreateTranspose(game.payoffs);
	hmatrix = null;

	if (null != game.roundup) {
		/* Transpose vectors and matrix. */
		ravg = 0 == res.player.role ? 
			game.roundup.navgp1 : game.roundup.navgp2;
		cavg = 0 == res.player.role ? 
			game.roundup.navgp2 : game.roundup.navgp1;
		hmatrix = 0 == res.player.role ? 
			matrixCreate(game.roundup.navgs) : 
			matrixCreateTranspose(game.roundup.navgs);
	} 

	c = res.player.rseed % colours.length;
	oc = (0 == c % 2) ? c + 1 : c - 1;

	appendBimatrix(doClear('exprMatrix'), 1, matrix, c, oc, 
		res.roworders[res.gameorders[resindex]],
		res.colorders[res.gameorders[resindex]]);

	if (null != hmatrix) {
		doUnhide('exprHistory');
		if (0 != game.roundup.skip) 
			doUnhide('skipExplain');
		else
			doHide('skipExplain');
		lot = res.lotteries[res.expr.round - 1].plays[res.gameorders[resindex]];
		appendMatrix(doClear('exprHistoryMatrix'), hmatrix, 
			res.roworders[res.gameorders[resindex]], 
			res.colorders[res.gameorders[resindex]], 
			ravg, cavg, matrix, c);
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

function showHistory()
{
	var e, game, graph;

	e = document.getElementById('historySelectGame');
	game = e.options[e.selectedIndex].value;
	e = document.getElementById('historySelectGraph');
	graph = e.options[e.selectedIndex].value;

	if (null != shownBimatrix)
		doHideNode(shownBimatrix);
	shownBimatrix = doUnhide('bimatrix' + game);

	if (null != shownGraph)
		doHideNode(shownGraph);
	shownGraph = doUnhide(graph + game);

}

/*
 * This re-creates the graphs showing (1) payoffs as they are
 * accumulated over all rounds and (2) "instantaneous" payoffs in the
 * given round.
 * It uses the flotr2 library for drawing.
 */
function loadGraphs()
{
	var	e, c, i, j, k, l, data, datas, graph, lot, 
		avg, len, len2, matrix, sum;

	if (null == res)
		return;
	else if (res.expr.round <= 0)
		return;

	/*
	 * Begin with the accumulate payoffs.
	 * For this, we go from the first to the last round and sum the
	 * payoffs for ourselves over each game.
	 */
	e = doClear('historyGraphs');

	for (i = 0; i < res.history.length; i++) {
		c = document.createElement('div');
		e.appendChild(c);
		c.setAttribute('class', 'graphin');
		c.setAttribute('id', 'historyGraphAccumRound' + res.gameorders[i]);
		data = [];
		data.push([0, 0.0]);
		for (k = 0.0, j = 0; j < res.history[i].roundups.length; j++) {
			lot = res.lotteries[j].plays[res.gameorders[i]];
			if (null != lot)
				k += lot.poff;
			data.push([j + 1, k]);
		}
		graph = Flotr.draw(c, 
			[{ data: data }],
			{ xaxis: { tickDecimals: 0 },
			  lines: { show: true },
		          points: { show: true },
			  yaxis: { min: 0.0 }});
		doHideNode(c);
	}

	/*
	 * Next, show the instantaneous payoff for each round.
	 * This is the same as above, but without accumulating.
	 */
	for (i = 0; i < res.history.length; i++) {
		c = document.createElement('div');
		e.appendChild(c);
		c.setAttribute('class', 'graphin');
		c.setAttribute('id', 'historyGraphPerRound' + res.gameorders[i]);
		data = [];
		for (j = 0; j < res.history[i].roundups.length; j++) {
			lot = res.lotteries[j].plays[res.gameorders[i]];
			data.push([j + 1, null == lot ? 0.0 : lot.poff]);
		}
		graph = Flotr.draw(c, 
			[{ data: data }],
			{ xaxis: { tickDecimals: 0, title: 'Round' },
			  lines: { show: true },
		          points: { show: true },
			  yaxis: { min: 0.0 }});
		doHideNode(c);
	}

	/*
	 * Print the bar graph of our player's strategy.
	 */
	for (i = 0; i < res.history.length; i++) {
		c = document.createElement('div');
		e.appendChild(c);
		c.setAttribute('class', 'graphin');
		c.setAttribute('id', 'historyGraphSelfStrat' + res.gameorders[i]);
		datas = [];
		len = 0 == res.player.role ? 
			res.history[res.gameorders[i]].roundups[0].navgp1.length : 
			res.history[res.gameorders[i]].roundups[0].navgp2.length;
		for (j = 0; j < len; j++) {
			data = [];
			for (k = 0; k < res.history[res.gameorders[i]].roundups.length; k++) {
				lot = res.lotteries[k].plays[res.gameorders[i]];
				if (null == lot) {
					data.push([(k + 1), 0]);
					continue;
				}
				data.push([(k + 1), lot.stratsd[j]]);
			}
			datas.push({
				data: data, 
				label: 'Strategy ' + String.fromCharCode
					(97 + res.roworders[res.gameorders[i]][j])
			});
		}
		graph = Flotr.draw(c, datas, { 
			bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
			grid: { horizontalLines: 1 },
			xaxis: { tickDecimals: 0, title: 'Round' },
			yaxis: { max: 1.0, min: 0.0 },
			legend: { backgroundColor: '#D2E8FF' }
		});
		doHideNode(c);
	}

	/*
	 * Print the bar graph of our player population's strategy.
	 */
	for (i = 0; i < res.history.length; i++) {
		c = document.createElement('div');
		e.appendChild(c);
		c.setAttribute('class', 'graphin');
		c.setAttribute('id', 'historyGraphOwnStrat' + res.gameorders[i]);
		datas = [];
		len = 0 == res.player.role ? 
			res.history[res.gameorders[i]].roundups[0].navgp1.length : 
			res.history[res.gameorders[i]].roundups[0].navgp2.length;
		for (j = 0; j < len; j++) {
			data = [];
			for (k = 0; k < res.history[res.gameorders[i]].roundups.length; k++) {
				avg = 0 == res.player.role ? 
					res.history[res.gameorders[i]].roundups[k].navgp1 : 
					res.history[res.gameorders[i]].roundups[k].navgp2;
				data.push([(k + 1), avg[j]]);
			}
			datas.push({
				data: data, 
				label: 'Strategy ' + String.fromCharCode
					(97 + res.roworders[res.gameorders[i]][j])
			});
		}
		graph = Flotr.draw(c, datas, { 
			bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
			grid: { horizontalLines: 1 },
			xaxis: { tickDecimals: 0, title: 'Round' },
			yaxis: { max: 1.0, min: 0.0 },
			legend: { backgroundColor: '#D2E8FF' }
		});
		doHideNode(c);
	}

	/*
	 * Now print the bar graph of opponent strategy.
	 */
	for (i = 0; i < res.history.length; i++) {
		c = document.createElement('div');
		e.appendChild(c);
		c.setAttribute('class', 'graphin');
		c.setAttribute('id', 'historyGraphStrat' + res.gameorders[i]);
		datas = [];
		len = 0 == res.player.role ? 
			res.history[res.gameorders[i]].roundups[0].navgp2.length : 
			res.history[res.gameorders[i]].roundups[0].navgp1.length;
		for (j = 0; j < len; j++) {
			data = [];
			for (k = 0; k < res.history[res.gameorders[i]].roundups.length; k++) {
				avg = 0 == res.player.role ? 
					res.history[res.gameorders[i]].roundups[k].navgp2 : 
					res.history[res.gameorders[i]].roundups[k].navgp1;
				data.push([(k + 1), avg[j]]);
			}
			datas.push({
				data: data, 
				label: 'Strategy ' + String.fromCharCode(65 + j)
			});
		}
		graph = Flotr.draw(c, datas, { 
			bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
			grid: { horizontalLines: 1 },
			xaxis: { tickDecimals: 0, title: 'Round' },
			yaxis: { max: 1.0, min: 0.0 },
			legend: { backgroundColor: '#D2E8FF' }
		});
		doHideNode(c);
	}

	/*
	 * Print "hypothetical" payoffs, i.e., the expected utility from
	 * the player playing her pure strategies against the average of
	 * the opponent.
	 */
	for (i = 0; i < res.history.length; i++) {
		c = document.createElement('div');
		e.appendChild(c);
		c.setAttribute('class', 'graphin');
		c.setAttribute('id', 'historyGraphPurePoffs' + res.gameorders[i]);
		matrix = 0 == res.player.role ?
			bimatrixCreate(res.games[res.gameorders[i]].payoffs) :
			bimatrixCreateTranspose(res.games[res.gameorders[i]].payoffs);
		len = 0 == res.player.role ? 
			res.history[res.gameorders[i]].roundups[0].navgp1.length : 
			res.history[res.gameorders[i]].roundups[0].navgp2.length;
		datas = [];
		for (j = 0; j < len; j++) {
			data = [];
			sum = 0.0;
			for (k = 0; k < res.history[res.gameorders[i]].roundups.length; k++) {
				avg = 0 == res.player.role ?
					res.history[res.gameorders[i]].roundups[k].navgp2 :
					res.history[res.gameorders[i]].roundups[k].navgp1;
				sum = 0.0;
				for (l = 0; l < matrix[0].length; l++) {
					sum += avg[res.colorders[res.gameorders[i]][l]] *
						matrix[res.roworders[res.gameorders[i]][j]]
						      [res.colorders[res.gameorders[i]][l]][0];
				}
				data.push([(k + 1), sum]);
			}
			datas.push({
				data: data,
				label: 'Strategy ' + String.fromCharCode(97 + j)
			});
		}
		graph = Flotr.draw(c, datas, { 
			grid: { horizontalLines: 1 },
			xaxis: { tickDecimals: 0, title: 'Round' },
			lines: { show: true },
		        points: { show: true },
			legend: { backgroundColor: '#D2E8FF' }
		});
		doHideNode(c);
	}
}

function loadHistory(res)
{
	var gamee, e, i, j, k, child, matrix, bmatrix, c, oc, 
	    ravg, cavg, tbl, game, par, lot, list, listitem, data, graph, datas;

	loadGraphs();

	c = res.player.rseed % colours.length;
	oc = (0 == c % 2) ? c + 1 : c - 1;

	doClearReplace('historyLottery', res.aggrlottery.toFixed(2));

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
	doClearReplaceMarkup('exprFinishedWin', expr.instrWin);

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

	if (expr.round < 0) {
		/*
		 * If we haven't yet started, then simply set our timer
		 * and exit: we have nothing to show.
		 */
		next = expr.start - Math.floor(new Date().getTime() / 1000);
		doUnhide('historyNotStarted');
		e = doClear('exprCountdown');
		formatCountdown(next, e);
		setTimeout(timerCountdown, 1000, loadExpr, e, expr.start);
		doUnhide('exprNotStarted');
	} else if (expr.round < expr.rounds) {
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
		doClearReplace('exprFinishedScore', res.aggrlottery.toFixed(2));
		doClearReplace('exprFinishedTicketsMax', expr.maxtickets);
		doClearReplace('exprFinishedFinalRank', res.player.finalrank);
		doClearReplace('exprFinishedTickets', res.player.finalscore);
		v = res.player.finalrank + res.player.finalscore;
		doClearReplace('exprFinishedFinalRankEnd', v);
		doClearReplace('exprCountdown', 'finished');
		if (null == res.winner) {
			doHide('exprFinishedResults');
			doUnhide('exprFinishedWinWait');
			doHide('exprFinishedWin');
			doHide('exprFinishedLose');
			doHide('exprFinishedWinRnums');
			setTimeout(loadExpr, 1000 * 60);
		} else if (res.winner < 0) {
			doUnhide('exprFinishedResults');
			doHide('exprFinishedWinWait');
			doHide('exprFinishedWin');
			doUnhide('exprFinishedLose');
			doUnhide('exprFinishedWinRnums');
			e = doClear('exprFinishedRnums');
			for (i = 0; i < res.winrnums.length; i++)
				e.appendChild(document.createTextNode(res.winrnums[i]));
		} else {
			doUnhide('exprFinishedResults');
			doHide('exprFinishedWinWait');
			doUnhide('exprFinishedWin');
			doHide('exprFinishedLose');
			doUnhide('exprFinishedWinRnums');
			e = doClear('exprFinishedRnums');
			for (i = 0; i < res.winrnums.length; i++)
				e.appendChild(document.createTextNode(res.winrnums[i]));
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

function loadExpr() 
{

	sendQuery('@LABURI@/doloadexpr.json', 
		loadExprSetup, loadExprSuccess, 
		function(err) { location.href = '@HTURI@/playerlogin.html#loggedout'; });
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

	return(sendForm(form, null, null, 
		function() { window.location.reload(true); }));
}
