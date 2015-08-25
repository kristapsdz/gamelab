var ques = 0;

function doError(err) 
{

	console.log('moop');
	doValue('questionSubmit' + ques, 'Submit');
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

	if (res.questionnaire && res.answered < 7) {
		doHide('main');
		doUnhide('questionnaire');
		if (res.answered > 0) {
			doUnhide('question' + res.answered);
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
