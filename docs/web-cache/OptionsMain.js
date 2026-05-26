
//--07
var OptEngTx = ["Options","Shadow function","Power limitation","Power","Function test","Setting","Result","Setting","Result","U High","U Low","F High","F Low"];
var OptGerTx = ["Options","Schatten-Funktion","Leistungbegrenzung","Leistung","Funktionstest","Einstellungen","Ergebnis","Einstellungen","Ergebnis","U High","U Low","F High","F Low"];
var OptFrnTx = ["Options","Fonction ombre","Limitation de puissance","Power","Auto test","R�glage","R�sultat","R�glage","R�sultat","U High","U Low","F High","F Low"];
var OptSpnTx = ["Opciones","Funci�n sombra","Limitaci�n de potencia","Potencia","Prueba autom�tica","Propiedad","Resultado","Propiedad","Resultado","U High","U Low","F High","F Low"];
var OptItlTx = ["Opzioni","Funzione ombra","Limitazione di potenza","Potenza","Auto test","Impostazioni","Risultato","Impostazioni","Risultato","U High","U Low","F High","F Low"];
var OptDutTx = ["Opties","Schaduwfunctie","Vermogensbegrenzing","Vermogen","Functie test","Instelling","Resultaat","Instelling","Resultaat","U High","U Low","F High","F Low"];

var Strttxt=["Start","Start","D�marrage","Inicio","Inizio","Start"];
var failtxt=["Fail","Fehler","Erreur","Fallo","Errore","Fout"];

function TranslateOptions()
{
	var arrlen = OptEngTx.length;
	if(g_langArray[g_langIndex] == "EN")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Opt"+(i);
	  document.getElementById(pgid).innerHTML = OptEngTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "GR")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Opt"+(i);
	  document.getElementById(pgid).innerHTML = OptGerTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "FR")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Opt"+(i);
	  document.getElementById(pgid).innerHTML = OptFrnTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "SP")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Opt"+(i);
	  document.getElementById(pgid).innerHTML = OptSpnTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "IT")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Opt"+(i);
	  document.getElementById(pgid).innerHTML = OptItlTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "DT")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Opt"+(i);
	  document.getElementById(pgid).innerHTML = OptDutTx[i];
	 }
	}
	document.getElementById('saveopt').value = savetxt[g_langIndex];
	document.getElementById('startfunction').value = Strttxt[g_langIndex];
	ClearOptErr();
 GetOptions();
}

var ItalytestStatus = "0";
function GetOptions()
{
	GetFirstStatus();
	GetOptionData(2);
	GetOptionData(1);
	GetOptionData(3);
	GetOptionData(4);
	if(UserTypeid == 1)
	{
		document.OptionsFrm.enable_mxpower.disabled = true;
		document.OptionsFrm.maxpower.disabled = true;
	}
	else
	{
		document.OptionsFrm.enable_mxpower.disabled = false;
		document.OptionsFrm.maxpower.disabled = false;
	}
	document.getElementById('content-wrapper').style.visibility='visible';
}

function StartFunctionTest()
{
	document.getElementById('startfunction').disabled = true;
	document.getElementById('ft_loader').style.display = 'block';
	document.getElementById('FunctionTable').style.display = 'none';
	GetOptionData(6);
}

function GetOptionData(req)
{
	var xmlhttp;
	if (window.XMLHttpRequest)
	{	// code for IE7+, Firefox, Chrome, Opera, Safari
		xmlhttp=new XMLHttpRequest();
	}
	else
	{	// code for IE6, IE5
		xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
	}

	xmlhttp.onreadystatechange=function()
	{
		if (xmlhttp.readyState==4 && xmlhttp.status==200)
		{
			collect=xmlhttp.responseText;
			if(req == 1)
			{
				ParseShadow(collect);
			}
			if(req == 2)
			{
				ParseMppt(collect);
			}
			if(req == 3)
			{
				ParsePower(collect);
			}
			if(req == 4)
			{
				ParseCountryOptions(collect);
			}
			if(req == 5)
			{
				ParseFunction(collect);
			}
			if(req == 6)
			{
				ParseFunctionStatus(collect);
			}
		}
	}
	if(req == 1)
	{
		xmlhttp.open("GET","/shadow " + Math.random(),false);
		xmlhttp.send();
	}
	if(req == 2)
	{
		xmlhttp.open("GET","/mppt " + Math.random(),false);
		xmlhttp.send();
	}
	if(req=="3")
	{
		xmlhttp.open("GET","/power " + Math.random(),false);
		xmlhttp.send();
	}
	if(req=="4")
	{
		xmlhttp.open("GET","/cid " + Math.random(),false);
		xmlhttp.send();
	}
	if(req=="5")
	{
		xmlhttp.open("GET","/function " + Math.random(),false);
		xmlhttp.send();
	}
	if(req=="6")
	{
		xmlhttp.open("GET","/StartFunction " + Math.random(),false);
		xmlhttp.send();
	}
}

function ParseMppt(collect)
{
	NumOfParams=0;
	var MaxnumOfParams = 2;
	var outstring = new Array(MaxnumOfParams);
	var count=0;
	for(i=0;i<MaxnumOfParams;i++)
	{
		outstring[i]="";
		while(((collect.slice(count,count+1))!="\n")&&(count <= collect.length))
		{
			outstring[i] += (collect.slice(count,count+1));
			count++;
		}
		NumOfParams++;
		count++;
	}
	if(parseInt(outstring[0]) == 1)
	{
		FlagHideTxtbox = 0;
		document.OptionsFrm.enShadow.disabled=false;
	}
	else
	{
		FlagHideTxtbox = 1;
	}
}

function ParseShadow(collect)
{
	NumOfParams=0;
	var MaxnumOfParams = 2;
	var outstring = new Array(MaxnumOfParams);
	var count=0;
	for(i=0;i<MaxnumOfParams;i++)
	{
		outstring[i]="";
		while(((collect.slice(count,count+1))!="\n")&&(count <= collect.length))
		{
			outstring[i] += (collect.slice(count,count+1));
			count++;
		}
		NumOfParams++;
		count++;
	}

	tempH=Math.round(outstring[0]);
	if(tempH == "1")
	{
		document.getElementById('enShadow').checked = true;
	}
	else
	{
		document.getElementById('enShadow').checked = false;
	}
//	ToggleShadowBox();
	templow=Math.floor(outstring[1]); // divide by 60 for old webpages
//	document.OptionsFrm.shadow_interval.value = templow;
}

function ParsePower(collect)
{
	NumOfParams=0;
	var MaxnumOfParams = 1;
	var outstring = new Array(MaxnumOfParams);
	var count=0;
	for(i=0;i<MaxnumOfParams;i++)
	{
		outstring[i]="";
		while(((collect.slice(count,count+1))!="\n")&&(count <= collect.length))
		{
			outstring[i] += (collect.slice(count,count+1));
			count++;
		}
		NumOfParams++;
		count++;
	}
	document.OptionsFrm.maxpower.value =parseInt(outstring[0]);
	if(outstring[0] == MaxPower)
	{
		document.getElementById('enable_mxpower').checked = false;
	}
	else
	{
		document.getElementById('enable_mxpower').checked = true;
	}
	TogglePowerBox();
}

function ParseCountryOptions(collect)
{
	NumOfParams=0;
	var MaxnumOfParams = 1;
	var outstring = new Array(MaxnumOfParams);
	var count=0;
	for(i=0;i<MaxnumOfParams;i++)
	{
		outstring[i]="";
		while(((collect.slice(count,count+1))!="\n")&&(count <= collect.length))
		{
			CountryNameId = outstring[i] += (collect.slice(count,count+1));
			count++;
		}
		NumOfParams++;
		count++;
	}
	
	if(CountryNameId == "14")
	{
		GetOptionData(5);
		document.getElementById('FunctionTest').style.display = 'block';
	}
	else
	{
		document.getElementById('FunctionTest').style.display = 'none';
	}
}

function ToggleShadowBox()
{
	if(document.getElementById('enShadow').checked == true)
	{
		document.getElementById('intv').style.display = 'block';
	}
	else
	{
		document.getElementById('intv').style.display = 'none';
	}
}

var PowEnableFlg = 0;
function TogglePowerBox()
{
	if(document.getElementById('enable_mxpower').checked == true)
	{
		document.getElementById('maxpowdiv').style.display = 'block';
		PowEnableFlg = 1;
	}
	else
	{
		document.getElementById('maxpowdiv').style.display = 'none';
		PowEnableFlg = 0
	}
}

function Chkpower(id,evt,decchk)
{
	var piecesArray = id.value.split(".");
	var unicode= (evt.which) ? evt.which : event.keyCode;
	if (unicode < 48 || unicode > 57)
	{
		if(unicode == 8)
		{
			return true;
		}
		if(decchk == 1)
		{
			if(piecesArray.length > 1)
			{
				return false;
			}
			else if(unicode == 46)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
}

function ValidateOptions()
{
	ClearOptErr();
	if(PowEnableFlg == 1)
	{
		if(ValidateWoPopup(document.OptionsFrm.maxpower,0,MaxPower,'OptionsErr'))
		{
			submit_form(document.OptionsFrm,'\postoptions',"OptionsPage",'OptionsErr');
		}
	}
	else
	{
		if(document.getElementById('enable_mxpower').checked == false)
		{
			document.OptionsFrm.maxpower.value = MaxPower;	
		}
		submit_form(document.OptionsFrm,'\postoptions',"OptionsPage",'OptionsErr');
	}
}

function ParseFunction(collect)
{
	var Iq16Status;
	var rounddta;
	var Bindata;
	var BindataLen;

	NumOfParams=0;
	var MaxnumOfParams = 19;
	var outstring = new Array(MaxnumOfParams);
	var count=1;
	for(i=1;i<MaxnumOfParams;i++)
	{
		outstring[i]="";
		while(((collect.slice(count,count+1))!=" ")&&(count <= collect.length))
		{
			outstring[i] += (collect.slice(count,count+1));
			count++;
		}
		NumOfParams++;
		count+=2;
	}
	Iq16Status = outstring[2]*1;
	rounddta = Iq16Status;
	Bindata = rounddta.toString(2);
	BindataLen = Bindata.length;
	if(BindataLen < 8)
	{
		while(BindataLen!=8)
		{
			Bindata = 0+Bindata;
			BindataLen++;
		}
	}
	bin8 = Bindata.slice(7,8);
	bin7 = Bindata.slice(6,7);
	bin6 = Bindata.slice(5,6);
	bin5 = Bindata.slice(4,5);
	bin4 = Bindata.slice(3,4);
	bin3 = Bindata.slice(2,3);
	bin2 = Bindata.slice(1,2);
	bin1 = Bindata.slice(0,1);
	if((bin4 == "") ||(bin4 == 1))
	{
		document.getElementById('UHResult').className = 'result fail';
		document.getElementById('UHResult').innerHTML = failtxt[g_langIndex];
	}
	else if(bin8 == 1)
	{
		document.getElementById('UHResult').className = 'result';
		document.getElementById('UHResult').innerHTML = "Ok";
	}
	else
	{
		document.getElementById('UHResult').className = 'result fail';
		document.getElementById('UHResult').innerHTML = failtxt[g_langIndex];
	}
	if((bin3 == "")||(bin3 == 1))
	{
		document.getElementById('ULResult').className = 'result fail';
		document.getElementById('ULResult').innerHTML = failtxt[g_langIndex];
	}
	else if(bin7 == 1)
	{
		document.getElementById('ULResult').className = 'result';
		document.getElementById('ULResult').innerHTML = "Ok";
	}
	else
	{
		document.getElementById('ULResult').className = 'result fail';
		document.getElementById('ULResult').innerHTML = failtxt[g_langIndex];
	}
	if((bin2 == "")||(bin2 == 1))
	{
		document.getElementById('FHResult').className = 'result fail';
		document.getElementById('FHResult').innerHTML = failtxt[g_langIndex];
	}
	else if(bin6 == 1)
	{
		document.getElementById('FHResult').className = 'result';
		document.getElementById('FHResult').innerHTML = "Ok";
	}
	else
	{
		document.getElementById('FHResult').className = 'result fail';
		document.getElementById('FHResult').innerHTML = failtxt[g_langIndex];
	}
	if((bin1 == "")||(bin1 == 1))
	{
		document.getElementById('FLResult').className = 'result fail';
		document.getElementById('FLResult').innerHTML = failtxt[g_langIndex];
	}
	else if(bin5 == 1)
	{
		document.getElementById('FLResult').className = 'result';
		document.getElementById('FLResult').innerHTML = "Ok";
	}
	else
	{
		document.getElementById('FLResult').className = 'result fail';
		document.getElementById('FLResult').innerHTML = failtxt[g_langIndex];
	}
	document.getElementById('UHSet').innerHTML =parseInt(outstring[3]*10)/10;	//Uhighset
	document.getElementById('UHRes').innerHTML =parseInt(outstring[4]*10)/10;	//Uhighres
	document.getElementById('UHTimeSet').innerHTML =parseInt(outstring[5],10);	//TrptmSet
	document.getElementById('UHTimeRes').innerHTML =parseInt(outstring[6],10);	//TrptmRes

	document.getElementById('ULSet').innerHTML =parseInt(outstring[7]*10)/10;	//Ulowset
	document.getElementById('ULRes').innerHTML =parseInt(outstring[8]*10)/10;	//Ulowres
	document.getElementById('ULTimeSet').innerHTML =parseInt(outstring[9],10);	//TrptmSet
	document.getElementById('ULTimeRes').innerHTML =parseInt(outstring[10],10);	//TrptmRes

	document.getElementById('FHSet').innerHTML =parseInt(outstring[11]*100)/100;	//FHighset
	document.getElementById('FHRes').innerHTML =parseInt(outstring[12]*100)/100;	//FHighres
	document.getElementById('FHTimeSet').innerHTML =parseInt(outstring[13],10);	//FTrptmSet
	document.getElementById('FHTimeRes').innerHTML =parseInt(outstring[14],10);	//FTrptmRes

	document.getElementById('FLSet').innerHTML =parseInt(outstring[15]*100)/100;	//Flowset
	document.getElementById('FLRes').innerHTML =parseInt(outstring[16]*100)/100;	//Flowres
	document.getElementById('FLTimeSet').innerHTML =parseInt(outstring[17],10);	//FTrptmSet
	document.getElementById('FLTimeRes').innerHTML =parseInt(outstring[18],10);	//FTrptmRes
	document.getElementById('FunctionTest').style.visibility = 'visible';
	document.getElementById('ft_loader').style.display = 'none';
	document.getElementById('FunctionTable').style.display = 'block';
	document.getElementById('startfunction').disabled = false;	
}


function GetFirstStatus()
{
	var xmlhttp;
	if (window.XMLHttpRequest)
	{	// code for IE7+, Firefox, Chrome, Opera, Safari
		xmlhttp=new XMLHttpRequest();
	}
	else
	{	// code for IE6, IE5
		xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
	}
	xmlhttp.onreadystatechange=function()
	{
		if (xmlhttp.readyState==4 && xmlhttp.status==200)
		{
			collect=xmlhttp.responseText;
			if(collect == "0")
			{
				ItalytestStatus = "1";
				document.getElementById('startfunction').disabled = true;
				if(ItalyintervalFlag == 0)
				{
					Italyinterval = setInterval(GetFunctionStatus,(5000));
				}
				document.getElementById('ft_loader').style.display = 'block';
				document.getElementById('FunctionTable').style.display = 'none';
			}
		}
	}
	xmlhttp.open("GET","/Statusfunction " + Math.random(),false);
	xmlhttp.send();

}

function GetFunctionStatus()
{
	var xmlhttp;
	if(ItalytestStatus == "1")
	{
		if (window.XMLHttpRequest)
		{	// code for IE7+, Firefox, Chrome, Opera, Safari
			xmlhttp=new XMLHttpRequest();
		}
		else
		{	// code for IE6, IE5
			xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
		}
		xmlhttp.onreadystatechange=function()
		{
			if (xmlhttp.readyState==4 && xmlhttp.status==200)
			{
				collect=xmlhttp.responseText;
				if(collect == "1")
				{
					ItalytestStatus = 2;
					clearInterval(Italyinterval);
					ItalyintervalFlag = 0;
					GetOptionData(5);
				}
				else
				{
					document.getElementById('startfunction').disabled = true;
					document.getElementById('startfunction').disabled = true;
					document.getElementById('ft_loader').style.display = 'block';
					document.getElementById('FunctionTable').style.display = 'none';
				}
			}
		}
		xmlhttp.open("GET","/Statusfunction " + Math.random(),false);
		xmlhttp.send();
	}
}

function ParseFunctionStatus(collect)
{
	if(collect=="1")
	{
		if(ItalyintervalFlag == 0)
		{
			ItalytestStatus = "1";
			Italyinterval = setInterval(GetFunctionStatus,(5000));
			ItalyintervalFlag = 1;
		}
	}
}

function ClearOptErr()
{
 document.getElementById('OptionsErr').innerHTML = "";
}

