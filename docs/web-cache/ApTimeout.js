
//--7
var ApTimeoutEngTx = ["Session timeout","If you wish to open this page again, press the button repeatedly until the WiFi LED blinks 1 time. Reconnect to the Soladin network first and then type ","in your browser."];
var ApTimeoutGerTx = ["Zeitlimit �berschritten, Konfiguration abgebrochen","Wenn Sie w�nschen diese Seite erneut zu �ffnen, dr�cken Sie die WLAN-Taste merhmals, bis die Status-LED einmal blinkt, verbinden Sie sich nun mit dem Soladin WLAN-Netzwerk und �ffnen "," in Ihrem Webbrowser."];
var ApTimeoutFrnTx = ["Session expir�e","Si vous souhaitez ouvrir cette page � nouveau, appuyez sur le bouton de fa�on r�p�t�e jusqu'� ce que le voyant Wi-Fi clignote 1 fois. Reconnectez-vous au r�seau Soladin d'abord et tapez ensuite "," sur votre navigateur. "];
var ApTimeoutSpnTx = ["La sesi�n ha expirado","Si quiere volver a abrir esta p�gina, presione el bot�n varias veces hasta que el LED Wi-Fi parpadee 1 vez. Reconectar en primer lugar a la red Soladin y a continuaci�n teclear "," en la barra de direcciones."];
var ApTimeoutItlTx = ["Sessione scaduta","Se si desidera aprire nuovamente questa pagina, premere ripetutamente il pulsante fino a quando il LED del Wi-Fi lampeggia 1 volta. Ricollegarsi alla rete Soladin e quindi digitare "," nel vostro navigatore."];
var ApTimeoutDutTx = ["Sessie verlopen","Als u de deze pagina opnieuw wilt openen, druk herhaaldelijk op de WiFi-knop op de omvormer, totdat de de WiFi-LED 1 keer knippert. Verbind daarna met het Soladin Wifi netwerk en ga met uw browser naar ",""];

function TranslateAptmot()
{
	var arrlen = ApTimeoutEngTx.length;
	if(g_langArray[g_langIndex] == "EN")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Aptmot"+(i);
		document.getElementById(pgid).innerHTML = ApTimeoutEngTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "GR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Aptmot"+(i);
		document.getElementById(pgid).innerHTML = ApTimeoutGerTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "FR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Aptmot"+(i);
		document.getElementById(pgid).innerHTML = ApTimeoutFrnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "SP")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Aptmot"+(i);
		document.getElementById(pgid).innerHTML = ApTimeoutSpnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "IT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Aptmot"+(i);
		document.getElementById(pgid).innerHTML = ApTimeoutItlTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "DT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Aptmot"+(i);
		document.getElementById(pgid).innerHTML = ApTimeoutDutTx[i];
		}
	}
 }
 

