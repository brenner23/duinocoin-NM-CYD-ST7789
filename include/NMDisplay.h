#ifndef NM_DISPLAY_H
#define NM_DISPLAY_H

#if defined(DISPLAY_CYD)

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/AGENCYB12pt7b.h>
#include <Fonts/AGENCYB15pt7b.h>
#include <Fonts/AGENCYB16pt7b.h>
#include <Fonts/AGENCYB17pt7b.h>
#include <Fonts/AGENCYB18pt7b.h>
#include <Fonts/AGENCYB20pt7b.h>
#include <Fonts/AGENCYB30pt7b.h>
#include "pins.h"
#include "DisplayDiagLog.h"

// Die Instanz selbst wird in ESP_Code_NMTV154_Adafruit.ino erzeugt.
extern Adafruit_ST7789 tft;
extern SPIClass nm_tft_spi;

// Wallet-Daten
float duco_balance = 0.0f;
// Exakte Schreibweise aus der HTTP-JSON-Antwort fuer die Webseite.
String duco_balance_raw = "--";
double duco_balance_precise = 0.0;
String duco_miner_rows_html;
uint16_t duco_active_threads = 0;
uint16_t duco_active_devices = 0;
double duco_total_hashrate = 0.0;
uint32_t duco_total_accepted = 0;
uint32_t duco_total_rejected = 0;
int duco_max_miners = 0;
int duco_warnings = 0;
bool duco_verified = false;
bool duco_wallet_valid = false;
unsigned long duco_wallet_last_update = 0;
time_t duco_wallet_last_update_epoch = 0;

// Kompakte Hashrate: ca. vier aussagekraeftige Stellen.
// Einheit erst ab 10.000 der aktuellen Einheit hochschalten.
static String nm_format_hashrate(double hashes_per_second) {
  const char *unit = "H/s";
  double value = hashes_per_second;
  if (hashes_per_second >= 10000000000000.0) { value = hashes_per_second / 1000000000000.0; unit = "TH/s"; }
  else if (hashes_per_second >= 10000000000.0) { value = hashes_per_second / 1000000000.0; unit = "GH/s"; }
  else if (hashes_per_second >= 10000000.0) { value = hashes_per_second / 1000000.0; unit = "MH/s"; }
  else if (hashes_per_second >= 10000.0) { value = hashes_per_second / 1000.0; unit = "kH/s"; }
  uint8_t decimals;
  if (value < 10.0) decimals = 3;
  else if (value < 100.0) decimals = 2;
  else if (value < 1000.0) decimals = 1;
  else decimals = 0;
  return String(value, (unsigned int)decimals) + " " + unit;
}

// V8: Display-Zustand absichtlich ganz oben deklariert.
// So sind die Variablen bereits fuer screen_setup(), display_boot()
// und display_info() sichtbar.
static bool nm_dashboard_drawn = false;
static SemaphoreHandle_t nm_display_mutex = nullptr;

// BRIEFKASTEN-FIX: Nach dem Start besitzt nur noch der Display-Task den TFT.
// Produzenten schreiben nur den neuesten Zustand in diese Mailbox.
struct NmDisplayMailbox {
  String hashrate;
  String rejected;
  String accepted;
  String uptime;
  String node;
  String difficulty;
  String sharerate;
  String jobs;
  String acceptRate;
  bool miningDirty = false;
  bool clockDirty = false;
  bool forceRedraw = false;
  bool recoverRequested = false;
  bool testRequested = false;
  bool rotationRequested = false;
  uint8_t rotationIndex = 1;
};

static NmDisplayMailbox nm_display_mailbox;
static SemaphoreHandle_t nm_mailbox_mutex = nullptr;
static TaskHandle_t nm_display_task_handle = nullptr;
static volatile bool nm_display_task_started = false;

#define NM_DISPLAY_BUILD "V8"

// Farben RGB565
static const uint16_t C_BG     = 0x0841;
static const uint16_t C_PANEL  = 0x10A2;
static const uint16_t C_PANEL2 = 0x18E3;
static const uint16_t C_ACCENT = 0xF5C0;
static const uint16_t C_GREEN  = 0x4E68;
static const uint16_t C_BLUE   = 0x3D9F;
static const uint16_t C_RED    = 0xF986;
static const uint16_t C_TEXT   = ST77XX_WHITE;
static const uint16_t C_MUTED  = 0xAD55;
static const uint16_t C_LINE   = 0x2945;

static void nm_text(const String &text, int16_t x, int16_t y,
                    uint8_t size, uint16_t color,
                    uint16_t bg = C_PANEL) {
  tft.setTextWrap(false);
  tft.setTextSize(size);
  tft.setTextColor(color, bg);
  tft.setCursor(x, y);
  tft.print(text);
}

static int16_t nm_text_width(const String &text, uint8_t size) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(size);
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return (int16_t)w;
}

static void nm_text_right(const String &text, int16_t right, int16_t y,
                          uint8_t size, uint16_t color,
                          uint16_t bg = C_PANEL) {
  nm_text(text, right - nm_text_width(text, size), y, size, color, bg);
}

static void nm_text_center(const String &text, int16_t centerX, int16_t y,
                           uint8_t size, uint16_t color,
                           uint16_t bg) {
  nm_text(text, centerX - nm_text_width(text, size) / 2, y, size, color, bg);
}



// Eigene GFX-Fonts aus dem Adafruit_GFX/Fonts-Ordner.
static void nm_gfx_text(const String &text, int16_t x, int16_t baseline,
                        const GFXfont *font, uint16_t color) {
  tft.setTextWrap(false);
  tft.setFont(font);
  tft.setTextSize(1);
  tft.setTextColor(color);
  tft.setCursor(x, baseline);
  tft.print(text);
  tft.setFont();
}

static int16_t nm_gfx_width(const String &text, const GFXfont *font) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.setFont(font);
  tft.setTextSize(1);
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setFont();
  return (int16_t)w;
}

static void nm_gfx_center(const String &text, int16_t centerX, int16_t baseline,
                          const GFXfont *font, uint16_t color) {
  const int16_t w = nm_gfx_width(text, font);
  nm_gfx_text(text, centerX - w / 2, baseline, font, color);
}

// Exakten Pixelbereich eines GFX-Textes ermitteln und löschen.
// Das ist wichtig, weil GFX-Fonts transparent gezeichnet werden und
// ihre Glyphen deutlich über grobe "Textzeilen"-Rechtecke hinausragen können.
static void nm_clear_gfx_text(const String &text,
                              int16_t x, int16_t baseline,
                              const GFXfont *font,
                              uint16_t bg,
                              int16_t clipX, int16_t clipY,
                              int16_t clipW, int16_t clipH,
                              int16_t pad = 2) {
  if (text.length() == 0) return;

  int16_t x1, y1;
  uint16_t w, h;

  tft.setFont(font);
  tft.setTextSize(1);
  tft.getTextBounds(text, x, baseline, &x1, &y1, &w, &h);
  tft.setFont();

  int16_t rx = x1 - pad;
  int16_t ry = y1 - pad;
  int16_t rw = (int16_t)w + pad * 2;
  int16_t rh = (int16_t)h + pad * 2;

  // Auf den jeweiligen Kartenbereich begrenzen, damit Labels/Rahmen
  // nicht versehentlich weggeputzt werden.
  int16_t right  = min<int16_t>(rx + rw, clipX + clipW);
  int16_t bottom = min<int16_t>(ry + rh, clipY + clipH);
  rx = max<int16_t>(rx, clipX);
  ry = max<int16_t>(ry, clipY);
  rw = right - rx;
  rh = bottom - ry;

  if (rw > 0 && rh > 0) {
    tft.fillRect(rx, ry, rw, rh, bg);
  }
}

static const GFXfont* nm_share_font_for(const String &sharesText) {
  const size_t len = sharesText.length();
  if (len <= 11) return &AGENCYB20pt7b;
  if (len == 12) return &AGENCYB18pt7b;
  if (len == 13) return &AGENCYB17pt7b;
  if (len == 14) return &AGENCYB16pt7b;
  return &AGENCYB15pt7b;
}

static void nm_draw_wifi_bars(int16_t x, int16_t y) {
  const bool connected = (WiFi.status() == WL_CONNECTED);
  int bars = 0;

  if (connected) {
    const int rssi = WiFi.RSSI();
    if (rssi >= -55)      bars = 4;
    else if (rssi >= -67) bars = 3;
    else if (rssi >= -75) bars = 2;
    else if (rssi >= -85) bars = 1;
  }

  const uint16_t onColor  = connected ? C_GREEN : C_RED;
  const uint16_t offColor = C_LINE;
  const int16_t w = 5;
  const int16_t gap = 3;
  const int16_t heights[4] = {5, 9, 13, 17};

  // Vier klassische Handy-Empfangsbalken, von klein nach gross.
  for (int i = 0; i < 4; ++i) {
    const int16_t h = heights[i];
    const int16_t bx = x + i * (w + gap);
    const int16_t by = y + 17 - h;

    if (i < bars) {
      tft.fillRect(bx, by, w, h, onColor);
    } else {
      tft.drawRect(bx, by, w, h, offColor);
    }
  }

  // Bei komplett getrennter WLAN-Verbindung ein kleines rotes X davor.
  if (!connected) {
    tft.drawLine(x - 9, y + 4, x - 3, y + 10, C_RED);
    tft.drawLine(x - 3, y + 4, x - 9, y + 10, C_RED);
  }
}

static String nm_short_node(String node) {
  node.trim();
  if (node.length() > 12) return node.substring(0, 12);
  return node;
}

static String nm_format_uptime(unsigned long ms) {
  unsigned long sec = ms / 1000UL;
  unsigned int days = sec / 86400UL;
  unsigned int hours = (sec / 3600UL) % 24UL;
  unsigned int mins = (sec / 60UL) % 60UL;

  if (days > 0) return String(days) + "d " + String(hours) + "h";
  return String(hours) + "h " + String(mins) + "m";
}


static void nm_display_power_on() {
  // CYD: Backlight auf GPIO21, HIGH-aktiv. PWM wird nicht mit digitalWrite ueberschrieben.
  if (!nm_backlight_pwm_active) {
    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH);
  }
}

void screen_setup() {
  // FIX07: Backlight waehrend ST7789-Init AUS. Der LCD-Inhalt bleibt aktiv;
  // nur die Beleuchtung wird fuer die Initialisierung kurz abgeschaltet.
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, LOW);

  if (nm_display_mutex == nullptr) {
    nm_display_mutex = xSemaphoreCreateMutex();
  }

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  // Classic CYD benutzt die HSPI-Pinbelegung 14/12/13/15.
  nm_tft_spi.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);

  // ST7789 240x320 Panel; 90/270 Grad ergeben 320x240 Querformat.
  tft.init(240, 320);
  tft.setSPISpeed(20000000);
  tft.setRotation(nm_rotation_index);
  // FIX01: Das Panel soll die RGB565-Farben unvertauscht anzeigen.
  tft.invertDisplay(false);
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);

  // Erst jetzt PWM aktivieren und die gespeicherte Helligkeit wiederherstellen.
  nm_backlight_setup();
}

void display_boot() {
  nm_dashboard_drawn = false;
  nm_display_power_on();
  tft.fillScreen(C_BG);

  tft.fillRoundRect(10, 12, 300, 56, 8, C_ACCENT);
  nm_text_center("DUINO-COIN", 160, 23, 3, ST77XX_BLACK, C_ACCENT);
  nm_text_center("CYD 320x240", 160, 50, 1, ST77XX_BLACK, C_ACCENT);

  nm_text_center("Miner startet ...", 160, 102, 2, C_TEXT, C_BG);
  nm_text_center(String(RIG_IDENTIFIER), 160, 132, 1, C_MUTED, C_BG);
  nm_text_center("ESP32  |  v" + String(SOFTWARE_VERSION), 120, 154, 1, C_MUTED, C_BG);

  tft.drawRoundRect(30, 188, 180, 8, 4, C_LINE);
  tft.fillRoundRect(32, 190, 105, 4, 2, C_GREEN);
  nm_text_center("WLAN + Pool werden verbunden", 120, 214, 1, C_MUTED, C_BG);
}

void display_info(String message) {
  nm_dashboard_drawn = false;
  nm_display_power_on();
  tft.fillRect(0, 182, 240, 58, C_BG);
  tft.fillRoundRect(10, 190, 220, 40, 7, C_PANEL);
  if (message.length() > 30) message = message.substring(0, 30);
  nm_text_center(message, 120, 204, 1, C_ACCENT, C_PANEL);
}

String wallet_last_update_text() {
  if (!duco_wallet_valid || duco_wallet_last_update_epoch <= 0) return "--";
  struct tm timeinfo;
  localtime_r(&duco_wallet_last_update_epoch, &timeinfo);
  char buf[9];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

static String nm_html_escape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  return s;
}

// Holt den Balance-Zahlenwert direkt aus dem unveraenderten JSON-Text.
// Dadurch bleibt z.B. 695.31448377982030000 exakt so erhalten.
static String nm_extract_raw_balance(const String &payload) {
  const String marker = "\"balance\":{\"balance\":";
  int p = payload.indexOf(marker);
  if (p < 0) return "";
  p += marker.length();
  while (p < (int)payload.length() && (payload[p] == ' ' || payload[p] == '	')) p++;
  int e = p;
  while (e < (int)payload.length()) {
    const char c = payload[e];
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') e++;
    else break;
  }
  return payload.substring(p, e);
}

static String nm_wallet_usd_text() {
  if (!duco_wallet_valid) return "--";
  const double usd = duco_balance_precise * (double)DUCO_USD_RATE;
  return String(usd, 7);
}

bool update_wallet_data() {
  if (WiFi.status() != WL_CONNECTED) {
    #if defined(SERIAL_PRINTING) && WALLET_INFO
      Serial.println("[WALLET] Kein WLAN - Abfrage uebersprungen");
    #endif
    return false;
  }

  #if defined(SERIAL_PRINTING) && WALLET_INFO
    Serial.println();
    Serial.println("[WALLET] Anfrage startet...");
    Serial.println("[WALLET] URL: http://server.duinocoin.com/users/" + String(DUCO_USER));
    Serial.println("[WALLET] Free Heap vorher: " + String(ESP.getFreeHeap()));
  #endif

  WiFiClient client;
  client.setTimeout(8000);

  HTTPClient http;
  const String url = "http://server.duinocoin.com/users/" + String(DUCO_USER);

  if (!http.begin(client, url)) {
    #if defined(SERIAL_PRINTING) && WALLET_INFO
      Serial.println("[WALLET] http.begin() fehlgeschlagen");
    #endif
    return false;
  }

  http.setTimeout(8000);
  http.addHeader("Accept", "application/json");

  const int httpCode = http.GET();

  #if defined(SERIAL_PRINTING) && WALLET_INFO
    Serial.println("[WALLET] HTTP Code: " + String(httpCode));
  #endif

  if (httpCode != HTTP_CODE_OK) {
    #if defined(SERIAL_PRINTING) && WALLET_INFO
      Serial.println("[WALLET] HTTP Fehler: " + http.errorToString(httpCode));
    #endif
    http.end();
    return false;
  }

  // Erst die komplette Antwort holen. Das ist fuer den Test absichtlich
  // einfacher und besser zu diagnostizieren als direkt aus dem HTTP-Stream.
  String payload = http.getString();
  http.end();

  #if defined(SERIAL_PRINTING) && WALLET_INFO
    Serial.println("[WALLET] Antwortlaenge: " + String(payload.length()));
    Serial.println("[WALLET] Free Heap nach Download: " + String(ESP.getFreeHeap()));
    Serial.println("[WALLET] JSON Anfang:");
    Serial.println(payload.substring(0, 600));
  #endif

  if (payload.length() == 0) {
    #if defined(SERIAL_PRINTING) && WALLET_INFO
      Serial.println("[WALLET] Leere Serverantwort");
    #endif
    return false;
  }

  // Die aktuelle /users/<name>-Antwort enthaelt auch Transaktionen.
  // 4096 Byte sind fuer den Test bewusst grosszuegig dimensioniert.
  DynamicJsonDocument doc(4096);
  const DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    #if defined(SERIAL_PRINTING) && WALLET_INFO
      Serial.print("[WALLET] JSON Fehler: ");
      Serial.println(err.c_str());
    #endif
    return false;
  }

  if (!(doc["success"] | false)) {
    #if defined(SERIAL_PRINTING) && WALLET_INFO
      Serial.println("[WALLET] API meldet success=false");
    #endif
    return false;
  }

  JsonObject account = doc["result"]["balance"];
  if (account.isNull()) {
    #if defined(SERIAL_PRINTING) && WALLET_INFO
      Serial.println("[WALLET] result.balance fehlt");
    #endif
    return false;
  }

  JsonVariant balanceValue = account["balance"];
  if (balanceValue.isNull()) {
    #if defined(SERIAL_PRINTING) && WALLET_INFO
      Serial.println("[WALLET] result.balance.balance fehlt");
    #endif
    return false;
  }

  duco_balance_raw = nm_extract_raw_balance(payload);
  if (duco_balance_raw.length() == 0) {
    // Fallback nur fuer den unwahrscheinlichen Fall, dass sich das JSON-Layout aendert.
    serializeJson(balanceValue, duco_balance_raw);
  }
  duco_balance_precise = duco_balance_raw.toDouble();
  duco_balance = (float)duco_balance_precise; // kompakte Anzeige auf dem 240x240-Display
  duco_max_miners = account["max_miners"] | 0;
  duco_warnings = account["warnings"] | 0;

  String verified = account["verified"] | "no";
  verified.toLowerCase();
  duco_verified = (verified == "yes" || verified == "true" || verified == "1");

  // Miner-Uebersicht fuer das Web-Dashboard aus derselben JSON-Antwort erzeugen.
  duco_miner_rows_html = "";
  duco_active_threads = 0;
  duco_active_devices = 0;
  duco_total_hashrate = 0.0;
  duco_total_accepted = 0;
  duco_total_rejected = 0;

  JsonArray miners = doc["result"]["miners"].as<JsonArray>();
  String seenIdentifiers = "\n";
  for (JsonObject miner : miners) {
    String identifier = miner["identifier"] | "--";
    const double hr = miner["hashrate"] | 0.0;
    const uint32_t accepted = miner["accepted"] | 0;
    const uint32_t rejected = miner["rejected"] | 0;
    const long diff = miner["diff"] | 0;
    const double sharetime = miner["sharetime"] | 0.0;
    String pool = miner["pool"] | "--";

    duco_active_threads++;
    duco_total_hashrate += hr;
    duco_total_accepted += accepted;
    duco_total_rejected += rejected;

    const String key = "\n" + identifier + "\n";
    if (seenIdentifiers.indexOf(key) < 0) {
      seenIdentifiers += identifier + "\n";
      duco_active_devices++;
    }

    String ip = "--";
    if (identifier == String(RIG_IDENTIFIER)) ip = WiFi.localIP().toString();

    duco_miner_rows_html += "<tr><td>" + nm_html_escape(identifier) + "</td>";
    duco_miner_rows_html += "<td>" + nm_html_escape(ip) + "</td>";
    duco_miner_rows_html += "<td>" + nm_format_hashrate(hr) + "</td>";
    duco_miner_rows_html += "<td>" + String(diff) + "</td>";
    duco_miner_rows_html += "<td>" + String(accepted) + " / " + String(rejected) + "</td>";
    duco_miner_rows_html += "<td>" + String(sharetime, 3) + " s</td>";
    duco_miner_rows_html += "<td>" + nm_html_escape(pool) + "</td></tr>";
  }
  if (duco_miner_rows_html.length() == 0) {
    duco_miner_rows_html = "<tr><td colspan='7'>Keine aktiven Miner gemeldet</td></tr>";
  }

  duco_wallet_valid = true;
  duco_wallet_last_update = millis();
  duco_wallet_last_update_epoch = time(nullptr);

  #if defined(SERIAL_PRINTING) && WALLET_INFO
    Serial.println("[WALLET] Guthaben: " + duco_balance_raw + " DUCO");
    Serial.println("[WALLET] Max Miner: " + String(duco_max_miners));
    Serial.println("[WALLET] Verified: " + String(duco_verified ? "JA" : "NEIN"));
    Serial.println("[WALLET] Warnungen: " + String(duco_warnings));
    Serial.println("[WALLET] Free Heap fertig: " + String(ESP.getFreeHeap()));
    Serial.println("[WALLET] Fertig");
    Serial.println();
  #endif

  return true;
}

// NTP-Uhr. Die Anzeige wird unabhängig vom 3-Sekunden-Miner-Refresh
// aktualisiert, damit die Uhr sekundengenau läuft.
static bool nm_clock_started = false;

// FIX16: Senderseitiger TFT-Core-Waechter.
// Wir lesen den ST7789 nicht mehr zurueck, sondern beobachten unsere eigene
// Senderseite: Core-Wechsel und konkurrierende TFT-Zugriffe auf denselben Mutex.
static volatile int8_t nm_tft_owner_core = -1;
static volatile uint32_t nm_tft_collision_count = 0;
static volatile uint32_t nm_tft_wrong_core_count = 0;

static void nm_lock_display() {
  if (nm_display_mutex == nullptr) return;

  const int core = xPortGetCoreID();
#if TFT_CORE_ENABLED
  // DISPLAY MASTER FIX: Der einzige erlaubte TFT-Besitzer laeuft auf Core 1.
  // Ein TFT-Zugriff von Core 0 ist deshalb fuer die Diagnose interessant.
  if (core != 1) {
    ++nm_tft_wrong_core_count;
    nm_displaydiag_event(1, (int8_t)core);
    Serial.printf("[TFT-CORE] UNEXPECTED ACCESS core=%d count=%lu\n",
                  core, (unsigned long)nm_tft_wrong_core_count);
  }

  // Erst ohne Wartezeit testen. Scheitert das, greift bereits ein anderer
  // Aufrufer auf den TFT zu. Danach normal blockierend warten, damit nichts
  // am bisherigen Schutzverhalten geaendert wird.
  if (xSemaphoreTake(nm_display_mutex, 0) != pdTRUE) {
    ++nm_tft_collision_count;
    nm_displaydiag_event(2, (int8_t)core, nm_tft_owner_core);
    Serial.printf("[TFT-CORE] COLLISION requester=CORE%d owner=CORE%d count=%lu\n",
                  core, (int)nm_tft_owner_core,
                  (unsigned long)nm_tft_collision_count);
    xSemaphoreTake(nm_display_mutex, portMAX_DELAY);
  }
  nm_tft_owner_core = (int8_t)core;
#else
  xSemaphoreTake(nm_display_mutex, portMAX_DELAY);
  nm_tft_owner_core = (int8_t)core;
#endif
}

static void nm_unlock_display() {
  if (nm_display_mutex != nullptr) {
    nm_tft_owner_core = -1;
    xSemaphoreGive(nm_display_mutex);
  }
}

static void nm_start_clock() {
  if (nm_clock_started || WiFi.status() != WL_CONNECTED) return;

  // Deutsche Zeitzone inkl. Sommer-/Winterzeit.
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3",
               "pool.ntp.org", "time.nist.gov");
  nm_clock_started = true;
}

static String nm_clock_text() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) return "--:--";

  char buf[6];
  strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  return String(buf);
}

static String nm_date_text() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) return "--- --.--.----";
  static const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  char datebuf[11];
  strftime(datebuf, sizeof(datebuf), "%d.%m.%Y", &timeinfo);
  return String(days[timeinfo.tm_wday]) + " " + String(datebuf);
}

static bool nm_force_dynamic_redraw = false;
static bool nm_force_clock_redraw = false;
static bool nm_landscape() { return nm_rotation_index == 1 || nm_rotation_index == 3; }
static int16_t nm_y(int16_t y) { return nm_landscape() ? y : y + 40; }

static void nm_draw_static_dashboard() {
  tft.fillScreen(C_BG);
  const bool wide = nm_landscape();
  const int W = wide ? 320 : 240;
  const int y0 = wide ? 0 : 40;

  tft.fillRect(0, y0, W, 27, C_PANEL2);
  nm_gfx_text("DUINO-COIN", 7, y0 + 23, &AGENCYB15pt7b, C_ACCENT);

  tft.fillRoundRect(5, y0 + 30, W - 10, 39, 8, C_PANEL);
  nm_gfx_text("DUCO", wide ? 260 : 186, y0 + 61, &AGENCYB15pt7b, C_TEXT);

  if (wide) {
    tft.fillRoundRect(5, 74, 152, 51, 7, C_PANEL);
    nm_text("HASHRATE", 12, 79, 1, C_MUTED, C_PANEL);
    nm_text_right("kH/s", 149, 111, 1, C_GREEN, C_PANEL);
    tft.fillRoundRect(163, 74, 152, 51, 7, C_PANEL);
    nm_text("DIFFICULTY", 170, 79, 1, C_MUTED, C_PANEL);
    // FIX02: Ping nur dreistellig dimensioniert, Shares bekommen den Platz.
    tft.fillRoundRect(5, 130, 229, 51, 7, C_PANEL);
    nm_text("SHARES", 12, 135, 1, C_MUTED, C_PANEL);
    tft.fillRoundRect(239, 130, 76, 51, 7, C_PANEL);
    nm_text("PING", 246, 135, 1, C_MUTED, C_PANEL);
    nm_text_right("ms", 309, 167, 1, C_TEXT, C_PANEL);
    tft.drawFastHLine(7, 187, 306, C_LINE);
    tft.fillRoundRect(12, 191, 296, 45, 14, C_PANEL2);
    tft.drawRoundRect(12, 191, 296, 45, 14, C_LINE);
    nm_draw_wifi_bars(279, 5);
  } else {
    // 240x240 Originalstil, mittig im 240x320 Panel: 40 px Schwarz oben/unten.
    tft.fillRoundRect(5, y0 + 74, 151, 51, 7, C_PANEL);
    nm_text("HASHRATE", 12, y0 + 79, 1, C_MUTED, C_PANEL);
    nm_text_right("kH/s", 149, y0 + 111, 1, C_GREEN, C_PANEL);
    tft.fillRoundRect(161, y0 + 74, 74, 51, 7, C_PANEL);
    nm_text("DIFF", 168, y0 + 79, 1, C_MUTED, C_PANEL);
    tft.fillRoundRect(5, y0 + 130, 151, 51, 7, C_PANEL);
    nm_text("SHARES", 12, y0 + 135, 1, C_MUTED, C_PANEL);
    tft.fillRoundRect(161, y0 + 130, 74, 51, 7, C_PANEL);
    nm_text("PING", 168, y0 + 135, 1, C_MUTED, C_PANEL);
    nm_text_right("ms", 229, y0 + 167, 1, C_TEXT, C_PANEL);
    tft.drawFastHLine(7, y0 + 187, 226, C_LINE);
    tft.fillRoundRect(30, y0 + 191, 180, 45, 14, C_PANEL2);
    tft.drawRoundRect(30, y0 + 191, 180, 45, 14, C_LINE);
    nm_draw_wifi_bars(199, y0 + 5);
  }
  nm_dashboard_drawn = true;
}

static void nm_apply_rotation_direct(uint8_t rotationIndex) {
  nm_lock_display();
  tft.setRotation(rotationIndex);
  tft.fillScreen(C_BG);
  nm_dashboard_drawn = false;
  nm_draw_static_dashboard();
  // FIX03: Alle dynamischen Felder beim naechsten Miner-Refresh erzwingen.
  nm_force_dynamic_redraw = true;
  nm_unlock_display();
}

// FIX14 Diagnose: Zeichnet ohne Re-Init ein auffaelliges Testfeld.
// Erscheint es im Fehlerzustand, lebt der TFT/SPI-Pfad noch.
static void nm_display_test_pattern_direct() {
  Serial.println("[TFT] TEST pattern requested");
  nm_lock_display();
  tft.fillRect(8, 8, 72, 36, ST77XX_RED);
  tft.drawRect(10, 10, 68, 32, ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(20, 19);
  tft.print("TEST");
  nm_unlock_display();
}

// FIX14 Diagnose/Recovery: Nur TFT-SPI und ST7789 neu initialisieren.
// ESP32, WLAN, Webserver und Mining laufen dabei weiter.
static void nm_display_recover_direct() {
  Serial.println("[TFT] RECOVER start");
  nm_lock_display();

  digitalWrite(TFT_CS, HIGH);
  nm_tft_spi.end();
  delay(20);
  nm_tft_spi.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  delay(20);

  tft.init(240, 320);
  tft.setSPISpeed(20000000);
  tft.setRotation(nm_rotation_index);
  tft.invertDisplay(false);
  tft.setTextWrap(false);
  tft.fillScreen(C_BG);

  nm_dashboard_drawn = false;
  nm_draw_static_dashboard();
  nm_force_dynamic_redraw = true;

  nm_unlock_display();
  Serial.println("[TFT] RECOVER done");
}

// FIX16: RDDST-Watch entfernt; dieses CYD liefert keinen brauchbaren TFT-Readback.

static void nm_render_display_clock() {
  static unsigned long lastClockPoll = 0;
  static String lastClockText = "";
  static String lastDateText = "";
  if (!nm_dashboard_drawn) return;
  const unsigned long now = millis();
  if (now - lastClockPoll < 250UL) return;
  lastClockPoll = now;
  nm_start_clock();
  const String current = nm_clock_text();
  const String dateNow = nm_date_text();
  if (!nm_force_clock_redraw && current == lastClockText && dateNow == lastDateText) return;
  nm_force_clock_redraw = false;
  lastClockText = current; lastDateText = dateNow;
  nm_lock_display();
  if (nm_landscape()) {
    tft.fillRoundRect(12, 191, 296, 45, 14, C_PANEL2);
    tft.drawRoundRect(12, 191, 296, 45, 14, C_LINE);
    nm_gfx_center(dateNow, 104, 222, &AGENCYB15pt7b, C_MUTED);
    nm_gfx_center(current, 244, 226, &AGENCYB20pt7b, C_TEXT);
  } else {
    const int y0=40;
    tft.fillRoundRect(30, y0+191, 180, 45, 14, C_PANEL2);
    tft.drawRoundRect(30, y0+191, 180, 45, 14, C_LINE);
    // Original-Layout: Uhr gross; Datum kompakt darunter/links ist im 240er Feld bewusst reduziert.
    nm_gfx_center(current, 120, y0+226, &AGENCYB20pt7b, C_TEXT);
  }
  nm_unlock_display();
}

static void nm_render_mining_results(String hashrate_s, String rejected_shares, String accepted_shares,
                            String uptime_unused, String node_unused, String difficulty_s,
                            String sharerate, String jobs_s, String accept_rate) {
  nm_display_power_on(); nm_start_clock();
  (void)uptime_unused; (void)node_unused; (void)sharerate; (void)accept_rate;
  static String lastBalance="", lastHashrate="", lastDifficulty="", lastShares="", lastJobs="";
  static int lastWifiBars=-99;
  nm_lock_display();
  if (nm_force_dynamic_redraw) {
    lastBalance=lastHashrate=lastDifficulty=lastShares=lastJobs="";
    lastWifiBars=-99;
    nm_force_dynamic_redraw=false;
  }
  if (!nm_dashboard_drawn) { nm_draw_static_dashboard(); lastBalance=lastHashrate=lastDifficulty=lastShares=lastJobs=""; lastWifiBars=-99; }
  const bool wide=nm_landscape(); const int y0=wide?0:40; const int W=wide?320:240;
  int wifiBars=-1;
  if (WiFi.status()==WL_CONNECTED) { int rssi=WiFi.RSSI(); wifiBars=(rssi>=-55)?4:(rssi>=-67)?3:(rssi>=-75)?2:(rssi>=-85)?1:0; }
  if (wifiBars!=lastWifiBars) { tft.fillRect(0,y0,W,27,C_PANEL2); nm_gfx_text("DUINO-COIN",7,y0+23,&AGENCYB15pt7b,C_ACCENT); nm_draw_wifi_bars(wide?279:199,y0+5); lastWifiBars=wifiBars; }
  // FIX03: Breitbild nutzt den zusaetzlichen Platz fuer mehr Wallet-Nachkommastellen.
  int balanceDecimals;
  if (wide) {
    balanceDecimals = 9;
    if (duco_balance >= 100000.0f) balanceDecimals = 6;
    else if (duco_balance >= 10000.0f) balanceDecimals = 7;
    else if (duco_balance >= 1000.0f) balanceDecimals = 8;
  } else {
    balanceDecimals = 6;
    if (duco_balance >= 100000.0f) balanceDecimals = 3;
    else if (duco_balance >= 10000.0f) balanceDecimals = 4;
    else if (duco_balance >= 1000.0f) balanceDecimals = 5;
  }
  String balanceText=duco_wallet_valid?String(duco_balance_precise,balanceDecimals):String("---.------");
  if(balanceText!=lastBalance){ tft.fillRoundRect(5,y0+30,W-10,39,8,C_PANEL); const GFXfont *f=&AGENCYB20pt7b; if(nm_gfx_width(balanceText,f)>(wide?245:170)) f=&AGENCYB15pt7b; nm_gfx_text(balanceText,9,y0+61,f,duco_wallet_valid?C_ACCENT:C_MUTED); nm_gfx_text("DUCO",wide?260:186,y0+61,&AGENCYB15pt7b,C_TEXT); lastBalance=balanceText; }
  if(hashrate_s!=lastHashrate){ tft.fillRoundRect(5,y0+74,wide?152:151,51,7,C_PANEL); nm_text("HASHRATE",12,y0+79,1,C_MUTED,C_PANEL); nm_gfx_text(hashrate_s,12,y0+118,&AGENCYB20pt7b,C_GREEN); nm_text_right("kH/s",149,y0+111,1,C_GREEN,C_PANEL); lastHashrate=hashrate_s; }
  if(difficulty_s!=lastDifficulty){ int x=wide?163:161,w=wide?152:74; tft.fillRoundRect(x,y0+74,w,51,7,C_PANEL); nm_text(wide?"DIFFICULTY":"DIFF",x+7,y0+79,1,C_MUTED,C_PANEL); const GFXfont* f=wide?&AGENCYB20pt7b:&AGENCYB15pt7b; nm_gfx_text(difficulty_s,x+7,y0+(wide?118:114),f,C_BLUE); lastDifficulty=difficulty_s; }
  String sharesText=rejected_shares+"/"+accepted_shares; // Rejected / Accepted
  if(sharesText!=lastShares){ int sw=wide?229:151; tft.fillRoundRect(5,y0+130,sw,51,7,C_PANEL); nm_text("SHARES",12,y0+135,1,C_MUTED,C_PANEL); const GFXfont *f=nm_share_font_for(sharesText); int16_t base=y0+174; if(f==&AGENCYB18pt7b||f==&AGENCYB17pt7b) base=y0+173; else if(f==&AGENCYB16pt7b) base=y0+172; else if(f==&AGENCYB15pt7b) base=y0+171; nm_gfx_text(sharesText,12,base,f,C_TEXT); lastShares=sharesText; }
  if(jobs_s!=lastJobs){ int x=wide?239:161,w=wide?76:74; tft.fillRoundRect(x,y0+130,w,51,7,C_PANEL); nm_text("JOBS",x+7,y0+135,1,C_MUTED,C_PANEL); const GFXfont* f=(jobs_s.length()>=5)?&AGENCYB12pt7b:&AGENCYB15pt7b; int16_t base=y0+171; if(f==&AGENCYB12pt7b) base=y0+169; nm_gfx_text(jobs_s,x+7,base,f,C_TEXT); lastJobs=jobs_s; }
  nm_unlock_display(); nm_render_display_clock();
}

// ============================================================
// BRIEFKASTEN-FIX
// Neuester Stand gewinnt. Nach nm_display_mailbox_begin() darf nur noch
// nm_display_task_func() den ST7789/SPI ansprechen.
// ============================================================
static bool nm_mailbox_take(TickType_t waitTicks = pdMS_TO_TICKS(20)) {
  if (nm_mailbox_mutex == nullptr) return false;
  return xSemaphoreTake(nm_mailbox_mutex, waitTicks) == pdTRUE;
}

static void nm_mailbox_give() {
  if (nm_mailbox_mutex != nullptr) xSemaphoreGive(nm_mailbox_mutex);
}

static void nm_display_notify() {
  if (nm_display_task_handle != nullptr) xTaskNotifyGive(nm_display_task_handle);
}

void display_mining_results(String hashrate_s, String rejected_shares, String accepted_shares,
                            String uptime, String node, String difficulty_s,
                            String sharerate, String jobs_s, String accept_rate) {
  if (!nm_display_task_started) {
    nm_render_mining_results(hashrate_s, rejected_shares, accepted_shares, uptime, node,
                             difficulty_s, sharerate, jobs_s, accept_rate);
    return;
  }
  if (!nm_mailbox_take()) return;
  nm_display_mailbox.hashrate = hashrate_s;
  nm_display_mailbox.rejected = rejected_shares;
  nm_display_mailbox.accepted = accepted_shares;
  nm_display_mailbox.uptime = uptime;
  nm_display_mailbox.node = node;
  nm_display_mailbox.difficulty = difficulty_s;
  nm_display_mailbox.sharerate = sharerate;
  nm_display_mailbox.jobs = jobs_s;
  nm_display_mailbox.acceptRate = accept_rate;
  nm_display_mailbox.miningDirty = true;
  nm_mailbox_give();
  // Kein Sofort-Render: der Display-Task nimmt spaetestens nach 3 s den neuesten Stand.
}

void update_display_clock() {
  if (!nm_display_task_started) {
    nm_render_display_clock();
    return;
  }
  if (nm_mailbox_take(0)) {
    nm_display_mailbox.clockDirty = true;
    nm_mailbox_give();
  }
}

void nm_apply_rotation_now(uint8_t rotationIndex) {
  if (!nm_display_task_started) {
    nm_apply_rotation_direct(rotationIndex);
    return;
  }
  if (nm_mailbox_take()) {
    nm_display_mailbox.rotationIndex = rotationIndex;
    nm_display_mailbox.rotationRequested = true;
    nm_display_mailbox.forceRedraw = true;
    nm_display_mailbox.clockDirty = true;
    nm_mailbox_give();
    nm_display_notify();
  }
}

void nm_display_test_pattern() {
  if (!nm_display_task_started) {
    nm_display_test_pattern_direct();
    return;
  }
  if (nm_mailbox_take()) {
    nm_display_mailbox.testRequested = true;
    nm_mailbox_give();
    nm_display_notify();
  }
}

void nm_display_recover() {
  if (!nm_display_task_started) {
    nm_display_recover_direct();
    nm_force_clock_redraw = true;
    nm_render_display_clock();
    return;
  }
  if (nm_mailbox_take()) {
    nm_display_mailbox.recoverRequested = true;
    nm_display_mailbox.forceRedraw = true;
    nm_display_mailbox.clockDirty = true;
    nm_mailbox_give();
    nm_display_notify();
  }
}

static void nm_display_task_func(void *parameter) {
  (void)parameter;
  NmDisplayMailbox local;
  for (;;) {
    // Spaetestens alle 3 Sekunden; Recover/Rotation/Test wecken sofort auf.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(3000));

    bool have = false;
    if (nm_mailbox_take(pdMS_TO_TICKS(100))) {
      local = nm_display_mailbox;
      nm_display_mailbox.miningDirty = false;
      nm_display_mailbox.clockDirty = false;
      nm_display_mailbox.forceRedraw = false;
      nm_display_mailbox.recoverRequested = false;
      nm_display_mailbox.testRequested = false;
      nm_display_mailbox.rotationRequested = false;
      have = true;
      nm_mailbox_give();
    }
    if (!have) continue;

    if (local.recoverRequested) {
      nm_display_recover_direct();
      nm_force_clock_redraw = true;
      // Recovery soll Uhr/Datum sofort wiederherstellen, nicht erst beim naechsten Minutenwechsel.
      nm_render_display_clock();
    }
    if (local.rotationRequested) {
      nm_apply_rotation_direct(local.rotationIndex);
      nm_force_clock_redraw = true;
      nm_render_display_clock();
    }
    if (local.testRequested) nm_display_test_pattern_direct();

    if (local.forceRedraw) nm_force_dynamic_redraw = true;
    if (local.miningDirty || local.forceRedraw) {
      nm_render_mining_results(local.hashrate, local.rejected, local.accepted, local.uptime,
                               local.node, local.difficulty, local.sharerate, local.jobs,
                               local.acceptRate);
    } else if (local.clockDirty) {
      nm_render_display_clock();
    }
  }
}

void nm_display_mailbox_begin() {
  if (nm_display_task_started) return;
  if (nm_mailbox_mutex == nullptr) nm_mailbox_mutex = xSemaphoreCreateMutex();
  if (nm_mailbox_mutex == nullptr) return;
  BaseType_t ok = xTaskCreatePinnedToCore(nm_display_task_func, "display_task", 8192, nullptr, TASK_PRIO_DISPLAY,
                                          &nm_display_task_handle, 1);
  if (ok == pdPASS) nm_display_task_started = true;
}

// FIX04: reine LDR-Kalibrierseite. Mining laeuft im Hintergrund weiter.
void display_ldr_debug() {
  static unsigned long lastDraw = 0;
  const unsigned long now = millis();
  if (now - lastDraw < 250UL) return;
  lastDraw = now;

  nm_lock_display();
  tft.fillScreen(ST77XX_BLACK);
  const int16_t W = tft.width();
  const int16_t H = tft.height();
  nm_gfx_center("LDR DEBUG", W/2, 35, &AGENCYB20pt7b, C_ACCENT);

  nm_text("RAW", 18, 57, 1, C_MUTED, ST77XX_BLACK);
  nm_gfx_text(String(nm_ldr_raw), 18, 88, &AGENCYB20pt7b, C_TEXT);
  nm_text("MIN", W/2+8, 57, 1, C_MUTED, ST77XX_BLACK);
  nm_gfx_text(String(nm_ldr_min == 4095 ? 0 : nm_ldr_min), W/2+8, 88, &AGENCYB20pt7b, C_GREEN);

  nm_text("MAX", 18, 105, 1, C_MUTED, ST77XX_BLACK);
  nm_gfx_text(String(nm_ldr_max), 18, 136, &AGENCYB20pt7b, C_BLUE);
  nm_text("BASE", W/2+8, 105, 1, C_MUTED, ST77XX_BLACK);
  nm_gfx_text(String(nm_brightness_percent) + "%", W/2+8, 136, &AGENCYB20pt7b, C_TEXT);

  nm_text("OUTPUT", 18, 153, 1, C_MUTED, ST77XX_BLACK);
  nm_gfx_text(String(nm_ldr_output_percent) + "%", 18, 184, &AGENCYB20pt7b, C_ACCENT);
  nm_text("AUTO", W/2+8, 153, 1, C_MUTED, ST77XX_BLACK);
  nm_gfx_text(nm_auto_brightness ? "ON" : "OFF", W/2+8, 184, &AGENCYB20pt7b, nm_auto_brightness ? C_GREEN : C_RED);

  nm_text("Cover LDR / flashlight -> note RAW MIN MAX", 10, H-18, 1, C_MUTED, ST77XX_BLACK);
  nm_unlock_display();
}

#endif // DISPLAY_CYD
#endif // NM_DISPLAY_H
