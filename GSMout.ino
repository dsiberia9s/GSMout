#include <M5Stack.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WebServer.h>
#include "SPIFFS.h"
#include "FS.h"

#define RX_PIN  16
#define TX_PIN  17
#define RESET_PIN 5   

WiFiUDP udp;
NTPClient ntp(udp, 7 * 3600);
WebServer web(80);

unsigned long p = 0;

char * s2c(String s) {
  char * t_ = new char[s.length() + 1];
  strcpy(t_, s.c_str());
  char * t = t_;
  delete t_;
  return t;
}

String parseString(int idSeparator, char separator, String str) { 
  String output = "";
  int separatorCout = 0;
  for (int i = 0; i < str.length(); i++) {
    if ((char)str[i] == separator) {
      separatorCout++;
    } else {
      if (separatorCout == idSeparator) {
        output += (char)str[i];
      } else if (separatorCout > idSeparator) {
        break;
      }
    }
  }
  return output;
}

void debug(String s) {
  M5.Lcd.fillScreen(0);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print(s);
}

char * WiFiAuto(int timeout = 5000) {
  timeout = (timeout < 1000) ? 1000 : timeout;
  File file;
  char * WiFiSSID;
  char * WiFiPswd;
  const char * path;
  bool r = false;
  WiFi.disconnect(true);
  delay(100);
  for (int i = 0; i < WiFi.scanNetworks(); i++) {
    File root = SPIFFS.open("/");
    file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String WiFiSSID_ = WiFi.SSID(i);
        WiFiSSID = new char[WiFiSSID_.length() + 1];
        strcpy(WiFiSSID, WiFiSSID_.c_str());
        path = file.name();
        if ((strstr(path, WiFiSSID) != NULL) && strstr(file.name(), ".wifi")) {
          r = true;
        }
        if (r) break;
      }
      file = root.openNextFile();
    }
    root.close();
    if (r) break;
  }
  if (r) {
    file = SPIFFS.open(path);
    String WiFiPswd_ = "";
    while (file.available()){
      char ch = file.read();
      if (ch == 0x0A) break;
      WiFiPswd_ += ch;
    }
    file.close();
    WiFiPswd = new char[WiFiPswd_.length() + 1];
    strcpy(WiFiPswd, WiFiPswd_.c_str());
    WiFi.begin((char *)WiFiSSID, (char *)WiFiPswd);
    delete WiFiPswd;
    int s = 250;
    for (int i = 0; i < timeout; i += s) {
      if (WiFi.status() == WL_CONNECTED) return WiFiSSID;
      delay(s);
    }
  }
  return NULL;
}

String AT(String s, unsigned long timeout = 10000) {
  String b;
  unsigned long p = millis();
  while (true) {
    if (millis() - p >= timeout) break;
    b = "";
    Serial2.print(s);
    delay(2000);
    while (Serial2.available()) {
      b += (char)Serial2.read();
    }
    if (strstr(s2c(s), "+CREG?")) {
      if (strstr(s2c(parseString(1, ',', b)), "1"))
        break;
    } else {
      if (strstr(s2c(b), "OK"))
        break;
    }
  }
  delay(500);
  return b;
}

bool modemBegin() {
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);  
  pinMode(RESET_PIN, OUTPUT);
  //return true;
  
  digitalWrite(RESET_PIN, LOW);
  delay(1000);
  digitalWrite(RESET_PIN, HIGH);
  delay(10000);
  if (AT("ATZ0\r") != "")
  if (AT("AT\r") != "")
  if (AT("AT+CFUN=1,1\r") != "")
  if (AT("ATE0\r") != "")
  if (AT("AT+CSCS=\"GSM\"\r") != "")
  if (AT("AT+CSCB=1\r") != "")
  if (AT("AT+CMGF=1\r") != "")
  if (AT("AT+CMGD=1,4\r") != "")
  if (AT("AT+CNMI=1,2,2,1,0\r") != "")
  if (AT("AT+CREG?\r", 60000) != "")
  return true;
  return false;
}

void reg(String number, String message = "") {
  File file = SPIFFS.open(((message == "") ? "/calls.txt" : "/sms.txt"), FILE_APPEND);
  file.print(ntp.getEpochTime());
  file.print('\t');
  file.print(ntp.getFormattedTime());
  file.print('\t');
  if (message == "") {
    file.print(number);
  } else {
    file.print(number);
    file.print('\t');
    file.print(message);
  }
  file.print('\n');
  file.close();
}

String getReg(String path) {
  File file = SPIFFS.open(path.c_str());
  String t = "<script>var data = `";
  if (file) {
    while (file.available()) {
      t += (char)file.read();
    }
  }
  t += "`;</script>";
  file.close();
  file = SPIFFS.open("/index.html");
  if (file) {
    while (file.available()) {
      t += (char)file.read();
    }
  }
  file.close();
  return t;
}

void modemLive() {
  String b = "";
  while (Serial2.available()) {
    b += (char)Serial2.read();
  }
  if (strstr(s2c(b), "+CLIP:")) {
    String number = parseString(1, '\"', parseString(3, '\r', b));
    AT("ATH0\r"); // повесить трубку
    reg(number);
    //M5.Lcd.print(number);
  } else if (strstr(s2c(b), "+CMT:")) {
    String number = parseString(1, '\"', parseString(1, '\r', b));
    String message = parseString(1, '\n', parseString(1, '\r', parseString(6, '\"', b)));
    reg(number, message);
    //M5.Lcd.print(number);
    //M5.Lcd.print(message);
  }
}

void setup() {
  M5.begin();

  SPIFFS.begin(true);

  M5.Lcd.println("Searching Wi-Fi, please wait...");
  char * ssid = WiFiAuto(); 
  if (ssid) {
    M5.Lcd.println("Connectid to ");
    M5.Lcd.println(ssid);
    M5.Lcd.println(WiFi.localIP());
  } else {
    M5.Lcd.println("Can't connect to WiFi network :(\nTry to REBOOT device");
  }

  M5.Lcd.println("Starting NTP, please wait...");
  ntp.begin();

  M5.Lcd.println("Starting WEB, please wait...");
  web.on("/sms", []() {
    web.send(200, "text/html", getReg("/sms.txt")); 
  });
  web.on("/calls", []() {
    web.send(200, "text/html", getReg("/calls.txt"));
  });
  web.onNotFound([]() {
    web.send(404, "text/html", "404");
  });
  web.begin();

  M5.Lcd.println("Starting modem, please wait...");
  if ( modemBegin() ) 
    M5.Lcd.println("Modem OK");
  else
    M5.Lcd.println("Modem FAIL");
}

void loop() {
  if (millis() - p >= 1000) {
    ntp.update();
    p = millis();
  }
  web.handleClient();
  modemLive();
  delay(10);
}
