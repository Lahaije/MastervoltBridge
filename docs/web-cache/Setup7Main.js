
//--07
var Stp7EngTx = ["Ready","Your inverter will start delivering power to the grid and has closed its wireless network.","If you later wish to connect your inverter to the online Solar Monitor, you can return to this page:","Press the button on the inverter repeatedly until the Wi-Fi LED blinks slowly","Connect to your inverter's wireless network","Open your webbrowser and type in http://10.0.0.1"];
var Stp7GerTx = ["Fertig","Ihr Wechselrichter wird nun beginnen Leistung ins Netz einzuspei�en. Das WLAN-Netzwerk des Wechselrichters wurde deaktiviert.","Wenn Sie zu einem sp�teren Zeitpunkt w�nschen Ihren Wechselrichter mit Intelliweb zu verkn�pfen, k�nnen Sie auf diese Seite zur�ckkehren:","Dr�cken Sie hierzu mehrmals den WLAN-Knopf Ihres Wechselrichters, bis die WLAN-LED beginnt langsam zu blinken.","Verbinden Sie sich mit dem WLAN-Netzwerk Ihres Wechselrichters.","�ffnen Sie http://10.0.0.1 in Ihrem Webbrowser."];
var Stp7FrnTx = ["Pr�t","Votre onduleur va commencer � injecter de la puissance dans le r�seau et a ferm� son r�seau sans fil.","Si vous souhaitez connecter plus tard votre onduleur en ligne sur le Solar Monitor, vous pouvez retourner � cette page.","Appuyez sur le bouton sur l'onduleur de fa�on r�p�t�e jusqu'� ce que le voyant Wi-Fi clignote lentement. ","Connectez-vous sur votre r�seau sans fil onduleur.","Ouvrez votre navigateur et tapez http://10.0.0.1"];
var Stp7SpnTx = ["Preparado","El inversor comenzar� a suministrar energ�a a la red y ha cerrado su red Wi-Fi.","Si desea conectarse m�s tarde a Solar Monitor online, puede regresar a esta p�gina:","Presionar el bot�n del inversor varias veces hasta que el LED Wi-Fi parpadee lentamente.","Con�ctese a la red inal�mbrica del inversor.","Abra un navegador y teclee http://10.0.0.1"];
var Stp7ItlTx = ["Pronto","L'inverter comincer� a dare potenza alla rete di alimentazione e ha chiuso la propria rete wireless.","Se, successivamente, si desidera collegare l'inverter al Monitor Solare online, � possibile tornare a questa pagina:","Premere il pulsante sull'inverter ripetutamente fino a quando il LED del Wi-Fi lampeggia lentamente","Collegare l'invertere alla rete wireless","Aprire il navigatore e digitare http://10.0.0.1"];
var Stp7DutTx = ["Klaar","Uw omvormer zal nu electriciteit gaan leveren aan het net. Het WiFi netwerk wordt uitgeschakeld.","Als u op een later moment gebruik wilt gaan maken van monitoring via Intelliweb, kunt u dat op deze pagina instellen:","Druk herhaaldelijk op de WiFi-knop op de Soladin, totdat de de WiFi-LED 1 keer knippert.","Verbind met het Soladin Wifi netwerk","Ga met uw browser naar http://10.0.0.1/"];

function TranslateStp7()
{
	var arrlen = Stp7EngTx.length;
	if(g_langArray[g_langIndex] == "EN")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp7id"+(i);
		document.getElementById(pgid).innerHTML = Stp7EngTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "GR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp7id"+(i);
		document.getElementById(pgid).innerHTML = Stp7GerTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "FR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp7id"+(i);
		document.getElementById(pgid).innerHTML = Stp7FrnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "SP")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp7id"+(i);
		document.getElementById(pgid).innerHTML = Stp7SpnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "IT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp7id"+(i);
		document.getElementById(pgid).innerHTML = Stp7ItlTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "DT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp7id"+(i);
		document.getElementById(pgid).innerHTML = Stp7DutTx[i];
		}
	}
 ShowContent();
 WiFiOff();
}

function WiFiOff()
{
	HideMenu();
	GetSaveCountry("WiFiOFF");
}

function GetWifiOff()
{
	var logsetting;
	var data;
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
		}
	}
	xmlhttp.open("GET","/wifioff " + Math.random(),true);
	xmlhttp.send();
}

