
<!--07-->
var MainPgEngTx = ["Setup","Status","Upgrade","Options","Monitoring","Country Settings","Diagnostics","Advanced Settings"];
var MainPgGerTx = ["Einrichten","Status","Update","Einstellungen","Überwachung","Ländereinstellungen","Diagnostics","Erweiterte Einstellungen"];
var MainPgFrnTx = ["Configuration","État","Mise à jour","Reglages","Monitoring","Réglage pays","Diagnostics","Réglages avancés"];
var MainPgSpnTx = ["Configuración","Estado","Actualización ","Propiedades","Monitorización ","Configuración del país","Diagnostics","Configuración avanzada"];
var MainPgItlTx = ["Configurazione","Stato","Aggiornamento","Impostazioni","Monitoraggio","Impostazioni Paese","Diagnostics","Impostazioni avanzate"];
var MainPgDutTx = ["Installeren","Status","Update","Instellingen","Monitoring","Landinstellingen","Diagnostics","Geavanceerde Instellingen"];
var savetxt=["Save","Speichern","Sauvegarder","Guardar","Salva","Opslaan"];
var StngSaveTx =["Settings have been saved","Einstellungen wurden gespeichert","Les réglages ont été sauvegardés","La configuración se ha guardado","Le impostazioni sono state salvate","Instellingen opgeslagen"];
var StngFailTx =["Failed to save the settings","Einstellungen konnten nicht gespeichert werden","La sauvegarde des réglages a échoué","Error al guardar la configuración","Salvataggio impostazioni non riuscito","Instellingen opslaan mislukt"];
var EntrValttxt=["Please enter a value","Bitte geben Sie einen Wert ein","Saisir une valeur","Introducir un valor","Immettere un valore","Voer een waarde in"];

var InvPwd=["Invalid Password","Ungültiges Passwort","Mot de passe invalide","Contraseña invalida","Password non valida","Ongeldig wachtwoord"];
var EntrPwd=["Please enter Password","Geben Sie bitte das Passwort ein","Merci de saisir votre mot de passe","Por favor, ingrese la contraseña","Inserire la password","Voer wachtwoord in"];
//======Functions for the parser and the dyanamic part====//
var IntervalHome;
var IntervalHomeFlag  = 0;
var IntervalMonitor;
var IntervalMonitorFlag  = 0;
var Italyinterval;
var ItalyintervalFlag = 0;
var MaxPower;
var InProgressFlag=0;
var CountryNameId;
var DiagLoadedFlag = 0;
var IntervalApStatus;
var IntervalApStatusFlag = 0;
var g_langIndex;
var g_langArray = ["EN","GR","FR","SP","IT","DT"];
var g_lang = "EN";
var RegisterUrl;
var RuningPage;
var CommonURL = "/frontend/#register/";
tempImg = new Image();
tempImg.src="refresh_loading.gif";
var TransRunTimeTx = ["TranslateStp0","TranslateStp1","TranslateStp2","TranslateStp3","TranslateStp4","TranslateStp5","TranslateStp6","TranslateStp7","TranslateStatus","TranslateUpdate","TranslateOptions","TransMonitoring","TranslateCntrystngs"];

function TranslateMain()
{
var arrlen = MainPgEngTx.length;
if(g_langArray[g_langIndex] == "EN")
{
for(i=0;i<(arrlen);i++)
{
 var pgid = "main"+(i);
 document.getElementById(pgid).innerHTML = MainPgEngTx[i];
}
}
if(g_langArray[g_langIndex] == "GR")
{
for(i=0;i<(arrlen);i++)
{
 var pgid = "main"+(i);
 document.getElementById(pgid).innerHTML = MainPgGerTx[i];
}
}
if(g_langArray[g_langIndex] == "FR")
{
for(i=0;i<(arrlen);i++)
{
 var pgid = "main"+(i);
 document.getElementById(pgid).innerHTML = MainPgFrnTx[i];
}
}
if(g_langArray[g_langIndex] == "SP")
{
for(i=0;i<(arrlen);i++)
{
 var pgid = "main"+(i);
 document.getElementById(pgid).innerHTML = MainPgSpnTx[i];
}
}
if(g_langArray[g_langIndex] == "IT")
{
for(i=0;i<(arrlen);i++)
{
 var pgid = "main"+(i);
 document.getElementById(pgid).innerHTML = MainPgItlTx[i];
}
}
if(g_langArray[g_langIndex] == "DT")
{
for(i=0;i<(arrlen);i++)
{
 var pgid = "main"+(i);
 document.getElementById(pgid).innerHTML = MainPgDutTx[i];
}
}
}

function FirstLoad()
{
 GetBasicData("Lang");
 GetBasicData("Invertype");
 TranslateMain();
 GetBasicData("UsrType");
 GetBasicData("country");
 checkloadjscssfile("ApTimeout.js","js","GetApStatus");
}

function GetApStatus()
{
 GetBasicData("APStatus");
}

var recvdkey;
function GetBasicData(req)
{
var logsetting;
var data;
var xmlhttp;
if (window.XMLHttpRequest)
{ // code for IE7+, Firefox, Chrome, Opera, Safari
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
if(req == "country")
{
ParseCountryBasic(xmlhttp.responseText);
}
if(req == "Invertype")
{
ParseInvId(xmlhttp.responseText);
}
if(req == "UsrType")
{
UserTypeRights(xmlhttp.responseText);
}
if(req == "keydata")
{
recvdkey = xmlhttp.responseText;
document.frmLogin.password1.value = hex_hmac_sha1(recvdkey,pwd);
submit_Loginform(document.frmLogin,'/login');
}
if(req == "APStatus")
{
ParseApStatus(xmlhttp.responseText);
}
if(req == "Lang")
{
collect=xmlhttp.responseText;
NumOfParams=0;
var MaxnumOfParams = 1;
var langstring = new Array(MaxnumOfParams);
var count=0;
for(i=0;i<MaxnumOfParams;i++)
{
langstring[i]="";
while(((collect.slice(count,count+1))!="\n")&&(count <= collect.length))
{
langstring[i] += (collect.slice(count,count+1));
count++;
}
NumOfParams++;
count++;
}
document.getElementById('lang').value=document.f1.LanguageTrans.value = g_langIndex = Math.round(langstring[0]);
}
}
}
if(req == "country")
{
xmlhttp.open("GET","/cid " + Math.random(),false);
xmlhttp.send();
}
if(req == "Invertype")
{
xmlhttp.open("GET","/invid " + Math.random(),false);
xmlhttp.send();
}
if(req == "UsrType")
{
xmlhttp.open("GET","/usertype " + Math.random(),false);
xmlhttp.send();
}
if(req == "keydata")
{
xmlhttp.open("GET","/key  "+ Math.random(),false);
xmlhttp.send();
}
if(req == "APStatus")
{
xmlhttp.open("GET","/apstatus "+ Math.random(),false);
xmlhttp.send();
}
if(req == "Lang")
{
xmlhttp.open("GET","/language "+ Math.random(),false);
xmlhttp.send();
}
}

function GetMainData(req)
{
if(IntervalHomeFlag == 1)
{
IntervalHomeFlag = 0;
clearInterval(IntervalHome);
}
if(IntervalMonitorFlag == 1)
{
IntervalMonitorFlag = 0;
clearInterval(IntervalMonitor);
}
if(ItalyintervalFlag == 1)
{
ItalyintervalFlag = 0;
clearInterval(Italyinterval);
}
if(DiagLoadedFlag == 1)
{
ClearDiagInterval();
}
if(IntervalApStatusFlag == 0)
{
IntervalApStatusFlag = 1;
IntervalApStatus = setInterval(GetApStatus, (40000));
}
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
RuningPage = req;
if(xmlhttp.responseText == "<p>Either your session has timed out or you have not logged in.</p>")
{
location.reload();
return false;
}
document.getElementById('content-wrapper').style.visibility='hidden';
document.getElementById('content-wrapper').innerHTML= "";
document.getElementById('content-wrapper').innerHTML= xmlhttp.responseText;
DeselectMenu();
ShowMainMenu();
if((req >= 0) && (req < 8))
{
document.getElementById("SetupTab").className = "active";
if(req == 0)
{
checkloadjscssfile("Setup0Main.js","js","TranslateStp0");
}
if(req == 1)
{
checkloadjscssfile("Setup1Main.js","js","TranslateStp1");
}
if(req == 2)
{
document.getElementById('content-wrapper').style.visibility='hidden';
checkloadjscssfile("Setup2Main.js","js","TranslateStp2");
}
if(req == 3)
{
checkloadjscssfile("Setup3Main.js","js","TranslateStp3");
if(IntervalApStatusFlag == 1)
{
 IntervalApStatusFlag = 0;
 clearInterval(IntervalApStatus);
}
}
if(req == 4)
{
checkloadjscssfile("Setup4Main.js","js","TranslateStp4");
}
if(req == 5)
{
checkloadjscssfile("Setup5Main.js","js","TranslateStp5");
if(IntervalApStatusFlag == 1)
{
 IntervalApStatusFlag = 0;
 clearInterval(IntervalApStatus);
}
}
if(req == 6)
{
checkloadjscssfile("Setup6Main.js","js","TranslateStp6");
}
if(req == 7)
{
checkloadjscssfile("Setup7Main.js","js","TranslateStp7");
}
}
if(req == 8)
{
document.getElementById("StatusTab").className = "active";
checkloadjscssfile("Status.js","js","TranslateStatus");
}
if(req == 9)
{
if(IntervalHomeFlag == 1)
{
IntervalHomeFlag = 0;
clearInterval(IntervalHome);
}
document.getElementById("UpgradeTab").className = "active";
checkloadjscssfile("upgrade.js","js","TranslateUpdate");
}

if(req == 10)
{
document.getElementById("OptionsTab").className = "active";
checkloadjscssfile("OptionsMain.js","js","TranslateOptions");
}
if(req == 11)
{
document.getElementById("MonitorTab").className = "active";
checkloadjscssfile("Monitoring.js","js","TransMonitoring");
}
if(req == 12)
{
document.getElementById("InstallerTab").className = "active";
checkloadjscssfile("CountrySettingsMain.js","js","TranslateCntrystngs");
}
if(req == 13)
{
document.getElementById("DiagnosticTab").className = "active";
checkloadjscssfile("DiagnosticsMain.js","js","ShowContent");
}
if(req == 14)
{
checkloadjscssfile("ApTimeout.js","js","TranslateAptmot");
HideMenu();
ShowMainMenu();
ShowContent();
if(IntervalApStatusFlag == 1)
{
IntervalApStatusFlag = 0;
clearInterval(IntervalApStatus);
}
}
}
}
if(req == 0)
{
xmlhttp.open("GET","Setup0Main.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 1)
{
xmlhttp.open("GET","Setup1Main.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 2)
{
xmlhttp.open("GET","Setup2Main.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 3)
{
xmlhttp.open("GET","Setup3Main.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 4)
{
xmlhttp.open("GET","Setup4Main.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 5)
{
xmlhttp.open("GET","Setup5Main.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 6)
{
xmlhttp.open("GET","Setup6Main.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 7)
{
xmlhttp.open("GET","Setup7Main.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 8)
{
xmlhttp.open("GET","StatusMain.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 9)
{
xmlhttp.open("GET","UpgradeMain.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 10)
{
xmlhttp.open("GET","OptionsMain.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 11)
{
xmlhttp.open("GET","MonitoringMain.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 12)
{
xmlhttp.open("GET","CountryMain.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 13)
{
xmlhttp.open("GET","DiagnosticsMain.html "+ Math.random(),false);
xmlhttp.send();
}
if(req == 14)
{
xmlhttp.open("GET","ApTimeout.html "+ Math.random(),false);
xmlhttp.send();
}
}

function DeselectMenu()
{
document.getElementById("SetupTab").className = "advanced hide";
document.getElementById("StatusTab").className = "advanced hide";    
document.getElementById("UpgradeTab").className = "advanced hide";  
document.getElementById("OptionsTab").className = "advanced hide";    
document.getElementById("MonitorTab").className = "advanced hide";    
document.getElementById("InstallerTab").className = "advanced hide";    
document.getElementById("DiagnosticTab").className = "advanced hide";
}

function ParseCountryBasic(collect)
{
 if(collect== "1\n")
 {
  GetMainData(0);
 }
 else
 {
  GetMainData(8);
 }
}

function ParseApStatus(collect)
{
 if(collect== "1\n")
 {
  GetMainData(14);
 }
 else
 {
  //nothing
 }
}

var InvType = 0;
var MaxPower = 1575;
function ParseInvId(collect)
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
InvType= Math.round(outstring[0]);
if(InvType==0)
{
document.title = "SOLADIN 700";
MaxPower = 735;
}
if(InvType==1)
{
document.title = "SOLADIN 1000";
MaxPower = 1050;
}
if(InvType==2)
{
document.title = "SOLADIN 1500";
MaxPower = 1575;
}
}

var scriptLoaded=false;
function loadScript(url, callback)
{
    var script = document.createElement("script")
    script.type = "text/javascript";
    if (script.readyState){  //IE
        script.onreadystatechange = function(){
            if (script.readyState == "loaded" ||
                    script.readyState == "complete"){
                script.onreadystatechange = null;
                scriptLoaded=true;
                window[callback]();
            }
        };
    } else {  //Others
        script.onload = function(){
            scriptLoaded=true;
            window[callback]();
        };
    }
    script.src = url;
    document.body.appendChild(script);
}

function h2d(h) {return parseInt(h,16);} // hex to decimal

var filesadded="" //list of files already added

function checkloadjscssfile(filename, filetype ,func){
 if (filesadded.indexOf("["+filename+"]")==-1)
 {
  loadScript(filename,func);
  filesadded+="["+filename+"]";
 }
 else
 {
  window[func]();
 }
}

function ShowContent()
{
 document.getElementById('content-wrapper').style.visibility='visible';
}

var OperaFlag=0;
var g_reqpage;
var sbmtmsgid;
function submit_form(form, action_url,reqpage,msgid)
{

g_reqpage = reqpage;
sbmtmsgid = msgid;
OperaFlag=0;
// Create the iframe...
var iframe = document.createElement("iframe");
iframe.setAttribute("id","sbmt_iframe");
iframe.setAttribute("name","sbmt_iframe");
iframe.setAttribute("width","0");
iframe.setAttribute("height","0");
iframe.setAttribute("border","0");
iframe.setAttribute("style","width: 0; height: 0; border: none;");

// Add to document...
form.parentNode.appendChild(iframe);
window.frames['sbmt_iframe'].name="sbmt_iframe";

iframeId = document.getElementById("sbmt_iframe");

// Add event...
var eventHandler = function()  {
OperaFlag++;
if (iframeId.detachEvent)
iframeId.detachEvent("onload", eventHandler);

else
iframeId.removeEventListener("load", eventHandler, false);

// Message from server...
if (iframeId.contentDocument) {
content = iframeId.contentDocument.body.innerHTML;
} else if (iframeId.contentWindow) {
content = iframeId.contentWindow.document.body.innerHTML;
} else if (iframeId.document) {
//content = iframeId.document.body.innerHTML;
}
content = content.replace(/&(lt|gt);/g, function (strMatch, p1){
return (p1 == "lt")? "<" : ">";
})
content = content.replace(/<\/?[^>]+(>|$)/g, "");
if(OperaFlag == 1)
{
if (content=="Update successful!")
{
if(g_reqpage == "CountryWiz")
{
GetMainData(1);
}
else if(g_reqpage =="NormalMode")
{
CheckSelected();
}
else if(g_reqpage == "NwWizSave")
{
document.getElementById('Nwerr').innerHTML = StngSaveTx[g_langIndex];
GetSaveCountry("NwWizSave");
}
else if(g_reqpage == "NwWizSaveConn")
{
GetMainData(3);
}
else if(g_reqpage == "OptionsPage")
{
document.getElementById(sbmtmsgid).innerHTML = StngSaveTx[g_langIndex];
}
else if(g_reqpage == "CountryID")
{
GetCountrySettings();
}
else if(g_reqpage == "Insualtion")
{
document.getElementById(sbmtmsgid).innerHTML = StngSaveTx[g_langIndex];
}
else if(g_reqpage == "CountrySettings")
{
document.getElementById(sbmtmsgid).innerHTML = StngSaveTx[g_langIndex];
}
else if(g_reqpage == "loginpwd")
{
GetBasicData("UsrType");
GetBasicData("country");
}
else if(g_reqpage == "MpptSet")
{
document.getElementById(sbmtmsgid).innerHTML = "Settings are saved";
}
else if(g_reqpage == "IdentifierSet")
{
document.getElementById(sbmtmsgid).innerHTML = "Settings are saved";
}
else if(g_reqpage == "UrlPort")
{
document.getElementById(sbmtmsgid).innerHTML = "Settings are saved";
}
else if(g_reqpage == "lang")
{
//location.reload();
}
}
else
{
if((g_reqpage == "OptionsPage")||(g_reqpage == "CountryID")||(g_reqpage == "Insualtion")||(g_reqpage == "CountrySettings")||(g_reqpage == "MpptSet")||(g_reqpage == "IdentifierSet")||(g_reqpage == "UrlPort"))
document.getElementById(sbmtmsgid).innerHTML = StngFailTx[g_langIndex];
}
}
}
if (iframeId.addEventListener)
iframeId.addEventListener("load", eventHandler, false);
if (iframeId.attachEvent)
iframeId.attachEvent("onload", eventHandler);
// Set properties of form...
form.setAttribute("target","sbmt_iframe");
form.setAttribute("action", action_url);
form.setAttribute("method","post");
form.setAttribute("enctype","multipart/form-data");
form.setAttribute("encoding","multipart/form-data");
// Submit the form...
form.submit();
return false;
}

//================================ Common Js Functions ====================================//
function ToggleInfoBlock()
{
var d = document.getElementById('ToggleInfo').className;
if(d == "manual_legend togglebox opened")
{
document.getElementById('ToggleInfo').className = 'manual_legend togglebox closed';
}

if(d == "manual_legend togglebox closed")
{
document.getElementById('ToggleInfo').className = 'manual_legend togglebox opened';
}
}

function GetSaveCountry(lFlag)
{
var xmlhttp;
if (window.XMLHttpRequest)
{ // code for IE7+, Firefox, Chrome, Opera, Safari
xmlhttp=new XMLHttpRequest();
}
else
{ // code for IE6, IE5
xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
}
xmlhttp.onreadystatechange=function()
{
if (xmlhttp.readyState==4 && xmlhttp.status==200)
{
collect=xmlhttp.responseText;
if(lFlag == "NwWizSave")
{
document.getElementById('Nwerr').innerHTML = StngSaveTx[g_langIndex];
}
else if(lFlag == "NwConnect")
{
GetInfraconnected();
Redirect();
setInterval('Redirect()', 5000);
}
else if(lFlag == "WpsConnect")
{
GetWPSconnect();
}
else if(lFlag == "WiFiOFF")
{
    GetWifiOff();
}
}
}
xmlhttp.open("GET","/SaveCountryWiz " + Math.random(),false);
xmlhttp.send();
}


function GetRegisterInfo(NwOrWps)
{
var xmlhttp;
if (window.XMLHttpRequest)
{ // code for IE7+, Firefox, Chrome, Opera, Safari
xmlhttp=new XMLHttpRequest();
}
else
{ // code for IE6, IE5
xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
}
xmlhttp.onreadystatechange=function()
{
if (xmlhttp.readyState==4 && xmlhttp.status==200)
{
collect=xmlhttp.responseText;
ParseRegInfo(collect ,NwOrWps);
}
}
xmlhttp.open("GET","/reginfo " + Math.random(),false);
xmlhttp.send();
}

function ParseRegInfo(collect,NwOrWps)
{
NumOfParams=0;
var MaxnumOfParams = 3;
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
RegisterUrl  = "";
RegisterUrl += "http://";
RegisterUrl += outstring[2];
RegisterUrl += CommonURL;
RegisterUrl +=  outstring[0];
RegisterUrl +=  d2h(parseInt(outstring[1]));
ChkOnlineURl  = "";
ChkOnlineURl += "http://";
ChkOnlineURl += outstring[2]+ "/favicon.ico?d=" + escape(Date());
if(NwOrWps == "NW")
{
GetSaveCountry("NwConnect");
}
else if(NwOrWps == "WPS")
{
GetSaveCountry("WpsConnect");
}
}
function d2h(d) {return d.toString(16);}
function ParsesaveCountry(collect)
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
GetWPSconnect();
}

function Redirect()
{
CheckNetwork();
}

function doConnectFunction() {
window.top.location = RegisterUrl;
}
function doNoConnectFunction() {
//retry
}

var ChkOnlineURl;
function CheckNetwork()
{
var i = new Image();
i.onload = doConnectFunction;
i.onerror = doNoConnectFunction;
i.src = 'https://intelliweb.mastervolt.com/favicon.ico?d=' + escape(Date());
}

function RedirectInfoNw()
{
 HideMenu();
 GetRegisterInfo("NW");
}

function RedirectInfoWps()
{
 HideMenu();
 GetRegisterInfo("WPS");
}

function HideMenu(Req)
{
 document.getElementById('submenu').style.display = 'none';
 document.getElementById('footer').style.display = 'none';
}


function ValidateWoPopup(id,min,maxvalue,msgid)
{
  if(id.value =="")
{
document.getElementById(msgid).innerHTML = EntrValttxt[g_langIndex];
return false;
}
else if(id.value > maxvalue)
{
id.value= maxvalue;
return true;
}
else if(id.value < min)
{
id.value= min;
return true;
}
else
{
return true;
}
}

function check(evt)
{
var unicode= (evt.which) ? evt.which : event.keyCode;
if(unicode == 8)
{
return true;
}
if ((unicode > "32" && unicode < "48") || (unicode > '57' && unicode < '65') || (unicode > '90' && unicode < '97'))
{
return false;
}
if (unicode< "48" || unicode > "57")
{
return false;
}
}

var UserTypeid;
function UserTypeRights(collect)
{
temp=h2d((collect.slice(1,2))+(collect.slice(0,1)));
if(temp == 1)
{

UserTypeid = 1;//Normal User level
}
else if(temp==2)
{
UserTypeid = 3;//Installer
}
else if(temp==4)
{
UserTypeid = 5;//Diagnostics user
}
ShowMainMenu();
}

function ShowMainMenu()
{
if(UserTypeid == 1)
{
document.getElementById("UpgradeTab").className =  "advanced";
document.getElementById("SetupTab").className = "advanced";
document.getElementById("StatusTab").className = "advanced";
document.getElementById('OptionsTab').className = "advanced";
document.getElementById('MonitorTab').className = "advanced";
document.getElementById('InstallerTab').className = "advanced hide";
document.getElementById('DiagnosticTab').className = "advanced hide";
document.getElementById('login').className = "";
}
else if(UserTypeid == 3)
{
document.getElementById("UpgradeTab").className =  "advanced";
document.getElementById("SetupTab").className = "advanced";
document.getElementById("StatusTab").className = "advanced";
document.getElementById('OptionsTab').className = "advanced";
document.getElementById('MonitorTab').className = "advanced";
document.getElementById('InstallerTab').className = "advanced";
document.getElementById('DiagnosticTab').className = "advanced hide";
document.getElementById('login').className = "loggedin";
}
else if(UserTypeid == 5)
{
document.getElementById("UpgradeTab").className =  "advanced";
document.getElementById("SetupTab").className = "advanced";
document.getElementById("StatusTab").className = "advanced";
document.getElementById('OptionsTab').className = "advanced";
document.getElementById('MonitorTab').className = "advanced";
document.getElementById('InstallerTab').className = "advanced";
document.getElementById('DiagnosticTab').className = "advanced";
document.getElementById('login').className = "loggedin";
}
}

//-----Login--//
var key;
var hexcase = 0;
var b64pad  = "";
var chrsz   = 8;
var invalid = " ";
var pwd = "";
function Getpassword()
{
pwd=prompt(EntrPwd[g_langIndex],"");
if(pwd == null)
{
return false;
}
if((pwd.length <= 8)||(pwd == ""))
{
alert(InvPwd[g_langIndex]);
}
else if((pwd.length > 16))
{
alert(InvPwd[g_langIndex]);
}
else
{
GetBasicData("keydata");
}
}


function hex_hmac_sha1(key,data)
{
return binb2hex(core_hmac_sha1(key, data));
}

function core_sha1(x, len)
{
  x[len >> 5] |= 0x80 << (24 - len % 32); x[((len + 64 >> 9) << 4) + 15] = len;
  var w = Array(80); var a =  1732584193;var b = -271733879;var c = -1732584194;var d = 271733878;var e = -1009589776;
for(var i = 0; i < x.length; i += 16)
{
var olda = a;var oldb = b;var oldc = c;var oldd = d;var olde = e;
for(var j = 0; j < 80; j++)
{
if(j < 16) w[j] = x[i + j];
else w[j] = rol(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);
var t = safe_add(safe_add(rol(a, 5), sha1_ft(j, b, c, d)),
  safe_add(safe_add(e, w[j]), sha1_kt(j)));
e = d;d = c;c = rol(b, 30);b = a;a = t;}

a = safe_add(a, olda);b = safe_add(b, oldb);c = safe_add(c, oldc);d = safe_add(d, oldd);e = safe_add(e, olde);
}
return Array(a, b, c, d, e);}

function sha1_ft(t, b, c, d)
{if(t < 20) return (b & c) | ((~b) & d);
if(t < 40) return b ^ c ^ d;
if(t < 60) return (b & c) | (b & d) | (c & d);
return b ^ c ^ d;
}

function sha1_kt(t)
{
return (t < 20) ?  1518500249 : (t < 40) ?  1859775393 :
(t < 60) ? -1894007588 : -899497514;
}

function core_hmac_sha1(key, data)
{
var bkey = str2binb(key);
if(bkey.length > 16) bkey = core_sha1(bkey, key.length * chrsz);

var ipad = Array(16), opad = Array(16);
for(var i = 0; i < 16; i++)
{
ipad[i] = bkey[i] ^ 0x36363636;
opad[i] = bkey[i] ^ 0x5C5C5C5C;
}

var hash = core_sha1(ipad.concat(str2binb(data)), 512 + data.length * chrsz);
return core_sha1(opad.concat(hash), 512 + 160);
}

function safe_add(x, y)
{var lsw = (x & 0xFFFF) + (y & 0xFFFF);
var msw = (x >> 16) + (y >> 16) + (lsw >> 16);
return (msw << 16) | (lsw & 0xFFFF);
}


function rol(num, cnt)
{return (num << cnt) | (num >>> (32 - cnt));}


function str2binb(str)
{
var bin = Array();
var mask = (1 << chrsz) - 1;
for(var i = 0; i < str.length * chrsz; i += chrsz)
bin[i>>5] |= (str.charCodeAt(i / chrsz) & mask) << (32 - chrsz - i%32);
return bin;
}


function binb2hex(binarray)
{
var hex_tab = hexcase ? "0123456789ABCDEF" : "0123456789abcdef";
var str = "";
for(var i = 0; i < binarray.length * 4; i++)
{
str += hex_tab.charAt((binarray[i>>2] >> ((3 - i%4)*8+4)) & 0xF) +
  hex_tab.charAt((binarray[i>>2] >> ((3 - i%4)*8  )) & 0xF);
}
return str;
}


function binb2b64(binarray)
{
var tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
var str = "";
for(var i = 0; i < binarray.length * 4; i += 3)
{
var triplet = (((binarray[i   >> 2] >> 8 * (3 -  i   %4)) & 0xFF) << 16)
| (((binarray[i+1 >> 2] >> 8 * (3 - (i+1)%4)) & 0xFF) << 8 )
|  ((binarray[i+2 >> 2] >> 8 * (3 - (i+2)%4)) & 0xFF);
for(var j = 0; j < 4; j++)
{
  if(i * 8 + j * 6 > binarray.length * 32) str += b64pad;
  else str += tab.charAt((triplet >> 6*(3-j)) & 0x3F);
}
}
return str;
}

function submit_Loginform(form,action_url)
{
OperaFlag=0;
// Create the iframe...
var iframe = document.createElement("iframe");
iframe.setAttribute("id","login_iframe");
iframe.setAttribute("name","login_iframe");
iframe.setAttribute("width","0");
iframe.setAttribute("height","0");
iframe.setAttribute("border","0");
iframe.setAttribute("style","width: 0; height: 0; border: none;");
// Add to document...
form.parentNode.appendChild(iframe);
window.frames['login_iframe'].name="login_iframe";
iframeLId = document.getElementById("login_iframe");
// Add event...
var eventHandler = function()  {
OperaFlag++;
if (iframeLId.detachEvent)
iframeLId.detachEvent("onload", eventHandler);
else
iframeLId.removeEventListener("load", eventHandler, false);
// Message from server...
if (iframeLId.contentDocument) {
content = iframeLId.contentDocument.body.innerHTML;
} else if (iframeLId.contentWindow) {
content = iframeLId.contentWindow.document.body.innerHTML;
} else if (iframeLId.document) {
}
content = content.replace(/&(lt|gt);/g, function (strMatch, p1){
return (p1 == "lt")? "<" : ">";
})
content = content.replace(/<\/?[^>]+(>|$)/g, "");
if(OperaFlag == 1)
{
if (content=="Update successful!")
{
GetBasicData("UsrType");
GetBasicData("country");
}
else
{
alert(InvPwd[g_langIndex]);
}
}
}
if (iframeLId.addEventListener)
iframeLId.addEventListener("load", eventHandler, false);
if (iframeLId.attachEvent)
iframeLId.attachEvent("onload", eventHandler);
// Set properties of form...
form.setAttribute("target","login_iframe");
form.setAttribute("action", action_url);
form.setAttribute("method","post");
form.setAttribute("enctype","multipart/form-data");
form.setAttribute("encoding","multipart/form-data");
// Submit the form...
form.submit();
return false;
}

function submit_Lang(form,action_url)
{

OperaFlag=0;
var iframe = document.createElement("iframe");
iframe.setAttribute("id","lang_iframe");
iframe.setAttribute("name","lang_iframe");
iframe.setAttribute("width","0");
iframe.setAttribute("height","0");
iframe.setAttribute("border","0");
iframe.setAttribute("style","width: 0; height: 0; border: none;");
form.parentNode.appendChild(iframe);
window.frames['lang_iframe'].name="lang_iframe";
ifrmId = document.getElementById("lang_iframe");
var eventHandler = function()  {
OperaFlag++;
if (ifrmId.detachEvent)
ifrmId.detachEvent("onload", eventHandler);
else
ifrmId.removeEventListener("load", eventHandler, false);
// Message from server...
if (ifrmId.contentDocument) {
content = ifrmId.contentDocument.body.innerHTML;
} else if (ifrmId.contentWindow) {
content = ifrmId.contentWindow.document.body.innerHTML;
} else if (ifrmId.document) {
//content = ifrmId.document.body.innerHTML;
}
content = content.replace(/&(lt|gt);/g, function (strMatch, p1){
return (p1 == "lt")? "<" : ">";
})
content = content.replace(/<\/?[^>]+(>|$)/g, "");
if(OperaFlag == 1)
{
if (content=="Update successful!")
{
g_langIndex = document.f1.LanguageTrans.value;
if(RuningPage != 13)
{
window[TransRunTimeTx[RuningPage]]();
}
TranslateMain();
}
}
}
if(ifrmId.addEventListener)
ifrmId.addEventListener("load", eventHandler, false);
if(ifrmId.attachEvent)
ifrmId.attachEvent("onload", eventHandler);
form.setAttribute("target","lang_iframe");
form.setAttribute("action", action_url);
form.setAttribute("method","post");
form.setAttribute("enctype","multipart/form-data");
form.setAttribute("encoding","multipart/form-data");
form.submit();
return false;
}

function Precision(data,Decimal)
{
var retval,Convalue;
if(Decimal == 1)
{
Convalue = parseFloat(((data*10)/10).toFixed(1));
retval = Convalue.toFixed(1);
}
if(Decimal == 2)
{
Convalue = parseFloat(((data*100)/100).toFixed(2));
retval = Convalue.toFixed(2);
}
return retval;
}

function LangSubmit()
{
document.f1.LanguageTrans.value=document.getElementById('lang').value;
submit_Lang(document.f1,'\language');
}
