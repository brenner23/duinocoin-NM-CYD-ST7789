#ifndef CRASHLOG_H
#define CRASHLOG_H

#if defined(ESP32) && defined(CRASHLOG_ENABLED) && CRASHLOG_ENABLED

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>

#define CRASHLOG_MAX_RECORDS 10
#define CRASHLOG_RTC_MAGIC 0x4E4D4352UL  // "NMCR"
#define CRASHLOG_NVS_MAGIC 0x434C4F47UL  // "CLOG"

struct CrashRtcState {
    uint32_t magic;
    uint32_t shares;
    uint32_t accepted;
    uint32_t uptimeMs;
    uint16_t bootCheckpoint;
    uint16_t checkpointCore0;
    uint16_t checkpointCore1;
    uint16_t hashLenCore0;
    uint16_t hashLenCore1;
    int8_t lastCore;
    int16_t tempCurrent10;
    int16_t tempMax10;
    uint32_t epoch;
    uint8_t reserved[3];
};

struct CrashRecord {
    uint32_t magic;
    uint32_t sequence;
    uint32_t resetReason;
    uint32_t shares;
    uint32_t accepted;
    uint32_t uptimeMs;
    uint16_t bootCheckpoint;
    uint16_t checkpointCore0;
    uint16_t checkpointCore1;
    uint16_t hashLenCore0;
    uint16_t hashLenCore1;
    int8_t lastCore;
    int16_t tempCurrent10;
    int16_t tempMax10;
    uint32_t epoch;
    uint8_t reserved[3];
};

RTC_NOINIT_ATTR static CrashRtcState crashRtc;
static CrashRecord crashRecords[CRASHLOG_MAX_RECORDS] = {};
static uint8_t crashRecordCount = 0;
static uint32_t crashNextSequence = 1;

static bool crashlog_is_crash_reason(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
            return true;
        default:
            return false;
    }
}

static const char *crashlog_reason_name(uint32_t reason) {
    switch ((esp_reset_reason_t)reason) {
        case ESP_RST_POWERON:  return "POWER_ON";
        case ESP_RST_EXT:      return "RESET_TASTER";
        case ESP_RST_SW:       return "SOFTWARE_RESET";
        case ESP_RST_PANIC:    return "PANIC / GURU MEDITATION";
        case ESP_RST_INT_WDT:  return "INTERRUPT_WATCHDOG";
        case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
        case ESP_RST_WDT:      return "WATCHDOG";
        case ESP_RST_DEEPSLEEP:return "DEEP_SLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO:     return "SDIO_RESET";
        default:               return "UNBEKANNT";
    }
}

static const char *crashlog_boot_checkpoint_name(uint16_t cp) {
    switch (cp) {
        case 0:   return "NICHT_ERREICHT";
        case 100: return "BOOT_START";
        case 110: return "CRASHLOG_INIT";
        case 120: return "CRASHLOG_READY";
        case 130: return "DISPLAY_INIT";
        case 140: return "DISPLAY_READY";
        case 150: return "MINER0_INIT";
        case 160: return "MINER0_READY";
        case 170: return "WIFI_START";
        case 180: return "WIFI_CONNECTED_NODE_READY";
        case 190: return "NMTV_SERVICES_START";
        case 200: return "NMTV_SERVICES_READY";
        case 210: return "OTA_START";
        case 220: return "OTA_READY";
        case 230: return "WEB_START";
        case 240: return "WEB_READY";
        case 250: return "TASKS_CREATE";
        case 260: return "TASKS_CREATED";
        case 270: return "SETUP_DONE";
        default:  return "BOOT_CHECKPOINT_UNBEKANNT";
    }
}

static const char *crashlog_checkpoint_name(uint16_t cp) {
    switch (cp) {
        case 0:  return "IDLE";
        case 10: return "MINE_START";
        case 20: return "JOB_GELADEN";
        case 30: return "DSHA1_WRITE_BLOCKHASH";
        case 40: return "DSHA1_NONCE_LOOP";
        case 50: return "SHARE_SENDEN";
        case 60: return "MINE_FERTIG";
        case 70: return "TRYLOCK_BUSY_JOB";
        case 71: return "TRYLOCK_JOB_ENTER";
        case 72: return "TRYLOCK_JOB_LEAVE";
        case 73: return "TRYLOCK_BUSY_SUBMIT";
        case 74: return "TRYLOCK_SUBMIT_ENTER";
        case 75: return "TRYLOCK_SUBMIT_LEAVE";
        default: return "CHECKPOINT_UNBEKANNT";
    }
}

static void crashlog_load_records() {
    Preferences p;
    p.begin("nm_crashlog", true);
    size_t len = p.getBytesLength("records");
    if (len == sizeof(crashRecords)) {
        p.getBytes("records", crashRecords, sizeof(crashRecords));
    } else {
        memset(crashRecords, 0, sizeof(crashRecords));
    }
    crashRecordCount = p.getUChar("count", 0);
    crashNextSequence = p.getUInt("nextseq", 1);
    p.end();
    if (crashRecordCount > CRASHLOG_MAX_RECORDS) crashRecordCount = CRASHLOG_MAX_RECORDS;
    if (crashNextSequence == 0) crashNextSequence = 1;
}

static void crashlog_save_records() {
    Preferences p;
    p.begin("nm_crashlog", false);
    p.putBytes("records", crashRecords, sizeof(crashRecords));
    p.putUChar("count", crashRecordCount);
    p.putUInt("nextseq", crashNextSequence);
    p.end();
}

static void crashlog_append(esp_reset_reason_t reason, const CrashRtcState &previous) {
    CrashRecord r = {};
    r.magic = CRASHLOG_NVS_MAGIC;
    r.sequence = crashNextSequence++;
    r.resetReason = (uint32_t)reason;

    if (previous.magic == CRASHLOG_RTC_MAGIC) {
        r.shares = previous.shares;
        r.accepted = previous.accepted;
        r.uptimeMs = previous.uptimeMs;
        r.bootCheckpoint = previous.bootCheckpoint;
        r.checkpointCore0 = previous.checkpointCore0;
        r.checkpointCore1 = previous.checkpointCore1;
        r.hashLenCore0 = previous.hashLenCore0;
        r.hashLenCore1 = previous.hashLenCore1;
        r.lastCore = previous.lastCore;
        r.tempCurrent10 = previous.tempCurrent10;
        r.tempMax10 = previous.tempMax10;
        r.epoch = previous.epoch;
    } else {
        r.lastCore = -1;
    }

    if (crashRecordCount < CRASHLOG_MAX_RECORDS) {
        crashRecords[crashRecordCount++] = r;
    } else {
        memmove(&crashRecords[0], &crashRecords[1], sizeof(CrashRecord) * (CRASHLOG_MAX_RECORDS - 1));
        crashRecords[CRASHLOG_MAX_RECORDS - 1] = r;
    }
    crashlog_save_records();
}

static void crashlog_begin() {
    // Vorherigen RTC-Zustand sichern, dann SOFORT den neuen Boot markieren.
    // Falls der ESP bereits waehrend NVS/Crashlog-Init stirbt, sehen wir beim
    // naechsten Start wenigstens CRASHLOG_INIT statt nur IDLE/-1.
    const CrashRtcState previous = crashRtc;
    memset(&crashRtc, 0, sizeof(crashRtc));
    crashRtc.magic = CRASHLOG_RTC_MAGIC;
    crashRtc.lastCore = -1;
    crashRtc.bootCheckpoint = 110; // CRASHLOG_INIT
    crashRtc.uptimeMs = millis();

    crashlog_load_records();
    const esp_reset_reason_t reason = esp_reset_reason();
    if (crashlog_is_crash_reason(reason)) {
        crashlog_append(reason, previous);
    }

    crashRtc.bootCheckpoint = 120; // CRASHLOG_READY
    crashRtc.uptimeMs = millis();
}

static inline void crashlog_boot_checkpoint(uint16_t checkpoint) {
    crashRtc.bootCheckpoint = checkpoint;
    crashRtc.uptimeMs = millis();
}

static inline void crashlog_checkpoint(uint8_t core, uint16_t checkpoint, uint16_t hashLen = 0) {
    if (core == 0) {
        crashRtc.checkpointCore0 = checkpoint;
        if (hashLen) crashRtc.hashLenCore0 = hashLen;
    } else if (core == 1) {
        crashRtc.checkpointCore1 = checkpoint;
        if (hashLen) crashRtc.hashLenCore1 = hashLen;
    }
    crashRtc.lastCore = (int8_t)core;
}

static inline void crashlog_heartbeat(uint32_t shares, uint32_t accepted, uint32_t uptimeMs, float tempCurrent = 0.0f, float tempMax = 0.0f) {
    crashRtc.shares = shares;
    crashRtc.accepted = accepted;
    crashRtc.uptimeMs = uptimeMs;
    crashRtc.tempCurrent10 = (int16_t)(tempCurrent * 10.0f);
    crashRtc.tempMax10 = (int16_t)(tempMax * 10.0f);
    time_t now = time(nullptr);
    crashRtc.epoch = (now > 1700000000) ? (uint32_t)now : 0;
}

static String crashlog_format_uptime(uint32_t ms) {
    uint32_t total = ms / 1000UL;
    uint32_t days = total / 86400UL;
    uint8_t hours = (total / 3600UL) % 24;
    uint8_t mins = (total / 60UL) % 60;
    uint8_t secs = total % 60;
    char buf[32];
    if (days) snprintf(buf, sizeof(buf), "%lud %02u:%02u:%02u", (unsigned long)days, hours, mins, secs);
    else snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hours, mins, secs);
    return String(buf);
}


static String crashlog_format_time(uint32_t epoch) {
    if (epoch == 0) return "NICHT_SYNCHRONISIERT";
    time_t t = (time_t)epoch;
    struct tm ti;
    localtime_r(&t, &ti);
    char b[32];
    strftime(b, sizeof(b), "%d.%m.%Y %H:%M:%S", &ti);
    return String(b);
}

static String crashlog_html() {
    if (crashRecordCount == 0) {
        return "<div class='crash-empty'>Keine Absturzberichte gespeichert.</div>";
    }

    String out;
    out.reserve(3500);
    for (int i = crashRecordCount - 1; i >= 0; --i) {
        const CrashRecord &r = crashRecords[i];
        if (r.magic != CRASHLOG_NVS_MAGIC) continue;
        out += "<div class='crash-entry'><div class='crash-head'>CRASH #" + String(r.sequence) + "</div>";
        out += "<div class='crash-grid'>";
        out += "<span>Ursache</span><b>" + String(crashlog_reason_name(r.resetReason)) + "</b>";
        out += "<span>Shares</span><b>" + String(r.shares) + "</b>";
        out += "<span>Accepted</span><b>" + String(r.accepted) + "</b>";
        out += "<span>Zeit</span><b>" + crashlog_format_time(r.epoch) + "</b>";
        out += "<span>Uptime</span><b>" + crashlog_format_uptime(r.uptimeMs) + "</b>";
        out += "<span>CPU zuletzt</span><b>" + String(r.tempCurrent10 / 10.0f, 1) + " &deg;C</b>";
        out += "<span>CPU Maximum</span><b>" + String(r.tempMax10 / 10.0f, 1) + " &deg;C</b>";
        out += "<span>Boot-Checkpoint</span><b>" + String(crashlog_boot_checkpoint_name(r.bootCheckpoint)) + " (" + String(r.bootCheckpoint) + ")</b>";
        out += "<span>Letzter Core</span><b>" + String((int)r.lastCore) + "</b>";
        out += "<span>Core 0</span><b>" + String(crashlog_checkpoint_name(r.checkpointCore0)) + " (" + String(r.checkpointCore0) + ")</b>";
        out += "<span>HashLen C0</span><b>" + String(r.hashLenCore0) + "</b>";
        out += "<span>Core 1</span><b>" + String(crashlog_checkpoint_name(r.checkpointCore1)) + " (" + String(r.checkpointCore1) + ")</b>";
        out += "<span>HashLen C1</span><b>" + String(r.hashLenCore1) + "</b>";
        out += "</div></div>";
    }
    return out;
}

static void crashlog_clear() {
    memset(crashRecords, 0, sizeof(crashRecords));
    crashRecordCount = 0;
    crashNextSequence = 1;
    Preferences p;
    p.begin("nm_crashlog", false);
    p.clear();
    p.end();
}

#else

static inline void crashlog_begin() {}
static inline void crashlog_boot_checkpoint(uint16_t) {}
static inline void crashlog_checkpoint(uint8_t, uint16_t, uint16_t = 0) {}
static inline void crashlog_heartbeat(uint32_t, uint32_t, uint32_t) {}
static inline String crashlog_html() { return "<div class='crash-empty'>Crashlog ist deaktiviert.</div>"; }
static inline void crashlog_clear() {}

#endif

#endif
