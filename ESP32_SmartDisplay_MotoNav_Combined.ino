/*
  ============================================================
  ESP32 SMARTDISPLAY + MOTONAV
  Single-file all-in-one sketch
  Hardware:
  - ESP32 DevKit V1
  - OLED SSD1306 128x64 I2C (SDA=21, SCL=22, addr=0x3C)
  - Buzzer GPIO25
  - No physical buttons
  ============================================================
*/
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════
// HARDWARE & KONFIGURASI DASAR
// ═══════════════════════════════════════════════════════════
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDR     0x3C

#define SDA_PIN         21
#define SCL_PIN         22
#define BUZZER_PIN      25

#define WIFI_MODE_ID     0
#define NAV_MODE_ID      1

#define BT_DEVICE_NAME  "MotoNav"

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;     // WIB
const int daylightOffset_sec = 0;

// WeatherAPI.com config
const char* weatherApiKey   = "9cefcc6eb6564c29aa611710261805";
const char* weatherLocation  = "Arcawinangun";
const char* cityDisplay      = "Banyumas";

// ═══════════════════════════════════════════════════════════
// OBJEK GLOBAL
// ═══════════════════════════════════════════════════════════
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BluetoothSerial SerialBT;
WebServer server(80);
Preferences prefs;

// ═══════════════════════════════════════════════════════════
// PROTOTYPES
// ═══════════════════════════════════════════════════════════
void bootScreen(const String &line1, const String &line2);
void loadPrefs();
void savePrefs();

void ensureWiFiMode();
void connectWiFiFromPrefs(bool showBootMessages);
void startWebServer();
void stopWebServer();

void syncNTP();
void updateTimeFromNTP();
bool fetchWeatherFromAPI();
void applyOLEDContrast(uint8_t level);
void setDisplayOn(bool on);

void saveWeatherToPrefs();
void loadWeatherFromPrefs();

void drawSmartDisplay();
void drawClockScreen();
void drawWeatherScreen();
void drawRobotEyesScreen();
void drawInfoScreen();

void updateAutoMode();
void updateSleepState();
bool isInSleepWindow(int startHour, int endHour, int currentHour);
void updateAlarmState();
void startAlarmBeepPattern(uint8_t count, uint16_t onMs, uint16_t offMs);
void updateBuzzerTask();
void buzzerOn(uint16_t freq);
void buzzerOff();
void beepTest();

void setupWebServer();
String buildHTML();
String jsonEscape(const String &in);
String formatDateString();
String formatTimeString();
String wifiSignalBars(int rssi);

void handleWebStatus();
void handleWebMode();
void handleWebAuto();
void handleWebWeather();
void handleWebBrightness();
void handleWebAutoBright();
void handleWebAlarm();
void handleWebSleep();
void handleWebBeep();
void handleWebSwitchNav();

void switchToNavMode();
void switchToWiFiMode();

void enterNavMode();
void enterWiFiMode();
void updateNavMode();
void clearNavState();
void parseBTLine(const String &line);
void processBTInput();
void drawNavWaitingScreen();
void drawNavConnectedFlash();
void drawNavArrowScreen();
void drawNavIdleScreen();
void drawNavWaitScreen();
void drawNavArrivedScreen();
void drawNavGPSWeakScreen();
void drawNavIconArrowUp(int cx, int cy, int sz);
void drawNavIconArrowRight(int cx, int cy, int sz);
void drawNavIconArrowLeft(int cx, int cy, int sz);
void drawNavIconUTurn(int cx, int cy, int sz);
void drawNavIconRoundabout(const String &type, int cx, int cy, int sz);
void drawNavIconArrived(int cx, int cy, int sz);
void drawNavIconWarningOutline(const String &type, int cx, int cy, int sz);
void drawCenteredText(const String &txt, int y, uint8_t size);
void drawScrollingText(const String &txt, int x, int y, uint8_t size, int maxWidth);

String getCurrentModeText();
String getCurrentSleepText();
String getCurrentAlarmText();
void saveModeToPrefs(uint8_t mode);
uint8_t readModeFromPrefs();

String readPrefsString(const char* key, const String &fallback);
float readPrefsFloat(const char* key, float fallback);
int readPrefsInt(const char* key, int fallback);
bool readPrefsBool(const char* key, bool fallback);

void saveWifiCredentials(const String &ssid, const String &pass);
bool ensureWifiCredentialsFromSerial();
bool parseCredentialLine(const String &line, String &ssidOut, String &passOut);

String buildWeatherURL();
String getNavAction(const String &packetType);

void updateOLEDForMode();
void updateSystemDisplay();
void handleSleepBrightness();
void applyBrightnessForTime();
void saveSystemSettings();
void loadSystemSettings();

void processWebEndpointsCommentary();

// ═══════════════════════════════════════════════════════════
// STATE GLOBAL
// ═══════════════════════════════════════════════════════════
enum SystemMode : uint8_t { WIFI_MODE = 0, NAV_MODE = 1 };
SystemMode systemMode = WIFI_MODE;

bool wifiStarted = false;
bool webStarted = false;
bool webRoutesConfigured = false;
bool btEnabled = false;

String wifiSSID;
String wifiPASS;

// Clock/time
int curHour = 0, curMin = 0, curSec = 0;
int curDay = 1, curMon = 1, curYear = 2026, curWday = 0;
const char* dayNames[] = {"Minggu","Senin","Selasa","Rabu","Kamis","Jumat","Sabtu"};
const char* monNames[] = {"","Jan","Feb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des"};

// Weather cache
float wTemp = 0.0f;
float wHumid = 0.0f;
float wFeels = 0.0f;
float wWind = 0.0f;
String wDesc = "Belum ada data";
String wLastUpdate = "--:--";
bool weatherValid = false;
unsigned long lastWeatherFetchMs = 0;

// Display mode
uint8_t displayMode = 0;      // 0..3
bool autoMode = true;
unsigned long lastModeRotateMs = 0;
const unsigned long modeRotateIntervalMs = 8000;

// Sleep / brightness
bool autoBrightness = true;
int manualBrightness = 220;
int dayBrightness = 220;
int nightBrightness = 10;
bool oledSleepState = false;
int sleepStartHour = 22;
int sleepEndHour = 6;

// Alarm
bool alarmEnabled = false;
int alarmHour = 7;
int alarmMin = 0;
int alarmLastTriggeredYMD = -1;

// Buzzer pattern
bool buzzerPatternActive = false;
uint8_t buzzerPatternCount = 0;
uint8_t buzzerPatternDone = 0;
uint16_t buzzerPatternOnMs = 120;
uint16_t buzzerPatternOffMs = 80;
bool buzzerPatternOnState = false;
unsigned long buzzerPatternTs = 0;
const uint16_t buzzerFreq = 2200;

// WiFi reconnect support
unsigned long lastWiFiRetryMs = 0;
const unsigned long wifiRetryIntervalMs = 15000;

// Bluetooth nav state
String btBuffer = "";
String navType = "";
String navDistance = "";
String navRoad = "";
String idleSpeed = "--";
String waitMessage = "";
bool navGPSWeak = false;
bool navArrived = false;
bool btClientPrev = false;
unsigned long lastBTDataMs = 0;
unsigned long lastScrollMs = 0;
unsigned long lastBlinkMs = 0;
unsigned long lastNavFrameMs = 0;
bool blinkState = false;
int scrollIndex = 0;
const unsigned long navTimeoutMs = 30000;
const unsigned long scrollIntervalMs = 350;
const unsigned long blinkIntervalMs = 500;
const unsigned long navFrameIntervalMs = 28; // ~35fps

// Robot animation
float eyeLx = 38, eyeRx = 90;
float eyeY = 32;
float pupilLx = 38, pupilRx = 90;
float pupilY = 32;
float pupilTargetLx = 38, pupilTargetRx = 90;
float pupilTargetY = 32;
int pupilDirIndex = 0;
unsigned long robotMoveMs = 0;
unsigned long robotBlinkMs = 0;
unsigned long robotBlinkDurationMs = 0;
unsigned long robotNextBlinkGapMs = 0;
bool robotBlinking = false;
bool robotEyesOpen = true;
bool robotMouthSmile = true;

// ═══════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("[BOOT] ESP32 SmartDisplay + MotoNav start");
  randomSeed(esp_random());

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[BOOT] OLED tidak ditemukan");
    while (true) { delay(500); }
  }

  prefs.begin("motonav", false);
  loadPrefs();
  loadWeatherFromPrefs();

  bootScreen("Booting ESP32...", "Mohon tunggu");
  delay(700);

  systemMode = (SystemMode)readModeFromPrefs();
  if (systemMode != WIFI_MODE && systemMode != NAV_MODE) systemMode = WIFI_MODE;

  String savedSSID = prefs.getString("ssid", "");
  String savedPASS = prefs.getString("pass", "");

  if (savedSSID.length() == 0) {
    bootScreen("Setup WiFi via Serial", "SSID:namaWifi,PASS:password");
    Serial.println("[BOOT] WiFi credential belum ada");
    while (!ensureWifiCredentialsFromSerial()) {
      delay(50);
    }
    savedSSID = prefs.getString("ssid", "");
    savedPASS = prefs.getString("pass", "");
  }

  wifiSSID = savedSSID;
  wifiPASS = savedPASS;

  if (systemMode == NAV_MODE) {
    Serial.println("[BOOT] Last mode = NAV_MODE");
    enterNavMode();
  } else {
    Serial.println("[BOOT] Last mode = WIFI_MODE");
    enterWiFiMode();
  }

  Serial.println("[BOOT] Setup selesai");
}

// ═══════════════════════════════════════════════════════════
// LOOP
// ═══════════════════════════════════════════════════════════
void loop() {
  updateBuzzerTask();

  if (systemMode == WIFI_MODE) {
    if (WiFi.status() == WL_CONNECTED) {
      updateTimeFromNTP();
      if (!webStarted) {
        Serial.println("[WIFI] Connected again, starting web server");
        syncNTP();
        fetchWeatherFromAPI();
        startWebServer();
      }
    } else if (millis() - lastWiFiRetryMs > wifiRetryIntervalMs) {
      lastWiFiRetryMs = millis();
      Serial.println("[WIFI] Retry reconnect");
      WiFi.disconnect(false, false);
      WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());
    }

    updateSleepState();
    applyBrightnessForTime();
    updateAlarmState();
    updateAutoMode();

    if (webStarted) server.handleClient();

    if (!oledSleepState) {
      drawSmartDisplay();
    } else {
      setDisplayOn(false);
    }
  } else {
    updateNavMode();
  }
}

// ═══════════════════════════════════════════════════════════
// UTILITIES
// ═══════════════════════════════════════════════════════════
String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

void bootScreen(const String &line1, const String &line2) {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(5, 3);
  display.print("ESP32 SmartDisplay");
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 22);
  display.print(line1);
  display.setCursor(5, 38);
  display.print(line2);
  display.display();
}

String readPrefsString(const char* key, const String &fallback) {
  return prefs.getString(key, fallback);
}
float readPrefsFloat(const char* key, float fallback) {
  return prefs.getFloat(key, fallback);
}
int readPrefsInt(const char* key, int fallback) {
  return prefs.getInt(key, fallback);
}
bool readPrefsBool(const char* key, bool fallback) {
  return prefs.getBool(key, fallback);
}

void saveModeToPrefs(uint8_t mode) {
  prefs.putUChar("mode", mode);
}

uint8_t readModeFromPrefs() {
  return prefs.getUChar("mode", WIFI_MODE);
}

void saveWifiCredentials(const String &ssid, const String &pass) {
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  wifiSSID = ssid;
  wifiPASS = pass;
  Serial.println("[BOOT] WiFi credentials saved to NVS");
}

bool parseCredentialLine(const String &line, String &ssidOut, String &passOut) {
  String s = line;
  s.trim();
  if (!(s.startsWith("SSID:") && s.indexOf(",PASS:") > 0)) return false;

  int commaPos = s.indexOf(",PASS:");
  ssidOut = s.substring(5, commaPos);
  passOut = s.substring(commaPos + 6);
  ssidOut.trim();
  passOut.trim();
  return ssidOut.length() > 0;
}

bool ensureWifiCredentialsFromSerial() {
  static String serialLine;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialLine.length() == 0) continue;
      String s, p;
      if (parseCredentialLine(serialLine, s, p)) {
        saveWifiCredentials(s, p);
        bootScreen("WiFi tersimpan", s);
        Serial.println("[BOOT] WiFi credentials accepted");
        serialLine = "";
        delay(700);
        return true;
      } else {
        Serial.println("[BOOT] Format salah. Gunakan: SSID:namaWifi,PASS:passwordWifi");
        bootScreen("Format salah", "SSID:namaWifi,PASS:password");
        serialLine = "";
        return false;
      }
    } else {
      serialLine += c;
      if (serialLine.length() > 120) serialLine = "";
    }
  }

  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 1200) {
    lastMsg = millis();
    Serial.println("[BOOT] Kirim format: SSID:namaWifi,PASS:passwordWifi");
  }
  return false;
}

String formatTimeString() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", curHour, curMin, curSec);
  return String(buf);
}

String formatDateString() {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s, %02d %s %04d", dayNames[curWday], curDay, monNames[curMon], curYear);
  return String(buf);
}

String wifiSignalBars(int rssi) {
  if (rssi == 0) return "----";
  if (rssi > -55) return "████";
  if (rssi > -67) return "███░";
  if (rssi > -78) return "██░░";
  return "█░░░";
}

String getCurrentModeText() {
  return systemMode == WIFI_MODE ? "WIFI_MODE" : "NAV_MODE";
}

String getCurrentSleepText() {
  return oledSleepState ? "SLEEP" : "AWAKE";
}

String getCurrentAlarmText() {
  char buf[12];
  snprintf(buf, sizeof(buf), "%02d:%02d", alarmHour, alarmMin);
  return String(buf);
}

bool isInSleepWindow(int startHour, int endHour, int currentHour) {
  if (startHour == endHour) return false;
  if (startHour < endHour) return (currentHour >= startHour && currentHour < endHour);
  return (currentHour >= startHour || currentHour < endHour);
}

void applyOLEDContrast(uint8_t level) {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(level);
}

void setDisplayOn(bool on) {
  static bool lastOn = true;
  if (on == lastOn) return;
  lastOn = on;
  if (on) display.ssd1306_command(SSD1306_DISPLAYON);
  else    display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void buzzerOn(uint16_t freq) {
  ledcWriteTone(0, freq);
  ledcWrite(0, 128);
}

void buzzerOff() {
  ledcWriteTone(0, 0);
  ledcWrite(0, 0);
}

void startAlarmBeepPattern(uint8_t count, uint16_t onMs, uint16_t offMs) {
  buzzerPatternActive = true;
  buzzerPatternCount = count;
  buzzerPatternDone = 0;
  buzzerPatternOnMs = onMs;
  buzzerPatternOffMs = offMs;
  buzzerPatternOnState = false;
  buzzerPatternTs = 0;
  Serial.println("[ALARM] Beep pattern started");
}

void updateBuzzerTask() {
  if (!buzzerPatternActive) return;

  unsigned long now = millis();

  if (buzzerPatternTs == 0) {
    buzzerPatternOnState = true;
    buzzerPatternTs = now;
    buzzerOn(buzzerFreq);
    return;
  }

  if (buzzerPatternOnState) {
    if (now - buzzerPatternTs >= buzzerPatternOnMs) {
      buzzerOff();
      buzzerPatternOnState = false;
      buzzerPatternTs = now;
      buzzerPatternDone++;
      if (buzzerPatternDone >= buzzerPatternCount) {
        buzzerPatternActive = false;
        return;
      }
    }
  } else {
    if (now - buzzerPatternTs >= buzzerPatternOffMs) {
      buzzerPatternOnState = true;
      buzzerPatternTs = now;
      buzzerOn(buzzerFreq);
    }
  }
}

void beepTest() {
  startAlarmBeepPattern(2, 120, 90);
}

void updateAutoMode() {
  if (!autoMode) return;
  if (millis() - lastModeRotateMs >= modeRotateIntervalMs) {
    lastModeRotateMs = millis();
    displayMode = (displayMode + 1) % 4;
    Serial.println("[WEB] Auto display mode rotate");
  }
}

void updateSleepState() {
  int h = curHour;
  bool sleepNow = isInSleepWindow(sleepStartHour, sleepEndHour, h);
  if (sleepNow != oledSleepState) {
    oledSleepState = sleepNow;
    Serial.print("[SLEEP] ");
    Serial.println(oledSleepState ? "OLED off" : "OLED on");
    setDisplayOn(!oledSleepState);
  }
}

void applyBrightnessForTime() {
  if (!autoBrightness) {
    applyOLEDContrast((uint8_t)manualBrightness);
    return;
  }
  int level = oledSleepState ? 0 : ((curHour >= 6 && curHour < 18) ? dayBrightness : nightBrightness);
  level = constrain(level, 0, 255);
  applyOLEDContrast((uint8_t)level);
}

void updateAlarmState() {
  if (!alarmEnabled || oledSleepState) return;
  if (curHour == alarmHour && curMin == alarmMin && curSec < 3) {
    int ymd = curYear * 10000 + curMon * 100 + curDay;
    if (alarmLastTriggeredYMD != ymd) {
      alarmLastTriggeredYMD = ymd;
      Serial.println("[ALARM] Triggered");
      startAlarmBeepPattern(5, 120, 90);
    }
  }
}

void syncNTP() {
  if (WiFi.status() != WL_CONNECTED) return;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("[WIFI] NTP sync requested");

  struct tm t;
  unsigned long start = millis();
  while (!getLocalTime(&t, 250) && millis() - start < 10000) {
    delay(50);
  }
  if (getLocalTime(&t, 100)) {
    Serial.println("[WIFI] NTP sync OK");
    curHour = t.tm_hour;
    curMin = t.tm_min;
    curSec = t.tm_sec;
    curDay = t.tm_mday;
    curMon = t.tm_mon + 1;
    curYear = t.tm_year + 1900;
    curWday = t.tm_wday;
  } else {
    Serial.println("[WIFI] NTP sync timeout");
  }
}

void updateTimeFromNTP() {
  struct tm t;
  if (getLocalTime(&t, 10)) {
    curHour = t.tm_hour;
    curMin = t.tm_min;
    curSec = t.tm_sec;
    curDay = t.tm_mday;
    curMon = t.tm_mon + 1;
    curYear = t.tm_year + 1900;
    curWday = t.tm_wday;
  }
}

void saveWeatherToPrefs() {
  prefs.putFloat("w_temp", wTemp);
  prefs.putFloat("w_humid", wHumid);
  prefs.putFloat("w_feels", wFeels);
  prefs.putFloat("w_wind", wWind);
  prefs.putString("w_desc", wDesc);
  prefs.putString("w_last", wLastUpdate);
  prefs.putBool("w_valid", weatherValid);
}

void loadWeatherFromPrefs() {
  wTemp = prefs.getFloat("w_temp", 0.0f);
  wHumid = prefs.getFloat("w_humid", 0.0f);
  wFeels = prefs.getFloat("w_feels", 0.0f);
  wWind = prefs.getFloat("w_wind", 0.0f);
  wDesc = prefs.getString("w_desc", "Belum ada data");
  wLastUpdate = prefs.getString("w_last", "--:--");
  weatherValid = prefs.getBool("w_valid", false);
}

void loadPrefs() {
  systemMode = (SystemMode)readModeFromPrefs();
  wifiSSID = prefs.getString("ssid", "");
  wifiPASS = prefs.getString("pass", "");
  displayMode = prefs.getUChar("dispMode", 0);
  autoMode = prefs.getBool("autoMode", true);
  autoBrightness = prefs.getBool("autoBright", true);
  manualBrightness = prefs.getInt("manBright", 220);
  dayBrightness = prefs.getInt("dayBright", 220);
  nightBrightness = prefs.getInt("nightBright", 10);
  sleepStartHour = prefs.getInt("sleepStart", 22);
  sleepEndHour = prefs.getInt("sleepEnd", 6);
  alarmEnabled = prefs.getBool("alarmEn", false);
  alarmHour = prefs.getInt("alarmHour", 7);
  alarmMin = prefs.getInt("alarmMin", 0);
  Serial.println("[BOOT] Preferences loaded");
}

void saveSystemSettings() {
  prefs.putUChar("dispMode", displayMode);
  prefs.putBool("autoMode", autoMode);
  prefs.putBool("autoBright", autoBrightness);
  prefs.putInt("manBright", manualBrightness);
  prefs.putInt("dayBright", dayBrightness);
  prefs.putInt("nightBright", nightBrightness);
  prefs.putInt("sleepStart", sleepStartHour);
  prefs.putInt("sleepEnd", sleepEndHour);
  prefs.putBool("alarmEn", alarmEnabled);
  prefs.putInt("alarmHour", alarmHour);
  prefs.putInt("alarmMin", alarmMin);
  prefs.putUChar("mode", systemMode);
}

String buildWeatherURL() {
  String url = "http://api.weatherapi.com/v1/current.json?key=";
  url += weatherApiKey;
  url += "&q=";
  url += weatherLocation;
  url += "&aqi=no&lang=id";
  return url;
}

bool fetchWeatherFromAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Tidak bisa ambil cuaca: WiFi offline");
    return false;
  }

  HTTPClient http;
  String url = buildWeatherURL();
  Serial.println("[WEB] Fetch weather");
  http.begin(url);
  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    Serial.print("[WEB] Weather HTTP code: ");
    Serial.println(code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<8192> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("[WEB] Weather JSON error: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonObject cur = doc["current"];
  if (cur.isNull()) return false;

  wTemp = cur["temp_c"] | 0.0f;
  wHumid = cur["humidity"] | 0.0f;
  wFeels = cur["feelslike_c"] | 0.0f;
  wWind = cur["wind_kph"] | 0.0f;
  wDesc = String((const char*)(cur["condition"]["text"] | "N/A"));
  weatherValid = true;

  struct tm t;
  if (getLocalTime(&t, 5)) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    wLastUpdate = String(buf);
  } else {
    wLastUpdate = "--:--";
  }

  saveWeatherToPrefs();
  lastWeatherFetchMs = millis();

  Serial.println("[WEB] Weather updated");
  return true;
}

void stopWebServer() {
  if (webStarted) {
    server.stop();
    webStarted = false;
    Serial.println("[WEB] Server stopped");
  }
}

void startWebServer() {
  if (!webRoutesConfigured) {
    setupWebServer();
    webRoutesConfigured = true;
  }
  server.begin();
  webStarted = true;
  Serial.println("[WEB] Server started");
}

void connectWiFiFromPrefs(bool showBootMessages) {
  if (wifiSSID.isEmpty()) {
    Serial.println("[WIFI] SSID kosong");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  if (showBootMessages) {
    bootScreen("Connecting WiFi...", wifiSSID);
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    if (showBootMessages) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.print("Connecting WiFi...");
      display.setCursor(0, 16);
      display.print(wifiSSID);
      display.setCursor(0, 32);
      display.print("Tunggu...");
      display.display();
    }
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WIFI] Connected IP: ");
    Serial.println(WiFi.localIP());
    if (showBootMessages) bootScreen("WiFi Connected", WiFi.localIP().toString());
    syncNTP();
    updateTimeFromNTP();
    fetchWeatherFromAPI();
    startWebServer();
    wifiStarted = true;
  } else {
    Serial.println("[WIFI] Gagal konek, masuk mode offline");
    if (showBootMessages) bootScreen("WiFi Failed", "Mode Offline");
    wifiStarted = false;
  }

  saveModeToPrefs(WIFI_MODE);
  systemMode = WIFI_MODE;
}

void ensureWiFiMode() {
  if (WiFi.status() != WL_CONNECTED && millis() - lastWiFiRetryMs > wifiRetryIntervalMs) {
    lastWiFiRetryMs = millis();
    Serial.println("[WIFI] Rekoneksi background");
    WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());
  }
}

void enterWiFiMode() {
  Serial.println("[MODE] Enter WIFI_MODE");
  if (btEnabled) {
    SerialBT.end();
    btEnabled = false;
    Serial.println("[BT] SerialBT stopped");
  }

  delay(500);
  WiFi.mode(WIFI_STA);
  connectWiFiFromPrefs(true);
  systemMode = WIFI_MODE;
  saveModeToPrefs(WIFI_MODE);
  saveSystemSettings();
  setDisplayOn(true);
  applyBrightnessForTime();
}

void enterNavMode() {
  Serial.println("[MODE] Enter NAV_MODE");
  stopWebServer();
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true, true);
    delay(500);
    WiFi.mode(WIFI_OFF);
    Serial.println("[WIFI] WiFi OFF");
  }

  if (!btEnabled) {
    SerialBT.begin(BT_DEVICE_NAME);
    btEnabled = true;
    Serial.println("[BT] SerialBT started as MotoNav");
  }

  clearNavState();
  systemMode = NAV_MODE;
  saveModeToPrefs(NAV_MODE);
  saveSystemSettings();
  bootScreen("MotoNav Aktif", "BT: MotoNav");
  setDisplayOn(true);
}

void switchToNavMode() {
  Serial.println("[MODE] switchToNavMode()");
  enterNavMode();
}

void switchToWiFiMode() {
  Serial.println("[MODE] switchToWiFiMode()");
  enterWiFiMode();
}

void handleWebStatus() {
  StaticJsonDocument<512> doc;
  doc["time"] = formatTimeString();
  doc["date"] = formatDateString();
  doc["mode"] = (int)displayMode;
  doc["auto"] = autoMode;
  doc["sleep"] = oledSleepState;
  doc["temp"] = wTemp;
  doc["humidity"] = wHumid;
  doc["feels"] = wFeels;
  doc["wind"] = wWind;
  doc["desc"] = wDesc;
  doc["last"] = wLastUpdate;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -999;
  doc["alarm_en"] = alarmEnabled;
  doc["alarm"] = getCurrentAlarmText();
  doc["autoBright"] = autoBrightness;
  doc["brightness"] = autoBrightness ? (oledSleepState ? 0 : ((curHour >= 6 && curHour < 18) ? dayBrightness : nightBrightness)) : manualBrightness;
  doc["systemMode"] = getCurrentModeText();
  doc["wifi"] = (WiFi.status() == WL_CONNECTED);
  doc["sleepWindow"] = String(sleepStartHour) + "-" + String(sleepEndHour);

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleWebMode() {
  if (server.hasArg("val")) {
    displayMode = constrain(server.arg("val").toInt(), 0, 3);
    autoMode = false;
    saveSystemSettings();
    Serial.print("[WEB] displayMode=");
    Serial.println(displayMode);
  }
  server.send(200, "text/plain", "OK");
}

void handleWebAuto() {
  if (server.hasArg("val")) {
    autoMode = (server.arg("val") == "1");
    saveSystemSettings();
    Serial.print("[WEB] autoMode=");
    Serial.println(autoMode ? "1" : "0");
  }
  server.send(200, "text/plain", autoMode ? "1" : "0");
}

void handleWebWeather() {
  bool ok = fetchWeatherFromAPI();
  StaticJsonDocument<256> doc;
  doc["ok"] = ok;
  doc["temp"] = wTemp;
  doc["humidity"] = wHumid;
  doc["feels"] = wFeels;
  doc["wind"] = wWind;
  doc["desc"] = wDesc;
  doc["last"] = wLastUpdate;
  String out;
  serializeJson(doc, out);
  server.send(ok ? 200 : 500, "application/json", out);
}

void handleWebBrightness() {
  if (server.hasArg("val")) {
    manualBrightness = constrain(server.arg("val").toInt(), 0, 255);
    autoBrightness = false;
    saveSystemSettings();
    applyOLEDContrast((uint8_t)manualBrightness);
    Serial.print("[WEB] manualBrightness=");
    Serial.println(manualBrightness);
  }
  server.send(200, "text/plain", "OK");
}

void handleWebAutoBright() {
  if (server.hasArg("val")) {
    autoBrightness = (server.arg("val") == "1");
    saveSystemSettings();
    Serial.print("[WEB] autoBrightness=");
    Serial.println(autoBrightness ? "1" : "0");
  }
  server.send(200, "text/plain", autoBrightness ? "1" : "0");
}

void handleWebAlarm() {
  if (server.hasArg("hour")) alarmHour = constrain(server.arg("hour").toInt(), 0, 23);
  if (server.hasArg("min")) alarmMin = constrain(server.arg("min").toInt(), 0, 59);
  if (server.hasArg("en")) alarmEnabled = (server.arg("en") == "1");
  saveSystemSettings();
  Serial.print("[ALARM] Set ");
  Serial.print(alarmHour);
  Serial.print(":");
  Serial.print(alarmMin);
  Serial.print(" en=");
  Serial.println(alarmEnabled ? "1" : "0");
  server.send(200, "text/plain", "OK");
}

void handleWebSleep() {
  if (server.hasArg("start")) sleepStartHour = constrain(server.arg("start").toInt(), 0, 23);
  if (server.hasArg("end")) sleepEndHour = constrain(server.arg("end").toInt(), 0, 23);
  saveSystemSettings();
  Serial.print("[SLEEP] ");
  Serial.print(sleepStartHour);
  Serial.print(" - ");
  Serial.println(sleepEndHour);
  server.send(200, "text/plain", "OK");
}

void handleWebBeep() {
  beepTest();
  server.send(200, "text/plain", "Beep");
}

void handleWebSwitchNav() {
  server.send(200, "text/plain", "Switching to NAV_MODE");
  delay(100);
  switchToNavMode();
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html; charset=utf-8", buildHTML());
  });
  server.on("/status", HTTP_GET, handleWebStatus);
  server.on("/mode", HTTP_GET, handleWebMode);
  server.on("/auto", HTTP_GET, handleWebAuto);
  server.on("/weather", HTTP_GET, handleWebWeather);
  server.on("/brightness", HTTP_GET, handleWebBrightness);
  server.on("/autobright", HTTP_GET, handleWebAutoBright);
  server.on("/alarm", HTTP_GET, handleWebAlarm);
  server.on("/sleep", HTTP_GET, handleWebSleep);
  server.on("/beep", HTTP_GET, handleWebBeep);
  server.on("/switchnav", HTTP_GET, handleWebSwitchNav);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not Found");
  });
}

String buildHTML() {
  String html = R"HTML(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 SmartDisplay + MotoNav</title>
<style>
:root{
  --bg:#0b0f14; --card:#131a22; --card2:#0f141b; --text:#e8eef7;
  --muted:#8b97a7; --line:#223040; --accent:#4fc3f7; --orange:#ff9800;
  --green:#34d399; --red:#fb7185;
}
*{box-sizing:border-box} body{
  margin:0; font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;
  background:linear-gradient(180deg,#081018,#0b0f14); color:var(--text);
}
.header{
  padding:18px 16px 8px; position:sticky; top:0; backdrop-filter:blur(10px);
  background:rgba(11,15,20,.82); border-bottom:1px solid var(--line); z-index:10;
}
h1{font-size:20px; margin:0 0 6px}
.sub{font-size:12px; color:var(--muted)}
.wrap{padding:14px; max-width:1100px; margin:0 auto}
.grid{display:grid; gap:12px; grid-template-columns:repeat(auto-fit,minmax(240px,1fr));}
.card{
  background:linear-gradient(180deg,var(--card),var(--card2));
  border:1px solid var(--line); border-radius:18px; padding:14px;
  box-shadow:0 10px 28px rgba(0,0,0,.18);
}
.card h2{font-size:15px; margin:0 0 10px}
.row{display:flex; gap:8px; flex-wrap:wrap; align-items:center}
.badge{display:inline-flex; padding:4px 8px; border-radius:999px; font-size:12px; background:#1b2430; color:var(--muted)}
.big{font-size:34px; font-weight:700; line-height:1}
.small{font-size:12px; color:var(--muted)}
.btn{
  border:0; border-radius:12px; padding:10px 12px; background:#233041; color:var(--text);
  font-weight:600; cursor:pointer; width:100%;
}
.btn:hover{filter:brightness(1.08)}
.btn.primary{background:var(--accent); color:#00121d}
.btn.orange{background:var(--orange); color:#1b1000; font-size:16px; padding:14px 16px}
.btn.ghost{background:#1c2530}
.btn.active{outline:2px solid var(--accent)}
input[type="time"],input[type="number"],input[type="range"]{
  width:100%; accent-color:var(--accent)
}
label{display:block; font-size:12px; color:var(--muted); margin:8px 0 6px}
.footerBtn{margin-top:16px}
.kv{display:grid; grid-template-columns:1fr auto; gap:6px; font-size:13px}
.kv div:nth-child(odd){color:var(--muted)}
hr{border:0; border-top:1px solid var(--line); margin:12px 0}
.mini{font-size:13px}
@media (max-width:480px){ h1{font-size:18px} .big{font-size:30px} }
</style>
</head>
<body>
<div class="header">
  <h1>ESP32 SmartDisplay + MotoNav</h1>
  <div class="sub">Dashboard realtime — WiFi mode aktif, navigasi motor lewat Bluetooth.</div>
</div>

<div class="wrap">
  <div class="grid">

    <div class="card">
      <h2>Waktu Realtime</h2>
      <div class="big" id="time">--:--:--</div>
      <div class="small" id="date">--</div>
      <div class="row" style="margin-top:10px">
        <span class="badge" id="modeTxt">MODE: --</span>
        <span class="badge" id="sleepTxt">SLEEP: --</span>
      </div>
    </div>

    <div class="card">
      <h2>Cuaca</h2>
      <div class="big" id="temp">--°C</div>
      <div class="mini" id="desc">--</div>
      <hr>
      <div class="kv">
        <div>Humidity</div><div id="hum">--%</div>
        <div>Feels like</div><div id="feels">--°C</div>
        <div>Wind</div><div id="wind">-- kph</div>
        <div>Update</div><div id="last">--</div>
      </div>
      <div style="margin-top:10px">
        <button class="btn primary" onclick="call('/weather')">Update Cuaca</button>
      </div>
    </div>

    <div class="card">
      <h2>Mode OLED</h2>
      <div class="row">
        <button class="btn ghost" onclick="setMode(0)">Jam</button>
        <button class="btn ghost" onclick="setMode(1)">Cuaca</button>
        <button class="btn ghost" onclick="setMode(2)">Mata</button>
        <button class="btn ghost" onclick="setMode(3)">Info</button>
      </div>
      <label><input type="checkbox" id="autoMode" onchange="toggleAuto(this.checked)"> Auto ganti tiap 8 detik</label>
    </div>

    <div class="card">
      <h2>Brightness</h2>
      <label><input type="checkbox" id="autoBright" onchange="toggleAutoBright(this.checked)"> Auto brightness</label>
      <label>Manual brightness</label>
      <input id="bright" type="range" min="0" max="255" value="220" oninput="liveBright(this.value)" onchange="setBright(this.value)">
      <div class="small">Nilai contrast OLED tersimpan ke NVS.</div>
    </div>

    <div class="card">
      <h2>Alarm Harian</h2>
      <label><input type="checkbox" id="alarmEn" onchange="saveAlarm()"> Alarm aktif</label>
      <label>Jam alarm</label>
      <input id="alarmTime" type="time" onchange="saveAlarm()">
      <div class="small">Buzzer berbunyi 5x saat jam cocok.</div>
    </div>

    <div class="card">
      <h2>Auto Sleep</h2>
      <label>Mulai tidur</label>
      <input id="sleepStart" type="time" onchange="saveSleep()">
      <label>Bangun</label>
      <input id="sleepEnd" type="time" onchange="saveSleep()">
    </div>

    <div class="card">
      <h2>Buzzer Test</h2>
      <button class="btn" onclick="call('/beep')">Tes buzzer</button>
    </div>

    <div class="card">
      <h2>Info ESP32</h2>
      <div class="kv">
        <div>IP</div><div id="ip">--</div>
        <div>RSSI</div><div id="rssi">--</div>
        <div>Mode aktif</div><div id="sysmode">--</div>
        <div>Status</div><div id="status">--</div>
      </div>
    </div>

  </div>

  <div class="card footerBtn" style="margin-top:14px; border-color:#5f3600">
    <h2>Kontrol MotoNav</h2>
    <button class="btn orange" onclick="call('/switchnav')">🏍️ Mulai MotoNav</button>
    <div class="small" style="margin-top:8px">Tombol ini memindahkan ESP32 ke mode Bluetooth navigasi.</div>
  </div>
</div>

<script>
async function call(url){
  try{
    await fetch(url);
    await load();
  }catch(e){}
}
function setMode(v){ call('/mode?val=' + v); }
function toggleAuto(v){ call('/auto?val=' + (v ? 1 : 0)); }
function toggleAutoBright(v){ call('/autobright?val=' + (v ? 1 : 0)); }
function liveBright(v){ document.getElementById('bright').title = v; }
function setBright(v){ call('/brightness?val=' + v); }
function saveAlarm(){
  const t = document.getElementById('alarmTime').value || '07:00';
  const [h,m] = t.split(':');
  const en = document.getElementById('alarmEn').checked ? 1 : 0;
  call(`/alarm?hour=${h}&min=${m}&en=${en}`);
}
function saveSleep(){
  const s = document.getElementById('sleepStart').value || '22:00';
  const e = document.getElementById('sleepEnd').value || '06:00';
  const [sh] = s.split(':'); const [eh] = e.split(':');
  call(`/sleep?start=${sh}&end=${eh}`);
}
async function load(){
  try{
    const r = await fetch('/status');
    const j = await r.json();
    document.getElementById('time').textContent = j.time || '--:--:--';
    document.getElementById('date').textContent = j.date || '--';
    document.getElementById('temp').textContent = (j.temp ?? '--') + '°C';
    document.getElementById('desc').textContent = j.desc || '--';
    document.getElementById('hum').textContent = (j.humidity ?? '--') + '%';
    document.getElementById('feels').textContent = (j.feels ?? '--') + '°C';
    document.getElementById('wind').textContent = (j.wind ?? '--') + ' kph';
    document.getElementById('last').textContent = j.last || '--';
    document.getElementById('ip').textContent = j.ip || '--';
    document.getElementById('rssi').textContent = (j.rssi ?? '--');
    document.getElementById('modeTxt').textContent = 'MODE OLED: ' + (j.mode ?? '--');
    document.getElementById('sleepTxt').textContent = 'SLEEP: ' + ((j.sleep) ? 'ON' : 'OFF');
    document.getElementById('sysmode').textContent = j.systemMode || '--';
    document.getElementById('status').textContent = (j.wifi ? 'WiFi connected' : 'Offline');
    document.getElementById('autoMode').checked = !!j.auto;
    document.getElementById('autoBright').checked = !!j.autoBright;
    document.getElementById('alarmEn').checked = !!j.alarm_en;
    document.getElementById('alarmTime').value = j.alarm || '07:00';
    document.getElementById('sleepStart').value = String(j.sleepWindow || '22-6').split('-')[0].padStart(2,'0') + ':00';
    document.getElementById('sleepEnd').value = String(j.sleepWindow || '22-6').split('-')[1].padStart(2,'0') + ':00';
    if(typeof j.brightness !== 'undefined') document.getElementById('bright').value = j.brightness;
  }catch(e){}
}
setInterval(load, 1000); load();
</script>
</body>
</html>
)HTML";
  return html;
}

void drawCenteredText(const String &txt, int y, uint8_t size) {
  display.setTextSize(size);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - (int)w) / 2, y);
  display.print(txt);
}

void drawScrollingText(const String &txt, int x, int y, uint8_t size, int maxWidth) {
  display.setTextSize(size);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  if ((int)w <= maxWidth) {
    display.setCursor(x, y);
    display.print(txt);
    return;
  }

  String padded = txt + "   ";
  int chars = padded.length();
  int windowChars = max(1, maxWidth / (6 * size));
  if (scrollIndex >= chars) scrollIndex = 0;

  String visible;
  for (int i = 0; i < windowChars; i++) {
    visible += padded[(scrollIndex + i) % chars];
  }
  display.setCursor(x, y);
  display.print(visible);
}

// ═══════════════════════════════════════════════════════════
// SMARTDISPLAY
// ═══════════════════════════════════════════════════════════
void drawSmartDisplay() {
  switch (displayMode) {
    case 0: drawClockScreen(); break;
    case 1: drawWeatherScreen(); break;
    case 2: drawRobotEyesScreen(); break;
    case 3: drawInfoScreen(); break;
    default: drawClockScreen(); break;
  }
}

void drawClockScreen() {
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 250) return;
  lastDraw = millis();

  display.clearDisplay();

  char timeBuf[6];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", curHour, curMin);

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(3);
  display.setCursor(6, 4);
  display.print(timeBuf);

  display.setTextSize(1);
  char secBuf[4];
  snprintf(secBuf, sizeof(secBuf), "%02d", curSec);
  display.setCursor(107, 30);
  display.print(secBuf);

  display.drawLine(0, 38, 128, 38, SSD1306_WHITE);

  display.setTextSize(1);
  String dateStr = formatDateString();
  drawCenteredText(dateStr, 43, 1);

  display.setCursor(2, 54);
  display.print(WiFi.status() == WL_CONNECTED ? wifiSignalBars(WiFi.RSSI()) : "----");

  display.setCursor(62, 54);
  display.print(autoMode ? "[AUTO]" : "[MAN]");

  display.setTextSize(1);
  display.setCursor(94, 54);
  if (weatherValid) {
    display.print(String(wTemp, 0));
    display.print("C");
  } else {
    display.print("--C");
  }

  display.display();
}

void drawWeatherScreen() {
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 500) return;
  lastDraw = millis();

  display.clearDisplay();
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(3, 2);
  display.print("CUACA - ");
  display.print(cityDisplay);

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(3);
  display.setCursor(4, 16);
  display.print(String(wTemp, 0));
  display.setTextSize(2);
  display.setCursor(46, 20);
  display.print((char)247);
  display.print("C");

  display.setTextSize(1);
  String desc = wDesc;
  if (desc.length() > 24) desc = desc.substring(0, 24);
  display.setCursor(4, 43);
  display.print(desc);

  display.setCursor(4, 53);
  display.print("Hum:");
  display.print(String(wHumid, 0));
  display.print("% Feels:");
  display.print(String(wFeels, 0));
  display.print("C");

  display.setCursor(92, 53);
  display.print("Wind:");
  display.print(String(wWind, 0));

  display.setCursor(84, 14);
  display.print(wLastUpdate);

  display.display();
}

void drawRobotEyesScreen() {
  unsigned long now = millis();
  if (now - lastNavFrameMs < navFrameIntervalMs) return;
  lastNavFrameMs = now;

  if (now - robotMoveMs > 220) {
    robotMoveMs = now;
    int dirs[6][2] = {
      { -1, -1 }, { 0, -1 }, { 1, -1 },
      { -1, 0 },  { 1, 0 },  { 0, 1 }
    };
    pupilDirIndex = random(0, 6);
    float dx = dirs[pupilDirIndex][0] * 5.0f;
    float dy = dirs[pupilDirIndex][1] * 4.0f;
    pupilTargetLx = eyeLx + dx;
    pupilTargetRx = eyeRx + dx;
    pupilTargetY = eyeY + dy;
    robotMouthSmile = random(0, 2) == 0;
  }

  if (!robotBlinking) {
    if (robotNextBlinkGapMs == 0) robotNextBlinkGapMs = random(2000, 7000);
    if (now - robotBlinkMs > robotNextBlinkGapMs) {
      robotBlinking = true;
      robotBlinkDurationMs = 140;
      robotBlinkMs = now;
      robotNextBlinkGapMs = 0;
    }
  } else if (now - robotBlinkMs > robotBlinkDurationMs) {
    robotBlinking = false;
    robotBlinkMs = now;
  }

  pupilLx += (pupilTargetLx - pupilLx) * 0.15f;
  pupilRx += (pupilTargetRx - pupilRx) * 0.15f;
  pupilY  += (pupilTargetY - pupilY) * 0.15f;

  display.clearDisplay();

  // wajah / mata
  display.drawCircle((int)eyeLx, (int)eyeY, 16, SSD1306_WHITE);
  display.drawCircle((int)eyeRx, (int)eyeY, 16, SSD1306_WHITE);

  if (robotBlinking) {
    display.fillRect((int)eyeLx - 13, (int)eyeY - 2, 26, 4, SSD1306_WHITE);
    display.fillRect((int)eyeRx - 13, (int)eyeY - 2, 26, 4, SSD1306_WHITE);
  } else {
    display.fillCircle((int)pupilLx, (int)pupilY, 5, SSD1306_WHITE);
    display.fillCircle((int)pupilRx, (int)pupilY, 5, SSD1306_WHITE);
  }

  // highlight
  if (!robotBlinking) {
    display.fillCircle((int)pupilLx - 2, (int)pupilY - 2, 1, SSD1306_BLACK);
    display.fillCircle((int)pupilRx - 2, (int)pupilY - 2, 1, SSD1306_BLACK);
  }

  // mulut
  if (robotMouthSmile) {
    display.drawLine(56, 48, 60, 50, SSD1306_WHITE);
    display.drawLine(60, 50, 68, 50, SSD1306_WHITE);
    display.drawLine(68, 50, 72, 48, SSD1306_WHITE);
  } else {
    display.drawLine(56, 48, 72, 48, SSD1306_WHITE);
  }

  display.setTextSize(1);
  drawCenteredText("[ SMART EYES ]", 0, 1);
  if (autoMode) drawCenteredText("AUTO MODE", 54, 1);

  display.display();
}

void drawInfoScreen() {
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 500) return;
  lastDraw = millis();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("INFO SISTEM");

  display.setCursor(0, 12);
  display.print("IP   : ");
  display.print(WiFi.localIP().toString());

  display.setCursor(0, 22);
  display.print("RSSI : ");
  display.print(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -999);

  display.setCursor(0, 32);
  display.print("MODE : ");
  display.print(getCurrentModeText());

  display.setCursor(0, 42);
  display.print("SLEEP: ");
  display.print(getCurrentSleepText());

  display.setCursor(0, 52);
  display.print("TEMP : ");
  display.print(String(wTemp, 1));
  display.print("C");

  display.display();
}

// ═══════════════════════════════════════════════════════════
// NAVIGATION MODE
// ═══════════════════════════════════════════════════════════
void clearNavState() {
  btBuffer = "";
  navType = "";
  navDistance = "";
  navRoad = "";
  idleSpeed = "--";
  waitMessage = "";
  navGPSWeak = false;
  navArrived = false;
  btClientPrev = false;
  lastBTDataMs = millis();
  lastScrollMs = millis();
  lastBlinkMs = millis();
  lastNavFrameMs = 0;
  blinkState = false;
  scrollIndex = 0;
}

void drawNavWaitingScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("[ MOTONAV v2.0 ]", 10, 1);
  drawCenteredText("Pair di HP Anda", 26, 1);
  drawCenteredText("Bluetooth: MotoNav", 40, 1);
  display.drawRoundRect(10, 52, 108, 10, 3, SSD1306_WHITE);
  display.fillRect(12, 54, 14, 6, SSD1306_WHITE);
  display.display();
}

void drawNavConnectedFlash() {
  bootScreen("HP Terhubung!", "Siap navigasi :)");
}

void drawNavIdleScreen() {
  display.clearDisplay();
  drawCenteredText("IDLE", 6, 1);

  display.setTextSize(3);
  String sp = idleSpeed;
  if (sp.length() == 0) sp = "--";
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(sp, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.print(sp);

  display.setTextSize(1);
  drawCenteredText("km/h", 48, 1);
  display.drawLine(0, 58, 128, 58, SSD1306_WHITE);
  drawCenteredText("Menunggu navigasi...", 60, 1);
  display.display();
}

void drawNavWaitScreen() {
  display.clearDisplay();
  drawCenteredText("GPS LEMAH", 4, 1);
  display.drawRoundRect(16, 18, 96, 28, 4, SSD1306_WHITE);
  int levels = blinkState ? 3 : 2;
  for (int i = 0; i < 4; i++) {
    int h = 5 + i * 5;
    int x = 24 + i * 16;
    if (i < levels) display.fillRect(x, 42 - h, 8, h, SSD1306_WHITE);
    else display.drawRect(x, 42 - h, 8, h, SSD1306_WHITE);
  }
  display.setTextSize(1);
  drawCenteredText(waitMessage.length() ? waitMessage : "WAIT GPS", 52, 1);
  display.display();
}

void drawNavArrivedScreen() {
  display.clearDisplay();
  display.drawCircle(64, 24, 18, SSD1306_WHITE);
  display.drawLine(57, 24, 62, 30, SSD1306_WHITE);
  display.drawLine(62, 30, 72, 18, SSD1306_WHITE);
  display.drawLine(48, 50, 80, 50, SSD1306_WHITE);
  drawCenteredText("TIBA", 52, 1);
  display.display();
}

void drawNavArrowScreen() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  String title = navType;
  if (title.startsWith("BUNDARAN")) title = "BUNDARAN";
  drawCenteredText(title, 0, 1);

  int cx = 64;
  int cy = 26;
  int sz = 14;

  if (navType == "LURUS") {
    drawNavIconArrowUp(cx, cy, sz);
  } else if (navType == "KANAN") {
    drawNavIconArrowRight(cx, cy, sz);
  } else if (navType == "KIRI") {
    drawNavIconArrowLeft(cx, cy, sz);
  } else if (navType == "BALIK") {
    drawNavIconUTurn(cx, cy, sz);
  } else if (navType.startsWith("BUNDARAN")) {
    drawNavIconRoundabout(navType, cx, cy, sz);
  } else if (navType.startsWith("SIAP-")) {
    drawNavIconWarningOutline(navType, cx, cy, sz);
  } else {
    drawNavIconArrowUp(cx, cy, sz);
  }

  display.setTextSize(2);
  drawCenteredText(navDistance.length() ? navDistance : "--", 43, 2);

  display.setTextSize(1);
  String roadLine = navRoad;
  if (roadLine.length() > 0) {
    display.setCursor(4, 57);
    int maxWidth = 120;
    if (roadLine.length() > 21) {
      if (millis() - lastScrollMs >= scrollIntervalMs) {
        lastScrollMs = millis();
        scrollIndex++;
      }
      String padded = roadLine + "   ";
      if (scrollIndex >= padded.length()) scrollIndex = 0;
      int visibleChars = max(1, maxWidth / 6);
      String visible;
      for (int i = 0; i < visibleChars; i++) {
        visible += padded[(scrollIndex + i) % padded.length()];
      }
      display.print(visible);
    } else {
      display.print(roadLine);
    }
  }

  display.display();
}

void drawNavGPSWeakScreen() {
  drawNavWaitScreen();
}

void drawNavIconArrowUp(int cx, int cy, int sz) {
  display.fillTriangle(cx, cy - sz, cx - sz * 6 / 10, cy, cx + sz * 6 / 10, cy, SSD1306_WHITE);
  display.fillRect(cx - 2, cy, 4, sz, SSD1306_WHITE);
}

void drawNavIconArrowRight(int cx, int cy, int sz) {
  display.fillTriangle(cx + sz, cy, cx, cy - sz * 6 / 10, cx, cy + sz * 6 / 10, SSD1306_WHITE);
  display.fillRect(cx - sz / 2, cy - 2, sz / 2 + 2, 4, SSD1306_WHITE);
  display.fillRect(cx - 2, cy, 4, sz / 2 + 1, SSD1306_WHITE);
}

void drawNavIconArrowLeft(int cx, int cy, int sz) {
  display.fillTriangle(cx - sz, cy, cx, cy - sz * 6 / 10, cx, cy + sz * 6 / 10, SSD1306_WHITE);
  display.fillRect(cx, cy - 2, sz / 2 + 2, 4, SSD1306_WHITE);
  display.fillRect(cx - 2, cy, 4, sz / 2 + 1, SSD1306_WHITE);
}

void drawNavIconUTurn(int cx, int cy, int sz) {
  display.drawCircle(cx - 6, cy + 3, 7, SSD1306_WHITE);
  display.fillRect(cx - 9, cy - 5, 4, 13, SSD1306_WHITE);
  display.fillTriangle(cx + 4, cy - 1, cx + 12, cy - 5, cx + 12, cy + 3, SSD1306_WHITE);
  display.fillRect(cx + 3, cy - 1, 9, 4, SSD1306_WHITE);
}

void drawNavIconRoundabout(const String &type, int cx, int cy, int sz) {
  display.drawCircle(cx, cy, 14, SSD1306_WHITE);
  display.drawCircle(cx, cy, 8, SSD1306_WHITE);
  if (type == "BUNDARAN-KANAN") {
    display.fillTriangle(cx + 12, cy - 2, cx + 4, cy - 6, cx + 4, cy + 2, SSD1306_WHITE);
    display.fillRect(cx + 2, cy - 2, 10, 4, SSD1306_WHITE);
  } else if (type == "BUNDARAN-LURUS") {
    display.fillTriangle(cx, cy - 14, cx - 5, cy - 4, cx + 5, cy - 4, SSD1306_WHITE);
    display.fillRect(cx - 2, cy - 2, 4, 12, SSD1306_WHITE);
  } else if (type == "BUNDARAN-KIRI") {
    display.fillTriangle(cx - 12, cy - 2, cx - 4, cy - 6, cx - 4, cy + 2, SSD1306_WHITE);
    display.fillRect(cx - 12, cy - 2, 10, 4, SSD1306_WHITE);
  } else if (type == "BUNDARAN-BALIK") {
    drawNavIconUTurn(cx, cy, 12);
  } else {
    display.drawCircle(cx, cy, 3, SSD1306_WHITE);
  }
}

void drawNavIconArrived(int cx, int cy, int sz) {
  display.drawCircle(cx, cy, sz, SSD1306_WHITE);
  display.drawLine(cx - 5, cy + 1, cx - 1, cy + 5, SSD1306_WHITE);
  display.drawLine(cx - 1, cy + 5, cx + 6, cy - 4, SSD1306_WHITE);
}

void drawNavIconWarningOutline(const String &type, int cx, int cy, int sz) {
  if (type == "SIAP-KANAN") {
    display.drawTriangle(cx + sz, cy, cx, cy - sz * 6 / 10, cx, cy + sz * 6 / 10, SSD1306_WHITE);
    display.drawRect(cx - sz / 2, cy - 2, sz / 2 + 2, 4, SSD1306_WHITE);
  } else if (type == "SIAP-KIRI") {
    display.drawTriangle(cx - sz, cy, cx, cy - sz * 6 / 10, cx, cy + sz * 6 / 10, SSD1306_WHITE);
    display.drawRect(cx, cy - 2, sz / 2 + 2, 4, SSD1306_WHITE);
  } else {
    drawNavIconArrowUp(cx, cy, sz);
  }
  if (blinkState) {
    display.drawCircle(cx, cy, 18, SSD1306_WHITE);
  }
}

String getNavAction(const String &packetType) {
  if (packetType.startsWith("SIAP-")) return "WARN";
  if (packetType.startsWith("BUNDARAN")) return "ROUND";
  if (packetType == "TIBA") return "ARRIVE";
  if (packetType == "WAIT") return "WAIT";
  if (packetType == "IDLE") return "IDLE";
  return "NAV";
}

void parseBTLine(const String &line) {
  String s = line;
  s.trim();
  if (s.length() == 0) return;

  Serial.print("[BT] RX: ");
  Serial.println(s);

  if (s == "SWITCH_MENU") {
    Serial.println("[BT] Command SWITCH_MENU");
    switchToWiFiMode();
    return;
  }

  lastBTDataMs = millis();

  if (s.startsWith("NAV:")) {
    int p1 = s.indexOf(':');
    int p2 = s.indexOf(':', p1 + 1);
    int p3 = s.indexOf(':', p2 + 1);
    if (p2 > 0 && p3 > 0) {
      navType = s.substring(p1 + 1, p2);
      navDistance = s.substring(p2 + 1, p3);
      navRoad = s.substring(p3 + 1);
      navArrived = (navType == "TIBA");
      navGPSWeak = false;
      if (navType == "TIBA") {
        navDistance = "0m";
        navArrived = true;
      }
      return;
    }
  }

  if (s.startsWith("IDLE:")) {
    idleSpeed = s.substring(5);
    idleSpeed.trim();
    navType = "";
    navDistance = "";
    navRoad = "";
    navGPSWeak = false;
    navArrived = false;
    return;
  }

  if (s.startsWith("WAIT:")) {
    navType = "WAIT";
    navDistance = "";
    navRoad = "";
    waitMessage = s.substring(5);
    waitMessage.trim();
    navGPSWeak = true;
    navArrived = false;
    return;
  }

  if (s.startsWith("GPS_LEMAH")) {
    navType = "WAIT";
    navGPSWeak = true;
    waitMessage = s;
    return;
  }
}

void processBTInput() {
  while (SerialBT.available()) {
    char c = (char)SerialBT.read();
    if (c == '\n' || c == '\r') {
      if (btBuffer.length()) {
        parseBTLine(btBuffer);
        btBuffer = "";
      }
    } else {
      btBuffer += c;
      if (btBuffer.length() > 200) btBuffer = "";
    }
  }
}

void updateNavMode() {
  if (!btEnabled) {
    SerialBT.begin(BT_DEVICE_NAME);
    btEnabled = true;
    Serial.println("[BT] SerialBT lazy-started");
  }

  bool hasClient = SerialBT.hasClient();
  if (hasClient && !btClientPrev) {
    Serial.println("[BT] Client connected");
    drawNavConnectedFlash();
    delay(900);
    clearNavState();
  } else if (!hasClient && btClientPrev) {
    Serial.println("[BT] Client disconnected");
    clearNavState();
  }
  btClientPrev = hasClient;

  processBTInput();

  if (hasClient) {
    if (millis() - lastBTDataMs > navTimeoutMs) {
      if (navType != "IDLE") {
        navType = "IDLE";
        idleSpeed = "--";
        navRoad = "";
        navDistance = "";
      }
    }
  } else {
    static unsigned long lastWaitDraw = 0;
    if (millis() - lastWaitDraw >= 300) {
      lastWaitDraw = millis();
      drawNavWaitingScreen();
    }
    return;
  }

  if (millis() - lastBlinkMs >= blinkIntervalMs) {
    lastBlinkMs = millis();
    blinkState = !blinkState;
  }

  if (navType == "WAIT" || navGPSWeak) {
    drawNavGPSWeakScreen();
  } else if (navArrived) {
    drawNavArrivedScreen();
  } else if (navType == "IDLE") {
    drawNavIdleScreen();
  } else if (navType.length() > 0) {
    drawNavArrowScreen();
  } else {
    drawNavIdleScreen();
  }
}

// ═══════════════════════════════════════════════════════════
// END
// ═══════════════════════════════════════════════════════════
