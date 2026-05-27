
<!--07-->
//--------------------------Translations-----------------------------------//
var Hwerr_txt=["Hardware error","Hardware fehler","Erreur hardware","Error hardware","Errore hardware","Hardwarefout"];
var SolarHg_txt=["Solar voltage high","Solarspannung hoch","PV tension �lev�e","Solar voltaje alto","Solare voltaggio alto","Solarspanning hoog"];
var Temp_txt=["Temperature fault","Temperaturfehler","D�faut Temp�rature ","Fallo temperatura ","Errore temperature ","Temperatuurfout"];
var Nc_txt=["Not configured","Nicht konfiguriert","Non configur�","No configurado","No configurazione","Geen configuratie"];
var Insulation_txt=["Insulation fault","Isolationsfehler","D�faut isolement","Fallo Aislamiento","Errore isolamento","Isolatiefout"];
var AcFail_txt=["Grid failure","Netzfehler","D�faut AC","Fallo CA","Errore AC","Netfout"];
var SolLow_txt=["Solar voltage low","Solarspannung tief","PV tension basse","Solar voltaje baja","Solare voltaggio basso","Solarspanning laag"];
var Start_txt=["Starting...","Starten...","D�marrage...","Arrancando...","Avviamento...","Starten..."];
var Off_txt=["Off","Aus","Arr�t","Desactivado","Off","Uit"];
var On_txt=["On","An","Marche","Activado","On","Aan"];

var MntrEngTx = ["Live Monitoring","Status","Status","Reclosure time","Input","Voltage","Current","Output","Power","Power (%)","Voltage","Current","Frequency","Day","Energy","Operation time","Total","Energy","Operation time","Temperature","Inverter","Technical logs","Download logs for service purposes."];
var MntrGerTx = ["Live Monitoring","Status","Status","Einschaltzeit","Input","Spannung","Strom","Ausgang","Leistung","Leistung(%)","Spannung","Strom","Frequenz","Tag","Energie","Betriebszeit","Total","Energie","Betriebszeit","Temperatur","Wechselrichter","Technische Logs","Logdatei herunterladen zwecks Service."];
var MntrFrnTx = ["Monitoring en direct","Status","Status","Temps de reconnexion","Entr�e","Tension","Courant","Sortie","Puissance","Puissance (%)","Tension","Courant","Fr�quence","Jour","Energie","Dur�e de fonctionnement","Total","Energie","Dur�e de fonctionnement","Temp�rature ","Convertisseur","Registre technique","T�l�charger les registres � des fins de service."];
var MntrSpnTx = ["Monitorizaci�n en tiempo real","Status","Status","Hora de apagado","Entrada","Voltaje","Corriente","Salida","Potencia","Potencia (%)","Voltaje","Corriente","Frecuencia","D�a","Energ�a","Tiempo de operaci�n","Total","Energ�a","Tiempo de operaci�n","Temperatura ","Inversor","Registro t�cnico","Descargar los registros para fines de servicio."];
var MntrItlTx = ["Monitoraggio in diretta","Status","Status","Tempo di ricollegamento","Ingresso","Voltaggio","Corrente","Uscita","Potenza","Potenza (%)","Voltaggio","Corrente","Frequenza","Giorno","Energia","Tempo di funzionamento","Totale","Energia","Tempo di funzionamento","Temperature ","Invertitore","Registro tecnico","Scaricare i registri per le finalit� di servizio."];
var MntrDutTx = ["Live Monitoring","Status","Status","Reclosure tijd","Input","Spanning","Stroom","Output","Vermogen","Vermogen (%)","Spanning","Stroom","Frequentie","Dag","Energie","Bedrijfsuren","Totaal","Energie","Bedrijfsuren","Temperatuur","Omvormer","Technische logs","Download logs voor servicedoeleinden."];

var DnldBtn = ["Download","Herunterladen","T�l�charger","Descargar","Scaricare","Download"];

function TransMonitor()
{
	if(g_status1 == 8)
	document.getElementById('MonitorStat').innerHTML = Hwerr_txt[g_langIndex];
	else if(g_status1==7)
	document.getElementById('MonitorStat').innerHTML = SolarHg_txt[g_langIndex];
	else if(g_status1==6)
	document.getElementById('MonitorStat').innerHTML = Temp_txt[g_langIndex];
	else if(g_status1==5)
	document.getElementById('MonitorStat').innerHTML = Nc_txt[g_langIndex];
	else if(g_status1==4)
	document.getElementById('MonitorStat').innerHTML = Insulation_txt[g_langIndex];
	else if(g_status1==3)
	document.getElementById('MonitorStat').innerHTML = AcFail_txt[g_langIndex];
	else if(g_status1==2)
	document.getElementById('MonitorStat').innerHTML = SolLow_txt[g_langIndex];
	else if(g_status1==1)
	document.getElementById('MonitorStat').innerHTML = Start_txt[g_langIndex];
	else if(g_status0_monitor==1)
	document.getElementById('MonitorStat').innerHTML = On_txt[g_langIndex];
	else
	document.getElementById('MonitorStat').innerHTML = Off_txt[g_langIndex];
}

function TransMonitoring()
{
 var arrlen = MntrEngTx.length;
 if(g_langArray[g_langIndex] == "EN")
 {
  for(i=0;i<(arrlen);i++)
  {
   var pgid = "Mntr"+(i);
   document.getElementById(pgid).innerHTML = MntrEngTx[i];
  }
 }
 if(g_langArray[g_langIndex] == "GR")
 {
  for(i=0;i<(arrlen);i++)
  {
   var pgid = "Mntr"+(i);
   document.getElementById(pgid).innerHTML = MntrGerTx[i];
  }
 }
 if(g_langArray[g_langIndex] == "FR")
 {
  for(i=0;i<(arrlen);i++)
  {
   var pgid = "Mntr"+(i);
   document.getElementById(pgid).innerHTML = MntrFrnTx[i];
  }
 }
 if(g_langArray[g_langIndex] == "SP")
 {
  for(i=0;i<(arrlen);i++)
  {
   var pgid = "Mntr"+(i);
   document.getElementById(pgid).innerHTML = MntrSpnTx[i];
  }
 }
 if(g_langArray[g_langIndex] == "IT")
 {
  for(i=0;i<(arrlen);i++)
  {
   var pgid = "Mntr"+(i);
   document.getElementById(pgid).innerHTML = MntrItlTx[i];
  }
 }
 if(g_langArray[g_langIndex] == "DT")
 {
  for(i=0;i<(arrlen);i++)
  {
   var pgid = "Mntr"+(i);
   document.getElementById(pgid).innerHTML = MntrDutTx[i];
  }
 }
 document.getElementById('DwnLog').innerHTML = DnldBtn[g_langIndex];
 GetMonitor();
}


function GetMonitor()
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
			ParseMonitor(collect);
		}
	}
	xmlhttp.open("GET","/monitor " + Math.random(),false);
	xmlhttp.send();
}

var g_status0_monitor=0;
var g_status1;
function ParseMonitor(collect)
{
	NumOfParams=0;
	var MaxnumOfParams = 15;
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
	if(parseInt(outstring[0]) == 0)
	{
		document.getElementById('ReclosureStat').innerHTML = "-";//recl
	}
	else
	{
		document.getElementById('ReclosureStat').innerHTML =parseInt(outstring[0]);//recl
	}
	document.getElementById('IpVoltageStat').innerHTML =parseInt(outstring[1]);//v
	document.getElementById('IpCurrentStat').innerHTML =Precision(outstring[2],1);//parseInt(outstring[2]*10)/10;//i
	
	
	document.getElementById('PowerStat').innerHTML =parseInt(outstring[3]);//p
	document.getElementById('PowerPerStat').innerHTML =parseInt(outstring[4]);//p%
	document.getElementById('OpVoltageStat').innerHTML =parseInt(outstring[5]);//v
	document.getElementById('OpCurrentStat').innerHTML =Precision(outstring[6],1);//parseInt(outstring[6]*100)/100;//i
	document.getElementById('OpFreqStat').innerHTML =Precision(outstring[7],1);//parseInt(outstring[7]*100)/100;//freq
	document.getElementById('DayEnergyStat').innerHTML =Precision(outstring[8],2);//parseInt(outstring[8]*100)/100;//day energy
	var HHMM = outstring[9].split(".");
	var hh = HHMM[0]*1;
	var mm = HHMM[1].slice(0,2);
	var hhmm1 = hh+":"+mm;
	document.getElementById('DayRunTimeStat').innerHTML =hhmm1;//oper time

	document.getElementById('TotEnergyStat').innerHTML =Precision(outstring[10],2);//parseInt(outstring[10]*100)/100;//total ener
 	var HHMM = outstring[11].split(".");
	var hh = HHMM[0]*1;
	var mm = HHMM[1].slice(0,2);
	var hhmm2 = hh+":"+mm;
	document.getElementById('TotRunTimeStat').innerHTML =hhmm2;//total ener time

	document.getElementById('InvTempStat').innerHTML =parseInt(outstring[12]);//temper

	tempH=outstring[13];
	g_status0_monitor=h2d(tempH);
	templow=outstring[14];
	templow=h2d(templow);
	statusbin1=templow.toString(2);//Binary string status1

	padindex=(8-statusbin1.length);
	if(statusbin1.length<8)
	{
		for(i=1;i<=padindex;i++)
		{
			statusbin1 = '0' + statusbin1;
		}
	}
	if(statusbin1.slice(0,1)==1)
	g_status1=8;
	else if(statusbin1.slice(1,2)==1)
	g_status1=7;
	else if(statusbin1.slice(2,3)==1)
	g_status1=6;
	else if(statusbin1.slice(3,4)==1)
	g_status1=5;
	else if(statusbin1.slice(4,5)==1)
	g_status1=4;
	else if(statusbin1.slice(5,6)==1)
	g_status1=3;
	else if(statusbin1.slice(6,7)==1)
	g_status1=2;
	else if(statusbin1.slice(7,8)==1)
	g_status1=1;
	else
	g_status1=0;
	TransMonitor();
	if(IntervalMonitorFlag == 0)
	{
		IntervalMonitor = setInterval(GetMonitor, (5000));
		IntervalMonitorFlag = 1 ;
	}
	document.getElementById('content-wrapper').style.visibility='visible';
}



