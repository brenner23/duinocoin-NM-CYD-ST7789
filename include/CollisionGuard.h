#ifndef COLLISION_GUARD_H
#define COLLISION_GUARD_H

#include <Arduino.h>

#if defined(ESP32) && !defined(CONFIG_FREERTOS_UNICORE)

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#ifndef COLLISION_DEBUG_ENABLED
#define COLLISION_DEBUG_ENABLED 1
#endif

// ============================================================
// MASTER 4.8 - CollisionGuard
//
// Zwei logische Tueren.
// Core 0 bevorzugt Tuer 0.
// Core 1 bevorzugt Tuer 1.
//
// Wichtig:
// - Kein FreeRTOS-Mutex mehr fuer die Tueren.
// - Nur sehr kurzer kritischer Abschnitt fuer Check-and-Set.
// - Kein Warten.
// - Keine Schleife.
// - Sind beide Tueren besetzt, wird genau EIN Vorgang verworfen.
// ============================================================

static portMUX_TYPE nm_collision_spinlock = portMUX_INITIALIZER_UNLOCKED;

static volatile bool nm_collision_door_busy[2] = {
    false,
    false
};

static volatile uint32_t nm_collision_accepted[2] = {
    0,
    0
};

static volatile uint32_t nm_collision_discarded[2] = {
    0,
    0
};


// ------------------------------------------------------------
// Initialisierung
// ------------------------------------------------------------
static inline void collision_guard_begin()
{
    portENTER_CRITICAL(&nm_collision_spinlock);

    nm_collision_door_busy[0] = false;
    nm_collision_door_busy[1] = false;

    portEXIT_CRITICAL(&nm_collision_spinlock);
}


// ------------------------------------------------------------
// Tuer versuchen zu belegen
//
// Rueckgabe:
//   0 = Tuer 0 belegt
//   1 = Tuer 1 belegt
//  -1 = beide Tueren besetzt
// ------------------------------------------------------------
static inline int8_t collision_try_enter(uint8_t core)
{
    uint8_t first;
    uint8_t second;

    if (core < 2) {
        first = core;
    } else {
        first = 0;
    }

    second = first ^ 1U;

    int8_t acquiredDoor = -1;

    // Der kritische Abschnitt umfasst absichtlich NUR
    // das atomare Pruefen und Setzen der beiden Flags.
    portENTER_CRITICAL(&nm_collision_spinlock);

    if (!nm_collision_door_busy[first]) {

        nm_collision_door_busy[first] = true;
        acquiredDoor = (int8_t)first;

    } else if (!nm_collision_door_busy[second]) {

        nm_collision_door_busy[second] = true;
        acquiredDoor = (int8_t)second;
    }

    portEXIT_CRITICAL(&nm_collision_spinlock);


#if COLLISION_DEBUG_ENABLED

    if (core < 2) {

        if (acquiredDoor >= 0) {
            __atomic_fetch_add(
                &nm_collision_accepted[core],
                1,
                __ATOMIC_RELAXED
            );
        } else {
            __atomic_fetch_add(
                &nm_collision_discarded[core],
                1,
                __ATOMIC_RELAXED
            );
        }
    }

#endif

    return acquiredDoor;
}


// ------------------------------------------------------------
// Tuer wieder freigeben
// ------------------------------------------------------------
static inline void collision_leave(int8_t door)
{
    if (door < 0 || door > 1) {
        return;
    }

    portENTER_CRITICAL(&nm_collision_spinlock);

    nm_collision_door_busy[(uint8_t)door] = false;

    portEXIT_CRITICAL(&nm_collision_spinlock);
}


// ------------------------------------------------------------
// Statistik loeschen
// ------------------------------------------------------------
static inline void collision_clear()
{
#if COLLISION_DEBUG_ENABLED

    __atomic_store_n(
        &nm_collision_accepted[0],
        0,
        __ATOMIC_RELAXED
    );

    __atomic_store_n(
        &nm_collision_accepted[1],
        0,
        __ATOMIC_RELAXED
    );

    __atomic_store_n(
        &nm_collision_discarded[0],
        0,
        __ATOMIC_RELAXED
    );

    __atomic_store_n(
        &nm_collision_discarded[1],
        0,
        __ATOMIC_RELAXED
    );

#endif
}


// ------------------------------------------------------------
// Prozent verworfener Zugriffe
// ------------------------------------------------------------
static inline String collision_percent(
    uint32_t ok,
    uint32_t drop
)
{
    uint32_t total = ok + drop;

    if (total == 0) {
        return "0.00";
    }

    return String(
        (100.0 * (double)drop) / (double)total,
        2
    );
}


// ------------------------------------------------------------
// HTML fuer Weboberflaeche
// ------------------------------------------------------------
static inline String collision_html()
{
#if COLLISION_DEBUG_ENABLED

    uint32_t a0 = __atomic_load_n(
        &nm_collision_accepted[0],
        __ATOMIC_RELAXED
    );

    uint32_t a1 = __atomic_load_n(
        &nm_collision_accepted[1],
        __ATOMIC_RELAXED
    );

    uint32_t d0 = __atomic_load_n(
        &nm_collision_discarded[0],
        __ATOMIC_RELAXED
    );

    uint32_t d1 = __atomic_load_n(
        &nm_collision_discarded[1],
        __ATOMIC_RELAXED
    );


    uint32_t at = a0 + a1;
    uint32_t dt = d0 + d1;


    String out;

    out.reserve(700);

    out += "<div class='crash-entry'>";
    out += "<div class='crash-grid'>";

    out += "<span>Core 0 angenommen</span>";
    out += "<b>";
    out += String(a0);
    out += "</b>";

    out += "<span>Core 0 verworfen</span>";
    out += "<b>";
    out += String(d0);
    out += " (";
    out += collision_percent(a0, d0);
    out += "%)</b>";


    out += "<span>Core 1 angenommen</span>";
    out += "<b>";
    out += String(a1);
    out += "</b>";

    out += "<span>Core 1 verworfen</span>";
    out += "<b>";
    out += String(d1);
    out += " (";
    out += collision_percent(a1, d1);
    out += "%)</b>";


    out += "<span>Gesamt angenommen</span>";
    out += "<b>";
    out += String(at);
    out += "</b>";

    out += "<span>Gesamt verworfen</span>";
    out += "<b>";
    out += String(dt);
    out += " (";
    out += collision_percent(at, dt);
    out += "%)</b>";

    out += "</div>";
    out += "</div>";

    return out;

#else

    return
        "<div class='crash-empty'>"
        "Kollisions-Debug ist deaktiviert."
        "</div>";

#endif
}


#else

// ============================================================
// Single-Core / Nicht-ESP32
// ============================================================

static inline void collision_guard_begin()
{
}

static inline int8_t collision_try_enter(uint8_t core)
{
    return (core < 2)
        ? (int8_t)core
        : 0;
}

static inline void collision_leave(int8_t)
{
}

static inline void collision_clear()
{
}

static inline String collision_html()
{
    return
        "<div class='crash-empty'>"
        "Kollisionsschutz nur bei Dual-Core ESP32."
        "</div>";
}

#endif

#endif