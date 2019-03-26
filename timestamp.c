#include "app_timer.h"
#include "app_util_platform.h"
#include "timestamp.h"

#define TIMESTAMP_PRECISION_MILLIS 1

// timestamp timer
APP_TIMER_DEF(m_timer_timestamp_ms);
uint32_t m_timestamp_ms = 0;

void timer_timestamp_ms_handler(void* p_context) {
    CRITICAL_REGION_ENTER();
    m_timestamp_ms+=TIMESTAMP_PRECISION_MILLIS;
    CRITICAL_REGION_EXIT();
}


void timestamp_init() {
    app_timer_create(&m_timer_timestamp_ms, APP_TIMER_MODE_REPEATED, timer_timestamp_ms_handler);
    app_timer_start(m_timer_timestamp_ms, APP_TIMER_TICKS(TIMESTAMP_PRECISION_MILLIS), NULL);
}

uint32_t timestamp_get() {
    uint32_t result = 0;
    CRITICAL_REGION_ENTER();
    result = m_timestamp_ms;
    CRITICAL_REGION_EXIT();
    return result;
}

void timestamp_reset() {
    CRITICAL_REGION_ENTER();
    m_timestamp_ms = 0;
    CRITICAL_REGION_EXIT();
}
