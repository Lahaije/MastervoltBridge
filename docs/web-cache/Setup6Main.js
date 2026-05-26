
//--07
var Stp6EngTx =["Setup your inverter","You decided not to connect your inverter to the internet. Therefore, you won't be able use Intelliweb at this moment.","Please click"," \"Finish\" ","to finalize the setup."];
var Stp6GerTx =["Wechselrichterkonfiguration","Sie haben sich dazu entschlossen Ihren Wechselrichter nicht mit dem Internet zu verbinden. Sie k�nnen somit zur Zeit Intelliweb nicht benutzen.","Bitte klicken sie auf"," \"Abschlie�en\" ",", um die Konfiguration zu vollenden."];
var Stp6FrnTx =["Configuration de l�onduleur","Vous avez d�cid� de pas connecter votre onduleur � internet.","Vous ne pourrez donc pas utiliser Intelliweb pour le moment.","",""];
var Stp6SpnTx =["Configuraci�n del inversor","Ha decidido no conectar el inversor a internet, por lo tanto no podr� hacer uso de Intelliweb por el momento.","Haga clic en"," \"Finalizar\" ","para finalizar el proceso de configuraci�n."];
var Stp6ItlTx =["Impostazione dell�inverter","Si � deciso di non collegare il vostro inverter a internet. Non sar� possibile usare il servizio Intelliweb in questo momento.","Si prega di cliccare"," \"Finire\" ","per concludere il setup."];
var Stp6DutTx =["Uw omvormer instellen","U heeft ervoor gekozen om nu geen verbinding te maken met internet. U kunt nu geen gebruik maken van IntelliWeb.","Zodra u op"," \"Afsluiten\" ","klikt, is de installatie afgerond."];
var StpfinshTx = ["Finish","Abschlie�en","Terminez","Finalizar","Finire","Afsluiten"];

var backOflineTx = ["Back","Zur�ck","Retour","Volver","Tornare","Terug"];


function TranslateStp6()
{
	var arrlen = Stp6EngTx.length;
	if(g_langArray[g_langIndex] == "EN")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp6id"+(i);
		document.getElementById(pgid).innerHTML = Stp6EngTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "GR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp6id"+(i);
		document.getElementById(pgid).innerHTML = Stp6GerTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "FR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp6id"+(i);
		document.getElementById(pgid).innerHTML = Stp6FrnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "SP")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp6id"+(i);
		document.getElementById(pgid).innerHTML = Stp6SpnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "IT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp6id"+(i);
		document.getElementById(pgid).innerHTML = Stp6ItlTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "DT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp6id"+(i);
		document.getElementById(pgid).innerHTML = Stp6DutTx[i];
		}
	}
	document.getElementById('Stp6Fnsh').value = StpfinshTx[g_langIndex];
	document.getElementById('Stp6Bck').value = backOflineTx[g_langIndex];
 ShowContent();
}
