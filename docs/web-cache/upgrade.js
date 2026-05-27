
//--07
var UpgrEngTx =["Firmware Upgrade","Select a valid firmware file from your computer to upload to your inverter.","Device Info","Model","Article Number","Serial Number","Currently installed firmware version","AC mcu","DC mcu","Webpage version","WIFI mcu","Firmware File","Allow downgrade","Please wait while the new firmware is installing on your inverter. After the installation is complete, the inverter will reboot itself.","Uploading firmware to the inverter...","Your inverter is now rebooting."];
var UpgrGerTx =["Firmware Update","W�hlen Sie einen g�ltigen Firmware-File auf Ihrem Computer aus, um diesen auf Ihren Wechselrichter zu laden.","Ger�te Information","Modell","Artikelnummer","Seriennummer","Aktuelle Firmware Version","AC mcu","DC mcu","Webpage","WIFI mcu","Firmware file","Downgrade erlauben","Warten Sie bitte, bis die neue Firmware auf Ihrem Wechselrichter installiert ist. Wenn die Installation abgeschlossen ist, l�dt der Wechselrichter sich wieder hoch.","Hochladen der Firmware auf den Wechselrichter","Ihr Wechselricher startet jetzt wieder im."];
var UpgrFrnTx =["Mise � jour du firmware","S�lectionnez un firmware valide pour votre ordinateur et chargez-le sur votre onduleur.","Information appareil","Type d'appareil","Num�ro de l'article","Num�ro de S�rie","Version firmware install�e actuellement","AC mcu","DC mcu","Webpage","WIFI mcu","Firmware file","Autoriser r�trogradage","Merci de patienter pendant que le nouveau firmware s'installe sur votre onduleur. Une fois l'installation termin�e, l'onduleur red�marre automatiquement.","T�l�chargement du firmware sur l'onduleur","Votre onduleur red�marre."];
var UpgrSpnTx =["Actualizaci�n del firmware","Seleccione un archivo v�lido de firmware de su ordenador para cargar en el inversor.","Informaci�n del equipo","Modelo","N�mero de art�culo","N�mero de serie","Versi�n del firmware actual","AC mcu","DC mcu","Webpage","WIFI mcu","Firmware file","Permitir degradar","Por favor, espere mientras el nuevo firmware se instala en su inversor. El inversor se reiniciara, despu�s de que la instalaci�n se haya completado.","Cargando el firmware al inversor","Su inversor se est� reiniciando."];
var UpgrItlTx =["Aggiornamento di software operativo","Selezionare un file di software operativo valido dal proprio computer per caricare l'inverter.","Informazioni sul dispositivo","Modello","Numero di articolo","Numero di seriale","Versione  software operativo attuale","AC mcu","DC mcu","Webpage","WIFI mcu","Firmware file","Consente declassare","Attendere. Installazione del nuovo firmware sull'inverter in corso. Al termine dell'istallazione l'inverter si riavvier�.","Caricamento del firmware sull'inverter in corso...","L'inverter si sta ora riavviando."];
var UpgrDutTx =["Firmware update","Kies een geldige firmware file, om hiermee de firmware te updaten.","Systeeminfo","Model","Artikelnummer","Serienummer","Huidige firmwareversie","AC mcu","DC mcu","Webpagina","WIFI mcu","Firmware file","Downgrade toestaan","Wacht todat de nieuwe firmware is ge�nstalleerd. Na de installatie start de omvormer op nieuw op.","Bezig met downloaden van firmware...","Uw omvormer start opnieuw op."];

var InstlFwTx = ["Install Firmware","Firmware installieren","Installez firmware","Instalar firmware","Installare software operativo","Firmware installeren"];
var UpdtfaildTx =["Update has failed","Update fehlgeschlagen","Mise � jour a �chou�","Error en la actualizaci�n","Aggiornamento fallito","Update mislukt"];

var UpgrdTimeOutVar;
function TranslateUpdate()
{
	var arrlen = UpgrEngTx.length;
	if(g_langArray[g_langIndex] == "EN")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "upg"+(i); 
	  document.getElementById(pgid).innerHTML = UpgrEngTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "GR")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "upg"+(i);
	  document.getElementById(pgid).innerHTML = UpgrGerTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "FR")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "upg"+(i);
	  document.getElementById(pgid).innerHTML = UpgrFrnTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "SP")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "upg"+(i);
	  document.getElementById(pgid).innerHTML = UpgrSpnTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "IT")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "upg"+(i);
	  document.getElementById(pgid).innerHTML = UpgrItlTx[i];
	 }
	}
	if(g_langArray[g_langIndex] == "DT")
	{
	 for(i=0;i<(arrlen);i++)
	 {
	  var pgid = "upg"+(i);
	  document.getElementById(pgid).innerHTML = UpgrDutTx[i];
	 }
	}
	document.getElementById('InstlFW').value = InstlFwTx[g_langIndex];
	ClearUpdteErr()
 GetUpdate();
}
function CheckFile()
{
	ClearUpdteErr();
	if(document.SoftUpdate.firmware.value=="")
	{
		document.SoftUpdate.Start.disabled = true;
		return false;
	}
	else
	{
		document.SoftUpdate.Start.disabled = false;
	}

}
function Fun_Confirm()
{
	var RetVal =CheckFile();
	if(RetVal!=false)
	{
	//	var a=confirm("Are you sure you want to upgrade?");
	//	if(a==true)
	//	{
			if(document.SoftUpdate.DowngradeChk.checked == true)
			{
				document.SoftUpdate.Downgrade.value = 1;
			}
			else
			{
				document.SoftUpdate.Downgrade.value = 0;
			}
			document.SoftUpdate.Start.disabled = true;
			document.getElementById('FW1Page').style.display = 'none';
			document.getElementById('FW2Page').style.display = 'inline';
			if(IntervalApStatusFlag == 1)
			{
				IntervalApStatusFlag = 0;
				clearInterval(IntervalApStatus);
			}
			document.f1.LanguageTrans.disabled = true;
			UpgrdTimeOutVar=setTimeout(UpdateFailFunc,300000);
			fileUpload(document.SoftUpdate,'/fwupdate');
			return false;
	//	}
	}
}

function ajaxRequest(){
 var activexmodes=["Msxml2.XMLHTTP", "Microsoft.XMLHTTP"] //activeX versions to check for in IE
 if (window.ActiveXObject)
 { //Test for support for ActiveXObject in IE first (as XMLHttpRequest in IE7 is broken)
  for (var i=0; i<activexmodes.length; i++)
  {
   try{
    return new ActiveXObject(activexmodes[i]);
   }
   catch(e)
   {
    //suppress error
   }
  }
 }
 else if (window.XMLHttpRequest) // if Mozilla, Safari etc
  return new XMLHttpRequest();
 else
  return false;
}
var OperaFlag=0;
function fileUpload(form, action_url)
{
  OperaFlag=0;
  // Create the iframe...
  var iframe = document.createElement("iframe");
  if(document.getElementById("upload_iframe"))
  {
        var elem = document.getElementById('upload_iframe');
  	elem.parentNode.removeChild(elem);
    } else {
  
  }
  iframe.setAttribute("id","upload_iframe");
  iframe.setAttribute("name","upload_iframe");
  iframe.setAttribute("width","0");
  iframe.setAttribute("height","0");
  iframe.setAttribute("border","0");
  iframe.setAttribute("style","width: 0; height: 0; border: none;");

  // Add to document...
  form.parentNode.appendChild(iframe);
  window.frames['upload_iframe'].name="upload_iframe";

  iframeId = document.getElementById("upload_iframe");

  // Add event...

  var eventHandler = function()
  {
  OperaFlag++;
  if (iframeId.detachEvent)
  {
  	iframeId.detachEvent("onload", eventHandler);
  }

  else
  {
 	 iframeId.removeEventListener("load", eventHandler, false);
  }

 ErrorFlag = 0;
 content = "Update failed";
 try{

		 // Message from server...
		  if (iframeId.contentDocument)
		  {
		  	 content = iframeId.contentDocument.body.innerHTML;
		  } else if (iframeId.contentWindow)
		  {
		  	 content = iframeId.contentWindow.document.body.innerHTML;
		  } else if (iframeId.document)
		  {
		  	// content = iframeId.document.body.innerHTML;
		  }
	}
	catch(err)
	{
		ErrorFlag = 1;

	}

	content = content.replace(/&(lt|gt);/g, function (strMatch, p1){return (p1 == "lt")? "<" : ">";})
	content = content.replace(/<\/?[^>]+(>|$)/g, "");
    if(OperaFlag == 1)
	{
	  if (content=="Update successful!")
	  {
	  	 clearTimeout(UpgrdTimeOutVar);
	  	 document.f1.LanguageTrans.disabled = false;
	  	 ErrorFlag =0;
	  	 document.getElementById('FW1Page').style.display = 'none';
	         document.getElementById('FW2Page').style.display = 'none';
		 document.getElementById('FW3Page').style.display = 'inline';
		 document.SoftUpdate.Start.disabled=false;
	  }
	  else if((content=="Update failed")||(ErrorFlag == 1))
	  {
		  UpdateFailFunc();
	  }
	}
   }
  if (iframeId.addEventListener)
  iframeId.addEventListener("load", eventHandler, true);
  if (iframeId.attachEvent)
  iframeId.attachEvent("onload", eventHandler);

  // Set properties of form...
  form.setAttribute("target","upload_iframe");
  form.setAttribute("action", action_url);
  form.setAttribute("method","post");
  form.setAttribute("enctype","multipart/form-data");
  form.setAttribute("encoding","multipart/form-data");
  // Submit the form...
  form.submit();
  return false;
}

function UpdateFailFunc()
{
	clearTimeout(UpgrdTimeOutVar);
	document.f1.LanguageTrans.disabled = false;
	document.getElementById('Upgradeerr').innerHTML = UpdtfaildTx[g_langIndex];
	document.SoftUpdate.Start.disabled=false;
	document.getElementById('FW1Page').style.display = 'inline';
	document.getElementById('FW2Page').style.display = 'none';
	document.getElementById('FW3Page').style.display = 'none';
}
function h2d(h) {return parseInt(h,16);} // hex to decimal

function GetUpdate()
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
			ParseUpdate(collect);
		}
	}
	xmlhttp.open("GET","/update " + Math.random(),false);
	xmlhttp.send();
}

function ParseUpdate(collect)
{
	var partsOfStr;
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
	if(outstring[0]==0)
	{
		document.getElementById('article_title').innerHTML = "SOLADIN 700";
		document.title = "SOLADIN 700";
	}
	if(outstring[0]==1)
	{
		document.getElementById('article_title').innerHTML ="SOLADIN 1000";
		document.title = "SOLADIN 1000";
	}
	if(outstring[0]==2)
	{
		document.getElementById('article_title').innerHTML ="SOLADIN 1500";
		document.title = "SOLADIN 1500";
	}
	document.getElementById('article_no').innerHTML = outstring[1];
	document.getElementById('serial_no').innerHTML = outstring[2];
	
	partsOfStr = outstring[3].split('.');
	document.getElementById('current_firmware_version').innerHTML = (partsOfStr[0]*100/100)+"."+(partsOfStr[1]*100/100);
	
	partsOfStr = outstring[4].split('.');
	document.getElementById('ac_mcu').innerHTML = (partsOfStr[0]*100/100)+"."+(partsOfStr[1]*100/100);
	
	partsOfStr = outstring[5].split('.');
	document.getElementById('dc_mcu').innerHTML = (partsOfStr[0]*100/100)+"."+(partsOfStr[1]*100/100);
	partsOfStr = outstring[6].split('.');
	document.getElementById('webpage_version').innerHTML = (partsOfStr[0]*100/100)+"."+(partsOfStr[1]*100/100);
	document.getElementById('wifi_mcu').innerHTML = outstring[7];
	document.getElementById('content-wrapper').style.visibility='visible';
}

function FirstLoad()
{
	GetUpdate();
}

function ToggleFwBlock()
{
	var d = document.getElementById('toggleupgrade').className;
	if(d == "togglebox opened")
	{
	 document.getElementById('toggleupgrade').className = 'togglebox closed';
	}

	if(d == "togglebox closed")
	{
	 document.getElementById('toggleupgrade').className = 'togglebox opened';
	}
}

function ClearUpdteErr()
{
 document.getElementById('Upgradeerr').innerHTML = "";
}

