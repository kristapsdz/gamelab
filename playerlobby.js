"use strict";

var ques = 0;

var hist = [{"p1": 2, "p2": 2, "name": "test", "payoffs": [[["1", "2"], ["3", "4"]], [["5", "6"], ["7", "8"]]], "id": 1, "roundups": [{"skip": 0, "navgp1": [0.49754, 0.50246], "navgp2": [0.47656, 0.52344], "navgs": [[0.237108, 0.260432], [0.239452, 0.263008]]}, {"skip": 0, "navgp1": [0.54116, 0.45884], "navgp2": [0.49376, 0.50624], "navgs": [[0.267203, 0.273957], [0.226557, 0.232283]]}, {"skip": 0, "navgp1": [0.45408, 0.54592], "navgp2": [0.47774, 0.52226], "navgs": [[0.216932, 0.237148], [0.260808, 0.285112]]}, {"skip": 0, "navgp1": [0.53654, 0.46346], "navgp2": [0.4868, 0.5132], "navgs": [[0.261188, 0.275352], [0.225612, 0.237848]]}, {"skip": 0, "navgp1": [0.44534, 0.55466], "navgp2": [0.48886, 0.51114], "navgs": [[0.217709, 0.227631], [0.271151, 0.283509]]}, {"skip": 0, "navgp1": [0.4764, 0.5236], "navgp2": [0.47446, 0.52554], "navgs": [[0.226033, 0.250367], [0.248427, 0.275173]]}, {"skip": 0, "navgp1": [0.54806, 0.45194], "navgp2": [0.53366, 0.46634], "navgs": [[0.292478, 0.255582], [0.241182, 0.210758]]}, {"skip": 0, "navgp1": [0.41504, 0.58496], "navgp2": [0.53906, 0.46094], "navgs": [[0.223731, 0.191309], [0.315329, 0.269631]]}, {"skip": 0, "navgp1": [0.53898, 0.46102], "navgp2": [0.5087, 0.4913], "navgs": [[0.274179, 0.264801], [0.234521, 0.226499]]}, {"skip": 0, "navgp1": [0.55794, 0.44206], "navgp2": [0.55394, 0.44606], "navgs": [[0.309065, 0.248875], [0.244875, 0.197185]]}]}];

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

function loadGraphs()
{
	var	e, c, i, j, k, l, m, data, datas, lot, 
		avg, len, matrix, sum, sub, gameidx, stratidx;

	e = doClear('historyLineGraphs');

	for (i = 0; i < hist.length; i++) {
		gameidx = i;
		sub = document.createElement('div');
		sub.setAttribute('id', 'historyLineGraphs' + gameidx);
		e.appendChild(sub);

		matrix = bimatrixCreate(hist[gameidx].payoffs);

		c = document.createElement('div');
		sub.appendChild(c);
		len = hist[gameidx].roundups[0].navgp1.length;
		datas = [];
		for (j = 0; j < len; j++) {
			data = [];
			/* Start with zero! */
			data.push([0, 0.0]);
			sum = 0.0;
			k = 0;
			l = k;
			for (k = 0; k < hist[gameidx].roundups.length; k++) {
				avg = hist[gameidx].roundups[k].navgp2;
				for (sum = 0.0, m = 0; m < matrix[0].length; m++)
					sum += avg[m] * matrix[j][m][0];
				data.push([(k + l) + 1, sum]);
			}
			datas[j] = {
				data: data,
				label: 'Row ' + String.fromCharCode(97 + j)
			};
		}
		Flotr.draw(c, datas, 
			{ grid: { horizontalLines: 1 },
			  xaxis: { ticks: [[ 0, 'oldest' ], [(l + k), 'newest']] },
		          shadowSize: 0,
			  subtitle: 'Previous row payoffs',
		          yaxis: { min: 0.0 },
			  lines: { show: true },
		          points: { show: true }});
	}

	e = doClear('historyBarGraphs');

	for (i = 0; i < hist.length; i++) {
		gameidx = i;
		sub = document.createElement('div');
		sub.setAttribute('id', 'historyBarGraphs' + gameidx);
		e.appendChild(sub);

		c = document.createElement('div');
		sub.appendChild(c);
		datas = [];
		len = hist[gameidx].roundups[0].navgp1.length;
		for (l = k = j = 0; j < len; j++) {
			stratidx = j;
			data = [];
			k = 0;
			l = k;
			for (k = 0; k < hist[gameidx].roundups.length; k++) {
				avg = hist[gameidx].roundups[k].navgp1;
				data.push([(l + k), avg[stratidx]]);
			}
			datas[j] = {
				data: data, 
				label: 'Row ' + String.fromCharCode(97 + j)
			};
		}
		Flotr.draw(c, datas, { 
			bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
			grid: { horizontalLines: 1 },
		        subtitle: 'Previous row',
			xaxis: { ticks: [[ 0, 'oldest' ], [(l + k) - 1, 'newest']] },
			yaxis: { max: 1.0, min: 0.0 }
		});

		c = document.createElement('div');
		sub.appendChild(c);
		datas = [];
		len = hist[gameidx].roundups[0].navgp2.length;
		for (k = l = j = 0; j < len; j++) {
			stratidx = j;
			data = [];
			k = 0;
			l = k;
			for (k = 0; k < hist[gameidx].roundups.length; k++) {
				avg = hist[gameidx].roundups[k].navgp2;
				data.push([(l + k), avg[stratidx]]);
			}
			datas[j] = {
				data: data, 
				label: 'Column ' + String.fromCharCode(65 + j)
			};
		}
		Flotr.draw(c, datas, { 
			bars: { show: true , shadowSize: 0, stacked: true, barWidth: 1.0, lineWidth: 1 }, 
			grid: { horizontalLines: 1 },
			xaxis: { ticks: [[ 0, 'oldest' ], [(l + k) - 1, 'newest']] },
		        subtitle: 'Previous column',
			yaxis: { max: 1.0, min: 0.0 }
		});
	}
}

function doError(err) 
{

	doValue('questionSubmit' + ques, 'Submit');
	doUnhide('questionerror' + ques);
}

function doErrorPlay(err) 
{

	doValue('questionSubmit' + ques, 'Submit play');
	doUnhide('questionerror' + ques);
}

function doSuccess() 
{

	doValue('questionSubmit' + ques, 'Success!');
	doHide('question' + ques);
	doUnhide('question' + ques + '-correct');
}

function doSetup() 
{
	doHide('questionerror' + ques);
	doValue('questionSubmit' + ques, 'Submitting...');
}

function startQuestions()
{

	doUnhide('question0');
	doHide('question-1');
}

function doInitSuccess(resp)
{
	var res;

	try  { 
		res = JSON.parse(resp);
	} catch (error) {
		return;
	}

	doUnhide('ploaded');
	doHide('ploading');

	ques = res.answered;

	if (res.questionnaire && res.answered < 8) {
		doHide('main');
		doUnhide('questionnaire');
		if (res.answered > 0) {
			doUnhide('question' + res.answered);
			if (res.answered == 7)
				loadGraphs(history);
		} else {
			doUnhide('question-1');
		}
	} else {
		doUnhide('main');
		doHide('questionnaire');
		setTimeout(function(){location.href = '@HTURI@/playerhome.html'; }, 10000);
	}
}

function doInitFailure()
{

	location.href = '@HTURI@/playerlogin.html';
}

function doInitSetup()
{

	doHide('ploaded');
	doUnhide('ploading');
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
