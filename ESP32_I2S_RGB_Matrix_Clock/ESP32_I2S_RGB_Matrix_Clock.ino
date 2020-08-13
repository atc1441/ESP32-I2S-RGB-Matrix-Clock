#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "FS.h"
#include "SPIFFS.h"
#include "sec.h"
#include "matrix.h"
#include "init.h"
#include "time.h"

struct tm tmstruct;
long refresh, refresh_min;
int brightness, brightness_night, start_time, stop_time;
char dots;
bool is_night_brightness;

RGB64x32MatrixPanel_I2S_DMA display(true);
WebServer server(80);

void setup() {
  static int counter;
  init_matrix();
  Serial.begin(115200);
  display.begin();
  display.setRotation(2);
  display.fillScreen(display.color444(0, 0, 0));
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(display.color444(255, 255, 255));
  display.println("Please\r\n     wait");

  ArduinoOTA.setHostname("RGBclock");
  ArduinoOTA.setPassword("admin");
  ArduinoOTA.onStart([]() {
    disp_update_msg("OTA start");
  });
  ArduinoOTA.onEnd([]() {
    disp_update_msg("OTA end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    disp_update_msg("OTA " + String((progress / (total / 100))) + "%");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    disp_update_msg("OTA Err: " + String(error));
  });
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    disp_update_msg("Connect\r\ncount:\r\n" + String(counter++));
    delay(500);
  }
  ArduinoOTA.begin();
  configTime(3600, 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  if (!SPIFFS.begin(true)) {
    disp_update_msg("SPIFFS Mount Failed"); delay(10000);
  }
  brightness = readFile(SPIFFS, "/brightness.txt").toInt();
  brightness_night = readFile(SPIFFS, "/brightness_night.txt").toInt();
  start_time = readFile(SPIFFS, "/brightness_start.txt").toInt();
  stop_time = readFile(SPIFFS, "/brightness_stop.txt").toInt();
  if (brightness == 0 || brightness_night == 0 || start_time == 0 || stop_time == 0) {
    brightness = 16;
    start_time = 22;
    stop_time = 7;
    writeFile(SPIFFS, "/brightness.txt", String(brightness).c_str());
    writeFile(SPIFFS, "/brightness_night.txt", String(brightness_night).c_str());
    writeFile(SPIFFS, "/brightness_start.txt", String(start_time).c_str());
    writeFile(SPIFFS, "/brightness_stop.txt", String(stop_time).c_str());
  }
  display.setPanelBrightness(brightness);
}

void loop() {
  server.handleClient();
  if (millis() - refresh >= 1000) {
    refresh = millis();
    ArduinoOTA.handle();
    display.flipDMABuffer();
    display.fillScreen(display.color444(0, 0, 0));
    getLocalTime(&tmstruct);
    display.setCursor(3, (64 / 2) - 19);
    display.setTextSize(2);
    display.setTextColor(display.color444(0, 255, 0));
    dots = (dots == ':') ? ' ' : ':';
    display.printf("%02i%c%02i\r\n", tmstruct.tm_hour, dots, tmstruct.tm_min);
    display.drawLine(2, 31, 2 + tmstruct.tm_sec, 31, display.color565(0, 255, 0));
    display.setCursor(2, (64 / 2) + 7);
    display.setTextSize(1);
    display.setTextColor(display.color444(255, 0, 255));
    display.printf("%02i.%02i.%04i\r\n", tmstruct.tm_mday, ( tmstruct.tm_mon) + 1, (tmstruct.tm_year) + 1900);
    display.showDMABuffer();
    check_light(tmstruct.tm_hour);
  }
}

void disp_update_msg(String msg) {
  display.flipDMABuffer();
  display.fillScreen(display.color444(0, 0, 0));
  display.setCursor(0, 0);
  display.println(msg);
  display.showDMABuffer();
}

void check_light(int hour) {
  if ((hour >= start_time) || (hour <= stop_time)) {
    if (!is_night_brightness) {
      is_night_brightness = true;
      display.setPanelBrightness(brightness_night);
    }
  } else {
    if (is_night_brightness) {
      is_night_brightness = false;
      display.setPanelBrightness(brightness);
    }
  }
}

void handleRoot() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "brightness") {
      brightness = server.arg(i).toInt();
      writeFile(SPIFFS, "/brightness.txt", String(brightness).c_str());
    }
    if (server.argName(i) == "brightness_night") {
      brightness_night = server.arg(i).toInt();
      writeFile(SPIFFS, "/brightness_night.txt", String(brightness_night).c_str());
    }
    if (server.argName(i) == "start") {
      start_time = server.arg(i).toInt();
      writeFile(SPIFFS, "/brightness_start.txt", String(start_time).c_str());
    }
    if (server.argName(i) == "stop") {
      stop_time = server.arg(i).toInt();
      writeFile(SPIFFS, "/brightness_stop.txt", String(stop_time).c_str());
    }
  }
  server.send(200, "text/plain", "OK Day: " + String(brightness) + " , Night: " + String(brightness_night) + "\r\nStart: " + String(start_time) + " , Stop: " + String(stop_time));
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

String readFile(fs::FS &fs, const char * path) {
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    return String();
  }
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_WRITE);
  if (file) file.print(message);
}
