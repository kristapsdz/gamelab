"use strict";

function sendForm(oFormElement, setup, error, success) 
{
	var xmlhttp = new XMLHttpRequest();
	var formdata = new FormData(oFormElement);

	setup();

	xmlhttp.onreadystatechange=function() {
		if (xmlhttp.readyState==4 && xmlhttp.status==200)
			success(xmlhttp.responseText);
		else if (xmlhttp.readyState == 4)
			error(xmlhttp.status);
	} 

	xmlhttp.open(oFormElement.method, oFormElement.action, true);
	xmlhttp.send(formdata);
	return(false);
}
