#ifndef MINING_MAILBOX_H
#define MINING_MAILBOX_H

#include <Arduino.h>
#include "Settings.h"

#if defined(ESP32) && !defined(CONFIG_FREERTOS_UNICORE)
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// BRIEFKASTEN FIX02: zwei getrennte 8er-Briefkaesten, je einer pro Mining-Core.
// Die Mining-Cores schreiben nur Ereignisse hinein. Ein einzelner Auswerter
// aktualisiert die gemeinsamen Share-Zaehler. Kein Core schreibt mehr direkt
// gleichzeitig auf share_count / accepted_share_count / rejected_share_count.
#define NM_MINING_MAILBOX_DEPTH 8

enum NmMiningEventType : uint8_t {
    NM_EVT_SHARE_FOUND = 1,
    NM_EVT_ACCEPTED    = 2,
    NM_EVT_REJECTED    = 3
};

struct NmMiningEvent {
    uint8_t type;
};

static QueueHandle_t nm_mining_mailbox[2] = {nullptr, nullptr};
static TaskHandle_t nm_mining_mailbox_task = nullptr;
static volatile uint32_t nm_mining_mailbox_full[2] = {0, 0};

static inline void nm_mining_apply_event(const NmMiningEvent &ev) {
    switch (ev.type) {
        case NM_EVT_SHARE_FOUND: share_count++; break;
        case NM_EVT_ACCEPTED:    accepted_share_count++; break;
        case NM_EVT_REJECTED:    rejected_share_count++; break;
        default: break;
    }

    if (share_count > 9999999UL || accepted_share_count > 9999999UL || rejected_share_count > 9999999UL) {
        share_count = 0;
        accepted_share_count = 0;
        rejected_share_count = 0;
    }
}

static inline bool nm_mining_post(uint8_t core, NmMiningEventType type) {
    if (core > 1 || nm_mining_mailbox[core] == nullptr) return false;
    NmMiningEvent ev{(uint8_t)type};
    if (xQueueSend(nm_mining_mailbox[core], &ev, 0) == pdTRUE) return true;
    __atomic_fetch_add(&nm_mining_mailbox_full[core], 1, __ATOMIC_RELAXED);
    return false;
}

static void nm_mining_mailbox_task_func(void *parameter) {
    NmMiningEvent ev;
    for (;;) {
        bool didWork = false;
        for (uint8_t core = 0; core < 2; ++core) {
            if (nm_mining_mailbox[core] != nullptr) {
                while (xQueueReceive(nm_mining_mailbox[core], &ev, 0) == pdTRUE) {
                    nm_mining_apply_event(ev);
                    didWork = true;
                }
            }
        }
        if (!didWork) ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
        else taskYIELD();
    }
}

static inline void nm_mining_mailbox_begin() {
    if (nm_mining_mailbox_task != nullptr) return;
    if (nm_mining_mailbox[0] == nullptr) nm_mining_mailbox[0] = xQueueCreate(NM_MINING_MAILBOX_DEPTH, sizeof(NmMiningEvent));
    if (nm_mining_mailbox[1] == nullptr) nm_mining_mailbox[1] = xQueueCreate(NM_MINING_MAILBOX_DEPTH, sizeof(NmMiningEvent));
    if (nm_mining_mailbox[0] == nullptr || nm_mining_mailbox[1] == nullptr) return;
    xTaskCreatePinnedToCore(nm_mining_mailbox_task_func, "mailbox_eval", 3072, nullptr, 1, &nm_mining_mailbox_task, 0);
}

static inline void nm_mining_signal(uint8_t core, NmMiningEventType type) {
    if (nm_mining_post(core, type) && nm_mining_mailbox_task != nullptr) xTaskNotifyGive(nm_mining_mailbox_task);
}

static inline uint32_t nm_mining_mailbox_dropped(uint8_t core) {
    if (core > 1) return 0;
    return __atomic_load_n(&nm_mining_mailbox_full[core], __ATOMIC_RELAXED);
}

#else
static inline void nm_mining_mailbox_begin() {}
static inline void nm_mining_signal(uint8_t, uint8_t) {}
#endif

#endif
