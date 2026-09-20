#pragma GCC optimize("-Ofast")

#ifndef MINING_JOB_H
#define MINING_JOB_H

#include <Arduino.h>
#include <assert.h>
#include <string.h>
#include <Ticker.h>
#include <WiFiClient.h>

#include "DSHA1.h"
#include "Counter.h"
#include "Settings.h"
#include "CrashLog.h"
#include "CollisionGuard.h"
#include "MiningMailbox.h"

// https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TypeConversion.cpp
const char base36Chars[36] PROGMEM = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};

const uint8_t base36CharValues[75] PROGMEM{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0,                                                                        // 0 to 9
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 0, 0, 0, 0, 0, 0, // Upper case letters
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35                    // Lower case letters
};

#define SPC_TOKEN ' '
#define END_TOKEN '\n'
#define SEP_TOKEN ','
#define IOT_TOKEN '@'

// -----------------------------------------------------------------------------
// MINING_MODE 0/2 helpers
// Diese Routinen liegen bewusst in MiningJob.h: Im Demo-Modus kommen die Werte
// aus dem Mining-Modul, laufen danach aber durch dieselben globalen Statistik-
// Variablen / Mining-Mailbox und dieselben Display-Routinen wie beim echten Miner.
// Keine Netzwerk-Jobs, keine SHA-Berechnung und kein MiningJob-Objekt noetig.
// -----------------------------------------------------------------------------
static inline void mining_mode_zero_values() {
    hashrate = 0;
    hashrate_core_two = 0;
    difficulty = 0;
    share_count = 0;
    accepted_share_count = 0;
    rejected_share_count = 0;
    job_count_core0 = 0;
    job_count_core1 = 0;
}

static inline void mining_demo_tick(uint8_t core) {
    // Leichtgewichtiger Demo-Ablauf: nur ein paar Integer-Operationen und Random-Werte.
    // Core 0 pflegt die gemeinsamen Werte; beide Cores liefern eigene Hashrate/Jobs.
    const uint32_t r = esp_random();

    if (core == 0) {
        // ca. 36..72 kH/s pro Core; Anzeige summiert beide Cores.
        hashrate = 36000U + (r % 36001U);

        // Display teilt difficulty durch 100. Ergebnis hier: ca. 50..2500.
        difficulty = (50U + ((r >> 8) % 2451U)) * 100U;
        job_count_core0++;

        // Nicht jeder Demo-Job erzeugt einen Share. Die Share-Zaehler laufen wie
        // beim echten Miner ueber die vorhandene MiningMailbox.
        if ((r % 7U) == 0U) {
            nm_mining_signal(0, NM_EVT_SHARE_FOUND);
            if (((r >> 16) % 20U) == 0U)
                nm_mining_signal(0, NM_EVT_REJECTED);
            else
                nm_mining_signal(0, NM_EVT_ACCEPTED);
        }
    } else {
        hashrate_core_two = 36000U + (r % 36001U);
        job_count_core1++;

        if ((r % 9U) == 0U) {
            nm_mining_signal(1, NM_EVT_SHARE_FOUND);
            if (((r >> 16) % 24U) == 0U)
                nm_mining_signal(1, NM_EVT_REJECTED);
            else
                nm_mining_signal(1, NM_EVT_ACCEPTED);
        }
    }
}

struct MiningConfig {
    String host = "";
    int port = 0;
    String DUCO_USER = "";
    String RIG_IDENTIFIER = "";
    String MINER_KEY = "";
    String MINER_VER = SOFTWARE_VERSION;
    #if defined(ESP8266)
        // "High-band" 8266 diff
        String START_DIFF = "ESP8266H";
    #elif defined(CONFIG_FREERTOS_UNICORE)
        // Single core 32 diff
        String START_DIFF = "ESP32S";
    #else
        // Normal 32 diff
        String START_DIFF = "ESP32";
    #endif

    MiningConfig(String DUCO_USER, String RIG_IDENTIFIER, String MINER_KEY)
            : DUCO_USER(DUCO_USER), RIG_IDENTIFIER(RIG_IDENTIFIER), MINER_KEY(MINER_KEY) {}
};

class MiningJob {

public:
    MiningConfig *config;
    int core = 0;
    unsigned long job_difficulty_display = 0; // FIX8: Difficulty dieses Jobs, ohne Core-uebergreifenden Statistikzugriff

    MiningJob(int core, MiningConfig *config) {
        this->core = core;
        this->config = config;
        this->client_buffer = "";
        dsha1 = new DSHA1();
        dsha1->warmup();
        generateRigIdentifier();
    }

    // FIX03_001 LED DISABLED: void blink(uint8_t count, uint8_t pin = LED_BUILTIN) {
    // FIX03_001 LED DISABLED:     #if defined(LED_BLINKING)
    // FIX03_001 LED DISABLED:         uint8_t state = HIGH;
    // FIX03_001 LED DISABLED:
    // FIX03_001 LED DISABLED:         for (int x = 0; x < (count << 1); ++x) {
    // FIX03_001 LED DISABLED:             digitalWrite(pin, state ^= HIGH);
    // FIX03_001 LED DISABLED:             delay(50);
    // FIX03_001 LED DISABLED:         }
    // FIX03_001 LED DISABLED:     #else
    // FIX03_001 LED DISABLED:         digitalWrite(LED_BUILTIN, HIGH);
    // FIX03_001 LED DISABLED:     #endif
    // FIX03_001 LED DISABLED: }

    bool max_micros_elapsed(unsigned long current, unsigned long max_elapsed) {
        // V11: pro MiningJob-Instanz eigener Timer.
        // Vorher war _start als "static" lokal in dieser Funktion definiert.
        // Dadurch teilten sich Core 0 und Core 1 denselben Zeitstempel und
        // konnten sich gegenseitig das Watchdog-Yield wegnehmen.
        if ((current - system_event_timer) > max_elapsed) {
            system_event_timer = current;
            return true;
        }
        return false;
    }

    void handleSystemEvents(void) {
        #if defined(ESP32) && CORE == 2
          esp_task_wdt_reset();
        #endif
        delay(1); // Minimal delay - füttert den RTOS Watchdog ohne viel Zeit zu verlieren
                  // delay(10) war vorher hier = 10% Verlust, delay(1) = nur ~1%
    }

    void mine() {
        crashlog_checkpoint((uint8_t)core, 10);

        // FIX7: Zwei Tueren. Erst bevorzugte Tuer des Cores, dann die andere.
        // Nur wenn beide besetzt sind, wird dieser Vorgang einmal verworfen.
        int8_t jobDoor = collision_try_enter((uint8_t)core);
        if (jobDoor < 0) {
            crashlog_checkpoint((uint8_t)core, 70); // beide Tueren besetzt -> genau einmal verwerfen
            return;
        }
        crashlog_checkpoint((uint8_t)core, 71); // eine der zwei Tueren belegt
        connectToNode();
        askForJob();
        collision_leave(jobDoor);
        crashlog_checkpoint((uint8_t)core, 72, (uint16_t)getLastBlockHash().length()); // TRYLOCK_JOB_LEAVE
        crashlog_checkpoint((uint8_t)core, 20, (uint16_t)getLastBlockHash().length());

        crashlog_checkpoint((uint8_t)core, 30, (uint16_t)getLastBlockHash().length());
        dsha1->reset().write((const unsigned char *)getLastBlockHash().c_str(), getLastBlockHash().length());
        crashlog_checkpoint((uint8_t)core, 40, (uint16_t)getLastBlockHash().length());

        int start_time = micros();
        max_micros_elapsed(start_time, 0);
        // FIX03_001 LED DISABLED: #if defined(LED_BLINKING)
        // FIX03_001 LED DISABLED:     #if defined(BLUSHYBOX)
        // FIX03_001 LED DISABLED:       for (int i = 0; i < 72; i++) {
        // FIX03_001 LED DISABLED:         analogWrite(LED_BUILTIN, i);
        // FIX03_001 LED DISABLED:         delay(1);
        // FIX03_001 LED DISABLED:       }
        // FIX03_001 LED DISABLED:     #else
        // FIX03_001 LED DISABLED:       digitalWrite(LED_BUILTIN, LOW);
        // FIX03_001 LED DISABLED:     #endif
        // FIX03_001 LED DISABLED: #endif
        for (Counter<10> counter; counter < difficulty; ++counter) {
            DSHA1 ctx = *dsha1;
            ctx.write((const unsigned char *)counter.c_str(), counter.strlen()).finalize(hashArray);
            
            #ifndef CONFIG_FREERTOS_UNICORE
                #if defined(ESP32)
                    // FIX04: Core 0 traegt WiFi/Web/TFT/Systemarbeit und gibt deshalb
                    // deutlich haeufiger CPU-Zeit frei. Core 1 bleibt der schnelle Miner.
                    const unsigned long SYSTEM_TIMEOUT = (core == 0)
                        ? CORE0_MINING_YIELD_US
                        : CORE1_MINING_YIELD_US;
                #else 
                    #define SYSTEM_TIMEOUT 500000 // 500 ms for 8266
                #endif
                if (max_micros_elapsed(micros(), SYSTEM_TIMEOUT)) {
                    handleSystemEvents();
                } 
            #endif

            if (memcmp(getExpectedHash(), hashArray, 20) == 0) {
                unsigned long elapsed_time = micros() - start_time;
                float elapsed_time_s = elapsed_time * .000001f;
                nm_mining_signal((uint8_t)core, NM_EVT_SHARE_FOUND);

                // FIX03_001 LED DISABLED: #if defined(LED_BLINKING)
                // FIX03_001 LED DISABLED:     #if defined(BLUSHYBOX)
                // FIX03_001 LED DISABLED:         for (int i = 72; i > 0; i--) {
                // FIX03_001 LED DISABLED:           analogWrite(LED_BUILTIN, i);
                // FIX03_001 LED DISABLED:           delay(1);
                // FIX03_001 LED DISABLED:         }
                // FIX03_001 LED DISABLED:     #else
                // FIX03_001 LED DISABLED:         digitalWrite(LED_BUILTIN, HIGH);
                // FIX03_001 LED DISABLED:     #endif
                // FIX03_001 LED DISABLED: #endif

                if (String(core) == "0") {
                    hashrate = counter / elapsed_time_s;
                    submitWithTryLock(counter, hashrate, elapsed_time_s);
                } else {
                    hashrate_core_two = counter / elapsed_time_s;
                    submitWithTryLock(counter, hashrate_core_two, elapsed_time_s);
                }

                #if defined(BLUSHYBOX)
                    gauge_set(hashrate + hashrate_core_two);
                #endif
                
                break;
            }
        }
    }

private:
    // Eigener Event-/Watchdog-Timer je MiningJob/Core.
    unsigned long system_event_timer = 0;

    String client_buffer;
    uint8_t hashArray[20];
    String last_block_hash;
    String expected_hash_str;
    uint8_t expected_hash[20];
    DSHA1 *dsha1;
    WiFiClient client;
    String chipID = "";

    #if defined(ESP8266)
        #if defined(BLUSHYBOX)
          String MINER_BANNER = "Official BlushyBox Miner (ESP8266)";
        #else
          String MINER_BANNER = "Official ESP8266 Miner";
        #endif
    #elif defined(CONFIG_FREERTOS_UNICORE)
        String MINER_BANNER = "Official ESP32-S2 Miner";
    #else
        #if defined(BLUSHYBOX)
          String MINER_BANNER = "Official BlushyBox Miner (ESP32)";
        #else
          String MINER_BANNER = "Official ESP32 Miner";
        #endif
    #endif

    uint8_t *hexStringToUint8Array(const String &hexString, uint8_t *uint8Array, const uint32_t arrayLength) {
        // Sicherer Check statt assert() - kein Crash bei ungültigem Server-Paket
        if (hexString.length() < arrayLength * 2) {
            #if defined(SERIAL_PRINTING)
              Serial.println("WARNUNG: Ungültiger Hash vom Server (" + String(hexString.length()) + " Zeichen), Job wird übersprungen");
            #endif
            memset(uint8Array, 0, arrayLength); // Nullen statt Crash
            return uint8Array;
        }
        const char *hexChars = hexString.c_str();
        for (uint32_t i = 0; i < arrayLength; ++i) {
            uint8Array[i] = (pgm_read_byte(base36CharValues + hexChars[i * 2] - '0') << 4) + pgm_read_byte(base36CharValues + hexChars[i * 2 + 1] - '0');
        }
        return uint8Array;
    }

    void generateRigIdentifier() {
        String AutoRigName = "";

        #if defined(ESP8266)
            chipID = String(ESP.getChipId(), HEX);

            if (strcmp(config->RIG_IDENTIFIER.c_str(), "Auto") != 0)
                return;

            AutoRigName = "ESP8266-" + chipID;
            AutoRigName.toUpperCase();
            config->RIG_IDENTIFIER = AutoRigName.c_str();
        #else
            uint64_t chip_id = ESP.getEfuseMac();
            uint16_t chip = (uint16_t)(chip_id >> 32); // Prepare to print a 64 bit value into a char array
            char fullChip[23];
            snprintf(fullChip, 23, "%04X%08X", chip,
                    (uint32_t)chip_id); // Store the (actually) 48 bit chip_id into a char array

            chipID = String(fullChip);

            if (strcmp(config->RIG_IDENTIFIER.c_str(), "Auto") != 0)
                return;
            // Autogenerate ID if required
            AutoRigName = "ESP32-" + String(fullChip);
            AutoRigName.toUpperCase();
            config->RIG_IDENTIFIER = AutoRigName.c_str();
        #endif 
        #if defined(SERIAL_PRINTING)
          Serial.println("Core [" + String(core) + "] - Rig identifier: "
                          + config->RIG_IDENTIFIER);
        #endif
    }

    void connectToNode() {
        if (client.connected()) return;

        unsigned int stopWatch = millis();
        #if defined(SERIAL_PRINTING)
          Serial.println("Core [" + String(core) + "] - Connecting to a Duino-Coin node...");
        #endif
        while (!client.connect(config->host.c_str(), config->port)) {
            if (max_micros_elapsed(micros(), 100000)) {
                handleSystemEvents();
            } 
            if (millis()-stopWatch>100000) ESP.restart();
        }
        
        waitForClientData();
        #if defined(SERIAL_PRINTING)
          Serial.println("Core [" + String(core) + "] - Connected. Node reported version: "
                          + client_buffer);
        #endif

        // FIX03_001 LED DISABLED: blink(BLINK_CLIENT_CONNECT); 

        /* client.print("MOTD" + END_TOKEN);
        waitForClientData();
        #if defined(SERIAL_PRINTING)
          Serial.println("Core [" + String(core) + "] - MOTD: "
                          + client_buffer);
        #endif */
    }

    void waitForClientData() {
        client_buffer = "";
        unsigned int stopWatch = millis();
        while (client.connected()) {
            if (client.available()) {
                client_buffer = client.readStringUntil(END_TOKEN);
                if (client_buffer.length() == 1 && client_buffer[0] == END_TOKEN)
                    client_buffer = "???\n"; // NOTE: Should never happen
                break;
            }
            if (max_micros_elapsed(micros(), 100000)) {
                handleSystemEvents();
            }
            if (millis()-stopWatch>120000) {
              Serial.println("Timeout after 120s. Forced restart..");
              ESP.restart();
            }
        }
    }

    void submitWithTryLock(unsigned long counter, float current_hashrate, float elapsed_time_s) {
        // Auch beim Share-Senden niemals die gemeinsame Tuer einrennen.
        int8_t submitDoor = collision_try_enter((uint8_t)core);
        if (submitDoor < 0) {
            crashlog_checkpoint((uint8_t)core, 73); // beide Tueren besetzt -> genau einmal verwerfen
            client.stop();
            return;
        }
        crashlog_checkpoint((uint8_t)core, 74); // eine der zwei Tueren belegt
        submit(counter, current_hashrate, elapsed_time_s);
        collision_leave(submitDoor);
        crashlog_checkpoint((uint8_t)core, 75); // TRYLOCK_SUBMIT_LEAVE
    }

    void submit(unsigned long counter, float hashrate, float elapsed_time_s) {
        client.print(String(counter) +
                     SEP_TOKEN + String(hashrate) +
                     SEP_TOKEN + MINER_BANNER +
                     SPC_TOKEN + config->MINER_VER +
                     SEP_TOKEN + config->RIG_IDENTIFIER +
                     SEP_TOKEN + "DUCOID" + String(chipID) +
                     SEP_TOKEN + String(WALLET_ID) +
                     END_TOKEN);

        // FIX8: Keine Ping-/ms-Messung mehr fuer die NMTV-Anzeige.
        waitForClientData();

        // FIX5: Serverantwort sauber trennen. Nur GOOD ist Accepted;
        // BAD (auch BAD,<Grund>) ist ein echter Server-Reject.
        if (client_buffer == "GOOD") {
          nm_mining_signal((uint8_t)core, NM_EVT_ACCEPTED);
        } else if (client_buffer.startsWith("BAD")) {
          nm_mining_signal((uint8_t)core, NM_EVT_REJECTED);
        }

        // BRIEFKASTEN FIX02: Zaehler werden zentral vom Mailbox-Auswerter gepflegt.

        #if defined(SERIAL_PRINTING)
          Serial.println("Core [" + String(core) + "] - " +
                          client_buffer +
                          " share #" + String(share_count) +
                          " (" + String(counter) + ")" +
                          " hashrate: " + String(hashrate / 1000, 2) + " kH/s (" +
                          String(elapsed_time_s) + "s) " + 
                          "(" + node_id + ")\n");
        #endif
    }

    bool parse() {
        // Ungültige oder leere Antwort vom Server abfangen
        if (client_buffer.length() < 10) {
            #if defined(SERIAL_PRINTING)
              Serial.println("Core [" + String(core) + "] - Ungültige Server-Antwort, überspringe Job: '" + client_buffer + "'");
            #endif
            client.stop(); // Verbindung trennen, beim nächsten mine() neu verbinden
            return false;
        }

        // Create a non-constant copy of the input string
        char *job_str_copy = strdup(client_buffer.c_str());

        if (job_str_copy) {
            String tokens[3];
            char *token = strtok(job_str_copy, ",");
            for (int i = 0; token != NULL && i < 3; i++) {
                tokens[i] = token;
                token = strtok(NULL, ",");
            }

            // Prüfen ob alle Tokens vorhanden und gültig sind
            if (tokens[0].length() < 10 || tokens[1].length() < 40 || tokens[2].length() == 0) {
                #if defined(SERIAL_PRINTING)
                  Serial.println("Core [" + String(core) + "] - Unvollständiger Job, überspringe");
                #endif
                free(job_str_copy);
                client.stop();
                return false;
            }

            last_block_hash = tokens[0];
            expected_hash_str = tokens[1];
            hexStringToUint8Array(expected_hash_str, expected_hash, 20);
            job_difficulty_display = tokens[2].toInt();
            difficulty = (unsigned int)(job_difficulty_display * 100UL + 1UL);

            // FIX9: Ein vollstaendig empfangener und gueltig geparster Job zaehlt genau einmal.
            // Getrennte Core-Zaehler vermeiden einen gemeinsamen Schreibzugriff.
            if (core == 0) job_count_core0++;
            else job_count_core1++;

            free(job_str_copy);
            return true;
        }
        else {
            return false;
        }
    }

    void askForJob() {
        Serial.println("Core [" + String(core) + "] - Asking for a new job for user: " 
                        + String(config->DUCO_USER));

        #if defined(USE_DS18B20)
            sensors.requestTemperatures(); 
            float temp = sensors.getTempCByIndex(0);
            #if defined(SERIAL_PRINTING)
              Serial.println("DS18B20 reading: " + String(temp) + "Â°C");
            #endif
        
            client.print("JOB," +
                         String(config->DUCO_USER) +
                         SEP_TOKEN + config->START_DIFF + 
                         SEP_TOKEN + String(config->MINER_KEY) + 
                         SEP_TOKEN + "Temp:" + String(temp) + "*C" +
                         END_TOKEN);
        #elif defined(USE_DHT)
            float temp = dht.readTemperature();
            float hum = dht.readHumidity();
            #if defined(SERIAL_PRINTING)
              Serial.println("DHT reading: " + String(temp) + "Â°C");
              Serial.println("DHT reading: " + String(hum) + "%");
            #endif

            client.print("JOB," +
                         String(config->DUCO_USER) +
                         SEP_TOKEN + config->START_DIFF + 
                         SEP_TOKEN + String(config->MINER_KEY) + 
                         SEP_TOKEN + "Temp:" + String(temp) + "*C" +
                         IOT_TOKEN + "Hum:" + String(hum) + "%" +
                         END_TOKEN);
        #elif defined(USE_HSU07M)
            float temp = read_hsu07m();
            #if defined(SERIAL_PRINTING)
              Serial.println("HSU reading: " + String(temp) + "Â°C");
            #endif

            client.print("JOB," +
                         String(config->DUCO_USER) +
                         SEP_TOKEN + config->START_DIFF + 
                         SEP_TOKEN + String(config->MINER_KEY) + 
                         SEP_TOKEN + "Temp:" + String(temp) + "*C" +
                         END_TOKEN);
        #elif defined(USE_INTERNAL_SENSOR)
            float temp = 0;
            temp_sensor_read_celsius(&temp);
            #if defined(SERIAL_PRINTING)
              Serial.println("Internal temp sensor reading: " + String(temp) + "Â°C");
            #endif

            client.print("JOB," +
                         String(config->DUCO_USER) +
                         SEP_TOKEN + config->START_DIFF + 
                         SEP_TOKEN + String(config->MINER_KEY) + 
                         SEP_TOKEN + "CPU Temp:" + String(temp) + "*C" +
                         END_TOKEN);
        #else
            client.print("JOB," +
                         String(config->DUCO_USER) +
                         SEP_TOKEN + config->START_DIFF + 
                         SEP_TOKEN + String(config->MINER_KEY) + 
                         END_TOKEN);
        #endif

        waitForClientData();
        #if defined(SERIAL_PRINTING)
          Serial.println("Core [" + String(core) + "] - Received job with size of "
                          + String(client_buffer.length()) 
                          + " bytes " + client_buffer);
        #endif

        parse();
        #if defined(SERIAL_PRINTING)
          Serial.println("Core [" + String(core) + "] - Parsed job: " 
                          + getLastBlockHash() + " " 
                          + getExpectedHashStr() + " " 
                          + String(getDifficulty()));
        #endif
    }

    const String &getLastBlockHash() const { return last_block_hash; }
    const String &getExpectedHashStr() const { return expected_hash_str; }
    const uint8_t *getExpectedHash() const { return expected_hash; }
    unsigned int getDifficulty() const { return difficulty; }
};

#endif
