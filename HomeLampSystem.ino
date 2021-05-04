/*
説明
このプログラムは実家で動いている夜間に廊下を照らすライトの制御用プログラムである。
手動モードと自動モードの２種類がある。
手動モードでは点灯、消灯と手動点灯時のオフタイマーがある。
自動モードではオン時間とオフ時間を基準に動作する
操作や設定にはGUIの画面を用いる。
また、時刻を基準に動作するので、定期的にNTPから時刻を取得する。
さらに、変数のオーバーフロー防止のために１日に一回再起動をする。

設計ノート
以下の変数を操作するCRUDのUDに該当するHTTPハンドルも定義する。
変数　モード：現在のモードを格納する変数。
変数　ライトステート：現在のLEDの点灯状況を表す。
変数　オンタイム：自動モードで使用するオン時刻を格納する。
変数　オフタイム：自動モードで使用するオフ時刻を格納する。
変数　オフタイマー：手動モードのときに消灯する経過時間を指定する。

2021/4/30
コード改修
REST　APIで制御できるようにする。
操作用GUIの作成とデプロイ
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <NTP.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>

const char *ssid = "aterm-9ac1c7-g";
const char *password = "9a8bd40c42d12";

int Year, Month, Day, Hour, Minute, Second;
String ledState = "OFF", sensorState = "OFF";
unsigned long timer = 0, zero;

ESP8266WebServer server(80);

enum Mode
{
  MODE_MANUAL,
  MODE_AUTO
};
Mode mode = MODE_MANUAL;
enum LightState
{
  LIGHTSTATE_OFF,
  LIGHTSTATE_ON
};
LightState lightState = LIGHTSTATE_OFF;
struct OnTime
{
  unsigned short hour = 0;
  unsigned short minutes = 0;
};
OnTime ontime;
struct OffTime
{
  unsigned short hour = 0;
  unsigned short minutes = 0;
};
OffTime offtime;
unsigned long long offtimer = 0;
void httpHandleSet()
{
  String resMsg = "<meta charset=\"utf-8\"/>";
  String resMsg_t = "";
  if (server.args() > 0)
  {
    for (int i = 0; i < server.args(); i++)
    {
      String paraName = server.argName(i);
      String paraValue = server.arg(i);
      if (paraName == "mode")
      {
        resMsg_t = "mode : ok<br>";
        if (paraValue == "MODE_MANUAL")
          mode = MODE_MANUAL;
        else if (paraValue == "MODE_AUTO")
          mode = MODE_AUTO;
        else
          resMsg_t = "mode : error : モードに指定された値は設定できません。MODE_MANUAL,MODE_AUTO<br>";
        resMsg += resMsg_t;
      }
      else if (paraName == "lightstate")
      {
        if (mode == MODE_MANUAL)
        {
          resMsg_t = "lightstate : ok<br>";
          if (paraValue == "LIGHTSTATE_OFF")
            lightState = LIGHTSTATE_OFF;
          else if (paraValue == "LIGHTSTATE_ON")
            lightState = LIGHTSTATE_ON;
          else
            resMsg_t = "lightstate : error : ライト状態に指定された値は設定できません。LIGHTSTATE_OFF,LIGHTSTATE_ON<br>";
        }
        else
          resMsg_t = "モードがMODE_AUTOのときは、lightstateの設定をできません。";
        resMsg += resMsg_t;
      }
      else if (paraName == "ontime")
      {
        resMsg_t = "ontime : ok<br>";
        char buf[5];
        paraValue.toCharArray(buf, sizeof(buf));
        unsigned short temp = atoi(buf);
        unsigned short hour = temp / 100;
        unsigned short minutes = temp % 100;
        if (hour < 24 && minutes < 60)
        {
          ontime.hour = hour;
          ontime.minutes = minutes;
        }
        else
          resMsg_t = "ontime : error : ライト点灯開始時間に指定された値は設定できません。例 : 11:21 -> 1121<br>";
        resMsg += resMsg_t;
      }
      else if (paraName == "offtime")
      {
        resMsg_t = "offtime : ok<br>";
        char buf[5];
        paraValue.toCharArray(buf, sizeof(buf));
        unsigned short temp = atoi(buf);
        unsigned short hour = temp / 100;
        unsigned short minutes = temp % 100;
        if (hour < 24 && minutes < 60)
        {
          offtime.hour = hour;
          offtime.minutes = minutes;
        }
        else
          resMsg_t = "offtime : error : ライト点灯終了時間に指定された値は設定できません。例 : 11:21 -> 1121<br>";
        resMsg += resMsg_t;
      }
      else if (paraName == "offtimer")
      {
        resMsg_t = "offtimer : ok<br>";
        char buf[8];
        paraValue.toCharArray(buf, sizeof(buf));
        offtimer = atoi(buf);
        resMsg += resMsg_t;
      }
      else
      {
        resMsg_t = paraName + " : 指定されたパラメータは存在しません。<br>";
        resMsg += resMsg_t;
      }
    }
  }
  else
  {
    resMsg_t = "最低でも一つのクエリパラメータを含めてください。<br>";
    resMsg += resMsg_t;
  }
  resMsg += "mode : " + String(mode) + "<br>";
  resMsg += "lightState : " + String(lightState) + "<br>";
  resMsg += "ontime : " + String(ontime.hour) + ":" + String(ontime.minutes) + "<br>";
  resMsg += "offtime : " + String(offtime.hour) + ":" + String(offtime.minutes) + "<br>";
  resMsg += "offtimer : " + String((int)offtimer) + "<br>";
  resMsg += "LED State : " + ledState + "<br>";
  server.send(200, "text/html", resMsg);
}

void handleRoot()
{
  String htmlStr = "";
  htmlStr += "<!DOCTYPE html>\n";
  htmlStr += "<html>\n";
  htmlStr += "    <head>\n";
  htmlStr += "        <meta charset=\"utf-8\">\n";
  htmlStr += "    </head>\n";
  htmlStr += "    <body>\n";
  htmlStr += "        <p>動作モード</p>\n";
  htmlStr += "        <input type=\"radio\" name=\"mode\" value=\"MODE_MANUAL\" checked>手動\n";
  htmlStr += "        <input type=\"radio\" name=\"mode\" value=\"MODE_AUTO\">自動\n";
  htmlStr += "        <br>\n";
  htmlStr += "        <p>ライト状態</p>\n";
  htmlStr += "        <input type=\"radio\" name=\"lightstate\" value=\"LIGHTSTATE_ON\">点灯\n";
  htmlStr += "        <input type=\"radio\" name=\"lightstate\" value=\"LIGHTSTATE_OFF\">消灯\n";
  htmlStr += "        <p>点灯時刻</p>\n";
  htmlStr += "        <input type=\"time\" id=\"ontime\" disabled>\n";
  htmlStr += "        <p>消灯時間</p>\n";
  htmlStr += "        <input type=\"time\" id=\"offtime\" disabled>\n";
  htmlStr += "        <p>オフタイマー</p>\n";
  htmlStr += "        <input type=\"number\" id=\"offtimer\" placeholder=\"ミリ秒\" value=\"0\">　ミリ秒\n";
  htmlStr += "        <br>\n";
  htmlStr += "        <p>設定ボタン押下で設定情報を送信します。</p>\n";
  htmlStr += "        <input type=\"button\" value=\"設定\" id=\"send\">\n";
  htmlStr += "    </body>\n";
  htmlStr += "    <script>\n";
  htmlStr += "        //-------初期化--------\n";
  htmlStr += "        var mode = \"MODE_MANUAL\";\n";
  htmlStr += "        var lightstate = \"LIGHTSTATE_OFF\";\n";
  htmlStr += "        var ontime = 0;\n";
  htmlStr += "        var offtime = 0;\n";
  htmlStr += "        var offtimer = 0;\n";
  htmlStr += "        //---------------------\n";
  htmlStr += "        document.getElementsByName(\"mode\").forEach((ele) => {\n";
  htmlStr += "            ele.addEventListener(\"click\",()=>{\n";
  htmlStr += "                var temp = false;\n";
  htmlStr += "                if(ele.value == \"MODE_AUTO\"){\n";
  htmlStr += "                    document.getElementsByName(\"lightstate\").forEach((ele) => {\n";
  htmlStr += "                        temp = true;\n";
  htmlStr += "                    });\n";
  htmlStr += "                }else{\n";
  htmlStr += "                    temp = false;\n";
  htmlStr += "                }\n";
  htmlStr += "                document.getElementsByName(\"lightstate\").forEach((ele) => {\n";
  htmlStr += "                    ele.disabled = temp;\n";
  htmlStr += "                });\n";
  htmlStr += "                document.getElementById(\"ontime\").disabled = !temp;\n";
  htmlStr += "                document.getElementById(\"offtime\").disabled = !temp;\n";
  htmlStr += "                document.getElementById(\"offtimer\").disabled = temp;\n";
  htmlStr += "            });\n";
  htmlStr += "        });\n";
  htmlStr += "        document.getElementById(\"send\").addEventListener(\"click\",() => {\n";
  htmlStr += "            document.getElementsByName(\"mode\").forEach((ele) => {\n";
  htmlStr += "                if(ele.checked) mode = ele.value;\n";
  htmlStr += "            });\n";
  htmlStr += "            document.getElementsByName(\"lightstate\").forEach((ele) => {\n";
  htmlStr += "                if(ele.checked) lightstate = ele.value;\n";
  htmlStr += "            });\n";
  htmlStr += "            var t = document.getElementById(\"ontime\").value.split(\":\").map(v => parseInt(v));\n";
  htmlStr += "            ontime = t[0] * 100 + t[1];\n";
  htmlStr += "            t = document.getElementById(\"offtime\").value.split(\":\").map(v => parseInt(v));\n";
  htmlStr += "            offtime = t[0] * 100 + t[1];\n";
  htmlStr += "            offtimer = parseInt(document.getElementById(\"offtimer\").value);\n";
  htmlStr += "            console.log(`http://192.168.1.145/set?mode=${mode}&lightstate=${lightstate}&ontime=${ontime}&offtime=${offtime}&offtimer=${offtimer}`);\n";
  htmlStr += "            //fetch(`http://192.168.1.196/set?${}`)\n";
  htmlStr += "            if(mode == \"MODE_AUTO\"){\n";
  htmlStr += "                fetch(`http://192.168.1.145/set?mode=${mode}&ontime=${ontime}&offtime=${offtime}`,{\n";
  htmlStr += "                    mode:\"no-cors\"\n";
  htmlStr += "                })\n";
  htmlStr += "                .then(res => console.log(res));\n";
  htmlStr += "            }else{\n";
  htmlStr += "                fetch(`http://192.168.1.145/set?mode=${mode}&lightstate=${lightstate}&offtimer=${offtimer}`,{\n";
  htmlStr += "                    mode:\"no-cors\"\n";
  htmlStr += "                })\n";
  htmlStr += "                .then(res => console.log(res));\n";
  htmlStr += "            }\n";
  htmlStr += "        });\n";
  htmlStr += "    </script>\n";
  htmlStr += "</html>\n";
  server.send(200, "text/html", htmlStr);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

const unsigned long NTPintervalSec = 10;
unsigned long NTPLastGetTime = 0; //前回の取得時刻
void getTime()
{
  if (abs(millis() - NTPLastGetTime) >= NTPintervalSec * 1000 )
  {
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

void setup()
{
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  for (int i = 0; i < 10; i++)
  {
    delay(100);
    digitalWrite(4, HIGH);
    delay(100);
    digitalWrite(4, LOW);
  }
  pinMode(2, INPUT);
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  //WiFi.config(IPAddress(192,168,1,240),IPAddress(192,168,10,48),IPAddress(255,255,255,0),IPAddress(8,8,8,8));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    digitalWrite(4, HIGH);
    delay(500);
    digitalWrite(4, LOW);
    delay(500);
    Serial.println(".");
  }
  Serial.println("WIFIReady");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266"))
  {
    Serial.println("MDNS responder started");
  }

  ntp_begin(2390); //ntp

  server.on("/", handleRoot);
  server.on("/reboot/", reboot);
  server.on("/set", httpHandleSet);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("ServerReady!");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    }
    else
    { // U_FS
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
    if (error == OTA_AUTH_ERROR)
    {
      Serial.println("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      Serial.println("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      Serial.println("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      Serial.println("Receive Failed");
    }
    else if (error == OTA_END_ERROR)
    {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reboot()
{
  ESP.restart();
}

int nowState = 0;
void loop()
{
  server.handleClient();
  MDNS.update();
  ArduinoOTA.handle();
  getTime();
  if (mode == MODE_AUTO)
  {
    if ((ontime.hour * 60) + (ontime.minutes) <= (offtime.hour * 60) + (offtime.minutes))
    {
      if (
          (ontime.hour * 60) + (ontime.minutes) <= (Hour * 60 + Minute) &&
          (offtime.hour * 60) + (offtime.minutes) > (Hour * 60 + Minute))
      {
        digitalWrite(4, HIGH);
        ledState = "ON_1";
      }
      else
      {
        digitalWrite(4, LOW);
        ledState = "OFF_2";
      }
    }
    else
    {
      if (
          ((ontime.hour * 60) + (ontime.minutes) <= (Hour * 60 + Minute) &&
           (Hour * 60 + Minute) < (23 * 60) + (59)) ||
          (0 <= (Hour * 60 + Minute) &&
           (Hour * 60 + Minute) < (offtime.hour * 60) + (offtime.minutes)))
      {
        digitalWrite(4, HIGH);
        ledState = "ON_3";
      }
      else
      {
        digitalWrite(4, LOW);
        ledState = "OFF_4";
      }
    }
  }
  else
  {
    if (lightState == LIGHTSTATE_ON)
      digitalWrite(4, HIGH);
    else
      digitalWrite(4, LOW);
  }
}