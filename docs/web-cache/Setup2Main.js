
//--07
var Stp2EngTx =["Setup your inverter","Choose your Wi-Fi network.","Wi-Fi Network","Select a Wi-Fi network to connect:","Manual","Network name�","Password","Show characters","Get IP Address","Automatic","Static IP Address","IP Address","Subnet Mask","Gateway","Primary DNS","Secondary DNS","That's all. When you click�","Save & Connect","�you will leave these pages and the inverter will connect to the selected network."];
var Stp2GerTx =["Wechselrichterkonfiguration","W�hlen sie ihr WLAN-Netzwerk.","WLAN-Netzwerk","Verbinden mit Wi-Fi-Netzwerk:","Manuell","Name des WLAN-Netzwerkes (SSID)","Netzwerkschl�ssel","Zeichen sichtbar machen","IP-Adresse konfigurieren","Automatisch","Static IP Address","IP-Adresse","Subnet Mask","Gateway","Prim�re DNS","Secund�re DNS","Das ist alles. Sobald sie mit ","Speichern & Verbinden","die Konfiguration abschlie�en, verbindet sich Ihr Wechselrichter mit dem Internet und diese Seite wird geschlossen."];
var Stp2FrnTx =["Configuration de l�onduleur","S�lec. r�s. Wi-Fi auquel se connect:","R�seau Wi-Fi","S�lec. r�s. Wi-Fi auquel se connect:","Manuel","Nom du r�seau (SSID)","Mot de passe","Afficher les caract�res","R�cup�rez l'adresse IP","Automatique","Static IP Address","IP Address","Subnet Mask","Gateway","Primaire DNS","Secondaire DNS","C'est tout. Lorsque vous cliquez sur ","Save & Connect","vous quitterez ces pages et l'onduleur se connectera au r�seau s�lectionn�"];
var Stp2SpnTx =["Configuraci�n del inversor","Seleccione una red Wi-Fi para conectar:","Red Wi-Fi","Seleccione una red Wi-Fi para conectar:","Manual","Nombre de red (SSID)","Contrase�a","Mostrar caracteres","Obtener direcci�n IP","Autom�tico","Static IP Address","IP Address","Subnet Mask","Gateway","Primaria DNS","Secondaria DNS","Esto es todo. Cuando haga clic en ","Guardar y Conectar","abandonar� esta p�gina y el inversor se conectar� a la red seleccionada."];
var Stp2ItlTx =["Impostazione dell�inverter","Seleziona rete Wi-Fi per connessione:","Rete Wi-Fi","Seleziona rete Wi-Fi per connessione:","Manuale","Nome rete (SSID)","Password","Mostra caratteri","Reperimento indirizzo IP","Automatico","Static IP Address","IP Address","Subnet Mask","Gateway","Primaria DNS","Secondaria DNS","� tutto. Cliccando su ","Salvare e Collegare","si esce dalle presenti pagine e l'inverter si collega alla rete selezionata."];
var Stp2DutTx =["Uw omvormer instellen","Kies een Wi-Fi netwerk","Wi-Fi netwerk","Selecteer een Wi-Fi netwerk","Handmatig","Netwerknaam (SSID)","Wachtwoord","Toon karakters","IP-adres configureren","Automatisch","Static IP Address","IP adres","Subnet mask","Gateway","Primaire DNS","Secundaire DNS","Dat is alles! Als u op ","Opslaan & Verbinden","klikt, verlaat u deze pagina en zal de omvormer verbinding maken met internet."];

var selnwtx = [Stp2EngTx[3],Stp2GerTx[3],Stp2FrnTx[3],Stp2SpnTx[3],Stp2ItlTx[3],Stp2DutTx[3]];
var manualtx = [Stp2EngTx[4],Stp2GerTx[4],Stp2FrnTx[4],Stp2SpnTx[4],Stp2ItlTx[4],Stp2DutTx[4]];
var saveConnTx =["Save & Connect","Speichern & Verbinden","Sauvegarder & Connecter","Guardar y Conectar","Salvare e Collegare","Opslaan & Verbinden"];
var NwnamevalidTx =["Please enter a network name","Bitte w�hlen sie einen Netzwerknamen","Saisir un nom de r�seau","Introducir el nombre de la red","Si prega di inserire il nome di una rete","Voer een netwerknaam in"];

function TranslateStp2()
{
	var arrlen = Stp2EngTx.length;
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp2id"+(i);
		if(((i > 2) && (i <= 3))||((i >= 9) && (i <= 10)))
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp2EngTx[(i)];
		}
		else
		document.getElementById(pgid).innerHTML = Stp2EngTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "GR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp2id"+(i);
		if(((i > 2) && (i <= 3))||((i >= 9) && (i <= 10)))
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp2GerTx[(i)];
		}
		else
		document.getElementById(pgid).innerHTML = Stp2GerTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "FR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp2id"+(i);
		if(((i > 2) && (i <= 3))||((i >= 9) && (i <= 10)))
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp2FrnTx[(i)];
		}
		else
		document.getElementById(pgid).innerHTML = Stp2FrnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "SP")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp2id"+(i);
		if(((i > 2) && (i <= 3))||((i >= 9) && (i <= 10)))
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp2SpnTx[(i)];
		}
		else
		document.getElementById(pgid).innerHTML = Stp2SpnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "IT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp2id"+(i);
		if(((i > 2) && (i <= 3))||((i >= 9) && (i <= 10)))
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp2ItlTx[(i)];
		}
		else
		document.getElementById(pgid).innerHTML = Stp2ItlTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "DT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp2id"+(i);
		if(((i > 2) && (i <= 3))||((i >= 9) && (i <= 10)))
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp2DutTx[(i)];
		}
		else
		document.getElementById(pgid).innerHTML = Stp2DutTx[i];
		}
	}
	document.getElementById('saveConnNw').value = saveConnTx[g_langIndex];
	document.getElementById('SaveNw').value = savetxt[g_langIndex];
	document.getElementById('BackNw').value = backTx[g_langIndex];
	ShowContent();
	ClearNwerr();
	GetDataNw();
}

function ScanSSID()//1-First Time  2-Scan button
{
	document.getElementById('refresh_wifi').className = 'refresh_wifi loading';
	GetSsidScan(2);
}

function GetDataNw()
{
	var xmlhttp;
	if(UserTypeid == 1)
	{
		document.getElementById('SaveNw').style.display = 'none';		
	}
	else
	{
		document.getElementById('SaveNw').style.display = 'block';
	}
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
			ParseNwData(collect);
		}
	}
	xmlhttp.open("GET","/ap "+ Math.random(),false);
	xmlhttp.send();
}

function GetSsidScan(req)
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
			ParseSsidScan(collect);
			if(req == 1)
			{
				PostScan();
				document.getElementById('refresh_wifi').className = '';
			}
			else if(req == 2)
			{
			  document.getElementById("wifinetwork").value = "selectnw";
			  CheckManual(2);//Disable Buttons and Select NW on Scan Remove SSID and PWD
			  document.getElementById('refresh_wifi').className = '';
			}
			document.getElementById('refresh_wifi').className = '';
		}
	}
	xmlhttp.open("GET","/ssidscan "+ Math.random(),true);
	xmlhttp.send();
}

var outstring = new Array(MaxnumOfParams);
var MaxnumOfParams = 25;
function ParseNwData(collect)
{
NumOfParams=0;
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
document.NW.IP1.value =outstring[3];
document.NW.IP2.value =outstring[4];
document.NW.IP3.value =outstring[5];
document.NW.IP4.value =outstring[6];
document.NW.SNMASK1.value =outstring[7];
document.NW.SNMASK2.value =outstring[8];
document.NW.SNMASK3.value =outstring[9];
document.NW.SNMASK4.value =outstring[10];
document.NW.GW1.value =outstring[11];
document.NW.GW2.value =outstring[12];
document.NW.GW3.value =outstring[13];
document.NW.GW4.value =outstring[14];
document.NW.PD1.value =outstring[15];
document.NW.PD2.value =outstring[16];
document.NW.PD3.value =outstring[17];
document.NW.PD4.value =outstring[18];
document.NW.SD1.value =outstring[19];
document.NW.SD2.value =outstring[20];
document.NW.SD3.value =outstring[21];
document.NW.SD4.value =outstring[22];
if(outstring[23] == "1")
{
	document.NW.StaticIp.value = "on";
	document.getElementById('DHCP').style.display = 'none';
}
else
{
	document.NW.StaticIp.value = 0;
	document.getElementById('DHCP').style.display = 'inline';
}
document.getElementById('refresh_wifi').className = 'refresh_wifi loading';
GetSsidScan(1);
}

function PostScan()
{
	//check manual
	if(outstring[24] == "1")
	{
	  document.getElementById("wifinetwork").value = "manual";
	  if(outstring[0] == "")
	  {
		ClearSSIDPwd();
	  }
	  else
	  {
		document.NW.SSID.value = outstring[0];
		document.NW.PASSWORD.value =outstring[2];//password
		document.NW.wifipassword.value = outstring[2];
		document.NW.wifipassword_text.value = outstring[2];
	  }
	}
	else
	{
	 if(outstring[0] == "")
	  {
		ClearSSIDPwd();
	  }
	  else
	  {
		//code to select the ssid frm the dropdown
		count = 0;
		while(count <= LengthDropdown)
		{
			if(document.NW.wifinetwork.options[count].value == outstring[0])
			{
				document.getElementById("wifinetwork").value = outstring[0];
				document.NW.SSID.value = outstring[0];
				document.NW.PASSWORD.value =outstring[2];//password
				document.NW.wifipassword.value = outstring[2];
				document.NW.wifipassword_text.value = outstring[2];
				count = 0;
				break;
			}
			count++;
			//if(count == LengthDropdown)
			//{
				//SSID not found in scan
			//	ClearSSIDPwd();
			//}
		}
	  }
	}
	CheckManual(1);//1-First Time
	document.getElementById('content-wrapper').style.visibility='visible';
}

var LengthDropdown = 0;//update in case of duplication of SSID
function ParseSsidScan(collect)
{
	var MaxSsid = "";
	var count=0;
	var NumOfParams=0;
	while(((collect.slice(count,count+1))!="\n")&&(count <= collect.length))
	{
		MaxSsid += (collect.slice(count,count+1));
		count++;
	}
	count++;
	if(MaxSsid > 0)
	{
		var scanssid = new Array(MaxSsid);
		var scanstrngth = new Array(MaxSsid);
		for(i=0;i<MaxSsid;i++)
		{
			scanssid[i]="";
			while(((collect.slice(count,count+1))!="\n")&&(count <= collect.length))
			{
				scanssid[i] += (collect.slice(count,count+1));
				count++;
			}
			count++;
			scanstrngth[i] = "";
			while(((collect.slice(count,count+1))!="\n")&&(count <= collect.length))
			{
				scanstrngth[i] += (collect.slice(count,count+1));
				count++;
			}
			count++;
		}
		for(i=0;i < MaxSsid;i++)
		{
			k=0;
			var tmpstrngth = new Array();
			for(j = i+1 ;j < MaxSsid;j++)
			{
				tmpstrngth[k] = "";
				if(scanssid[i] == scanssid[j])
				{
					tmpstrngth[k] += scanstrngth[j];
					scanssid.splice(j, 1);
					scanstrngth.splice(j, 1);
					MaxSsid--;
					j--;
					k++;
				}
			}
			//alert(scanstrngth.max);
			//if((tmpstrngth!= "")||(tmpstrngth!= "undefined"))
			if(k>0)
			{
				scanstrngth[i] =  Math.max.apply(Math, tmpstrngth);
				k = 0;
			}
		}
		MaxSsid = scanssid.length;
		LengthDropdown = scanssid.length;
	}
	removeAllOptions(document.NW.wifinetwork);
	addOption(document.NW.wifinetwork,"selectnw",selnwtx[g_langIndex] ,"Stp2id3");//change as per html
	document.NW.wifinetwork.value="Select a network...";
	for (var i=0; i < MaxSsid;++i)
	{
		addOption(document.NW.wifinetwork,scanssid[i],scanssid[i]+" (" + scanstrngth[i]+"%)","");
	}
	addOption(document.NW.wifinetwork,"manual",manualtx[g_langIndex],"Stp2id4");
}

function removeAllOptions(selectbox)
{
	var i;
	for(i=selectbox.options.length-1;i>=0;i--)
	{
		selectbox.remove(i);
	}
}
function addOption(selectbox, value, text , id)
{
	var optn = document.createElement("OPTION");
	optn.text = text;
	optn.value = value;
	optn.id = id;
	selectbox.options.add(optn);
}

function CheckManual(req)
{
	if(req == 2) //OnChange Clear Password and SSID
	{
		ClearSSIDPwd();
	}
	if(document.NW.wifinetwork.value == "manual")
	{
		document.getElementById('ManualSSID').style.display = 'inline';
	}
	else
	{
		document.getElementById('ManualSSID').style.display = 'none';
	}
	if(document.NW.wifinetwork.value == "selectnw")
	{
		document.NW.SaveNWConnect.disabled = true;
		document.NW.SaveNW.disabled = true;
	}
	else
	{
		document.NW.SaveNWConnect.disabled = false;
		document.NW.SaveNW.disabled = false;
	}
}

function TogglePassword()
{
	if(document.getElementById('show_chars').checked == true)
	{
		document.getElementById('wifipassword').style.display = 'none';
		document.getElementById('wifipassword_text').style.display = 'inline';
		document.NW.wifipassword_text.value = document.NW.wifipassword.value ;
	}
	else
	{
		document.NW.wifipassword.value = document.NW.wifipassword_text.value;
		document.getElementById('wifipassword').style.display = 'inline';
		document.getElementById('wifipassword_text').style.display = 'none';
	}
}

var StaticIpFlg = 0;
function ToggleDHCP()
{
	if(document.NW.StaticIp.value == "on")
	{
		StaticIpFlg = 1 ; 
		document.getElementById('DHCP').style.display = 'none';
	}
	else
	{
		StaticIpFlg = 0;
		document.getElementById('DHCP').style.display = 'inline';
	}
}

function ValidateNetwork(lreq)
{
	ClearNwerr();
	var data=document.NW.SSID.value;
	if(document.NW.wifinetwork.value == "manual")
	{
		if(data == "")
		{
			document.getElementById('Nwerr').innerHTML = NwnamevalidTx[g_langIndex];
			return false;
		}
	
	}
	else
	{
		document.NW.SSID.value = document.NW.wifinetwork.value;
	}
	if(document.getElementById('show_chars').checked == true)
	{
		document.NW.PASSWORD.value = document.NW.wifipassword_text.value ;
	}
	else
	{
		document.NW.PASSWORD.value = document.NW.wifipassword.value;
	}
	if(ValidatePassword(document.NW.PASSWORD.value))
	{
		if(lreq == 1)
		{
			if(StaticIpFlg == 0)
			{
				if(ValidateNw())
				submit_form(document.NW,'/nwmanualwiz',"NwWizSave",'Nwerr');
				else
				return false;
			}
			else
				submit_form(document.NW,'/nwmanualwiz',"NwWizSave",'Nwerr');
		}
		else
		{
			if(StaticIpFlg == 0)
			{
				if(ValidateNw())
				submit_form(document.NW,'/nwmanualwiz',"NwWizSaveConn",'Nwerr');
				else
				return false;
			}
			else
				submit_form(document.NW,'/nwmanualwiz',"NwWizSaveConn",'Nwerr');
		}
	}
	else
	{
	document.getElementById('Nwerr').innerHTML = StngFailTx[g_langIndex];
	}
}
function ClearSSIDPwd()
{
	document.NW.SSID.value = "";
	document.NW.PASSWORD.value == "";
	document.NW.wifipassword.value = "";
	document.NW.wifipassword_text.value = "";
}
//------------Validations for the POST value before submit--------------------------//
function ClearNwerr()
{
	document.getElementById('Nwerr').innerHTML = "";
}

function ValidateNw()
{
	if(!ValidateWoPopup(document.NW.IP1,0,223,'Nwerr'))
	{
		return false;
	}
	else if(!ValidateWoPopup(document.NW.IP2,0,255,'Nwerr')) 
	{
		return false;
	}
	else if(!ValidateWoPopup(document.NW.IP3,0,255,'Nwerr')) 
	{
		return false;
	}
	else if(!ValidateWoPopup(document.NW.IP4,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.SNMASK1,0,255,'Nwerr')) 
	{
		return false;
	}
	else if(!ValidateWoPopup(document.NW.SNMASK2,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.SNMASK3,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.SNMASK4,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.GW1,0,223,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.GW2,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.GW3,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.GW4,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.PD1,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.PD2,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.PD3,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.PD4,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.SD1,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.SD2,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.SD3,0,255,'Nwerr')) 
	{
		return false;		
	}
	else if(!ValidateWoPopup(document.NW.SD4,0,255,'Nwerr')) 
	{
		return false;		
	}
	else
		return true;
}

function ValidatePassword(id)
{
	var entry = id;
	validChar='0123456789ABCDEFabcdef'; //ok chars
	strlen=entry.length; //test string length
	if(strlen == 64)
	{
		for(idx=0;idx<strlen;idx++)
		{
			if(validChar.indexOf(entry.charAt(idx))<0)
			{
				return false;
			}
		} //end scan
		return true;
	}
	else if(strlen < 64)
	{
		return true;
	}
}
