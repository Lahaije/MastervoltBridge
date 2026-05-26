
//--07
var StsEngTx = ["Status","Serial number:","MAC Address:","Output power:","Total yield:","Yield today:"];
var StsGerTx = ["Status","Serienummer:","MAC Adresse:","Ausgangsleistung:","Gesamtertrag:","Ertrag von heute:"];
var StsFrnTx = ["�tat","Num�ro de S�rie:","MAC Adress:","Puissance de sortie maximale:","Production totale:","Production � ce jour:"];
var StsSpnTx = ["Estado","N�mero de serie:","MAC Adress:","Potencia de salida:","Rendimiento total:","Rendimiento hoy:"];
var StsItlTx = ["Stato","Numero di seriale:","MAC Adress:","Massima uscita di potenza:","Rendimento totale:","Rendimento odierno:"];
var StsDutTx = ["Status","Serienummer:","MAC-adres:","Uitgangsvermogen:","Totaalopbrengst:","Opbrengst van vandaag:"];

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


function TranslateStatus()
{
	var arrlen = StsEngTx.length;
	if(g_langArray[g_langIndex] == "EN")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Sts"+(i);
	  document.getElementById(pgid).innerHTML = StsEngTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "GR")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Sts"+(i);
	  document.getElementById(pgid).innerHTML = StsGerTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "FR")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Sts"+(i);
	  document.getElementById(pgid).innerHTML = StsFrnTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "SP")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Sts"+(i);
	  document.getElementById(pgid).innerHTML = StsSpnTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "IT")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Sts"+(i);
	  document.getElementById(pgid).innerHTML = StsItlTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "DT")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "Sts"+(i);
	  document.getElementById(pgid).innerHTML = StsDutTx[i];
	 }
	}
 GetStatus();
}

var g_statusInfo;
function TransStatus()
{
	if(g_statusInfo == 8)
	document.getElementById('statustxt').innerHTML = Hwerr_txt[g_langIndex];
	else if(g_statusInfo==7)
	document.getElementById('statustxt').innerHTML = SolarHg_txt[g_langIndex];
	else if(g_statusInfo==6)
	document.getElementById('statustxt').innerHTML = Temp_txt[g_langIndex];
	else if(g_statusInfo==5)
	document.getElementById('statustxt').innerHTML = Nc_txt[g_langIndex];
	else if(g_statusInfo==4)
	document.getElementById('statustxt').innerHTML = Insulation_txt[g_langIndex];
	else if(g_statusInfo==3)
	document.getElementById('statustxt').innerHTML = AcFail_txt[g_langIndex];
	else if(g_statusInfo==2)
	document.getElementById('statustxt').innerHTML = SolLow_txt[g_langIndex];
	else if(g_statusInfo==1)
	document.getElementById('statustxt').innerHTML = Start_txt[g_langIndex];
	else if(g_status0==1)
	document.getElementById('statustxt').innerHTML = On_txt[g_langIndex];
	else
	document.getElementById('statustxt').innerHTML = Off_txt[g_langIndex];
}

function GetStatus()
{
	
	var monitor = new Array();
	var xmlhttp;
	if (window.XMLHttpRequest)
	{// code for IE7+, Firefox, Chrome, Opera, Safari
	  xmlhttp=new XMLHttpRequest();
	}
	else
	{
	// code for IE6, IE5
	xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
	}
	xmlhttp.onreadystatechange=function()
	{
		if (xmlhttp.readyState==4 && xmlhttp.status==200)
		{
			monitor=xmlhttp.responseText;
			ParseStatus(monitor);
		}
	}
	xmlhttp.open("GET","/home "+ Math.random(),false);
	xmlhttp.send();
}

var g_status0 = 0;
function ParseStatus(collect)
{
	NumOfParams=0;
	var MaxnumOfParams = 8;
	var outstring = new Array(MaxnumOfParams);
	var count=0;
	for(i=0;i<MaxnumOfParams;i++)
	{
		outstring[i]="";
		while(((collect.slice(count,count+1))!="\n")&&(count <= collect.length))
		{
			outstring[i] += (collect.slice(count,count+1));
			//alert("outstring[i]="+outstring[i] );
			count++;
		}
		NumOfParams++;
		count++;
	}
	tempH=parseInt(outstring[0]);
	g_status0=h2d(tempH);
	templow=outstring[1];
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
	{
		g_statusInfo=8;
	}
	else if(statusbin1.slice(1,2)==1)
	{
		g_statusInfo=7;
	}
	else if(statusbin1.slice(2,3)==1)
	{
		g_statusInfo=6;
	}
	else if(statusbin1.slice(3,4)==1)
	{
		g_statusInfo=5;
	}
	else if(statusbin1.slice(4,5)==1)
	{
		g_statusInfo=4;
	}
	else if(statusbin1.slice(5,6)==1)
	{
		g_statusInfo=3;
	}
	else if(statusbin1.slice(6,7)==1)
	{
		g_statusInfo=2;
	}
	else if(statusbin1.slice(7,8)==1)
	{
		g_statusInfo=1;
	}
	else
	{
		g_statusInfo=0;
	}
	StatusImage();
	TransStatus();

	if(outstring[2] == 0)
	{
		document.getElementById('model').innerHTML= "Soladin 700";
	}
	else if(outstring[2] == 1)
	{
		document.getElementById('model').innerHTML= "Soladin 1000";
	}
	else if(outstring[2] == 2)
	{
		document.getElementById('model').innerHTML= "Soladin 1500";
	}
	document.getElementById('serial').innerHTML= outstring[3];//serial
	document.getElementById('mac').innerHTML= outstring[4];//Mac
	document.getElementById('Power').innerHTML= parseInt(outstring[5]);//parseInt(outstring[5]*10/10);//power
	document.getElementById('TotalKwh').innerHTML= Precision(outstring[6],2);//(outstring[6]*10)/10;//Total
	document.getElementById('Todaykwh').innerHTML= Precision(outstring[7],2);//parseInt(outstring[7]*10)/10;//day kwh
	//document.getElementById("UpgradeTab").className = "advanced hide";
	if(IntervalHomeFlag == 0)
	{
		IntervalHome = setInterval(GetStatus, (5000));
		IntervalHomeFlag  = 1;
	}
	document.getElementById('content-wrapper').style.visibility='visible';
}

function StatusImage()
{
	if((g_statusInfo > 1)&&(g_statusInfo <9))
	{
	 document.getElementById("statusimg").src = "status_03.png";
	}
	else if(g_statusInfo == 1)
	{
	 document.getElementById("statusimg").src = "status_02.png";
	}
	else if(g_status0 == "1")
	{
	 document.getElementById("statusimg").src = "status_01.png";
	}
	else
	{
	 document.getElementById("statusimg").src = "status_03.png";
	}
}

function h2d(h) {return parseInt(h,16);} // hex to decimal

//=================End of functions===============//
