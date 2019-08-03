#include <NTP.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>


const char* ssid = "aterm-9ac1c7-g";
const char* password = "9a8bd40c42d12";

int Year,Month,Day,Hour,Minute,Second;
String ledState = "OFF";
unsigned long timer=0;

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
    //msg += "document.getElementById(\"LEDstatus\").innerHTML=\"OFF!\";";
    msg += "}";
    msg += "function send(url){";
    msg += "var xhr = new XMLHttpRequest();";
    msg += "xhr.open(\"GET\", url, true);";
    msg += "xhr.send();";
    msg += "}";
    msg += "</script>";
    msg += "<button id=\"on\" onClick=sendOn()>LED ON</button>";
    msg += "<button id=\"off\" onClick=sendOff()>LED OFF</button>";
    msg += "<h1>"+ledState+"</h1>";
    msg += DateTime;
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

void getTime(){
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

void setup() {
  pinMode(4,OUTPUT);
  pinMode(A0,INPUT);
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  //WiFi.config(IPAddress(192,168,1,240),IPAddress(192,168,10,48),IPAddress(255,255,255,0),IPAddress(8,8,8,8));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
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
  server.on("/on/", LedOn);
  server.on("/off/", LedOff);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("ServerReady!");
}

void LedOn(){
  Serial.println("ON");
  digitalWrite(4,HIGH);
  ledState = "ON";
  server.send(200, "text/html","OK");
  timer = millis();
}

void LedOff(){
  Serial.println("OFF");
  digitalWrite(4,LOW);
  ledState = "OFF";
  server.send(200, "text/html","OK");
  timer = 0;
}

int nowState = 0;
void loop() {
  delay(1000);
  server.handleClient();
  MDNS.update();
  if(analogRead(A0) > 512){
    if(nowState == 0){
      getTime();
      nowState = 1;
    }
    digitalWrite(4,HIGH);
    ledState = "ON";
  }else if(millis() - timer > 1000 * 10 * 60/*10分*/){
    nowState = 0;
    digitalWrite(4,LOW);
    ledState = "OFF";
  }
}
