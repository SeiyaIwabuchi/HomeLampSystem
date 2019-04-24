#include <NTP.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>


const char* ssid = "aterm-9ac1c7-g";
const char* password = "9a8bd40c42d12";

int Year,Month,Day,Hour,Minute,Second;

ESP8266WebServer server(8080); 

void handleRoot() {
    Serial.println("receive req: /");
    String msg = "<h1>";
    msg += String(Year);
    msg += ":";
    msg += String(Month);
    msg += ":";
    msg += String(Day);
    msg += ":";
    msg += String(Hour);
    msg += ":";
    msg += String(Minute);
    msg += ":";
    msg += String(Second);
    msg += "</h1>";
    server.send(200, "text/html", msg);
}

void getTime(){
  time_t t = now();
  t = localtime(t, 9);
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
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
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
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  ntp_begin(2390); //ntp
  
  server.on("/", handleRoot); 
  server.begin();
}

void loop() {
  int flag=1;
  ArduinoOTA.handle();
  server.handleClient(); 
  //delay(3000);
  if(analogRead(A0) > 512){
    if(flag == 0){
      getTime();
      flag = 1;
    }
    digitalWrite(4,HIGH);
  }else{
    flag = 0;
    digitalWrite(4,LOW);
  }
}
