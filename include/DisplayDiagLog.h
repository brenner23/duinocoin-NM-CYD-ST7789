#ifndef DISPLAY_DIAG_LOG_H
#define DISPLAY_DIAG_LOG_H

#if defined(ESP32) && defined(DISPLAY_CYD)
#include <Arduino.h>
#include <Preferences.h>

#define DISPLAYDIAG_MAX_RECORDS 10

struct DisplayDiagRecord {
  uint32_t sequence;
  uint32_t uptimeMs;
  time_t epoch;       // 0 solange NTP/Systemzeit noch nicht gueltig ist
  uint8_t code;      // 1 = unexpected core, 2 = TFT mutex collision
  int8_t core;
  int8_t ownerCore;
};

static DisplayDiagRecord nm_displaydiag_records[DISPLAYDIAG_MAX_RECORDS] = {};
static uint8_t nm_displaydiag_count = 0;
static uint32_t nm_displaydiag_next_sequence = 1;
static bool nm_displaydiag_enabled = true;
static bool nm_displaydiag_ready = false;

static void nm_displaydiag_save() {
  Preferences p;
  if (!p.begin("nm_tftdiag", false)) return;
  p.putBool("enabled", nm_displaydiag_enabled);
  p.putUChar("count", nm_displaydiag_count);
  p.putUInt("nextseq", nm_displaydiag_next_sequence);
  p.putBytes("records", nm_displaydiag_records, sizeof(nm_displaydiag_records));
  p.end();
}

static void nm_displaydiag_begin() {
  if (nm_displaydiag_ready) return;
  Preferences p;
  if (p.begin("nm_tftdiag", true)) {
    nm_displaydiag_enabled = p.getBool("enabled", true);
    nm_displaydiag_count = p.getUChar("count", 0);
    nm_displaydiag_next_sequence = p.getUInt("nextseq", 1);
    if (nm_displaydiag_count > DISPLAYDIAG_MAX_RECORDS) nm_displaydiag_count = DISPLAYDIAG_MAX_RECORDS;
    if (nm_displaydiag_next_sequence == 0) nm_displaydiag_next_sequence = 1;
    if (p.getBytesLength("records") == sizeof(nm_displaydiag_records)) {
      p.getBytes("records", nm_displaydiag_records, sizeof(nm_displaydiag_records));
    } else {
      memset(nm_displaydiag_records, 0, sizeof(nm_displaydiag_records));
      nm_displaydiag_count = 0;
    }
    p.end();
  }
  nm_displaydiag_ready = true;
}

static inline bool nm_displaydiag_is_enabled() {
  nm_displaydiag_begin();
  return nm_displaydiag_enabled;
}

static void nm_displaydiag_set_enabled(bool enabled) {
  nm_displaydiag_begin();
  nm_displaydiag_enabled = enabled;
  nm_displaydiag_save();
}

static void nm_displaydiag_clear() {
  nm_displaydiag_begin();
  memset(nm_displaydiag_records, 0, sizeof(nm_displaydiag_records));
  nm_displaydiag_count = 0;
  nm_displaydiag_next_sequence = 1;
  nm_displaydiag_save();
}

static void nm_displaydiag_event(uint8_t code, int8_t core, int8_t ownerCore = -1) {
  nm_displaydiag_begin();
  if (!nm_displaydiag_enabled) return;
  // Absichtlich bei 10 stoppen: kein permanentes NVS-Schreiben bei einem Dauerfehler.
  if (nm_displaydiag_count >= DISPLAYDIAG_MAX_RECORDS) return;
  DisplayDiagRecord &r = nm_displaydiag_records[nm_displaydiag_count++];
  r.sequence = nm_displaydiag_next_sequence++;
  r.uptimeMs = millis();
  time_t nowEpoch = time(nullptr);
  r.epoch = (nowEpoch > 1700000000) ? nowEpoch : 0;
  r.code = code;
  r.core = core;
  r.ownerCore = ownerCore;
  nm_displaydiag_save();
}

static String nm_displaydiag_uptime(uint32_t ms) {
  uint32_t sec = ms / 1000UL;
  uint32_t h = sec / 3600UL;
  uint32_t m = (sec % 3600UL) / 60UL;
  uint32_t s = sec % 60UL;
  char b[24];
  snprintf(b, sizeof(b), "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
  return String(b);
}

static String nm_displaydiag_datetime(time_t epoch) {
  if (epoch <= 1700000000) return "NICHT_SYNCHRONISIERT";
  struct tm ti;
  localtime_r(&epoch, &ti);
  char b[24];
  strftime(b, sizeof(b), "%d.%m.%Y %H:%M:%S", &ti);
  return String(b);
}

static const char *nm_displaydiag_name(uint8_t code) {
  switch (code) {
    case 1: return "UNERWARTETER TFT-CORE";
    case 2: return "TFT-MUTEX-KOLLISION";
    default: return "UNBEKANNT";
  }
}

static String nm_displaydiag_html() {
  nm_displaydiag_begin();
  if (nm_displaydiag_count == 0) return "<div class='crash-empty'>Keine Display-Fehler gespeichert.</div>";
  String out;
  for (int i = (int)nm_displaydiag_count - 1; i >= 0; --i) {
    const DisplayDiagRecord &r = nm_displaydiag_records[i];
    out += "<div class='crash-entry'><div class='crash-head'>TFT #" + String(r.sequence) + " · E" + (r.code < 10 ? String("0") : String("")) + String(r.code) + "</div>";
    out += "<div class='crash-grid'><span>Fehler</span><b>" + String(nm_displaydiag_name(r.code)) + "</b>";
    out += "<span>Zeit</span><b>" + nm_displaydiag_datetime(r.epoch) + "</b>";
    out += "<span>Uptime</span><b>" + nm_displaydiag_uptime(r.uptimeMs) + "</b>";
    out += "<span>Core</span><b>" + String((int)r.core) + "</b>";
    if (r.code == 2) out += "<span>Besitzer-Core</span><b>" + String((int)r.ownerCore) + "</b>";
    out += "</div></div>";
  }
  return out;
}
#endif
#endif
