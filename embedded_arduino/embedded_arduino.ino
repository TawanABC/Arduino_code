#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266Firebase.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <SoftwareSerial.h>
#include <UrlEncode.h>
#include <ESP8266HTTPClient.h>


EspSoftwareSerial::UART DataSerial;

// change to ur wifi
#define WIFI_SSID "iPhonettt"
#define WIFI_PASSWORD "abcxyz123"

// do not change
#define API_KEY "9aftrIGwlJFQOtzyyUog8zrj3eF6gZhv1m0ojjBQ64y"
#define REFERENCE_URL "https://embedded-summ-default-rtdb.asia-southeast1.firebasedatabase.app/"

Firebase firebase(REFERENCE_URL);

unsigned long sendDataPrevMillis = 0;
unsigned long sendNotifyPrevMillis = 0;
unsigned long sendDailyNotiPrevMillis = 0;
float humidity = 50;
float light = 50;
float temp = 50;
String Data[3] = { "0", "0", "0" };


WiFiUDP udp;
const char* ntpServerName = "pool.ntp.org";
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
unsigned long lastNTPSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000;  // Sync every 1 hour
const int GMT_OFFSET = 7 * 3600;                  // GMT+7 offset in seconds

String phoneNumber = "66989829510";
String apiKey = "7153866";

void sendMessage(String message){

  // Data to send with HTTP POST
  String url = "http://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);
  WiFiClient client;    
  HTTPClient http;
  http.begin(client, url);

  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Send HTTP POST request
  int httpResponseCode = http.POST(url);
  if (httpResponseCode == 200){
    Serial.println("Message sent successfully");
  }
  else{
    Serial.println("Error sending the message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

  // Free resources
  http.end();
}


void getTimeFromNTP() {
  udp.begin(8888);
  sendNTPPacket();
  delay(1000);

  if (udp.parsePacket()) {
    udp.read(packetBuffer, NTP_PACKET_SIZE);
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;
    setTime(epoch + GMT_OFFSET);  // Add GMT offset
    lastNTPSync = millis();
    Serial.println("Time synchronized.");
  }
  udp.stop();
}

void sendNTPPacket() {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  udp.beginPacket(ntpServerName, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void setup() {
  Serial.begin(115200);
  DataSerial.begin(115200, EspSoftwareSerial::SWSERIAL_8N1, D7, D8, false, 100, 100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  
  sendMessage("เปิดเครื่องแล้ว");
  getTimeFromNTP();
}

void Update_Data() {
  if (DataSerial.available() > 0) {
    Data[0] = "";
    Data[1] = "";
    Data[2] = "";
    uint8_t state = 0;
    while (DataSerial.available()) {
      if (state >= 3) break;
      char c = DataSerial.read();
      if (c == '|') {
        // Serial.println(Data[state]);
        state++;
      } else {
        Data[state] += c;
      }
    }
  }
}

// void notifyLine(String s) {
//   LINE.setToken(API_KEY);
//   LINE.notify(s);
//   // LINE.notifyPicture("https://preview.ibb.co/j6G51n/capture25610417181915334.png");
//   Serial.println("noti success");
// }

void loop() {
  // sendMessage("in loop");
  if ((millis() - sendDataPrevMillis > 3000 || sendDataPrevMillis == 0)) {
    Update_Data();

    sendDataPrevMillis = millis();
    humidity = Data[0].toFloat() / 100.0;
    temp = Data[1].toFloat() / 100.0;
    light = Data[2].toFloat();
    // double lux = ((500.0 * 3.7 / (light / 1000.0)) - 500) / 2.0;  // equation
    double lux = light / 45.96;
    Serial.print("light : ");
    Serial.println(lux);
    Serial.print("humidity : ");
    Serial.println(humidity);
    Serial.print("temp : ");
    Serial.println(temp);

    // error detection
    bool error = false;
    if (light > 5000 or light == 0 or humidity > 100 or humidity == 0 or temp == 0 or temp > 100) {
      sendMessage("error detected");
      error = true;
    }
    if (!error) {
      firebase.setFloat("humidity", humidity);
      firebase.setFloat("light", lux);
      firebase.setFloat("temperature", temp);
    }


    if (millis() - lastNTPSync > NTP_SYNC_INTERVAL) {
      getTimeFromNTP();
    }

    String hoursStr = hour() < 10 ? "0" + String(hour()) : String(hour());
    String minutesStr = minute() < 10 ? "0" + String(minute()) : String(minute());
    String secondsStr = second() < 10 ? "0" + String(second()) : String(second());
    String timeStr = hoursStr + ":" + minutesStr;
    firebase.setString("lastUpdate", hoursStr + ":" + minutesStr + ":" + secondsStr);
    Serial.println(error);
    Serial.println(light);
    Serial.println(millis() - sendNotifyPrevMillis > 15000);
    if (!error and (light > 3800 || humidity < 30 || temp > 38) and (millis() - sendNotifyPrevMillis > 15000 or sendNotifyPrevMillis == 0)) {
      sendNotifyPrevMillis = millis();
      Serial.println("asdasdasd");
      sendMessage("แจ้งเตือนการรดน้ำต้นไม้");
    }
    if ((timeStr == "06:00" || timeStr == "17:06") && (millis() - sendDailyNotiPrevMillis >= 1000 * 3600 * 12 || sendDailyNotiPrevMillis == 0)) {
      sendMessage("แจ้งเตือนการรดน้ำต้นไม้ประจำเวลา");
      sendDailyNotiPrevMillis = millis();
    }
  }
}
