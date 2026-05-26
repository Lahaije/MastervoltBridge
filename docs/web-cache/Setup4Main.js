
//--07
var Stp4EngTx =["Setup your inverter","If your router supports WPS technology, click the button�","WPS Connect"," to start the connection process. The inverter will then try to connect to your home network."];
var Stp4GerTx =["Wechselrichterkonfiguration","Falls Ihr Router WPS unterst�tzt, klicken Sie den WPS-Knopf",""," um eine Verbindung mit Ihrem WLAN-Netzwerk herzustellen.."];
var Stp4FrnTx =["Configuration de l�onduleur","Si votre routeur supporte la technologie WPS, cliquez sur le bouton","WPS."," Connectez vous pour d�marrer le processus de connexion. L'onduleur tentera alors de se connecter au r�seau domestique."];
var Stp4SpnTx =["Configuraci�n del inversor","Si el router tiene soporte para protocolos WPS, hacer clic en el bot�n","Conexi�n WPS"," para iniciar el proceso de conexi�n. El inversor intentar� conectarse a la red dom�stica."];
var Stp4ItlTx =["Impostazione dell�inverter","Se il vostro router supporta la tecnologia WPS, cliccare il pulsante","Connect"," per avviare la procedura di connessione. L'inverter cercher� quindi di collegarsi alla vostra rete domestica."];
var Stp4DutTx =["Uw omvormer instellen","Als uw router WPS ondersteunt, kunt u nu op die manier verbinding maken. Klik op ","Verbinden met WPS"," om door te gaan."];

var WpsConnTx = ["WPS Connect","Verbinden mit WPS","Connexion WPS","Conexi�n WPS","Connessione WPS Connect","Verbinden met WPS"];

function TranslateStp4()
{
	var arrlen = Stp4EngTx.length;
	if(g_langArray[g_langIndex] == "EN")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp4id"+(i);
		document.getElementById(pgid).innerHTML = Stp4EngTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "GR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp4id"+(i);
		document.getElementById(pgid).innerHTML = Stp4GerTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "FR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp4id"+(i);
		document.getElementById(pgid).innerHTML = Stp4FrnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "SP")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp4id"+(i);
		document.getElementById(pgid).innerHTML = Stp4SpnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "IT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp4id"+(i);
		document.getElementById(pgid).innerHTML = Stp4ItlTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "DT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp4id"+(i);
		document.getElementById(pgid).innerHTML = Stp4DutTx[i];
		}
	}
	document.getElementById('Stp4WpsCon').value = WpsConnTx[g_langIndex];
	document.getElementById('Stp4bck').value = backTx[g_langIndex];
 ShowContent();
}
