/*
   ____  __  __  ____  _  _  _____       ___  _____  ____  _  _
  (  _ \(  )(  )(_  _)( \( )(  _  )___  / __)(  _  )(_  _)( \( )
   )(_) ))(__)(  _)(_  )  (  )(_)((___)( (__  )(_)(  _)(_  )  (
  (____/(______)(____)(_)\_)(_____)     \___)(_____)(____)(_)\_)
  Official code for all ESP8266/32 boards            version 4.3
  Main .ino file

  The Duino-Coin Team & Community 2019-2024 Â© MIT Licensed
  https://duinocoin.com
  https://github.com/revoxhere/duino-coin

  If you don't know where to start, visit official website and navigate to
  the Getting Started page. Have fun mining!

  To edit the variables (username, WiFi settings, etc.) use the Settings.h tab!
*/

/* If optimizations cause problems, change them to -O0 (the default) */
#pragma GCC optimize("-Ofast")

/* If during compilation the line below causes a
  "fatal error: arduinoJson.h: No such file or directory"
  message to occur; it means that you do NOT have the
  ArduinoJSON library installed. To install it,
  go to the below link and follow the instructions:
  https://github.com/revoxhere/duino-coin/issues/832 */
#include <ArduinoJson.h>

unsigned long lastDisplayUpdate = 0;

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESP8266mDNS.h>
    #include <ESP8266HTTPClient.h>
    #include <ESP8266WebServer.h>
#else
    #include <ESPmDNS.h>
    #include <WiFi.h>
    #include <HTTPClient.h>
    #include <WebServer.h>
#endif

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <Ticker.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "MiningJob.h"
#include "CollisionGuard.h"
#include "CrashLog.h"
#include "Settings.h"

#if defined(DISPLAY_CYD)
  #include <SPI.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_ST7789.h>
  #include <XPT2046_Touchscreen.h>
  #include <Preferences.h>
  #include "pins.h"

  // FIX05: Web-Diagnose wird ausschliesslich in Settings.h kompiliert.
  // Im Web gibt es keine Laufzeit-Schalter mehr.
  static uint32_t nm_temp_interval_seconds = DIAG_DEFAULT_TEMP_INTERVAL;
  static volatile float nm_cpu_temp = 0.0f;
  static volatile float nm_cpu_temp_max = 0.0f;
  static unsigned long nm_temp_last_read = 0;

  static void nm_diag_load() {
    Preferences p; p.begin("nm_diag", true);
    nm_temp_interval_seconds = constrain((uint32_t)p.getUInt("tempint", DIAG_DEFAULT_TEMP_INTERVAL), 1UL, 3600UL);
    p.end();
  }
  static void nm_temp_update(bool force=false) {
    #if !WEB_DIAG_CPU_TEMP
      return;
    #endif
    unsigned long now=millis();
    if (!force && now-nm_temp_last_read < nm_temp_interval_seconds*1000UL) return;
    nm_temp_last_read=now;
    float t=temperatureRead();
    if (isfinite(t)) { nm_cpu_temp=t; if (nm_cpu_temp_max==0.0f || t>nm_cpu_temp_max) nm_cpu_temp_max=t; }
  }

  // DISPLAY MASTER FIX: eigener expliziter HSPI-Host fuer den ST7789.
  // Entspricht dem stabilen A/B-Stresstest: Displayzugriff auf Core 1.
  SPIClass nm_tft_spi(HSPI);
  Adafruit_ST7789 tft(&nm_tft_spi, TFT_CS, TFT_DC, TFT_RST);

  // V12: Backlight per PWM, LOW-aktiv.
  // ESP32 Arduino Core 3.x: LEDC wird direkt am Pin angehaengt.
  // Genau dieselbe API wie im funktionierenden Wetter-Projekt.
  static const uint16_t NM_BL_FREQ = 1000;
  static const uint8_t NM_BL_RESOLUTION = 8;
  static uint8_t nm_brightness_percent = 100;
  static bool nm_backlight_pwm_active = false;
  static bool nm_display_light_on = true;
  // FIX08: Touch schaltet NUR das Backlight. LCD/Mining laufen unveraendert weiter.
  static bool nm_backlight_user_enabled = true;
  static unsigned long nm_touch_last_toggle = 0;
  static bool nm_touch_was_down = false;
  static SPIClass nm_touch_spi(VSPI);
  static XPT2046_Touchscreen nm_touch(TOUCH_CS, TOUCH_IRQ);

  // FIX01: Display-Rotation persistent: 0/90/180/270 Grad.
  // ST7789 setRotation() verwendet intern 0..3.
  static uint16_t nm_rotation_degrees = 90;
  static uint8_t nm_rotation_index = 1;

  static uint8_t nm_rotation_index_from_degrees(uint16_t degrees) {
    switch (degrees) {
      case 0:   return 0;
      case 90:  return 1;
      case 180: return 2;
      case 270: return 3;
      default:  return 1;
    }
  }

  static void nm_load_rotation() {
    Preferences p;
    p.begin("nm_display", true);
    nm_rotation_degrees = p.getUShort("rotation", 90);
    p.end();
    if (nm_rotation_degrees != 0 && nm_rotation_degrees != 90 &&
        nm_rotation_degrees != 180 && nm_rotation_degrees != 270) {
      nm_rotation_degrees = 90;
    }
    nm_rotation_index = nm_rotation_index_from_degrees(nm_rotation_degrees);
  }

  static void nm_save_rotation(uint16_t degrees) {
    nm_rotation_degrees = degrees;
    nm_rotation_index = nm_rotation_index_from_degrees(degrees);
    Preferences p;
    p.begin("nm_display", false);
    p.putUShort("rotation", nm_rotation_degrees);
    p.end();
  }

  // Wallet-Refresh: Standard 300 Sekunden, in NVS gespeichert.
  static uint32_t nm_wallet_interval_seconds = 300;
  static unsigned long nm_wallet_last_attempt = 0;

  static void nm_load_wallet_interval() {
    Preferences p; p.begin("nm_display", true);
    nm_wallet_interval_seconds = p.getUInt("wallet_sec", 300); p.end();
    if (nm_wallet_interval_seconds < 60) nm_wallet_interval_seconds = 60;
    if (nm_wallet_interval_seconds > 86400) nm_wallet_interval_seconds = 86400;
  }

  static void nm_save_wallet_interval(uint32_t seconds) {
    seconds = constrain(seconds, 60UL, 86400UL);
    nm_wallet_interval_seconds = seconds;
    Preferences p; p.begin("nm_display", false);
    p.putUInt("wallet_sec", nm_wallet_interval_seconds); p.end();
  }

  static void nm_apply_brightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    nm_brightness_percent = percent;

    // CYD-Backlight ist HIGH-aktiv:
    // 100% => Duty 255, 0% => Duty 0.
    const uint8_t pwm = (uint8_t)((percent * 255UL) / 100UL);
    ledcWrite(TFT_BACKLIGHT, pwm);
    nm_display_light_on = (percent > 0);
  }

  static void nm_backlight_setup() {
    // Arduino-ESP32 3.x API:
    // ledcAttach(pin, frequency, resolution)
    nm_backlight_pwm_active =
        ledcAttach(TFT_BACKLIGHT, NM_BL_FREQ, NM_BL_RESOLUTION);

    Preferences p;
    p.begin("nm_display", true);
    nm_brightness_percent = p.getUChar("brightness", 100);
    p.end();

    // Nach jedem Neustart soll das Licht AN sein. Ein eventuell alter
    // gespeicherter 0%-Wert darf den Bildschirm nicht dunkel starten lassen.
    if (nm_brightness_percent == 0) nm_brightness_percent = 100;
    nm_display_light_on = true;
    nm_apply_brightness(nm_brightness_percent);
    nm_display_light_on = true;
  }

  static void nm_save_brightness(uint8_t percent) {
    nm_apply_brightness(percent);
    Preferences p;
    p.begin("nm_display", false);
    p.putUChar("brightness", nm_brightness_percent);
    p.end();
  }

  // FIX12: LDR robust auf dem echten CYD-Messverhalten.
  // GPIO34, 12-Bit ADC, 10 Messungen gemittelt. CYD Pull-Up:
  // HELL = niedriger ADC, DUNKEL = hoher ADC.
  #define NM_LDR_BRIGHT      0
  #define NM_LDR_DARK      900
  #define NM_DIM_MEASURE    500UL
  #define NM_DIM_STEP        15

  static bool nm_auto_brightness = false;
  static uint16_t nm_ldr_raw = 0;
  static uint16_t nm_ldr_min = 4095;
  static uint16_t nm_ldr_max = 0;
  static uint8_t nm_ldr_output_percent = 0;
  static int nm_ldr_cur_pwm = 255;
  static int nm_ldr_target_pwm = 255;
  static unsigned long nm_ldr_last_read = 0;
  static unsigned long nm_ldr_last_step = 0;

  static void nm_load_auto_brightness() {
    Preferences p; p.begin("nm_display", true);
    nm_auto_brightness = p.getBool("auto_ldr", false);
    p.end();

    // Exakt wie im funktionierenden CYD-Projekt.
    analogReadResolution(12);
    pinMode(LDR_PIN, INPUT);
    analogSetPinAttenuation(LDR_PIN, ADC_11db);
    // Einen ersten ADC-Zyklus verwerfen; danach ist der Kanal sauber eingeschwungen.
    (void)analogRead(LDR_PIN);

    nm_ldr_cur_pwm = map(nm_brightness_percent, 0, 100, 0, 255);
    nm_ldr_target_pwm = nm_ldr_cur_pwm;
    nm_ldr_output_percent = nm_brightness_percent;
  }

  static void nm_save_auto_brightness(bool enabled) {
    nm_auto_brightness = enabled;
    Preferences p; p.begin("nm_display", false);
    p.putBool("auto_ldr", enabled); p.end();
    if (!enabled) {
      nm_apply_brightness(nm_brightness_percent);
      nm_ldr_cur_pwm = map(nm_brightness_percent, 0, 100, 0, 255);
      nm_ldr_target_pwm = nm_ldr_cur_pwm;
      nm_ldr_output_percent = nm_brightness_percent;
    }
  }

  static void nm_update_auto_brightness() {
    const unsigned long now = millis();

    // LDR fuer Debug auch bei AUTO=OFF weiter messen.
    if (now - nm_ldr_last_read >= NM_DIM_MEASURE) {
      nm_ldr_last_read = now;
      // Andere Initialisierungen duerfen GPIO34 nicht dauerhaft umkonfigurieren.
      pinMode(LDR_PIN, INPUT);
      uint32_t sum = 0;
      (void)analogRead(LDR_PIN); // erster ADC-Zyklus nach Kanalwechsel verwerfen
      for (uint8_t i = 0; i < 10; ++i) {
        sum += analogRead(LDR_PIN);
        delay(2);
      }
      nm_ldr_raw = (uint16_t)(sum / 10U);
      if (nm_ldr_raw < nm_ldr_min) nm_ldr_min = nm_ldr_raw;
      if (nm_ldr_raw > nm_ldr_max) nm_ldr_max = nm_ldr_raw;

      if (nm_auto_brightness) {
        const int brightMin = max(1, (int)map(nm_brightness_percent, 0, 100, 0, 255));
        const int brightMax = 255;
        nm_ldr_target_pwm = constrain(
          (int)map((long)nm_ldr_raw, NM_LDR_BRIGHT, NM_LDR_DARK, brightMax, brightMin),
          brightMin, brightMax);
      } else {
        nm_ldr_target_pwm = map(nm_brightness_percent, 0, 100, 0, 255);
      }
    }

    // Sanft und schnell zum Ziel: alle 50 ms um 15 PWM-Schritte.
    if (nm_auto_brightness && now - nm_ldr_last_step >= 50UL) {
      nm_ldr_last_step = now;
      if (nm_ldr_cur_pwm < nm_ldr_target_pwm)
        nm_ldr_cur_pwm = min(nm_ldr_target_pwm, nm_ldr_cur_pwm + NM_DIM_STEP);
      else if (nm_ldr_cur_pwm > nm_ldr_target_pwm)
        nm_ldr_cur_pwm = max(nm_ldr_target_pwm, nm_ldr_cur_pwm - NM_DIM_STEP);
      // Touch-OFF hat Vorrang vor LDR. Der LDR darf das Licht nicht wieder einschalten.
      if (nm_backlight_user_enabled) {
        ledcWrite(TFT_BACKLIGHT, nm_ldr_cur_pwm);
        nm_display_light_on = (nm_ldr_cur_pwm > 0);
      } else {
        ledcWrite(TFT_BACKLIGHT, 0);
        nm_display_light_on = false;
      }
    }

    nm_ldr_output_percent = (uint8_t)constrain((nm_ldr_cur_pwm * 100 + 127) / 255, 0, 100);

    #if LDR_INFO
      static unsigned long lastSerial = 0;
      if (now - lastSerial >= 1000UL) {
        lastSerial = now;
        Serial.printf("[LDR] RAW=%u MIN=%u MAX=%u AUTO=%s BASE=%u%% OUTPUT=%u%% TARGET=%d/255\n",
          nm_ldr_raw, nm_ldr_min, nm_ldr_max, nm_auto_brightness ? "ON" : "OFF",
          nm_brightness_percent, nm_ldr_output_percent, nm_ldr_target_pwm);
      }
    #endif
  }

  // FIX08: XPT2046 auf dem separaten Touch-SPI-Bus des CYD.
  // CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36 (aus dem funktionierenden CYD-Projekt).
  static void nm_touch_setup() {
    nm_touch_spi.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    nm_touch.begin(nm_touch_spi);
    nm_touch.setRotation(nm_rotation_index);
  }

  static void nm_touch_update() {
    const bool down = nm_touch.touched();
    const unsigned long now = millis();
    // Nur auf die neue Beruehrung reagieren, nicht mehrfach beim Gedrueckthalten.
    if (down && !nm_touch_was_down && (now - nm_touch_last_toggle > 350UL)) {
      nm_touch_last_toggle = now;
      nm_backlight_user_enabled = !nm_backlight_user_enabled;
      if (!nm_backlight_user_enabled) {
        ledcWrite(TFT_BACKLIGHT, 0);
        nm_display_light_on = false;
      } else {
        const int pwm = nm_auto_brightness ? nm_ldr_cur_pwm
                                           : map(nm_brightness_percent, 0, 100, 0, 255);
        ledcWrite(TFT_BACKLIGHT, pwm);
        nm_display_light_on = (pwm > 0);
      }
      #if TOUCH_INFO
        Serial.printf("[TOUCH] Backlight %s\n", nm_backlight_user_enabled ? "ON" : "OFF");
      #endif
    }
    #if TOUCH_INFO
      if (!down && nm_touch_was_down) {
        Serial.println("[TOUCH] RELEASE");
      }
    #endif
    nm_touch_was_down = down;
  }
#endif

#ifdef USE_LAN
  #include <ETH.h>
#endif

#if defined(WEB_DASHBOARD)
  #include "Dashboard.h"
#endif

#if defined(DISPLAY_CYD)
  #include "NMDisplay.h"
#elif defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2)
  #include "DisplayHal.h"
#endif

#if !defined(ESP8266) && defined(DISABLE_BROWNOUT)
    #include "soc/soc.h"
    #include "soc/rtc_cntl_reg.h"
#endif

// Auto adjust physical core count
// (ESP32-S2/C3 have 1 core, ESP32 has 2 cores, ESP8266 has 1 core)
#if defined(ESP8266)
    #define CORE 1
    typedef ESP8266WebServer WebServer;
#elif defined(CONFIG_FREERTOS_UNICORE)
    #define CORE 1
#else
    #define CORE 2

    void Task1Code( void * parameter );
    void Task2Code( void * parameter );
    TaskHandle_t Task1;
    TaskHandle_t Task2;
#endif

#if defined(WEB_DASHBOARD)
    WebServer server(80);
#endif 

#if defined(CAPTIVE_PORTAL)
  #include <FS.h> // This needs to be first, or it all crashes and burns...
  #include <WiFiManager.h>
  #include <Preferences.h>
  char duco_username[40];
  char duco_password[40];
  char duco_rigid[24];
  WiFiManager wifiManager;
  Preferences preferences;
  WiFiManagerParameter custom_duco_username("duco_usr", "Duino-Coin username", duco_username, 40);
  WiFiManagerParameter custom_duco_password("duco_pwd", "Duino-Coin mining key (if enabled in the wallet)", duco_password, 40);
  WiFiManagerParameter custom_duco_rigid("duco_rig", "Custom miner identifier (optional)", duco_rigid, 24);
  
  void saveConfigCallback() {
    preferences.begin("duino_config", false);
    preferences.putString("duco_username", custom_duco_username.getValue());
    preferences.putString("duco_password", custom_duco_password.getValue());
    preferences.putString("duco_rigid", custom_duco_rigid.getValue());
    preferences.end();
    RestartESP("Settings saved");
  }

  void reset_settings() {
    server.send(200, "text/html", "Settings have been erased. Please redo the configuration by connecting to the WiFi network that will be created");
    delay(500);
    wifiManager.resetSettings();
    RestartESP("Manual settings reset");
  }

  void saveParamCallback(){
    Serial.println("[CALLBACK] saveParamCallback fired");
    Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
  }

  String getParam(String name){
    //read parameter from server, for customhmtl input
    String value;
    if(wifiManager.server->hasArg(name)) {
      value = wifiManager.server->arg(name);
    }
    return value;
  }
#endif

void RestartESP(String msg) {
  #if defined(SERIAL_PRINTING)
    Serial.println(msg);
    Serial.println("Restarting ESP...");
  #endif

  #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
    display_info("Restarting ESP...");
  #endif

  #if defined(ESP8266)
    ESP.reset();
  #else
    ESP.restart();
    abort();
  #endif
}

#if defined(BLUSHYBOX)
    // FIX03_001 LED DISABLED: Ticker blinker;
    // FIX03_001 LED DISABLED: bool lastLedState = false;
    // FIX03_001 LED DISABLED: void changeState() {
      // FIX03_001 LED DISABLED: analogWrite(LED_BUILTIN, lastLedState ? 255 : 0);
      // FIX03_001 LED DISABLED: lastLedState = !lastLedState;
    // FIX03_001 LED DISABLED: }
#endif

#if defined(ESP8266)
    // WDT Loop 
    // See lwdtcb() and lwdtFeed() below
    Ticker lwdTimer;
    
    unsigned long lwdCurrentMillis = 0;
    unsigned long lwdTimeOutMillis = LWD_TIMEOUT;

    void ICACHE_RAM_ATTR lwdtcb(void) {
      if ((millis() - lwdCurrentMillis > LWD_TIMEOUT) || (lwdTimeOutMillis - lwdCurrentMillis != LWD_TIMEOUT))
        RestartESP("Loop WDT Failed!");
    }
    
    void lwdtFeed(void) {
      lwdCurrentMillis = millis();
      lwdTimeOutMillis = lwdCurrentMillis + LWD_TIMEOUT;
    }
#else
    void lwdtFeed(void) {
      Serial.println("lwdtFeed()");
    }
#endif

namespace {
    MiningConfig *configuration = new MiningConfig(
        DUCO_USER,
        RIG_IDENTIFIER,
        MINER_KEY
    );


    #ifdef USE_LAN
      static bool eth_connected = false;
    #endif

    void UpdateHostPort(String input) {
        // Thanks @ricaun for the code
        DynamicJsonDocument doc(256);
        deserializeJson(doc, input);
        const char *name = doc["name"];

        configuration->host = doc["ip"].as<String>().c_str();
        configuration->port = doc["port"].as<int>();
        node_id = String(name);

        #if defined(SERIAL_PRINTING)
          Serial.println("Poolpicker selected the best mining node: " + node_id);
        #endif

        #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
          display_info(node_id);
        #endif
    }

    void VerifyWifi() {
      #ifdef USE_LAN
        while ((!eth_connected) || (ETH.localIP() == IPAddress(0, 0, 0, 0))) {
          #if defined(SERIAL_PRINTING)
            Serial.println("Ethernet connection lost. Reconnect..." );
          #endif
          SetupWifi();
        }
      #else
        while (WiFi.status() != WL_CONNECTED 
                || WiFi.localIP() == IPAddress(0, 0, 0, 0)
                || WiFi.localIP() == IPAddress(192, 168, 4, 2) 
                || WiFi.localIP() == IPAddress(192, 168, 4, 3)) {
            #if defined(SERIAL_PRINTING)
              Serial.println("WiFi reconnecting...");
            #endif
            WiFi.disconnect();
            delay(500);
            WiFi.reconnect();
            delay(500);
        }
      #endif
    }

    String httpGetString(String URL) {
        String payload = "";
        
        WiFiClient client;
        HTTPClient http;

        http.begin(client, URL);
        http.addHeader("Accept", "*/*");
        
        int httpCode = http.GET();
        #if defined(SERIAL_PRINTING)
            Serial.printf("HTTP Response code: %d\n", httpCode);
        #endif

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = http.getString();
        } else {
            #if defined(SERIAL_PRINTING)
               Serial.printf("Error fetching node from poolpicker: %s\n", http.errorToString(httpCode).c_str());
               VerifyWifi();
            #endif
            #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
              display_info(http.errorToString(httpCode));
            #endif
        }
        http.end();
        return payload;
    }

    void SelectNode() {
        String input = "";
        int waitTime = 1;
        int poolIndex = 0;

        while (input == "") {
            #if defined(SERIAL_PRINTING)
              Serial.println("Fetching mining node from the poolpicker in " + String(waitTime) + "s");
            #endif
            delay(waitTime * 1000);
            
            input = httpGetString("http://server.duinocoin.com/getPool");
            
            // Increase wait time till a maximum of 32 seconds
            // (addresses: Limit connection requests on failure in ESP boards #1041)
            waitTime *= 2;
            if (waitTime > 32) 
                RestartESP("Node fetch unavailable");
        }

        UpdateHostPort(input);
      }

    void PrepareMiningNode() {
      #if MINING_MODE == 1
        SelectNode();
      #elif MINING_MODE == 2
        node_id = "DEMO";
        #if defined(SERIAL_PRINTING)
          Serial.println("MINING_MODE 2: Demo-Miner aktiv - kein Poolpicker, kein echtes Mining");
        #endif
        #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
          display_info("Demo miner");
        #endif
      #else
        node_id = "OFF";
        mining_mode_zero_values();
        #if defined(SERIAL_PRINTING)
          Serial.println("MINING_MODE 0: Mining aus - alle Mining-Werte = 0");
        #endif
        #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
          display_info("Mining off");
        #endif
      #endif
    }

    #ifdef USE_LAN
        void WiFiEvent(WiFiEvent_t event) {
            switch (event) {
              case ARDUINO_EVENT_ETH_START:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Started");
                #endif
                // The hostname must be set after the interface is started, but needs
                // to be set before DHCP, so set it from the event handler thread.
                ETH.setHostname("esp32-ethernet");
                break;
            case ARDUINO_EVENT_ETH_CONNECTED:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Connected");
                #endif
                break;
            case ARDUINO_EVENT_ETH_GOT_IP:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Got IP");
                #endif
                eth_connected = true;
                break;
            case ARDUINO_EVENT_ETH_DISCONNECTED:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Disconnected");
                #endif
                eth_connected = false;
                break;
            case ARDUINO_EVENT_ETH_STOP:
                #if defined(SERIAL_PRINTING)
                    Serial.println("ETH Stopped");
                #endif
                eth_connected = false;
                break;
            default:
                break;
            }
        }
    #endif

    void SetupWifi() {
      #ifdef USE_LAN
        #if defined(SERIAL_PRINTING)
            Serial.println("Connecting to Ethernet...");
        #endif
        WiFi.onEvent(WiFiEvent);  // Will call WiFiEvent() from another thread.
        ETH.begin();
        
        while (!eth_connected) {
            delay(500);
            #if defined(SERIAL_PRINTING)
                Serial.print(".");
            #endif
        }

        #if defined(SERIAL_PRINTING)
            Serial.println("\n\nSuccessfully connected to Ethernet");
            Serial.println("Local IP address: " + ETH.localIP().toString());
            Serial.println("Rig name: " + String(RIG_IDENTIFIER));
            Serial.println();
        #endif

      #else
        #if defined(SERIAL_PRINTING)
            Serial.println("Connecting to: " + String(SSID));
        #endif
        
        WiFi.begin(SSID, PASSWORD);
        while(WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(100);
        }
        VerifyWifi();
        
        #if !defined(ESP8266)
              WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), DNS_SERVER);
        #endif

        #if defined(SERIAL_PRINTING)
            Serial.println("\n\nSuccessfully connected to WiFi");
            Serial.println("Rig name: " + String(RIG_IDENTIFIER));
            Serial.println("Local IP address: " + WiFi.localIP().toString());
            Serial.println("Gateway: " + WiFi.gatewayIP().toString());
            Serial.println("DNS: " + WiFi.dnsIP().toString());
            Serial.println();
        #endif

      #endif

      #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
          display_info("Waiting for node...");
      #endif
      PrepareMiningNode();
    }

    void SetupOTA() {
        // Prepare OTA handler
        ArduinoOTA.onStart([]()
                           { 
                             #if defined(SERIAL_PRINTING)
                               Serial.println("Start"); 
                             #endif
                           });
        ArduinoOTA.onEnd([]()
                         { 
                            #if defined(SERIAL_PRINTING)
                              Serial.println("\nEnd"); 
                            #endif
                         });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                              { 
                                 #if defined(SERIAL_PRINTING)
                                   Serial.printf("Progress: %u%%\r", (progress / (total / 100))); 
                                 #endif
                              });
        ArduinoOTA.onError([](ota_error_t error)
                           {
                                Serial.printf("Error[%u]: ", error);
                                #if defined(SERIAL_PRINTING)
                                  if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
                                  else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
                                  else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
                                  else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
                                  else if (error == OTA_END_ERROR) Serial.println("End Failed");
                                #endif
                          });

        ArduinoOTA.setHostname("CYD-NerdMiner");
        #ifdef OTA_PASSWORD
          ArduinoOTA.setPassword(OTA_PASSWORD);
        #endif
        ArduinoOTA.begin();
    }

    #if defined(WEB_DASHBOARD)
        void dashboard() {
             #if defined(SERIAL_PRINTING)
               Serial.println("Handling HTTP client");
             #endif
             String s = WEBSITE;
             #ifdef USE_LAN
              s.replace("@@IP_ADDR@@", ETH.localIP().toString());
             #else
              s.replace("@@IP_ADDR@@", WiFi.localIP().toString());
             #endif
  
             s.replace("@@HASHRATE@@", String((hashrate+hashrate_core_two) / 1000));
             s.replace("@@DIFF@@", String(difficulty / 100));
             s.replace("@@SHARES@@", String(rejected_share_count) + "/" + String(accepted_share_count));
             s.replace("@@NODE@@", String(node_id));
             #if defined(DISPLAY_CYD)
               s.replace("@@BRIGHTNESS@@", String(nm_brightness_percent));
               s.replace("@@ROTATION@@", String(nm_rotation_degrees));
               s.replace("@@TEMP_INTERVAL@@", String(nm_temp_interval_seconds));
               s.replace("@@AUTO_BRIGHTNESS@@", nm_auto_brightness ? "checked" : "");
               s.replace("@@LDR_RAW@@", String(nm_ldr_raw));
               s.replace("@@WALLET_BALANCE@@", duco_wallet_valid ? duco_balance_raw : String("--"));
               s.replace("@@WALLET_USD@@", duco_wallet_valid ? nm_wallet_usd_text() : String("--"));
               s.replace("@@ACTIVE_DEVICES@@", String(duco_active_devices));
               s.replace("@@ACTIVE_THREADS@@", String(duco_active_threads));
               s.replace("@@TOTAL_HASHRATE@@", nm_format_hashrate(duco_total_hashrate));
               s.replace("@@TOTAL_ACCEPTED@@", String(duco_total_accepted));
               s.replace("@@TOTAL_REJECTED@@", String(duco_total_rejected));
               s.replace("@@MINER_ROWS@@", duco_miner_rows_html);
               s.replace("@@WALLET_INTERVAL@@", String(nm_wallet_interval_seconds));
               s.replace("@@WALLET_LAST_UPDATE@@", wallet_last_update_text());
               #if TFT_DIAGNOSTICS_ENABLED && WEB_DIAG_DISPLAY
                 {
                   String tftCard = R"HTML(<div class="card"><div class="title">DISPLAY-DIAGNOSE</div><div class="hint">TFT TEST zeichnet ein Testfeld. TFT RECOVER initialisiert SPI/ST7789 neu und zeichnet das Dashboard neu; Mining und WLAN laufen weiter.</div><button class="btn save" onclick="tftTest()">TFT TEST</button><button class="btn save" onclick="tftRecover()">TFT RECOVER</button>)HTML";
                   #if WEB_DIAG_TFT_ERRORLOG
                     tftCard += String(R"HTML(<hr style="border:0;border-top:1px solid #294552;margin:16px 0"><div class="title">DISPLAY-FEHLERPROTOKOLL</div><div id="tftLog">)HTML") + nm_displaydiag_html() + R"HTML(</div><button class="btn clearlog" onclick="clearTftLog()">DISPLAY-PROTOKOLL L&Ouml;SCHEN</button>)HTML";
                   #endif
                   tftCard += "<div id='tftStatus' class='status'></div></div>";
                   s.replace("@@TFT_DIAGNOSTICS_CARD@@", tftCard);
                 }
               #else
                 s.replace("@@TFT_DIAGNOSTICS_CARD@@", "");
               #endif
             #else
               s.replace("@@BRIGHTNESS@@", "100");
               s.replace("@@ROTATION@@", "90");
               s.replace("@@AUTO_BRIGHTNESS@@", "");
               s.replace("@@LDR_RAW@@", "--");
               s.replace("@@WALLET_BALANCE@@", "--");
               s.replace("@@WALLET_USD@@", "--");
               s.replace("@@ACTIVE_DEVICES@@", "0");
               s.replace("@@ACTIVE_THREADS@@", "0");
               s.replace("@@TOTAL_HASHRATE@@", "0.000 H/s");
               s.replace("@@TOTAL_ACCEPTED@@", "0");
               s.replace("@@TOTAL_REJECTED@@", "0");
               s.replace("@@MINER_ROWS@@", "<tr><td colspan='7'>--</td></tr>");
               s.replace("@@WALLET_INTERVAL@@", "300");
               s.replace("@@WALLET_LAST_UPDATE@@", "--");
               s.replace("@@TFT_DIAGNOSTICS_CARD@@", "");
             #endif
             
             #if defined(ESP8266)
                 s.replace("@@DEVICE@@", "ESP8266");
             #elif defined(CONFIG_FREERTOS_UNICORE)
                 s.replace("@@DEVICE@@", "ESP32-S2/C3");
             #else
                 s.replace("@@DEVICE@@", "ESP32");
             #endif
             
             s.replace("@@ID@@", String(RIG_IDENTIFIER));
             s.replace("@@MEMORY@@", String(ESP.getFreeHeap()));
             s.replace("@@VERSION@@", String(SOFTWARE_VERSION));

             String deviceDiagFields;
             String tempControls;
             #if WEB_DIAG_CPU_TEMP
               deviceDiagFields += "<div class='stat'><div class='value'><span id='cpuTemp'>" + String(nm_cpu_temp,1) + "</span> &deg;C</div><div class='label'>CPU TEMPERATUR</div></div>";
               deviceDiagFields += "<div class='stat'><div class='value'><span id='cpuTempMax'>" + String(nm_cpu_temp_max,1) + "</span> &deg;C</div><div class='label'>MAX. TEMPERATUR</div></div>";
               tempControls = "<div style='margin-top:14px'><div class='label' style='margin-bottom:6px'>TEMPERATUR-MESSINTERVALL IN SEKUNDEN</div><input id='tempInterval' type='number' min='1' max='3600' step='1' value='" + String(nm_temp_interval_seconds) + "'><div class='hint'>Web und Crashlogger verwenden den zuletzt gemessenen Wert.</div><button class='btn save' onclick='saveTempInterval()'>INTERVALL SPEICHERN</button><div id='tempStatus' class='status'></div></div>";
             #endif
             s.replace("@@DEVICE_DIAG_FIELDS@@", deviceDiagFields);
             s.replace("@@TEMP_CONTROLS@@", tempControls);

             #if WEB_DIAG_CRASHLOG
               s.replace("@@CRASH_CARD@@", String("<div class='card'><div class='title'>FEHLERBERICHT</div><div class='hint'>Maximal 10 echte Abstuerze. Normaler Neustart und Strom an/aus werden nicht als Absturz gezaehlt.</div><div id='crashlog'>") + crashlog_html() + "</div><button class='btn clearlog' onclick='clearCrashlog()'>FEHLERBERICHTE L&Ouml;SCHEN</button><div id='crashStatus' class='status'></div></div>");
             #else
               s.replace("@@CRASH_CARD@@", "");
             #endif
             #if WEB_DIAG_COLLISION
               s.replace("@@COLLISION_CARD@@", String("<div class='card'><div class='title'>KOLLISIONSSCHUTZ &middot; ZWEI T&Uuml;REN</div><div class='hint'>Diagnose des Dual-Core-Kollisionsschutzes.</div><div id='collisionStats'>") + collision_html() + "</div><button class='btn clearlog' onclick='clearCollisionStats()'>Z&Auml;HLER L&Ouml;SCHEN</button><div id='collisionStatus' class='status'></div></div>");
             #else
               s.replace("@@COLLISION_CARD@@", "");
             #endif

             #if defined(CAPTIVE_PORTAL)
                 s.replace("@@RESET_SETTINGS@@", "&bull; <a href='/reset'>Reset settings</a>");
             #else
                 s.replace("@@RESET_SETTINGS@@", "");
             #endif

             #if defined(USE_DS18B20)
                 sensors.requestTemperatures(); 
                 float temp = sensors.getTempCByIndex(0);
                 s.replace("@@SENSOR@@", "DS18B20: " + String(temp) + "*C");
             #elif defined(USE_DHT)
                 float temp = dht.readTemperature();
                 float hum = dht.readHumidity();
                 s.replace("@@SENSOR@@", "DHT11/22: " + String(temp) + "*C, " + String(hum) + "rh%");
             #elif defined(USE_HSU07M)
                 float temp = read_hsu07m();
                 s.replace("@@SENSOR@@", "HSU07M: " + String(temp) + "*C");
             #elif defined(USE_INTERNAL_SENSOR)
                 float temp = 0;
                 temp_sensor_read_celsius(&temp);
                 s.replace("@@SENSOR@@", "CPU: " + String(temp) + "*C");
             #else
                 s.replace("@@SENSOR@@", "None");
             #endif
                 
             server.send(200, "text/html", s);
        }

        #if defined(DISPLAY_CYD)
        void dashboard_wallet_refresh() {
            nm_wallet_last_attempt = millis();
            const bool ok = update_wallet_data();
            server.send(ok ? 200 : 503, "text/plain", ok ? "OK" : "Wallet-Aktualisierung fehlgeschlagen");
        }

        void dashboard_wallet_interval_save() {
            if (!server.hasArg("value")) { server.send(400, "text/plain", "value fehlt"); return; }
            long value = server.arg("value").toInt();
            if (value < 60 || value > 86400) { server.send(400, "text/plain", "Erlaubt: 60 bis 86400 Sekunden"); return; }
            nm_save_wallet_interval((uint32_t)value);
            server.send(200, "text/plain", "Gespeichert: " + String(nm_wallet_interval_seconds) + " Sekunden");
        }

        void dashboard_brightness_live() {
            if (!server.hasArg("value")) {
                server.send(400, "text/plain", "value fehlt");
                return;
            }
            int value = constrain(server.arg("value").toInt(), 0, 100);
            nm_apply_brightness((uint8_t)value);
            server.send(200, "text/plain", String(value));
        }

        void dashboard_brightness_save() {
            if (!server.hasArg("value")) {
                server.send(400, "text/plain", "value fehlt");
                return;
            }
            int value = constrain(server.arg("value").toInt(), 0, 100);
            nm_save_brightness((uint8_t)value);
            server.send(200, "text/plain", "Gespeichert: " + String(value) + "%");
        }

        void dashboard_auto_brightness_save() {
            if (!server.hasArg("enabled")) { server.send(400, "text/plain", "enabled fehlt"); return; }
            const bool enabled = server.arg("enabled") == "1";
            nm_save_auto_brightness(enabled);
            server.send(200, "text/plain", enabled ? "LDR-Automatik AN" : "LDR-Automatik AUS");
        }

        void dashboard_tft_test() {
            nm_display_test_pattern();
            server.send(200, "text/plain", "TFT TEST gesendet");
        }

        void dashboard_tft_recover() {
            nm_display_recover();
            server.send(200, "text/plain", "TFT RECOVER ausgefuehrt");
        }



        void dashboard_tft_log_clear() {
            nm_displaydiag_clear();
            server.send(200, "text/plain", "Display-Protokoll geloescht");
        }

        void dashboard_rotation_save() {
            if (!server.hasArg("value")) {
                server.send(400, "text/plain", "value fehlt");
                return;
            }
            const int value = server.arg("value").toInt();
            if (value != 0 && value != 90 && value != 180 && value != 270) {
                server.send(400, "text/plain", "Erlaubt: 0, 90, 180 oder 270 Grad");
                return;
            }
            nm_save_rotation((uint16_t)value);
            // FIX02: Rotation sofort sichtbar machen, kein Neustart noetig.
            nm_apply_rotation_now(nm_rotation_index);
            server.send(200, "text/plain", "Gespeichert und angewendet: " + String(value) + " Grad");
        }
        #endif

        void dashboard_diag_tempinterval() {
            if (!server.hasArg("value")) { server.send(400,"text/plain","value fehlt"); return; }
            uint32_t v=(uint32_t)server.arg("value").toInt(); if(v<1||v>3600){server.send(400,"text/plain","Erlaubt: 1 bis 3600 Sekunden");return;}
            nm_temp_interval_seconds=v; Preferences p; p.begin("nm_diag",false); p.putUInt("tempint",v); p.end(); nm_temp_update(true);
            server.send(200,"text/plain","Temperaturintervall: "+String(v)+" Sekunden");
        }


        void dashboard_api_status() {
            JsonDocument doc;
            doc["uptime_sec"] = (uint64_t)(millis() / 1000UL);
            doc["hashrate"] = (hashrate + hashrate_core_two) / 1000.0f;
            doc["difficulty"] = difficulty / 100;
            doc["shares"] = String(rejected_share_count) + "/" + String(accepted_share_count);
            doc["node"] = String(node_id);
            doc["free_heap"] = ESP.getFreeHeap();
            #if defined(DISPLAY_CYD)
              doc["wallet_balance"] = duco_wallet_valid ? duco_balance_raw : String("--");
              doc["wallet_usd"] = duco_wallet_valid ? nm_wallet_usd_text() : String("--");
              doc["wallet_last_update"] = wallet_last_update_text();
              doc["active_devices"] = duco_active_devices;
              doc["active_threads"] = duco_active_threads;
              doc["total_hashrate"] = nm_format_hashrate(duco_total_hashrate);
              doc["total_accepted"] = duco_total_accepted;
              doc["total_rejected"] = duco_total_rejected;
              doc["miner_rows_html"] = duco_miner_rows_html;
            #endif
            #if WEB_DIAG_CPU_TEMP
              doc["cpu_temp"] = nm_cpu_temp;
              doc["cpu_temp_max"] = nm_cpu_temp_max;
            #endif
            #if WEB_DIAG_CRASHLOG
              doc["crash_count"] = crashRecordCount;
            #endif
            #if WEB_DIAG_TFT_ERRORLOG
              doc["tft_error_count"] = nm_displaydiag_count;
            #endif
            String out; serializeJson(doc, out);
            server.sendHeader("Cache-Control", "no-store");
            server.send(200, "application/json", out);
        }

        void dashboard_api_diag() {
            JsonDocument doc;
            #if WEB_DIAG_CRASHLOG
              doc["crashlog_html"] = crashlog_html();
            #endif
            #if WEB_DIAG_COLLISION
              doc["collision_html"] = collision_html();
            #endif
            #if WEB_DIAG_TFT_ERRORLOG
              doc["tftlog_html"] = nm_displaydiag_html();
            #endif
            String out; serializeJson(doc, out);
            server.sendHeader("Cache-Control", "no-store");
            server.send(200, "application/json", out);
        }

        void dashboard_restart() {
            server.send(200, "text/plain", "Neustart...");
            delay(150);
            RestartESP("Web dashboard restart");
        }

        void dashboard_crashlog_clear() {
            crashlog_clear();
            server.send(200, "text/plain", "Fehlerberichte geloescht");
        }

        void dashboard_collision_clear() {
            collision_clear();
            server.send(200, "text/plain", "Kollisionszaehler geloescht");
        }
    #endif

} // End of namespace

MiningJob *job[CORE];

#if CORE == 2
#endif

void task1_func(void *parameter) {
    (void)parameter;
#if defined(ESP32) && CORE == 2
    for (;;) {
        #if MINING_MODE == 1
          job[0]->mine();
          // Echter Miner: Verhalten der RAW-Basis unveraendert.
          vTaskDelay(1);
        #elif MINING_MODE == 2
          // Demo: Werte kommen aus MiningJob.h, aber ohne Netzwerk/SHA/Mining-Last.
          mining_demo_tick(0);
          vTaskDelay(pdMS_TO_TICKS(120));
        #else
          // OFF: Anzeige bleibt aktiv, Mining-Werte bleiben Null.
          mining_mode_zero_values();
          vTaskDelay(pdMS_TO_TICKS(250));
        #endif

#if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
        unsigned long now = millis();
        if (now - lastDisplayUpdate >= 3000) {
            lastDisplayUpdate = now;
            float hashrate_float = (hashrate + hashrate_core_two) / 1000.0;
            float accept_rate = (share_count > 0) ? (accepted_share_count / 0.01 / share_count) : 0;
            long millisecs = now;
            int uptime_secs = int((millisecs / 1000) % 60);
            int uptime_mins = int((millisecs / (1000 * 60)) % 60);
            int uptime_hours = int((millisecs / (1000 * 60 * 60)) % 24);
            String uptime = String(uptime_hours) + "h" + String(uptime_mins) + "m" + String(uptime_secs) + "s";
            float sharerate = share_count / (millisecs / 1000.0);
            display_mining_results(String(hashrate_float, 1), String(rejected_share_count), String(accepted_share_count), String(uptime),
                                   String(node_id), String(difficulty / 100), String(sharerate, 1),
                                   String((uint32_t)((job_count_core0 + job_count_core1) % 1000000UL)), String(accept_rate, 1));
        }
#endif
    }
#endif
}

void task2_func(void *parameter) {
    (void)parameter;
#if defined(ESP32) && CORE == 2
    #if MINING_MODE == 1
      job[1] = new MiningJob(1, configuration);
    #endif

    for (;;) {
        #if MINING_MODE == 1
          job[1]->mine();
          vTaskDelay(1);
        #elif MINING_MODE == 2
          mining_demo_tick(1);
          vTaskDelay(pdMS_TO_TICKS(150));
        #else
          vTaskDelay(pdMS_TO_TICKS(250));
        #endif
        // Display wird nur in task1_func aktualisiert (kein doppeltes Rendern).
    }
#endif
}

void system_events_func(void *parameter);

void setup() {
    nm_diag_load();
    crashlog_begin();
    #if defined(DISPLAY_CYD)
      nm_displaydiag_begin();
      nm_displaydiag_set_enabled(WEB_DIAG_TFT_ERRORLOG != 0);
    #endif
    crashlog_boot_checkpoint(120); // CRASHLOG_READY
    collision_guard_begin();
    #if !defined(ESP8266) && defined(DISABLE_BROWNOUT)
        WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    #endif
    
    #if defined(SERIAL_PRINTING) || defined(TOUCH_DEBUG)
        Serial.begin(SERIAL_BAUDRATE);
    #endif
    #if defined(SERIAL_PRINTING)
        Serial.println("\n\nDuino-Coin " + String(configuration->MINER_VER));
    #endif
    // pinMode(LED_BUILTIN, OUTPUT);

    #if defined(BLUSHYBOX)
        // FIX03_001 LED DISABLED: analogWrite(LED_BUILTIN, 255);
        for (int i = 255; i > 0; i--) {
          // FIX03_001 LED DISABLED: analogWrite(LED_BUILTIN, i);
          delay(1);
        }
        pinMode(GAUGE_PIN, OUTPUT);
      
        // Gauge up and down effect on startup
        for (int i = GAUGE_MIN; i < GAUGE_MAX; i++) {
          analogWrite(GAUGE_PIN, i);
          delay(10);
        }
        for (int i = GAUGE_MAX; i > GAUGE_MIN; i--) {
          analogWrite(GAUGE_PIN, i);
          delay(10);
        }
    #endif

    #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
        crashlog_boot_checkpoint(130); // DISPLAY_INIT
        nm_load_rotation();
      screen_setup();
        nm_touch_setup();
        // FIX12: LDR erst NACH Display- und Touch-SPI initialisieren.
        // So ist GPIO34 garantiert zuletzt als ADC-Eingang konfiguriert.
        nm_load_auto_brightness();
        display_boot();
        delay(2500);
        crashlog_boot_checkpoint(140); // DISPLAY_READY
    #endif

    assert(CORE == 1 || CORE == 2);
    WALLET_ID = String(random(0, 2811)); // Needed for miner grouping in the wallet
    crashlog_boot_checkpoint(150); // MINER0_INIT
    #if MINING_MODE == 1
      job[0] = new MiningJob(0, configuration);
    #else
      job[0] = nullptr;
      mining_mode_zero_values();
    #endif
    crashlog_boot_checkpoint(160); // MINER0_READY

    #if defined(USE_DHT)
        #if defined(SERIAL_PRINTING)
          Serial.println("Initializing DHT sensor (Duino IoT)");
        #endif
        dht.begin();
        #if defined(SERIAL_PRINTING)
          Serial.println("Test reading: " + String(dht.readHumidity()) + "% humidity");
          Serial.println("Test reading: temperature " + String(dht.readTemperature()) + "Â°C");
        #endif
    #endif

    #if defined(USE_DS18B20)
        #if defined(SERIAL_PRINTING)
          Serial.println("Initializing DS18B20 sensor (Duino IoT)");
        #endif
        sensors.begin();
        sensors.requestTemperatures(); 
        #if defined(SERIAL_PRINTING)
          Serial.println("Test reading: " + String(sensors.getTempCByIndex(0)) + "Â°C");
        #endif
    #endif

    #if defined(USE_HSU07M)
        #if defined(SERIAL_PRINTING)
          Serial.println("Initializing HSU07M sensor (Duino IoT)");
          Serial.println("Test reading: " + String(read_hsu07m()) + "Â°C");
        #endif
    #endif

    #if defined(USE_INTERNAL_SENSOR)
       #if defined(SERIAL_PRINTING)
         Serial.println("Initializing internal ESP32 temperature sensor (Duino IoT)");
       #endif
       temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
       temp_sensor.dac_offset = TSENS_DAC_L2;
       temp_sensor_set_config(temp_sensor);
       temp_sensor_start();
       float result = 0;
       temp_sensor_read_celsius(&result);
       #if defined(SERIAL_PRINTING)
         Serial.println("Test reading: " + String(result) + "Â°C");
       #endif
    #endif

    WiFi.mode(WIFI_STA); // Setup ESP in client mode
    //WiFi.disconnect(true);
    #if defined(ESP8266)
        WiFi.setSleepMode(WIFI_NONE_SLEEP);
    #else
        WiFi.setSleep(false);
    #endif
    
    #if defined(CAPTIVE_PORTAL)
        preferences.begin("duino_config", false);
        strcpy(duco_username, preferences.getString("duco_username", "username").c_str());
        strcpy(duco_password, preferences.getString("duco_password", "None").c_str());
        strcpy(duco_rigid, preferences.getString("duco_rigid", "None").c_str());
        preferences.end();
        configuration->DUCO_USER = duco_username;
        configuration->RIG_IDENTIFIER = duco_rigid;
        configuration->MINER_KEY = duco_password;
        RIG_IDENTIFIER = duco_rigid;

        String captivePortalHTML = R"(
          <title>Duino BlushyBox</title>
          <style>
            body {
              background-color: #d4bff2;
              color: #363636;
            }
            button {
              box-shadow: 0px 0px 23px -7px #051700;
              background-color:#8645ef;
              border-radius:28px;
              border:1px solid #8645ef;
              display:inline-block;
              cursor:pointer;
              color:#ffffff;
              font-family:Arial;
              font-size:18px;
              padding:8px 16px;
              text-decoration:none;
            }
            input {
              box-shadow: 0px 0px 23px -7px #051700;
              background-color:#ffffff;
              border-radius:28px;
              border:1px solid #777777;
              font-family:Arial;
              font-size:18px;
              text-decoration:none;
            }
          </style>
        )";
      
        wifiManager.setCustomHeadElement(captivePortalHTML.c_str());
        
        wifiManager.setSaveConfigCallback(saveConfigCallback);
        wifiManager.addParameter(&custom_duco_username);
        wifiManager.addParameter(&custom_duco_password);
        wifiManager.addParameter(&custom_duco_rigid);

        #if defined(BLUSHYBOX)
          // FIX03_001 LED DISABLED: blinker.attach_ms(200, changeState);
        #endif
        wifiManager.autoConnect("Duino-Coin");
        delay(1000);
        VerifyWifi();
        #if defined(BLUSHYBOX)
          // FIX03_001 LED DISABLED: blinker.detach();
        #endif
        
        #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
            display_info("Waiting for node...");
        #endif
        #if defined(BLUSHYBOX)
          // FIX03_001 LED DISABLED: blinker.attach_ms(500, changeState);
        #endif
        PrepareMiningNode();
        #if defined(BLUSHYBOX)
          // FIX03_001 LED DISABLED: blinker.detach();
        #endif
    #else
        #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
          display_info("Waiting for WiFi...");
        #endif
        SetupWifi();
    #endif
    #if defined(DISPLAY_CYD)
      nm_load_wallet_interval();
      nm_wallet_last_attempt = millis();
      update_wallet_data();
    #endif

    SetupOTA();

    #if defined(WEB_DASHBOARD)
      if (!MDNS.begin(RIG_IDENTIFIER)) {
        #if defined(SERIAL_PRINTING)
          Serial.println("mDNS unavailable");
        #endif
      }
      MDNS.addService("http", "tcp", 80);
      #if defined(SERIAL_PRINTING)
        #ifdef USE_LAN
          Serial.println("Configured mDNS for dashboard on http://" + String(RIG_IDENTIFIER) 
                     + ".local (or http://" + ETH.localIP().toString() + ")");
        #else
          Serial.println("Configured mDNS for dashboard on http://" + String(RIG_IDENTIFIER) 
                     + ".local (or http://" + WiFi.localIP().toString() + ")");
        #endif
      #endif

      server.on("/", dashboard);
      server.on("/api/status", HTTP_GET, dashboard_api_status);
      server.on("/api/diag", HTTP_GET, dashboard_api_diag);
      #if defined(DISPLAY_CYD)
        server.on("/wallet/refresh", HTTP_POST, dashboard_wallet_refresh);
        server.on("/wallet/interval", HTTP_GET, dashboard_wallet_interval_save);
        server.on("/brightness", HTTP_GET, dashboard_brightness_live);
        server.on("/brightness/save", HTTP_GET, dashboard_brightness_save);
        server.on("/brightness/auto", HTTP_GET, dashboard_auto_brightness_save);
        server.on("/rotation/save", HTTP_GET, dashboard_rotation_save);
        #if TFT_DIAGNOSTICS_ENABLED
          server.on("/tft/test", HTTP_POST, dashboard_tft_test);
          server.on("/tft/recover", HTTP_POST, dashboard_tft_recover);
          server.on("/tft/log/clear", HTTP_POST, dashboard_tft_log_clear);
        #endif
      #endif
      server.on("/diag/tempinterval", HTTP_GET, dashboard_diag_tempinterval);
      server.on("/crashlog/clear", HTTP_POST, dashboard_crashlog_clear);
      server.on("/collision/clear", HTTP_POST, dashboard_collision_clear);
      server.on("/restart", HTTP_POST, dashboard_restart);
      #if defined(CAPTIVE_PORTAL)
        server.on("/reset", reset_settings);
      #endif
      server.begin();
    #endif

    #if defined(ESP8266)
        // Start the WDT watchdog
        lwdtFeed();
        lwdTimer.attach_ms(LWD_TIMEOUT, lwdtcb);
    #endif

    #if defined(ESP8266)
        // Fastest clock mode for 8266s
        system_update_cpu_freq(160);
        os_update_cpu_frequency(160);
        // Feed the watchdog
        lwdtFeed();
    #else
        // Fastest clock mode for 32s
        setCpuFrequencyMhz(240);
    #endif

    #if MINING_MODE == 1
      // FIX03_001 LED DISABLED: job[0]->blink(BLINK_SETUP_COMPLETE);
    #else
      // Kein MiningJob-Objekt in OFF/DEMO. LED einfach in den Ruhe-Zustand setzen.
      // FIX03_001 LED DISABLED: digitalWrite(LED_BUILTIN, HIGH);
    #endif

    #if defined(ESP32) && CORE == 2
      crashlog_boot_checkpoint(250); // TASKS_CREATE
      #if defined(DISPLAY_CYD)
        nm_display_mailbox_begin();
      #endif
      #if MINING_MODE != 0
        nm_mining_mailbox_begin();
      #endif
      xTaskCreatePinnedToCore(system_events_func, "system_events_func", 10000, NULL, TASK_PRIO_SYSTEM, NULL, 0);
      xTaskCreatePinnedToCore(task1_func, "task1_func", 10000, NULL, TASK_PRIO_MINING0, &Task1, 0);
      #if MINING_MODE != 0
        xTaskCreatePinnedToCore(task2_func, "task2_func", 10000, NULL, TASK_PRIO_MINING1, &Task2, 1);
      #endif
      crashlog_boot_checkpoint(260); // TASKS_CREATED
    #endif
    crashlog_boot_checkpoint(270); // SETUP_DONE
}

void system_events_func(void* parameter) {
  static unsigned long crashHeartbeatLast = 0;
  while (true) {
    delay(10);
    unsigned long crashNow = millis();
    nm_temp_update();
    if (crashNow - crashHeartbeatLast >= 1000UL) {
      crashHeartbeatLast = crashNow;
      crashlog_heartbeat(share_count, accepted_share_count, crashNow, nm_cpu_temp, nm_cpu_temp_max);
    }
    #if defined(WEB_DASHBOARD)
      server.handleClient();
    #endif
    ArduinoOTA.handle();

    #if defined(DISPLAY_CYD)
      nm_update_auto_brightness();
      nm_touch_update();

      // Uhr unabhängig vom Miner-Refresh aktualisieren.
      update_display_clock();
    #endif

    // Wallet mit dem im Web-Dashboard eingestellten Intervall aktualisieren.
    #if defined(DISPLAY_CYD)
      const unsigned long walletNow = millis();
      const unsigned long walletIntervalMs = nm_wallet_interval_seconds * 1000UL;
      if (walletNow - nm_wallet_last_attempt >= walletIntervalMs) {
        nm_wallet_last_attempt = walletNow;
        update_wallet_data();
      }
    #endif
  }
}

void single_core_loop() {
    #if MINING_MODE == 1
      job[0]->mine();
    #elif MINING_MODE == 2
      mining_demo_tick(0);
      delay(120);
    #else
      mining_mode_zero_values();
      delay(250);
    #endif
    
    lwdtFeed();
    
    #if defined(DISPLAY_SSD1306) || defined(DISPLAY_16X2) || defined(DISPLAY_CYD)
       // Display nur alle 3000ms aktualisieren - spart ~50% Rechenzeit beim SSD1306!
       #define DISPLAY_UPDATE_INTERVAL 3000
       unsigned long now = millis();
       if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
         lastDisplayUpdate = now;

         float hashrate_float = (hashrate+hashrate_core_two) / 1000.0;
         float accept_rate = (share_count > 0) ? (accepted_share_count / 0.01 / share_count) : 0;
         
         long millisecs = now;
         int uptime_secs = int((millisecs / 1000) % 60);
         int uptime_mins = int((millisecs / (1000 * 60)) % 60);
         int uptime_hours = int((millisecs / (1000 * 60 * 60)) % 24);
         String uptime = String(uptime_hours) + "h" + String(uptime_mins) + "m" + String(uptime_secs) + "s";
         
         float sharerate = share_count / (millisecs / 1000.0);

         display_mining_results(String(hashrate_float, 1), String(rejected_share_count), String(accepted_share_count), String(uptime), 
                                String(node_id), String(difficulty / 100), String(sharerate, 1),
                                String((uint32_t)((job_count_core0 + job_count_core1) % 1000000UL)), String(accept_rate, 1));
       }
    #endif

    // System-Events nur alle 5 Sekunden prüfen (nicht bei jedem Mine-Aufruf!)
    static unsigned long lastSysCheck = 0;
    unsigned long nowSys = millis();
    if (nowSys - lastSysCheck >= 5000) {
      lastSysCheck = nowSys;
      VerifyWifi();
    }
    ArduinoOTA.handle();
    #if defined(WEB_DASHBOARD) 
        server.handleClient();
    #endif
}

void loop() {
  #if defined(ESP8266) || defined(CONFIG_FREERTOS_UNICORE)
    single_core_loop();
  #elif defined(ESP32) && CORE == 2
    // Dual-Core: Mining läuft in Tasks, loop() nur für System-Events
    delay(10);
  #endif
}
