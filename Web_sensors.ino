// BME280/BMP280 test program 
// Using BMx280 Library & sample program https://github.com/PTSolns/BMx280
//
#include <Wire.h>
#include <BMx280.h>
#include <PTSolns_AHTx.h>
#include <DFRobot_ENS160.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "wifidata.h"
//
// ENS160+AHT21
//  SDA GPIO5
//  SCL GPIO4
// C:\Users\yoshiaki\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.1.2\libraries\Wire
// public TwoWire(int SDA, int SCL)

//#define SERIAL_DEBUG 1
//#define INIT_MESSAGE 1
//#define JSON_MESSAGE 1

#define BMSDA   (0)
#define BMSCL   (2)
#define ENSSDA  (5)
#define ENSSCL  (4)
#define ENSaddr (0x53)

#define I2C_COMMUNICATION 
TwoWire ENStw;
DFRobot_ENS160_I2C ENS160(&ENStw, ENSaddr);

PTSolns_AHTx aht;
TwoWire tw;
TwoWire bmxtw;
BMx280 bmx;

const uint8_t  I2C_ADDR      = 0x76;     // Default address
const unsigned POLL_MS       = 2000;     // [mS] How often to read sensor
const float    SEA_LEVEL_HPA = 1013.25f;

ESP8266WebServer server(80); 
// HTML
#define HTML_HEADER "<!doctype html>"\
  "<html><head><meta charset=\"UTF-8\"/>"\
  "<meta name=\"viewport\" content=\"width=device-width\"/>"\
  "<meta http-equiv=\"refresh\" content=\"10;\">" \
  "</head><body>"
#define HTML_FOOTER "</body></html>"

struct BMS_V {
  float temp, hPa, hum, alt;
  int st;
};
struct AHT_V {
  float temp, hum;
  AHTxStatus st;
} ;
struct ENS_V {
  uint8_t status, aqi;
  uint16_t tvoc, eco2;
};

int get_sensorsout(BMS_V *bmsvp, AHT_V *ahtvp, ENS_V *ensvp){
  bmxtw.begin(BMSDA,BMSCL);
  delay(20);
  bmsvp->st = bmx.read280(bmsvp->temp, bmsvp->hPa, bmsvp->hum);
  bmsvp->alt = bmx.readAltitude(SEA_LEVEL_HPA);
  if(bmsvp->st == 0){
#ifdef SERIAL_DEBUG  
    Serial.println("BMx280 read error");
#endif
  }
  tw.begin(ENSSDA,ENSSCL);
  delay(50);
  ahtvp->st = aht.readTemperatureHumidity(ahtvp->temp,ahtvp->hum, 120);
  if (ahtvp->st != AHTX_OK){
#ifdef SERIAL_DEBUG
    Serial.println("AHT21 read error");
#endif
    ENS160.setTempAndHum(/*temperature=*/25.0, /*humidity=*/50.0);
  } else {
    ENS160.setTempAndHum(ahtvp->temp, ahtvp->hum);
  }
  delay(200);
  ensvp->status = ENS160.getENS160Status();
  ensvp->aqi = ENS160.getAQI();
  ensvp->tvoc = ENS160.getTVOC();
  ensvp->eco2 = ENS160.getECO2();
  if (bmsvp->st && ahtvp->st){
    return 0;
  } else {
    return 1;
  } 
}

void web_handler(void) {
  String html, bmx_result, aht_result, ens_result;
  BMS_V bmsval;
  AHT_V ahtval;
  ENS_V ensval;

  if (get_sensorsout(&bmsval, &ahtval, &ensval) == 0){
    return;
  }
  if (bmsval.st) {
    bmx_result = "<table border=\"1\"><tr><th rowspan=\"5\">BME280</th><th>温度</th><td>" + String(bmsval.temp,2) + " ℃</td></tr>";
    if (bmsval.hum) {
      bmx_result += "<tr><th>湿度</th><td>"+ String(bmsval.hum,1) + " %</td></tr>";
    }
    bmx_result += "<tr><th>気圧</th><td>" + String(bmsval.hPa,2) + " </td><tr>" + "<tr><th>Alt</th><td>" 
    + String(bmsval.alt,1) + "m</td><tr></table>";
#ifdef SERIAL_DEBUG
    Serial.print("T="); 
    Serial.print(bmsval.temp,2); 
    Serial.print(" °C  ");
    if (bmx.hasHumidity()) { 
      Serial.print("H="); 
      Serial.print((bmsval.hum),1); 
      Serial.print(" %  "); 
    }
    Serial.print("P="); 
    Serial.print(bmsval.hPa,2); 
    Serial.print(" hPa  ");
    Serial.print("Alt="); 
    Serial.print(bmx.readAltitude(SEA_LEVEL_HPA),1); 
    Serial.println(" m");
#endif
  } else {
    bmx_result = "";
  }
  if (ahtval.st == AHTX_OK) {
    aht_result = "<table border=\"1\"><tr><th rowspan=\"3\">AHT21</th><th>温度</th><td>";
    aht_result += String(ahtval.temp,2) + " ℃</td></tr><tr><th>湿度</th><td>";
    aht_result += String(ahtval.hum,1) + " %</td></tr></table>";
#ifdef SERIAL_DEBUG
    Serial.print("T_C="); Serial.print(ahtval.temp, 2);
    Serial.print(" RH_="); Serial.println(ahtval.hum, 2);
#endif
  } else {
    aht_result = "";
#ifdef SERIAL_DEBUG
    Serial.print("Error="); Serial.println((int)ahtval.st);
#endif
  }
#ifdef SERIAL_DEBUG  
  Serial.print("AQI:"); Serial.print(ensval.aqi);
  Serial.print("  TVOC:"); Serial.print(ensval.tvoc); Serial.print("ppb ");
  Serial.print("eCO2:"); Serial.print(ensval.eco2); Serial.println("ppm");
#endif
  ens_result = "<table border=\"1\"><tr><th rowspan=\"5\">ENS160</th><th>status</th><td>";
  if (ensval.status == 0) {
    ens_result += "normal</td></tr><tr><th>AQI</th><td>";
    ens_result += String(ensval.aqi) + "</td></tr><tr><th>TVOC</th><td>";
    ens_result += String(ensval.tvoc) + " ppb</td><tr><tr><th>eCO2</th><td>";
    ens_result += String(ensval.eco2) + " ppm</td><tr></table>";
  } else {
    if (ensval.status == 1) {
      ens_result += "Warm-up </td></tr><tr><th>AQI</th><td>";
    } else if (ensval.status == 2){
      ens_result += "Initial Start-up </td></tr><tr><th>AQI</th><td>";
    } else {
      ens_result += "invalid </td></tr><tr><th>AQI</th><td>";
    }
    ens_result += "- </td></tr><tr><th>TVOC</th><td>- </td><tr><tr><th>eCO2</th><td>- </td><tr></table>";
  }

  //HTML記述
  html = HTML_HEADER + bmx_result + aht_result + ens_result + HTML_FOOTER;
  html +="";
  server.send(200, "text/html", html);
}

void json_handler(void){
  String json, bmx_result, aht_result, ens_result;
  BMS_V bmsval;
  AHT_V ahtval;
  ENS_V ensval;

  if (get_sensorsout(&bmsval, &ahtval, &ensval) == 0){
    return;
  }
  // {"BME280" : {"temparature":摂氏温度,"humidity":湿度,"pressure":気圧ヘクトパスカル,"altitude":高度} ,
  if (bmsval.st) {
    bmx_result = "{\"BME280\": {\"temparature\":" + String(bmsval.temp,2) + ",";
    if (bmsval.hum) {
      bmx_result += "\"humidity\":"+ String(bmsval.hum,1) + ",";
    }
    bmx_result += "\"pressure\":" + String(bmsval.hPa,2) + "," 
    + "\"altitude\":" + String(bmsval.alt,1) + "} ,";
  } else {
    bmx_result = "";
  }
  //  "AHT21": { "temparature":摂氏温度,"humidity":湿度} ,
  if (ahtval.st == AHTX_OK) {
    aht_result = "\"AHT21\": {\"temparature\":" + String(ahtval.temp,2) + ",\"humidity\":" + String(ahtval.hum,1) + "} ,";
  } else {
    aht_result = "";
  }
  // "ENS160": { "status":"ステータスコード", "AQI":AQI値, "TVOC":揮発性有機化合物濃度, "ECO2":等価二酸化炭素濃度} }
  ens_result = "\"ENS160\": { \"status\":" + String(ensval.status) + ",";
  ens_result += "\"AQI\":" + String(ensval.aqi) + ",\"TVOC\":" + String(ensval.tvoc) + ",\"ECO2\":" + String(ensval.eco2) + "} }";
  json = bmx_result + aht_result + ens_result;

  server.send(200, "application/json", json);
#ifdef JSON_MESSAGE
  Serial.println("output:");
  Serial.println(bmx_result);
  Serial.println(aht_result);
  Serial.println(ens_result);
#endif
} 

void setup() {
  Serial.begin(115200);
  Serial.print("");

  WiFi.begin(WIFISSID, WIFIPASS);
#ifdef INIT_MESSAGE
  Serial.print("Connecting");
#endif
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
#ifdef INIT_MESSAGE
    Serial.print(".");
#endif
  }
#ifdef INIT_MESSAGE  
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
#endif
  bmxtw.begin(BMSDA,BMSCL);
  if (!bmx.beginI2C(I2C_ADDR,&bmxtw)) {
#ifdef INIT_MESSAGE 
    Serial.println("BMx280 not found");
#endif 
    while(1){} 
  }
  bmx.setSampling(1,1,1, 0,0, BMx280::MODE_NORMAL);
#ifdef INIT_MESSAGE
  Serial.print("chipID=0x"); Serial.println(bmx.chipID(), HEX);
  Serial.print("hasHumidity="); Serial.println(bmx.hasHumidity());

  Serial.println("I2C Continuous");
#endif
  delay(20);
  //init AHT21 
  tw.begin(ENSSDA,ENSSCL);
  ENStw.begin(ENSSDA,ENSSCL);
  if (!aht.begin(tw)) {
#ifdef INIT_MESSAGE 
    Serial.println("AHT begin failed");
#endif
    while(1){} 
  }
  // Init ENS160
  while( NO_ERR != ENS160.begin() ){
#ifdef INIT_MESSAGE
    Serial.println("Communication with device failed, please check connection");
#endif
    delay(3000);
  }
  ENS160.setPWRMode(ENS160_STANDARD_MODE);
  ENS160.setTempAndHum(/*temperature=*/25.0, /*humidity=*/50.0);
#ifdef INIT_MESSAGE  
  Serial.println("ENS160 ok!");
#endif
  server.on("/", web_handler);
  server.on("/json/", json_handler);
  server.begin();
#ifdef INIT_MESSAGE
  Serial.println("WebServer start");
#endif
}

void loop() {
  server.handleClient();
}
