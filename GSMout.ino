#include <M5Stack.h>
#include <WiFi.h>
#include <Preferences.h>
#include <NTPClient.h>  // https://github.com/arduino-libraries/NTPClient
#include <ESP32Ping.h> // https://github.com/marian-craciunescu/ESP32Ping
#include <AsyncTCP.h> // https://github.com/me-no-dev/AsyncTCP
#include "ESPAsyncWebServer.h"  // https://github.com/me-no-dev/ESPAsyncWebServer
#include "SPIFFS.h"
#include "FS.h"

#include "AGENCYB14pt7b.h"

#define RX_PIN  16
#define TX_PIN  17
#define RESET_PIN 5

// you can edit this
int web_port = 80;
String web_mainPage = "GSMout";

// user CAN'T edit this
String path = "/" + web_mainPage + ".txt";

int watchCat_wifi = 0;
int watchCat_cell = 0;
int watchCat_ntp = 0;
int watchCat_wan = 0;
int watchCat_sms = 0;
int watchCat_call = 0;

String reg_call_number = "";
unsigned long reg_call_time = 0;

WiFiUDP udp;
Preferences settings;
NTPClient ntp(udp);
AsyncWebServer web(web_port);

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

// сколько раз подстрока встречается в строке
int strstrcnt(char * t, char * w) {
  char * q = t;
  int n = 0;
  while ((q = strstr(q, w)) != NULL) {
   n++;
   q++;
  }
  return n;
}

// удаляет символ из строки
String rchar(String s, char c) {
  String t = "";
  for (int i = 0; i < s.length(); i++) {
    if (s[i] != c)
      t += (char)s[i];
  }
  return t;
}

void debug(String s) {
  M5.Lcd.fillScreen(0);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print(s);
}

char * WiFiAuto(int timeout = 5000) {
  timeout = (timeout < 1000) ? 1000 : timeout;
  File file;
  char * WiFiSSID = NULL;
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
        if (WiFiSSID) delete WiFiSSID;
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

String AT(String s, unsigned long timeout = 10000, String target = "") { // target – желаемое получаемое значение
  unsigned long p = millis();
  String b;
  while (true) {
    b = "";
    if (millis() - p >= timeout) break;
    Serial2.print(s);
    delay(100);
    while (Serial2.available()) {
      b += (char)Serial2.read();
    }
    b = rchar(b, '\r');
    //  debug(b);
    if (strstr(s.c_str(), "AT+CREG?\r")) {
      if (strstr(b.c_str(), "+CREG:")) {
        b = parseString(1, ',', parseString(1, '\n', b));
        if (target == "") {
          break;
        } else {
          if (b == target)
            break;
        }
      }
    } else if (strstr(s.c_str(), "AT+CSQ\r")) {
        if (strstr(b.c_str(), "+CSQ:")) {
          b = parseString(1, ' ', parseString(0, ',', b));
          break;
        }
    } else {
      if (strstr(b.c_str(), "OK"))
        break;
    }
  }
  while (Serial2.available()) {
    Serial2.read();
  }
  return b;
}

bool modemBegin(bool restart = false) {
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);  
  pinMode(RESET_PIN, OUTPUT);

  digitalWrite(RESET_PIN, HIGH);
  
  if (AT("AT\r", 60000) != "") // модем отвечает?
  if (AT("ATE0\r") != "") // ЭХО 1 – вкл (по умолчанию) / 0 – выкл
  if (AT("AT+CPAS\r", 60000, "2") != "") // Информация о состояние модуля 0 – готов к работе 2 – неизвестно 3 – входящий звонок 4 – голосовое соединение
  if (AT("AT+CMGD=1,4\r") != "") // удалить все сообщения
  if (AT("AT+CSCB=1\r") != "") // Приём специальных сообщений 0 – разрешен (по умолчанию) 1 – запрещен
  if (AT("AT+CLIP=1\r") != "") // АОН 1 – включить 0 – выключить
  if (AT("AT+CMGF=1\r") != "") // Текстовый режим 1 – включить 0 – выключить
  if (AT("AT+CSCS=\"GSM\"\r") != "") // кодировка
  if (AT("AT+CNMI=2,2\r") != "") // разрешить индикацию содержимого SMS сообщений.
  if (AT("AT+CREG?\r", 60000, "1")) // Тип регистрации сети Второй параметр: 0 – не зарегистрирован, поиска сети нет 1 – зарегистрирован, домашняя сеть 2 – не зарегистрирован, идёт поиск новой сети 3 – регистрация отклонена 4 – неизвестно 5 – роуминг
  return true;
  return false;
}

void reg(String number, String message = "") {
  if ((reg_call_number == number) && (millis() - reg_call_time < 10000)) return; // избежание регистрации повторного гудка

  number = (strstr(number.c_str(), "+")) ? number : ("+" + number);
  File file = SPIFFS.open(path.c_str(), FILE_APPEND);
  file.print(ntp.getEpochTime());
  file.print('\t');
  file.print(number);
  file.print('\t');
  file.print((message == "") ? "D83DDCDE" : message);
  file.print('\n');
  file.close();

  //watchCat
  if (message != "") {    
    int watchCat_sms_i = settings.getInt("watchCat_sms_i", 0);
    watchCat_sms_i++;
    settings.putInt("watchCat_sms_i", watchCat_sms_i);
  } else {
    int watchCat_call_i = settings.getInt("watchCat_call_i", 0);
    watchCat_call_i++;
    settings.putInt("watchCat_call_i", watchCat_call_i);
    reg_call_number = number;
    reg_call_time = millis();
  }
}

String getReg() {
  File file = SPIFFS.open(path.c_str());
  String t = "var data = `";
  if (file) {
    while (file.available()) {
      t += (char)file.read();
    }
  }
  t += "`;";
  file.close();
  String c_ = "";
  file = SPIFFS.open("/index.html");
  if (file) {
    while (file.available()) {
      c_ += (char)file.read();
    }
  }
  file.close();
  const char * c = c_.c_str();
  char * a_ = strstr(c, "// $data");
  int a = a_ - c;
  String h = "";
  for (int i = 0; i < c_.length(); i++) {
    if ((i >= a) && (i <= a + 7))  {
      h += t;
      t = "";
    } else {
      h += (char)c_[i];
    }
  }

  // watchCat
  settings.putInt("watchCat_sms_i", 0);
  settings.putInt("watchCat_call_i", 0);
  
  return h;
}

String clearReg() {
  File file = SPIFFS.open(path.c_str(), FILE_WRITE);
  if (file) {
    file.close();
    return "Incoming log cleared.";
  }
  return "Err: can't clear incoming log.";
}

void watchCat(bool Idle = false) {
  int x = 34 + 10;
  int y = 62;

  int watchCat_sms_i = 0;
  int watchCat_call_i = 0;

  if (!Idle) {
    // wifi
    int wifi_rssi = WiFi.RSSI();
    wifi_rssi = (!wifi_rssi) ? -INT_MIN : wifi_rssi;
    if (wifi_rssi >= -50)
      watchCat_wifi = 1;
    else if ((wifi_rssi < -50) && (wifi_rssi >= -60))
      watchCat_wifi = 2;
    else if ((wifi_rssi < -60) && (wifi_rssi >= -70))
      watchCat_wifi = 3;
    else
      watchCat_wifi = 0;
  
    // cell
    int cell_rssi = (AT("AT+CSQ\r")).toInt();
    cell_rssi = (AT("AT+CREG?\r", 3000) != "1") ? 99 : cell_rssi;
    if ((cell_rssi >= 31) && (cell_rssi < 99))
      watchCat_cell = 1;
    else if ((cell_rssi >= 10) && (cell_rssi < 31))
      watchCat_cell = 2;
    else if ((cell_rssi >= 3) && (cell_rssi < 10))
      watchCat_cell = 3;
    else
      watchCat_cell = 0;    
  
    // wan
    if (watchCat_wifi)
      watchCat_wan = (Ping.ping("ya.com") || Ping.ping("google.com") || Ping.ping("baidu.com")) ? 1 : 0;
    else
      watchCat_wan = 0;

    // ntp
    if (watchCat_wan)
      watchCat_ntp = (ntp.update()) ? 1 : 0;
    else
      watchCat_ntp = 0;
  
    // sms
    watchCat_sms_i = settings.getInt("watchCat_sms_i", 0);
    if (watchCat_sms_i)
      watchCat_sms = 1;
    else
      watchCat_sms = 0;
  
     // call
    watchCat_call_i = settings.getInt("watchCat_call_i", 0);
    if (watchCat_call_i)
      watchCat_call = 1;
    else
      watchCat_call = 0;
  }
  
  // wifi
  switch (watchCat_wifi) {
    case 1:
      M5.Lcd.drawPngFile(SPIFFS, "/wifi_true-3.png", x, y);
      break;
    case 2:
      M5.Lcd.drawPngFile(SPIFFS, "/wifi_true-2.png", x, y);
      break;
    case 3:
      M5.Lcd.drawPngFile(SPIFFS, "/wifi_true-1.png", x, y);
      break;
    case 0:
      M5.Lcd.drawPngFile(SPIFFS, "/wifi_false.png", x, y);
      break;
  }

  // cell
  switch (watchCat_cell) {
    case 1:
      M5.Lcd.drawPngFile(SPIFFS, "/cell_true-3.png", x, y + 48 + 20);
      break;
    case 2:
      M5.Lcd.drawPngFile(SPIFFS, "/cell_true-2.png", x, y + 48 + 20);
      break;
    case 3:
      M5.Lcd.drawPngFile(SPIFFS, "/cell_true-1.png", x, y + 48 + 20);
      break;
    case 0:
      M5.Lcd.drawPngFile(SPIFFS, "/cell_false.png", x, y + 48 + 20);
      break;
  }

  // ntp
  switch (watchCat_ntp) {
    case 1:
      M5.Lcd.drawPngFile(SPIFFS, "/ntp_true.png", x + 20 + 48, y);
      break;
    case 0:
      M5.Lcd.drawPngFile(SPIFFS, "/ntp_false.png", x + 20 + 48, y);
      break;
  }

  // wan
  switch (watchCat_wan) {
    case 1:
      M5.Lcd.drawPngFile(SPIFFS, "/wan_true.png", x + 20 + 48, y + 48 + 20);
      break;
    case 0:
      M5.Lcd.drawPngFile(SPIFFS, "/wan_false.png", x + 20 + 48, y + 48 + 20);
      break;
  }
  
  // sms
  switch (watchCat_sms) {
    case 1:
      M5.Lcd.drawPngFile(SPIFFS, "/sms_true.png", x + 20 + 48 + 20 + 48, y);
      break;
    case 0:
      M5.Lcd.drawPngFile(SPIFFS, "/sms_false.png", x + 20 + 48 + 20 + 48, y);
      break;
  }
  M5.Lcd.fillRect(x + 20 + 48 + 20 + 48 + 20 + 48, y, 48, 48, TFT_WHITE);
  M5.Lcd.setCursor(x + 20 + 48 + 20 + 48 + 20 + 48, y + 34);
  M5.Lcd.setTextColor(TFT_BLACK);
  if (watchCat_sms_i > 10)
    M5.Lcd.print("10+");
  else
    M5.Lcd.print(watchCat_sms_i);

  // call
  switch (watchCat_call) {
    case 1:
      M5.Lcd.drawPngFile(SPIFFS, "/call_true.png", x + 20 + 48 + 20 + 48, y + 20 + 48);
      break;
    case 0:
      M5.Lcd.drawPngFile(SPIFFS, "/call_false.png", x + 20 + 48 + 20 + 48, y + 20 + 48);
      break;
  }
  M5.Lcd.fillRect(x + 20 + 48 + 20 + 48 + 20 + 48, y + 20 + 48, 48, 48, TFT_WHITE);
  M5.Lcd.setCursor(x + 20 + 48 + 20 + 48 + 20 + 48, y + 20 + 48 + 34);
  M5.Lcd.setTextColor(TFT_BLACK);
  if (watchCat_call_i > 10)
    M5.Lcd.print("10+");
  else
    M5.Lcd.print(watchCat_call_i);

  if (Idle) return;
  if (!watchCat_wifi) WiFiAuto();
  if (!watchCat_cell) {
    digitalWrite(RESET_PIN, LOW);
    delay(1000);
    digitalWrite(RESET_PIN, HIGH);
    delay(60000);
  }
}

void setup() {
  M5.begin();

  settings.begin("settings");
  
  M5.Lcd.setFreeFont(&AGENCYB14pt7b);
  M5.Lcd.println();

  if (SPIFFS.begin(true))
    M5.Lcd.println("SPIFFS OK");
  else
    M5.Lcd.println("SPIFFS Fail");

  M5.Lcd.println("Searching Wi-Fi, please wait...");
  char * ssid = WiFiAuto(); 
  if (ssid)
    M5.Lcd.println("Wi-Fi OK");
  else {
    M5.Lcd.println("Wi-Fi Fail. Restarting 5 s...");
    delay(5000);
    ESP.restart();
  }

  ntp.begin();

  web.on(("/" + web_mainPage).c_str(), [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", getReg()); 
  });
  web.on(("/" + web_mainPage + "0").c_str(), [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", clearReg()); 
  });
  web.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/x-icon");
  });
  web.on("/favicon.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.png", "image/png");
  });
  web.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/html", "404");
  });
  
  web.begin();

  M5.Lcd.println("Starting modem, please wait...");
  if (modemBegin()) {
    M5.Lcd.println("Modem OK");
  } else {
    // аппаратная перезагрузка
    M5.Lcd.println("Modem Fail. Restarting 60 s...");
    digitalWrite(RESET_PIN, LOW);
    delay(60000);
    ESP.restart();
  }

  M5.Lcd.fillScreen(TFT_WHITE);
  M5.Lcd.fillRect(0, 0, 320, 48, TFT_BLACK);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setCursor(10, 32);
  M5.Lcd.print("GSMout powered by M5Stack");
  M5.Lcd.setTextColor(TFT_BLACK);
  M5.Lcd.setCursor(10, 220);
  M5.Lcd.print("http://");
  M5.Lcd.print(WiFi.localIP());
  M5.Lcd.print(":");
  M5.Lcd.print(web_port);
  M5.Lcd.print("/");
  M5.Lcd.print(web_mainPage);

  watchCat(true);
}

void loop() {
  unsigned long watchCat_p = 0;
  unsigned long ntp_p = 0;
  String modem_recived = "";
  while (true) {    
    // watchCat
    if (millis() - watchCat_p >= 5000) {
      watchCat();
      watchCat_p = millis();
    }
      
    // modem
    if (Serial2.available()) {
      modem_recived += (char)Serial2.read();
    } else {
      if (modem_recived == "") break;
      int calls = strstrcnt((char *)modem_recived.c_str(), "+CLIP:");
      int sms = strstrcnt((char *)modem_recived.c_str(), "+CMT:");
      String z = "Calls: " + String(calls) + "\nSMS: " + sms;
      //debug(modem_recived);
      modem_recived = rchar(modem_recived, '\r');
      for (int i = 0; i < strstrcnt((char *)modem_recived.c_str(), "\n"); i++) {
        String n = parseString(i, '\n', modem_recived);
        if (strstr(n.c_str(), "+CLIP:")) {
          String number = parseString(1, '\"', n);
          AT("ATH0\r"); // повесить трубку
          reg(number);
        } else if (strstr(n.c_str(), "+CMT:")) {
          String number = parseString(1, '\"', n);
          i++;
          String message = parseString(i, '\n', modem_recived);
          reg(number, message);
        }
      }
      modem_recived = "";
    }
    
    delay(10);
  }
}
