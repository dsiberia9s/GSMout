#include <M5Stack.h>

#define RX_PIN  16
#define TX_PIN  17
#define RESET_PIN 5   

char * s2c(String s) {
  char * t = new char[s.length() + 1];
  strcpy(t, s.c_str());
  return t;
}

String parseString(int idSeparator, char separator, String str) { 
  String output = "";
  int separatorCout = 0;
  for (int i = 0; i < str.length(); i++)
  {
    if ((char)str[i] == separator)
    {
      separatorCout++;
    }
    else
    {
      if (separatorCout == idSeparator)
      {
        output += (char)str[i];
      }
      else if (separatorCout > idSeparator)
      {
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

void modemLive() {
  String b = "";
  while (Serial2.available()) {
    b += (char)Serial2.read();
  }
  if (strstr(s2c(b), "+CLIP:")) {
    String number = parseString(1, '\"', parseString(3, '\r', b));
    AT("ATH0\r"); // повесить трубку
    M5.Lcd.print(number);
  } 
  if (strstr(s2c(b), "+CMT:")) {
    String number = parseString(1, '\"', parseString(1, '\r', b));
    String message = parseString(1, '\n', parseString(1, '\r', parseString(6, '\"', b)));
    M5.Lcd.print(number);
    M5.Lcd.print(message);
  }
}

void setup() {
  M5.begin();
  if( modemBegin() ) 
    debug("modem ok");
  else
    debug("modem err");
    
}

void loop() {
  modemLive();
  delay(100);
}
