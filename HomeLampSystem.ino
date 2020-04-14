#include <NTP.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>


const char* ssid = "aterm-9ac1c7-g";
const char* password = "9a8bd40c42d12";

int Year,Month,Day,Hour,Minute,Second;
String ledState = "OFF",sensorState="OFF";
unsigned long timer=0,zero;

ESP8266WebServer server(80);

void handleRoot() {
    Serial.println("receive req: /");
    String DateTime = "<h1>";
    DateTime += String(Year);
    DateTime += ":";
    DateTime += String(Month);
    DateTime += ":";
    DateTime += String(Day);
    DateTime += ":";
    DateTime += String(Hour);
    DateTime += ":";
    DateTime += String(Minute);
    DateTime += ":";
    DateTime += String(Second);
    DateTime += "</h1>";
    String msg = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\"><title>LED Button</title></head><body>";
    msg += "<script>";
    msg += "function sendOn(){" ;
    msg += "send(\"/on/\");";
    //msg += "document.getElementById(\"LEDstatus\").innerHTML=\"ON!\";";
    msg += "}";
    msg += "function sendOff(){";
    msg += "send(\"/off/\");";
    msg += "}";
    msg += "function sendReboot(){";
    msg += "send(\"/reboot/\");";
    msg += "}";
    msg += "function send(url){";
    msg += "var xhr = new XMLHttpRequest();";
    msg += "xhr.open(\"GET\", url, true);";
    msg += "xhr.send();";
    msg += "}";
    msg += "</script>";
    msg += "<button id=\"reboot\" onClick=sendReboot()>REBOOT</button>";
    msg += "<h1>"+ledState+"</h1>";
    msg += DateTime;
    msg += "<h1>" + String(millis()) + "</h1>";
    msg += "</body></html>";
    server.send(200, "text/html", msg);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

const unsigned long NTPintervalSec = 10;
unsigned long NTPLastGetTime = 0; //前回の取得時刻
void getTime(){
  if(NTPLastGetTime+(NTPintervalSec*1000) <= millis()){
    NTPLastGetTime = millis();
    time_t t = now();
    t = localtime(t, 9);
    Serial.println(t);
    Year = year(t);
    Month = month(t);
    Day = day(t);
    Hour = hour(t);
    Minute = minute(t);
    Second = second(t);
  }
}

void setup() {
  pinMode(4,OUTPUT);
  digitalWrite(4,LOW);
  for(int i=0;i<10;i++){
    delay(100);
    digitalWrite(4,HIGH);
    delay(100);
    digitalWrite(4,LOW);
  }
  pinMode(2,INPUT);
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  //WiFi.config(IPAddress(192,168,1,240),IPAddress(192,168,10,48),IPAddress(255,255,255,0),IPAddress(8,8,8,8));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    digitalWrite(4,HIGH);
    delay(500);
    digitalWrite(4,LOW);
    delay(500);
    Serial.println(".");
  }
  Serial.println("WIFIReady");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
  
  ntp_begin(2390); //ntp
  
  server.on("/", handleRoot); 
  server.on("/reboot/", reboot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("ServerReady!");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void LedOn(){
  Serial.println("ON");
  digitalWrite(4,HIGH);
  ledState = "ON";
  timer = millis();
}

void LedOff(){
  Serial.println("OFF");
  digitalWrite(4,LOW);
  ledState = "OFF";
  timer = 0;
}

void reboot(){
  ESP.restart();
}

int nowState = 0;
void loop() {
  if(millis() >= 1000*60*60*24){
    reboot();
  }
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();
  getTime();
  if((19 <= Hour && Hour < 24) || (0 <= Hour && Hour < 5)){
    digitalWrite(4,HIGH);
    ledState = "ON";
  }else{
    digitalWrite(4,LOW);
    ledState = "OFF";
  }
}
