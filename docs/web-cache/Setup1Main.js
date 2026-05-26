
//--07
var Stp1EngTx = ["Setup your inverter","Connect your inverter to the internet to enjoy Mastervolt Intelliweb. This will give you helpful insight in the status and output of your system.","Connect this inverter to my Wi-Fi network. (recommended)","Connect this inverter to my Wi-Fi network using WPS","Don't connect this inverter to the internet now."];
var Stp1GerTx = ["Wechselrichterkonfiguration","Verbinden Sie Ihren Wechselrichter mit dem Internet um Mastervolt Intelliweb nutzen zu k�nnen. Dort k�nnen Sie den Status und die Ertr�ge ihres Wechselrichters einsehen.","Den Wechselrichter mit Ihrem WLAN-Netzwerk verbinden. (empfohlen)","Den Wechselrichter mittels WPS mit ihrem WLAN-Netzwerk verbinden.","Den Wechselrichter nocht nicht mit dem Internet verbinden."];
var Stp1FrnTx = ["Configuration de l�onduleur","Connectez votre onduleur � internet afin de profiter de Mastervolt Intelliweb. Cela vous donnera une vue d'ensemble du statut et de la production de votre syst�me.","Connectez cet onduleur � mon r�seau Wi-Fi (recommand�)","Connectez cet onduleur � mon r�seau Wi-Fi en utilisant WPS","Ne pas connecter cet onduleur � internet maintenant"];
var Stp1SpnTx = ["Configuraci�n del inversor","Conectar el inversor a internet para poder acceder a Mastervolt Intelliweb. Esto le proporcionar� una visi�n m�s profunda acerca del estado y el rendimiento de su sistema.","Conectar este inversor a mi red Wi-Fi (recomendado).","Conectar este inversor a mi red Wi-Fi mediante WPS.","No conectar este inversor a internet por el momento."];
var Stp1ItlTx = ["Impostazione dell�inverter","Collegare l'inverter a internet per usufruire del Mastervolt Intelliweb. Ci� permette di accedere a un'utile panoramica generale dello stato e della portata del vostro sistema.","Collegare questo inverter alla mia rete Wi-Fi (opzione raccomandata).","Collegare questo inverter alla mia rete Wi-Fi utilizzando WPS","Non collegare questo inverter a internet ora."];
var Stp1DutTx = ["Uw omvormer instellen","Verbind uw omvormer met het internet om gebruik te maken van Mastervolt Intelliweb. Bekijk de status en opbrengst van uw systeem online.","Maak verbinding met uw Wi-Fi netwerk. (aanbevolen)","Maak via WPS verbinding met mijn Wi-Fi netwerk.","Verbind de omvormer nu niet met internet."];

var backTx = ["Back","Zur�ck","Retour","Volver","Tornare","Vorige"];


function TranslateStp1()
{
	var arrlen = Stp1EngTx.length;
	if(g_langArray[g_langIndex] == "EN")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp1id"+(i);
		document.getElementById(pgid).innerHTML = Stp1EngTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "GR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp1id"+(i);
		document.getElementById(pgid).innerHTML = Stp1GerTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "FR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp1id"+(i);
		document.getElementById(pgid).innerHTML = Stp1FrnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "SP")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp1id"+(i);
		document.getElementById(pgid).innerHTML = Stp1SpnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "IT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp1id"+(i);
		document.getElementById(pgid).innerHTML = Stp1ItlTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "DT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp1id"+(i);
		document.getElementById(pgid).innerHTML = Stp1DutTx[i];
		}
	}
	document.getElementById('Stp1Next').value = NextTx[g_langIndex];
	document.getElementById('Stp1Back').value = backTx[g_langIndex];
 GetNormalStatus();
}
function SubmitMode()
{
	submit_form(document.ConnOrSkip,'/conskip' , "NormalMode",'Dummy');
}

function CheckSelected()
{
	if(document.getElementById('online').checked == true)
	{
		GetMainData(2); //Get the Wifi Settings Page
	}
	else if(document.getElementById('offline').checked == true)
	{
		GetMainData(6); //Get the Remain Offline Page
	}
	else
	{
		GetMainData(4); //Get the WPS Settings Page
	}
	//document.getElementById('content-wrapper').style.visibility='visible';
}

function Prev()
{
	GetMainData(0);
}

function GetNormalStatus()
{
	var xmlhttp;
	if (window.XMLHttpRequest)
	{
		xmlhttp=new XMLHttpRequest();
	}
	else
	{
		xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
	}
	xmlhttp.onreadystatechange=function()
	{
		if (xmlhttp.readyState==4 && xmlhttp.status==200)
		{
			collect=xmlhttp.responseText;
			ParseGetNormalStatus(collect);
		}
	}
	xmlhttp.open("GET","/CommunityOn "+ Math.random(),true);
	xmlhttp.send();
}

function ParseGetNormalStatus(collect)
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
	if(outstring[0] == 0)
	{
		radiobtn = document.getElementById("offline");
		radiobtn.checked = true;
	}
	else if(outstring[0] == 1)
	{
		radiobtn = document.getElementById("wps");
		radiobtn.checked = true;
	}
	else
	{
		radiobtn = document.getElementById("online");
		radiobtn.checked = true;
	}
	ShowContent();
}

