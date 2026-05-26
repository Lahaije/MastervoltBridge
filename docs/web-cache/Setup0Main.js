
//--07
var Stp0EngTx = ["Setup your inverter","Please select the country where your inverter was installed. This setting will be locked after you continue to the next step.","Country","Select:","Netherlands","Belgium","Germany","England","France","French Overseas","Spain","Italy","Greece","Ireland","Denmark","Israel","Austria","Australia","Bulgary","Czech Republic","Custom","Portugal","Poland"];
var Stp0GerTx = ["Wechselrichterkonfiguration","Bitte w�hlen Sie das Land aus in welchem Ihr Wechselrichter installiert ist. Diese Einstellung l�sst sich r�ckwirkend nicht mehr �ndern.","L�nd","Wahl:","Niederlande","Belgien","Deutschland","England","Frankreich","Franz�sische �berseegebiete","Spanien","Italien","Griechenland","Irland","D�nemark","Israel","�sterreich","Australien","Bulgarien","Tschechische Republik","Benutzerdefiniert","Portugal","Polen"];
var Stp0FrnTx = ["Configuration de l�onduleur","S�lectionnez le pays o� l'onduleur est install�. Ce r�glage sera verrouill� lorsque vous continuerez sur la page suivante.","Pays","S�lectionner:","Pays-Bas","Belgique","Allemagne","Royaume-Uni","France","France Dom-Tom","Espagne","Italie","Gr�ce","Irelande","Danemark","Isra�l","Autriche","Australie","Bulgarie","R�publique tch�que","Personnalis�","Portugal","Pologne"];
var Stp0SpnTx = ["Configuraci�n del inversor","Por favor, seleccionar el pa�s de instalaci�n del inversor. Esta configuraci�n quedar� bloqueada cuando proceda al siguiente paso.","Pa�s","Seleccionar:","Holanda ","Belgica","Alemania","Inglaterra","Francia","Francia de Ultramar","Espa�a","Italia","Grecia","Irlanda","Dinamarca","Israel","Austria","Australia","Bulgaria","la Rep�blica Checa","Personalizar","Portugal","Polonia"];
var Stp0ItlTx = ["Impostazione dell�inverter","Si prega di selezionare il Paese in cui � stato installato l'inverter. Questa impostazione sar� bloccata una volta che si prosegue con il passo successivo.","Paese","Selezionare:","Paesi Bassi","Belgio","Germania","Inghilterra","Francia","Francia d'oltremare","Spagna","Italia","Grecia","Irlanda","Danimarca","Israele","Austria","Australia","Bulgaria","Repubblica ceca","Personalizzato","Portogallo","Polonia"];
var Stp0DutTx = ["Uw omvormer instellen","Kies het land waar U de omvormer heeft geinstalleerd. Deze instelling zal vergrendeld worden als U doorgaat naar de volgende stap.","Land","Kies:","Nederland","Belgi�","Duitsland","Engeland","Frankrijk","Franse overzeese gebieden","Spanje","Itali�","Griekenland","Ierland","Denemarken","Israel","Oostenrijk","Australi�","Bulgarije","Tsjechische Republiek","Gebruikersinstellingen","Portugal","Polen"];

var NextTx = ["Next","Weiter","Suivant","Siguiente","Prossimo","Volgende"];
function TranslateStp0()
{
	var arrlen = Stp0EngTx.length;
	if(g_langArray[g_langIndex] == "EN")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp0id"+(i);
		if((i > 2) && (i < 21))
		{
			if(i == 11)
			{}
			else
			{
			myoption= document.getElementById(pgid);
			myoption.text =Stp0EngTx[(i)];
			}
		}
		else
		document.getElementById(pgid).innerHTML = Stp0EngTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "GR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp0id"+(i);
		if((i > 2) && (i < 21))
		{
		if(i == 11)
		{}
		else
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp0GerTx[(i)];
		}
		}
		else
		document.getElementById(pgid).innerHTML = Stp0GerTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "FR")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp0id"+(i);
		if((i > 2) && (i < 21))
		{
		if(i == 11)
		{}
		else
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp0FrnTx[(i)];
		}
		}
		else
		document.getElementById(pgid).innerHTML = Stp0FrnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "SP")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp0id"+(i);
		if((i > 2) && (i < 21))
		{
		if(i == 11)
		{}
		else
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp0SpnTx[(i)];
		}
		}
		else
		document.getElementById(pgid).innerHTML = Stp0SpnTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "IT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp0id"+(i);
		if((i > 2) && (i < 21))
		{
		if(i == 11)
		{}
		else
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp0ItlTx[(i)];
		}
		}
		else
		document.getElementById(pgid).innerHTML = Stp0ItlTx[i];
		}
	}
	if(g_langArray[g_langIndex] == "DT")
	{
		for(i=0;i<(arrlen);i++)
		{
		var pgid = "Stp0id"+(i);
		if((i > 2) && (i < 21))
		{
		if(i == 11)
		{}
		else
		{
		myoption= document.getElementById(pgid);
		myoption.text =Stp0DutTx[(i)];
		}
		}
		else
		document.getElementById(pgid).innerHTML = Stp0DutTx[i];
		}
	}
	document.getElementById('Stp0Nxt').value = NextTx[g_langIndex];
 GetCountry();
}

function GetCountry()
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
			ParseCountry(collect);
		}
	}
	xmlhttp.open("GET","/cid " + Math.random(),false);
	xmlhttp.send();
}

function ParseCountry(collect)
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
			//alert("outstring[i]="+outstring[i] );
			count++;
		}
		NumOfParams++;
		count++;
	}
	CountryNameId=parseInt(outstring[0]);
	document.CID.countryid.value = CountryNameId;
	ChkNC();
	if((document.CID.countryid.value > 1)&&(document.CID.countryid.value <=21))
	{
		if(CountryNameId == 21)
		{
			document.CID.countryid1.value = CountryNameId;
		}
		else
		{
			document.CID.countryid1.value = CountryNameId;
		}
		document.CID.countryid.disabled = true;
	}
	ShowContent();
}

function ValidateCountryWiz()
{
	submit_form(document.CID,'/wizcid' , "CountryWiz",'Setup0err');
}

function ChkNC()
{
	if(document.CID.countryid.value == "1")
	{
		document.CID.Next.disabled = true;
	}
	else
	{
		document.CID.countryid1.value = document.CID.countryid.value;
		document.CID.Next.disabled = false;
	}
}


